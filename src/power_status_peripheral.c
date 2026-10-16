/* SPDX-License-Identifier: MIT */
#include <hal/nrf_power.h>
#include <prospector/power_status.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/kernel.h>

/* On USB removal, send a fresh battery level before reporting BATTERY, so the
 * dongle never shows the USB-inflated value for up to a full report interval. */
#if defined(CONFIG_ZMK_BATTERY_REPORTING_FETCH_MODE_STATE_OF_CHARGE) && defined(CONFIG_BT_BAS) &&  \
    DT_HAS_CHOSEN(zmk_battery)
#define POWER_REFRESH_BATTERY 1
#include <zephyr/bluetooth/services/bas.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/sensor.h>
#include <zmk/workqueue.h>
#endif

static struct bt_uuid_128 service_uuid = BT_UUID_INIT_128(PROSPECTOR_POWER_SERVICE_UUID);
static struct bt_uuid_128 state_uuid = BT_UUID_INIT_128(PROSPECTOR_POWER_STATE_UUID);
static struct bt_uuid_128 side_uuid = BT_UUID_INIT_128(PROSPECTOR_SIDE_UUID);

static ssize_t side_read(struct bt_conn *conn, const struct bt_gatt_attr *attr, void *buf,
                         uint16_t len, uint16_t offset) {
    const uint8_t side = CONFIG_PROSPECTOR_SPLIT_SIDE;
    return bt_gatt_attr_read(conn, attr, buf, len, offset, &side, sizeof(side));
}
static struct bt_conn *power_conn;
static bool subscribed;
static uint8_t delivered = PROSPECTOR_POWER_UNKNOWN;
static uint32_t generation;
K_MUTEX_DEFINE(power_lock);

static void power_work_handler(struct k_work *work);
K_WORK_DELAYABLE_DEFINE(power_work, power_work_handler);

#ifdef POWER_REFRESH_BATTERY
#define BATTERY_SETTLE_MS 200
static bool refresh_pending, refresh_done;
static uint32_t refresh_generation;

/* Runs on ZMK's low-priority queue, the same queue as ZMK's own battery
 * sampler, so the two never use the battery sensor concurrently. */
static void battery_refresh_handler(struct k_work *work) {
    ARG_UNUSED(work);
    const struct device *battery = DEVICE_DT_GET(DT_CHOSEN(zmk_battery));
    struct sensor_value level;
    if (device_is_ready(battery) &&
        sensor_sample_fetch_chan(battery, SENSOR_CHAN_GAUGE_STATE_OF_CHARGE) == 0 &&
        sensor_channel_get(battery, SENSOR_CHAN_GAUGE_STATE_OF_CHARGE, &level) == 0 &&
        level.val1 >= 0 && level.val1 <= 100) {
        bt_bas_set_battery_level((uint8_t)level.val1);
    }
    k_mutex_lock(&power_lock, K_FOREVER);
    if (refresh_pending && refresh_generation == generation) {
        refresh_pending = false;
        refresh_done = true;
        k_work_reschedule(&power_work, K_MSEC(0));
    }
    k_mutex_unlock(&power_lock);
}
K_WORK_DELAYABLE_DEFINE(battery_refresh, battery_refresh_handler);

static void cancel_refresh_locked(void) {
    refresh_pending = false;
    refresh_done = false;
    k_work_cancel_delayable(&battery_refresh);
}
#else
static void cancel_refresh_locked(void) {}
#endif

static uint8_t sample_power(void) {
    return nrf_power_usbregstatus_vbusdet_get(NRF_POWER) ? PROSPECTOR_POWER_USB
                                                         : PROSPECTOR_POWER_BATTERY;
}

static ssize_t power_read(struct bt_conn *conn, const struct bt_gatt_attr *attr, void *buf,
                          uint16_t len, uint16_t offset) {
    uint8_t state = sample_power();
    return bt_gatt_attr_read(conn, attr, buf, len, offset, &state, sizeof(state));
}

static void power_ccc_changed(const struct bt_gatt_attr *attr, uint16_t value) {
    ARG_UNUSED(attr);
    k_mutex_lock(&power_lock, K_FOREVER);
    subscribed = value == BT_GATT_CCC_NOTIFY;
    generation++;
    delivered = PROSPECTOR_POWER_UNKNOWN;
    cancel_refresh_locked();
    if (subscribed && power_conn)
        k_work_reschedule(&power_work, K_MSEC(0));
    else
        k_work_cancel_delayable(&power_work);
    k_mutex_unlock(&power_lock);
}

/* Static services sort by symbol name. Append after split_svc to preserve the
 * numerical CCC handles already stored for bonded split peers. */
BT_GATT_SERVICE_DEFINE(zz_prospector_power_service, BT_GATT_PRIMARY_SERVICE(&service_uuid),
                       BT_GATT_CHARACTERISTIC(&state_uuid.uuid,
                                              BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY,
                                              BT_GATT_PERM_READ_ENCRYPT, power_read, NULL, NULL),
                       BT_GATT_CCC(power_ccc_changed,
                                   BT_GATT_PERM_READ_ENCRYPT | BT_GATT_PERM_WRITE_ENCRYPT),
                       /* Appended after the power CCC so existing handles never move. */
                       BT_GATT_CHARACTERISTIC(&side_uuid.uuid, BT_GATT_CHRC_READ,
                                              BT_GATT_PERM_READ_ENCRYPT, side_read, NULL, NULL));

static void power_work_handler(struct k_work *work) {
    ARG_UNUSED(work);
    k_mutex_lock(&power_lock, K_FOREVER);
    if (!subscribed || !power_conn) {
        k_mutex_unlock(&power_lock);
        return;
    }
    uint8_t state = sample_power();
#ifdef POWER_REFRESH_BATTERY
    if (state == PROSPECTOR_POWER_USB) {
        cancel_refresh_locked();
    } else if (delivered == PROSPECTOR_POWER_USB && !refresh_done) {
        if (!refresh_pending) {
            refresh_pending = true;
            refresh_generation = generation;
            k_work_reschedule_for_queue(zmk_workqueue_lowprio_work_q(), &battery_refresh,
                                        K_MSEC(BATTERY_SETTLE_MS));
        }
        k_work_reschedule(&power_work, K_MSEC(1000));
        k_mutex_unlock(&power_lock);
        return;
    }
#endif
    struct bt_conn *conn = bt_conn_ref(power_conn);
    uint32_t current = generation;
    bool changed = state != delivered;
    k_mutex_unlock(&power_lock);
    int err =
        changed ? bt_gatt_notify(conn, &zz_prospector_power_service.attrs[2], &state, sizeof(state))
                : 0;
    k_mutex_lock(&power_lock, K_FOREVER);
    if (subscribed && power_conn == conn && generation == current) {
        if (!err) {
            delivered = state;
#ifdef POWER_REFRESH_BATTERY
            if (state == PROSPECTOR_POWER_BATTERY)
                refresh_done = false;
#endif
        }
        k_work_reschedule(&power_work, K_MSEC(1000));
    }
    k_mutex_unlock(&power_lock);
    bt_conn_unref(conn);
}

static void power_connected(struct bt_conn *conn, uint8_t err) {
    struct bt_conn_info info;
    if (err || bt_conn_get_info(conn, &info) || info.role != BT_CONN_ROLE_PERIPHERAL)
        return;
    k_mutex_lock(&power_lock, K_FOREVER);
    if (!power_conn)
        power_conn = bt_conn_ref(conn);
    if (subscribed)
        k_work_reschedule(&power_work, K_MSEC(0));
    k_mutex_unlock(&power_lock);
}

static void power_disconnected(struct bt_conn *conn, uint8_t reason) {
    ARG_UNUSED(reason);
    k_mutex_lock(&power_lock, K_FOREVER);
    if (power_conn == conn) {
        subscribed = false;
        delivered = PROSPECTOR_POWER_UNKNOWN;
        generation++;
        k_work_cancel_delayable(&power_work);
        cancel_refresh_locked();
        power_conn = NULL;
        bt_conn_unref(conn);
    }
    k_mutex_unlock(&power_lock);
}

BT_CONN_CB_DEFINE(prospector_power_callbacks) = {
    .connected = power_connected,
    .disconnected = power_disconnected,
};

/* SPDX-License-Identifier: MIT */
#include <prospector/peripheral_slots.h>
#include <prospector/power_status.h>
#include <string.h>
#include <zephyr/bluetooth/att.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/init.h>
#include <zephyr/kernel.h>
#include <zmk/ble.h>
#include <zmk/events/peripheral_power_state_changed.h>

static struct bt_uuid_128 service_uuid = BT_UUID_INIT_128(PROSPECTOR_POWER_SERVICE_UUID);
static struct bt_uuid_128 state_uuid = BT_UUID_INIT_128(PROSPECTOR_POWER_STATE_UUID);
/* BT_UUID_GATT_CCC is a block-scope compound literal; Zephyr compares the filter
 * when the Find Information reply arrives, after the callback has returned. */
static const struct bt_uuid_16 ccc_uuid = BT_UUID_INIT_16(BT_UUID_GATT_CCC_VAL);
/* Read-by-UUID filter: must also outlive the request (see ccc_uuid). */
static struct bt_uuid_128 side_uuid = BT_UUID_INIT_128(PROSPECTOR_SIDE_UUID);
/* Owned per connection, not per reusable peripheral slot. ATT pending requests
 * are completed with errors during disconnect; keep references until then. */
enum retry_operation { RETRY_DISCOVER, RETRY_SUBSCRIBE, RETRY_READ, RETRY_SIDE };
struct power_client {
    struct bt_conn *conn;
    int slot;
    bool live, discovering, reading, subscribing, notified;
    bool subscribed;
    bool side_reading, side_done;
    uint16_t svc_start, svc_end;
    struct bt_gatt_read_params side_read;
    enum retry_operation retry_operation;
    int64_t retry_at;
    uint32_t notifications, read_notifications;
    uint8_t retries;
    uint16_t api_calls;
    struct bt_gatt_discover_params discovery;
    struct bt_gatt_subscribe_params subscription;
    struct bt_gatt_read_params read;
};
static struct power_client clients[CONFIG_BT_MAX_CONN];
static struct power_client *selected[ZMK_SPLIT_BLE_PERIPHERAL_COUNT];
static enum prospector_power_state states[ZMK_SPLIT_BLE_PERIPHERAL_COUNT];
static struct prospector_power_diagnostic diagnostics[ZMK_SPLIT_BLE_PERIPHERAL_COUNT];
/* Reported side per slot; kept across disconnects (slot addresses persist). */
static enum prospector_split_side sides[ZMK_SPLIT_BLE_PERIPHERAL_COUNT];
K_MUTEX_DEFINE(state_lock);
K_MUTEX_DEFINE(client_lock);
static struct k_work_q retry_queue;
K_THREAD_STACK_DEFINE(retry_stack, 1536);
static bool retry_queue_ready;
static void retry_work_handler(struct k_work *work);
K_WORK_DELAYABLE_DEFINE(retry_work, retry_work_handler);

static uint8_t power_discovered(struct bt_conn *, const struct bt_gatt_attr *,
                                struct bt_gatt_discover_params *);
static uint8_t power_notify(struct bt_conn *, struct bt_gatt_subscribe_params *, const void *,
                            uint16_t);
static uint8_t power_read_complete(struct bt_conn *, uint8_t, struct bt_gatt_read_params *,
                                   const void *, uint16_t);
static void power_subscribed(struct bt_conn *, uint8_t, struct bt_gatt_subscribe_params *);
static uint8_t side_read_complete(struct bt_conn *, uint8_t, struct bt_gatt_read_params *,
                                  const void *, uint16_t);
static void release_finished(struct power_client *c);

static bool side_valid(enum prospector_split_side side) {
    return side == PROSPECTOR_SIDE_LEFT || side == PROSPECTOR_SIDE_RIGHT;
}

static uint8_t position_locked(uint8_t source) {
    if (ARRAY_SIZE(sides) != 2 || source >= 2)
        return source;
    enum prospector_split_side own = sides[source], other = sides[1 - source];
    if (side_valid(own) && own != other)
        return own - 1;
    if (!side_valid(own) && side_valid(other))
        return 2 - other;
    return source;
}

uint8_t prospector_position_for_source(uint8_t source) {
    k_mutex_lock(&state_lock, K_FOREVER);
    uint8_t position = position_locked(source);
    k_mutex_unlock(&state_lock);
    return position;
}

uint8_t prospector_source_for_position(uint8_t position) {
    k_mutex_lock(&state_lock, K_FOREVER);
    uint8_t source = position;
    for (uint8_t s = 0; s < ARRAY_SIZE(sides); s++) {
        if (position_locked(s) == position) {
            source = s;
            break;
        }
    }
    k_mutex_unlock(&state_lock);
    return source;
}

bool prospector_sides_known(void) {
    if (ARRAY_SIZE(sides) != 2)
        return false;
    k_mutex_lock(&state_lock, K_FOREVER);
    bool known = (side_valid(sides[0]) || side_valid(sides[1])) && sides[0] != sides[1];
    k_mutex_unlock(&state_lock);
    return known;
}

struct prospector_power_diagnostic prospector_power_diagnostic_get(uint8_t source) {
    struct prospector_power_diagnostic result = {0};
    k_mutex_lock(&state_lock, K_FOREVER);
    if (source < ARRAY_SIZE(states)) {
        result = diagnostics[source];
        result.state = states[source];
    }
    k_mutex_unlock(&state_lock);
    return result;
}

static void diagnostic_locked(struct power_client *c, enum prospector_power_stage stage,
                              int error) {
    if (!c->live || c->slot < 0 || selected[c->slot] != c)
        return;
    k_mutex_lock(&state_lock, K_FOREVER);
    diagnostics[c->slot] =
        (struct prospector_power_diagnostic){.stage = stage,
                                             .error = error,
                                             .retries = c->retries,
                                             .security = bt_conn_get_security(c->conn)};
    k_mutex_unlock(&state_lock);
}

static void reschedule_locked(void) {
    if (!retry_queue_ready)
        return;
    int64_t next = INT64_MAX, now = k_uptime_get();
    for (size_t i = 0; i < ARRAY_SIZE(clients); i++)
        if (clients[i].live && clients[i].retry_at && clients[i].retry_at < next)
            next = clients[i].retry_at;
    if (next == INT64_MAX)
        k_work_cancel_delayable(&retry_work);
    else
        k_work_reschedule_for_queue(&retry_queue, &retry_work, K_MSEC(next > now ? next - now : 0));
}

enum prospector_power_state prospector_power_get(uint8_t source) {
    if (source >= ARRAY_SIZE(states))
        return PROSPECTOR_POWER_UNKNOWN;
    k_mutex_lock(&state_lock, K_FOREVER);
    enum prospector_power_state state = states[source];
    k_mutex_unlock(&state_lock);
    return state;
}

static void publish(struct power_client *c, enum prospector_power_state state) {
    if (!c->live || c->slot < 0 || (size_t)c->slot >= ARRAY_SIZE(states) || selected[c->slot] != c)
        return;
    k_mutex_lock(&state_lock, K_FOREVER);
    bool changed = states[c->slot] != state;
    states[c->slot] = state;
    k_mutex_unlock(&state_lock);
    if (changed)
        raise_zmk_peripheral_power_state_changed(
            (struct zmk_peripheral_power_state_changed){.source = c->slot, .state = state});
}

static void fail_locked(struct power_client *c, enum retry_operation operation, int error) {
    if (!c->live)
        return;
    enum prospector_power_stage stage = prospector_power_diagnostic_get(c->slot).stage;
    if (c->retries < UINT8_MAX)
        c->retries++;
    c->retry_operation = operation;
    uint32_t delay = c->retries < 6 ? 1000U << (c->retries - 1) : 30000U;
    c->retry_at = k_uptime_get() + delay;
    diagnostic_locked(c, stage, error);
    publish(c, PROSPECTOR_POWER_UNKNOWN);
}

/* Caller owns client_lock. Reserve request storage before dropping the lock:
 * disconnect cannot free/reuse it while an ATT operation or this call owns it.
 * Never hold client_lock while GATT may wait for Bluetooth RX progress. */
static int perform_locked(struct power_client *c, enum retry_operation operation) {
    if (!c->live || !c->conn)
        return -ENOTCONN;
    if (c->discovering || c->reading || c->subscribing || c->side_reading)
        return 0;
    if (c->api_calls) {
        /* A callback may run before its submitting API returns. Defer the next
         * phase rather than mutating/requesting the same storage on that stack. */
        c->retry_operation = operation;
        c->retry_at = k_uptime_get() + 1;
        return 0;
    }
    if (bt_conn_get_security(c->conn) < BT_SECURITY_L2) {
        diagnostic_locked(c, PROSPECTOR_POWER_SEC, 0);
        c->retry_at = k_uptime_get() + 1000;
        return -EACCES;
    }
    c->retry_at = 0;
    if (operation == RETRY_DISCOVER) {
        c->discovering = true;
        enum prospector_power_stage stage =
            c->discovery.type == BT_GATT_DISCOVER_PRIMARY          ? PROSPECTOR_POWER_SVC
            : c->discovery.type == BT_GATT_DISCOVER_CHARACTERISTIC ? PROSPECTOR_POWER_CHR
                                                                   : PROSPECTOR_POWER_CCC;
        diagnostic_locked(c, stage, 0);
    } else if (operation == RETRY_SUBSCRIBE) {
        /* Disconnect removal can clear value; always request NOTIFY explicitly. */
        c->subscription.value = BT_GATT_CCC_NOTIFY;
        c->subscribing = true;
        diagnostic_locked(c, PROSPECTOR_POWER_SUB, 0);
    } else if (operation == RETRY_SIDE) {
        /* Read by UUID within the service; diagnostics stay on the power stage. */
        c->side_reading = true;
        c->side_read = (struct bt_gatt_read_params){.func = side_read_complete, .handle_count = 0};
        c->side_read.by_uuid.start_handle = c->svc_start + 1;
        c->side_read.by_uuid.end_handle = c->svc_end;
        c->side_read.by_uuid.uuid = &side_uuid.uuid;
    } else {
        c->reading = true;
        c->read_notifications = c->notifications;
        c->read = (struct bt_gatt_read_params){
            .func = power_read_complete,
            .handle_count = 1,
            .single = {.handle = c->subscription.value_handle, .offset = 0}};
        diagnostic_locked(c, PROSPECTOR_POWER_READ, 0);
    }
    struct bt_conn *conn = bt_conn_ref(c->conn);
    c->api_calls++;
    k_mutex_unlock(&client_lock);
    int rc = operation == RETRY_DISCOVER    ? bt_gatt_discover(conn, &c->discovery)
             : operation == RETRY_SUBSCRIBE ? bt_gatt_subscribe(conn, &c->subscription)
             : operation == RETRY_SIDE      ? bt_gatt_read(conn, &c->side_read)
                                            : bt_gatt_read(conn, &c->read);
    k_mutex_lock(&client_lock, K_FOREVER);
    c->api_calls--;
    if (rc && c->conn == conn && operation == RETRY_SIDE) {
        /* Side is optional: retry later without disturbing the power state. */
        c->side_reading = false;
        if (c->live) {
            c->retry_operation = RETRY_SIDE;
            c->retry_at = k_uptime_get() + 1000;
        }
    } else if (rc && c->conn == conn) {
        if (operation == RETRY_DISCOVER)
            c->discovering = false;
        else if (operation == RETRY_SUBSCRIBE)
            c->subscribing = false;
        else
            c->reading = false;
        fail_locked(c, operation, rc);
    }
    release_finished(c);
    bt_conn_unref(conn);
    return rc;
}

static enum prospector_power_state decode(const void *data, uint16_t length) {
    if (!data || length != 1 || *(const uint8_t *)data > PROSPECTOR_POWER_USB)
        return PROSPECTOR_POWER_UNKNOWN;
    return *(const uint8_t *)data;
}

static void release_finished(struct power_client *c) {
    if (!c->live && !c->discovering && !c->reading && !c->subscribing && !c->side_reading &&
        !c->api_calls && c->conn) {
        struct bt_conn *conn = c->conn;
        c->conn = NULL;
        bt_conn_unref(conn);
    }
}

static uint8_t power_notify_locked(struct bt_conn *conn, struct bt_gatt_subscribe_params *params,
                                   const void *data, uint16_t length) {
    struct power_client *c = CONTAINER_OF(params, struct power_client, subscription);
    if (c->conn != conn || !c->live)
        return BT_GATT_ITER_STOP;
    c->notified = true;
    c->notifications++;
    enum prospector_power_state state = decode(data, length);
    if (!data) {
        c->subscribed = false;
        diagnostic_locked(c, PROSPECTOR_POWER_SUB, 0);
        if (c->subscribing) {
            /* Failed CCC write: Zephyr calls notify(NULL) then subscribe(err).
             * The acknowledgement owns the single failure count and ATT reason. */
            publish(c, PROSPECTOR_POWER_UNKNOWN);
        } else
            fail_locked(c, RETRY_SUBSCRIBE, -ENOTCONN);
    } else if (state == PROSPECTOR_POWER_UNKNOWN) {
        fail_locked(c, RETRY_READ, -EBADMSG);
    } else {
        c->retry_at = 0;
        diagnostic_locked(c, PROSPECTOR_POWER_LIVE, 0);
        publish(c, state);
    }
    return data ? BT_GATT_ITER_CONTINUE : BT_GATT_ITER_STOP;
}

static uint8_t power_read_complete_locked(struct bt_conn *conn, uint8_t err,
                                          struct bt_gatt_read_params *params, const void *data,
                                          uint16_t length) {
    struct power_client *c = CONTAINER_OF(params, struct power_client, read);
    if (c->conn != conn)
        return BT_GATT_ITER_STOP;
    c->reading = false;
    bool ok = !err && decode(data, length) != PROSPECTOR_POWER_UNKNOWN;
    if (c->notifications == c->read_notifications) {
        enum prospector_power_state state = decode(data, length);
        if (!ok)
            fail_locked(c, RETRY_READ, err ? -(int)err : -EBADMSG);
        else {
            c->retry_at = 0;
            diagnostic_locked(c, PROSPECTOR_POWER_LIVE, 0);
            publish(c, state);
        }
    }
    if (ok && c->live && c->subscribed && !c->side_done)
        perform_locked(c, RETRY_SIDE);
    release_finished(c);
    return BT_GATT_ITER_STOP;
}

static void set_side_locked(struct power_client *c, enum prospector_split_side side) {
    if (!c->live || c->slot < 0 || (size_t)c->slot >= ARRAY_SIZE(sides) || selected[c->slot] != c)
        return;
    k_mutex_lock(&state_lock, K_FOREVER);
    bool changed = sides[c->slot] != side;
    sides[c->slot] = side;
    enum prospector_power_state state = states[c->slot];
    k_mutex_unlock(&state_lock);
    /* Positions may have swapped; the power event makes widgets redraw both halves. */
    if (changed)
        raise_zmk_peripheral_power_state_changed(
            (struct zmk_peripheral_power_state_changed){.source = c->slot, .state = state});
}

static uint8_t side_read_complete_locked(struct bt_conn *conn, uint8_t err,
                                         struct bt_gatt_read_params *params, const void *data,
                                         uint16_t length) {
    struct power_client *c = CONTAINER_OF(params, struct power_client, side_read);
    if (c->conn != conn)
        return BT_GATT_ITER_STOP;
    c->side_reading = false;
    if (c->live) {
        c->side_done = true;
        if (!err && data && length == 1 && side_valid(*(const uint8_t *)data))
            set_side_locked(c, *(const uint8_t *)data);
        else if (!err || err == BT_ATT_ERR_ATTRIBUTE_NOT_FOUND)
            /* No (valid) side: an older or unconfigured half. Other ATT errors
             * keep the cached side for this connection. */
            set_side_locked(c, PROSPECTOR_SIDE_UNKNOWN);
    }
    release_finished(c);
    return BT_GATT_ITER_STOP;
}

static void power_subscribed_locked(struct bt_conn *conn, uint8_t err,
                                    struct bt_gatt_subscribe_params *params) {
    struct power_client *c = CONTAINER_OF(params, struct power_client, subscription);
    if (c->conn != conn)
        return;
    c->subscribing = false;
    if (!c->live) {
        release_finished(c);
        return;
    }
    if (err) {
        fail_locked(c, RETRY_SUBSCRIBE, -(int)err);
        return;
    }
    c->subscribed = true;
    perform_locked(c, RETRY_READ);
}

static uint8_t power_discovered_locked(struct bt_conn *conn, const struct bt_gatt_attr *attr,
                                       struct bt_gatt_discover_params *params) {
    struct power_client *c = CONTAINER_OF(params, struct power_client, discovery);
    if (c->conn != conn)
        return BT_GATT_ITER_STOP;
    c->discovering = false;
    if (!c->live) {
        release_finished(c);
        return BT_GATT_ITER_STOP;
    }
    if (!attr) {
        fail_locked(c, RETRY_DISCOVER, -ENOENT);
        return BT_GATT_ITER_STOP;
    }
    if (params->type == BT_GATT_DISCOVER_PRIMARY) {
        const struct bt_gatt_service_val *service = attr->user_data;
        c->svc_start = attr->handle;
        c->svc_end = service->end_handle;
        params->uuid = &state_uuid.uuid;
        params->start_handle = attr->handle + 1;
        params->end_handle = service->end_handle;
        params->type = BT_GATT_DISCOVER_CHARACTERISTIC;
    } else if (params->type == BT_GATT_DISCOVER_CHARACTERISTIC) {
        const struct bt_gatt_chrc *chrc = attr->user_data;
        if ((chrc->properties & (BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY)) !=
            (BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY)) {
            fail_locked(c, RETRY_DISCOVER, -EINVAL);
            return BT_GATT_ITER_STOP;
        }
        c->subscription.value_handle = chrc->value_handle;
        params->uuid = &ccc_uuid.uuid;
        params->start_handle = chrc->value_handle + 1;
        params->type = BT_GATT_DISCOVER_DESCRIPTOR;
    } else {
        c->subscription.ccc_handle = attr->handle;
        c->subscription.value = BT_GATT_CCC_NOTIFY;
        c->subscription.notify = power_notify;
        c->subscription.subscribe = power_subscribed;
        c->subscription.min_security = BT_SECURITY_L2;
        atomic_set_bit(c->subscription.flags, BT_GATT_SUBSCRIBE_FLAG_VOLATILE);
        perform_locked(c, RETRY_SUBSCRIBE);
        return BT_GATT_ITER_STOP;
    }
    perform_locked(c, RETRY_DISCOVER);
    return BT_GATT_ITER_STOP;
}

static int central_start_locked(struct bt_conn *conn) {
    struct bt_conn_info info;
    if (bt_conn_get_info(conn, &info) || info.role != BT_CONN_ROLE_CENTRAL)
        return -EINVAL;
    int slot = prospector_peripheral_index_for_conn(conn);
    if (slot < 0 || (size_t)slot >= ARRAY_SIZE(states))
        return -EINVAL;
    for (size_t i = 0; i < ARRAY_SIZE(clients); i++)
        if (clients[i].conn == conn) {
            struct power_client *c = &clients[i];
            if (c->subscribed && prospector_power_get(slot) != PROSPECTOR_POWER_UNKNOWN)
                return 0;
            return perform_locked(c, c->retry_operation);
        }
    for (size_t i = 0; i < ARRAY_SIZE(clients); i++) {
        struct power_client *c = &clients[i];
        if (c->conn)
            continue;
        memset(c, 0, sizeof(*c));
        c->conn = bt_conn_ref(conn);
        c->slot = slot;
        c->live = true;
        selected[slot] = c;
        publish(c, PROSPECTOR_POWER_UNKNOWN);
        c->discovery =
            (struct bt_gatt_discover_params){.uuid = &service_uuid.uuid,
                                             .func = power_discovered,
                                             .start_handle = BT_ATT_FIRST_ATTRIBUTE_HANDLE,
                                             .end_handle = BT_ATT_LAST_ATTRIBUTE_HANDLE,
                                             .type = BT_GATT_DISCOVER_PRIMARY};
        return perform_locked(c, RETRY_DISCOVER);
    }
    return -ENOMEM;
}

static void central_security_changed(struct bt_conn *conn, bt_security_t level,
                                     enum bt_security_err err) {
    ARG_UNUSED(level);
    k_mutex_lock(&client_lock, K_FOREVER);
    if (!err)
        central_start_locked(conn);
    else {
        for (size_t i = 0; i < ARRAY_SIZE(clients); i++) {
            struct power_client *c = &clients[i];
            if (c->conn == conn) {
                diagnostic_locked(c, PROSPECTOR_POWER_SEC, -(int)err);
                fail_locked(c, RETRY_DISCOVER, -(int)err);
            }
        }
    }
    reschedule_locked();
    k_mutex_unlock(&client_lock);
}

static void central_disconnected(struct bt_conn *conn, uint8_t reason) {
    ARG_UNUSED(reason);
    k_mutex_lock(&client_lock, K_FOREVER);
    for (size_t i = 0; i < ARRAY_SIZE(clients); i++) {
        struct power_client *c = &clients[i];
        if (c->conn != conn)
            continue;
        publish(c, PROSPECTOR_POWER_UNKNOWN);
        c->retry_at = 0;
        c->retries = 0;
        diagnostic_locked(c, PROSPECTOR_POWER_OFF, 0);
        if (selected[c->slot] == c)
            selected[c->slot] = NULL;
        c->live = false;
        /* Volatile subscription is removed by bt_gatt_disconnected before
         * application callbacks; in-flight ATT operations retain this context. */
        release_finished(c);
    }
    reschedule_locked();
    k_mutex_unlock(&client_lock);
}

static int central_start(struct bt_conn *conn) {
    k_mutex_lock(&client_lock, K_FOREVER);
    int result = central_start_locked(conn);
    reschedule_locked();
    k_mutex_unlock(&client_lock);
    return result;
}
static void central_connected(struct bt_conn *conn, uint8_t err) {
    if (!err)
        central_start(conn);
}
static uint8_t power_discovered(struct bt_conn *conn, const struct bt_gatt_attr *attr,
                                struct bt_gatt_discover_params *params) {
    k_mutex_lock(&client_lock, K_FOREVER);
    uint8_t result = power_discovered_locked(conn, attr, params);
    reschedule_locked();
    k_mutex_unlock(&client_lock);
    return result;
}
static uint8_t power_notify(struct bt_conn *conn, struct bt_gatt_subscribe_params *params,
                            const void *data, uint16_t length) {
    k_mutex_lock(&client_lock, K_FOREVER);
    uint8_t result = power_notify_locked(conn, params, data, length);
    reschedule_locked();
    k_mutex_unlock(&client_lock);
    return result;
}
static uint8_t power_read_complete(struct bt_conn *conn, uint8_t err,
                                   struct bt_gatt_read_params *params, const void *data,
                                   uint16_t length) {
    k_mutex_lock(&client_lock, K_FOREVER);
    uint8_t result = power_read_complete_locked(conn, err, params, data, length);
    reschedule_locked();
    k_mutex_unlock(&client_lock);
    return result;
}
static uint8_t side_read_complete(struct bt_conn *conn, uint8_t err,
                                  struct bt_gatt_read_params *params, const void *data,
                                  uint16_t length) {
    k_mutex_lock(&client_lock, K_FOREVER);
    uint8_t result = side_read_complete_locked(conn, err, params, data, length);
    reschedule_locked();
    k_mutex_unlock(&client_lock);
    return result;
}
static void power_subscribed(struct bt_conn *conn, uint8_t err,
                             struct bt_gatt_subscribe_params *params) {
    k_mutex_lock(&client_lock, K_FOREVER);
    power_subscribed_locked(conn, err, params);
    reschedule_locked();
    k_mutex_unlock(&client_lock);
}
static void retry_work_handler(struct k_work *work) {
    ARG_UNUSED(work);
    k_mutex_lock(&client_lock, K_FOREVER);
    for (size_t i = 0; i < ARRAY_SIZE(clients); i++) {
        struct power_client *c = &clients[i];
        if (!c->live || !c->retry_at || c->retry_at > k_uptime_get())
            continue;
        if (c->discovering || c->reading || c->subscribing || c->api_calls) {
            c->retry_at = k_uptime_get() + 1000;
            continue;
        }
        perform_locked(c, c->retry_operation);
    }
    reschedule_locked();
    k_mutex_unlock(&client_lock);
}
static int power_retry_init(void) {
    k_work_queue_start(&retry_queue, retry_stack, K_THREAD_STACK_SIZEOF(retry_stack),
                       K_PRIO_PREEMPT(10), NULL);
    k_mutex_lock(&client_lock, K_FOREVER);
    retry_queue_ready = true;
    reschedule_locked();
    k_mutex_unlock(&client_lock);
    return 0;
}
SYS_INIT(power_retry_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);

BT_CONN_CB_DEFINE(prospector_power_central_callbacks) = {
    .connected = central_connected,
    .security_changed = central_security_changed,
    .disconnected = central_disconnected,
};

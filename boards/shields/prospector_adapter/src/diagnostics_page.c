/* SPDX-License-Identifier: MIT */

#include "diagnostics_page.h"

#include <errno.h>
#include <stdbool.h>
#include <stdint.h>

#include <zephyr/app_version.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/atomic.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/sys/mem_stats.h>
#include <zmk/display.h>
#include <zmk/ble.h>
#include <zmk/event_manager.h>
#include <zmk/events/keycode_state_changed.h>
#include <zmk/events/position_state_changed.h>

#include <lvgl_mem.h>

#include <prospector/brightness.h>

#if LV_FONT_MONTSERRAT_14
#define DIAGNOSTICS_FONT (&lv_font_montserrat_14)
#else
#define DIAGNOSTICS_FONT LV_FONT_DEFAULT
#endif

#if defined(APP_VERSION_STRING)
#define FIRMWARE_VERSION_STRING APP_VERSION_STRING
#else
#define FIRMWARE_VERSION_STRING "--"
#endif

#define KEY_PRESS_COUNT_MAX 999999999
#define KEY_TUG_MIN -18
#define KEY_TUG_MAX 18
#define KEY_TUG_TRACK_X 104
#define KEY_TUG_TRACK_Y 30
#define KEY_TUG_TRACK_WIDTH 58
#define KEY_TUG_MARKER_WIDTH 6
#define KEY_TUG_PULL 3
#define KEY_TUG_DECAY 1
#define DIAGNOSTICS_PERIPHERAL_ROWS 2
#define RECENT_POSITION_EVENTS 8

static lv_obj_t *uptime_value;
static lv_obj_t *brightness_value;
static lv_obj_t *ambient_light_value;
static lv_obj_t *key_count_value;
static lv_obj_t *key_tug_marker;
static lv_obj_t *memory_value;
static lv_obj_t *peripheral_rssi_values[DIAGNOSTICS_PERIPHERAL_ROWS];
static lv_obj_t *peripheral_latency_values[DIAGNOSTICS_PERIPHERAL_ROWS];
static atomic_t diagnostics_visible;
static atomic_t key_press_count;
static atomic_t key_tug_balance;
static atomic_t peripheral_rssi_known[DIAGNOSTICS_PERIPHERAL_ROWS];
static atomic_t peripheral_rssi_dbm[DIAGNOSTICS_PERIPHERAL_ROWS];
static atomic_t peripheral_latency_known[DIAGNOSTICS_PERIPHERAL_ROWS];
static atomic_t peripheral_latency_ms[DIAGNOSTICS_PERIPHERAL_ROWS];

struct recent_position_event {
    int64_t timestamp;
    uint8_t source;
};

static struct bt_conn *peripheral_connections[DIAGNOSTICS_PERIPHERAL_ROWS];
K_MUTEX_DEFINE(peripheral_connections_mutex);

static struct recent_position_event recent_position_events[RECENT_POSITION_EVENTS];
static atomic_t recent_position_event_index;

static void diagnostics_tick_work_handler(struct k_work *work);
static void diagnostics_update_work_handler(struct k_work *work);

K_WORK_DELAYABLE_DEFINE(diagnostics_tick_work, diagnostics_tick_work_handler);
K_WORK_DEFINE(diagnostics_update_work, diagnostics_update_work_handler);

static void remember_position_event(uint8_t source, int64_t timestamp) {
    atomic_val_t index = atomic_inc(&recent_position_event_index) % RECENT_POSITION_EVENTS;

    recent_position_events[index] = (struct recent_position_event){
        .timestamp = timestamp,
        .source = source,
    };
}

static void pull_key_tug(uint8_t source) {
    atomic_val_t current;
    atomic_val_t next;
    atomic_val_t delta = source == 0 ? -KEY_TUG_PULL : KEY_TUG_PULL;

    do {
        current = atomic_get(&key_tug_balance);
        next = current + delta;

        if (next < KEY_TUG_MIN) {
            next = KEY_TUG_MIN;
        } else if (next > KEY_TUG_MAX) {
            next = KEY_TUG_MAX;
        }
    } while (!atomic_cas(&key_tug_balance, current, next));
}

static void decay_key_tug(void) {
    atomic_val_t current;
    atomic_val_t next;

    do {
        current = atomic_get(&key_tug_balance);

        if (current > KEY_TUG_DECAY) {
            next = current - KEY_TUG_DECAY;
        } else if (current < -KEY_TUG_DECAY) {
            next = current + KEY_TUG_DECAY;
        } else {
            next = 0;
        }
    } while (current != next && !atomic_cas(&key_tug_balance, current, next));
}

static bool recent_position_for_key_event(int64_t timestamp, uint8_t *source,
                                          int64_t *position_timestamp) {
    for (uint8_t i = 0; i < RECENT_POSITION_EVENTS; i++) {
        if (recent_position_events[i].timestamp == timestamp &&
            recent_position_events[i].source < DIAGNOSTICS_PERIPHERAL_ROWS &&
            recent_position_events[i].timestamp > 0) {
            *source = recent_position_events[i].source;
            *position_timestamp = recent_position_events[i].timestamp;
            return true;
        }
    }

    return false;
}

static int diagnostics_keycode_state_changed_listener(const zmk_event_t *eh) {
    const struct zmk_keycode_state_changed *ev = as_zmk_keycode_state_changed(eh);

    if (ev != NULL && ev->state) {
        atomic_val_t current;
        uint8_t source;

        do {
            current = atomic_get(&key_press_count);
            if (current >= KEY_PRESS_COUNT_MAX) {
                return ZMK_EV_EVENT_BUBBLE;
            }
        } while (!atomic_cas(&key_press_count, current, current + 1));

        int64_t position_timestamp;

        if (recent_position_for_key_event(ev->timestamp, &source, &position_timestamp)) {
            int64_t latency = k_uptime_get() - position_timestamp;

            if (latency < 0) {
                latency = 0;
            } else if (latency > 999) {
                latency = 999;
            }

            atomic_set(&peripheral_latency_ms[source], latency);
            atomic_set(&peripheral_latency_known[source], true);
        }
    }

    return ZMK_EV_EVENT_BUBBLE;
}

ZMK_LISTENER(prospector_diagnostics_key_count, diagnostics_keycode_state_changed_listener);
ZMK_SUBSCRIPTION(prospector_diagnostics_key_count, zmk_keycode_state_changed);

static int diagnostics_position_state_changed_listener(const zmk_event_t *eh) {
    const struct zmk_position_state_changed *ev = as_zmk_position_state_changed(eh);

    if (ev != NULL && ev->state && ev->source < DIAGNOSTICS_PERIPHERAL_ROWS) {
        remember_position_event(ev->source, ev->timestamp);
        pull_key_tug(ev->source);
    }

    return ZMK_EV_EVENT_BUBBLE;
}

ZMK_LISTENER(prospector_diagnostics_position_latency, diagnostics_position_state_changed_listener);
ZMK_SUBSCRIPTION(prospector_diagnostics_position_latency, zmk_position_state_changed);

static lv_obj_t *create_label(lv_obj_t *parent, const char *text, int x, int y,
                              lv_color_t color) {
    lv_obj_t *label = lv_label_create(parent);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_font(label, DIAGNOSTICS_FONT, LV_PART_MAIN);
    lv_obj_set_style_text_color(label, color, LV_PART_MAIN);
    lv_obj_set_pos(label, x, y);

    return label;
}

static lv_obj_t *create_centered_label(lv_obj_t *parent, const char *text, int y,
                                       lv_color_t color) {
    lv_obj_t *label = lv_label_create(parent);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_font(label, DIAGNOSTICS_FONT, LV_PART_MAIN);
    lv_obj_set_style_text_color(label, color, LV_PART_MAIN);
    lv_obj_align(label, LV_ALIGN_TOP_MID, 0, y);

    return label;
}

static lv_obj_t *create_right_label(lv_obj_t *parent, const char *text, int y,
                                    lv_color_t color) {
    lv_obj_t *label = lv_label_create(parent);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_font(label, DIAGNOSTICS_FONT, LV_PART_MAIN);
    lv_obj_set_style_text_color(label, color, LV_PART_MAIN);
    lv_obj_align(label, LV_ALIGN_TOP_RIGHT, -8, y);

    return label;
}

static lv_obj_t *create_panel(lv_obj_t *parent, int x, int y, int width, int height) {
    lv_obj_t *panel = lv_obj_create(parent);
    lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_pos(panel, x, y);
    lv_obj_set_size(panel, width, height);
    lv_obj_set_style_bg_color(panel, lv_color_hex(0x080808), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(panel, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_color(panel, lv_color_hex(0x30343a), LV_PART_MAIN);
    lv_obj_set_style_border_width(panel, 1, LV_PART_MAIN);
    lv_obj_set_style_radius(panel, 3, LV_PART_MAIN);
    lv_obj_set_style_pad_all(panel, 0, LV_PART_MAIN);

    return panel;
}

static lv_obj_t *create_metric_panel(lv_obj_t *parent, int x, int y, int width, const char *label,
                                     const char *value, lv_color_t value_color) {
    lv_obj_t *panel = create_panel(parent, x, y, width, 42);

    create_label(panel, label, 8, 6, lv_color_hex(0x8a949c));
    return create_label(panel, value, 8, 22, value_color);
}

static void create_key_count_panel(lv_obj_t *parent, int x, int y, int width) {
    lv_obj_t *panel = create_panel(parent, x, y, width, 42);

    create_label(panel, "KEYS PRESSED", 8, 5, lv_color_hex(0x8a949c));
    key_count_value = create_label(panel, "000000", 8, 25, lv_color_hex(0x70e8f0));

    lv_obj_t *track = lv_obj_create(panel);
    lv_obj_clear_flag(track, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_pos(track, KEY_TUG_TRACK_X, KEY_TUG_TRACK_Y);
    lv_obj_set_size(track, KEY_TUG_TRACK_WIDTH, 4);
    lv_obj_set_style_bg_color(track, lv_color_hex(0x30343a), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(track, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(track, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(track, 2, LV_PART_MAIN);
    lv_obj_set_style_pad_all(track, 0, LV_PART_MAIN);

    lv_obj_t *center = lv_obj_create(panel);
    lv_obj_clear_flag(center, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_pos(center, KEY_TUG_TRACK_X + (KEY_TUG_TRACK_WIDTH / 2) - 1, KEY_TUG_TRACK_Y - 3);
    lv_obj_set_size(center, 2, 10);
    lv_obj_set_style_bg_color(center, lv_color_hex(0x8a949c), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(center, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(center, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(center, 1, LV_PART_MAIN);
    lv_obj_set_style_pad_all(center, 0, LV_PART_MAIN);

    key_tug_marker = lv_obj_create(panel);
    lv_obj_clear_flag(key_tug_marker, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(key_tug_marker, KEY_TUG_MARKER_WIDTH, 12);
    lv_obj_set_style_bg_color(key_tug_marker, lv_color_hex(0x70e8f0), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(key_tug_marker, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(key_tug_marker, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(key_tug_marker, 2, LV_PART_MAIN);
    lv_obj_set_style_pad_all(key_tug_marker, 0, LV_PART_MAIN);
}

static void create_peripheral_row(lv_obj_t *parent, uint8_t row, int y, const char *side) {
    create_label(parent, side, 8, y, lv_color_hex(0x70e8f0));
    peripheral_rssi_values[row] = create_label(parent, "--dB", 34, y, lv_color_hex(0x70e8f0));
    peripheral_latency_values[row] = create_label(parent, "--ms", 116, y, lv_color_hex(0xffffff));
}

static lv_obj_t *create_firmware_version_label(lv_obj_t *parent, int x, int y) {
    static char firmware_text[16];

    snprintk(firmware_text, sizeof(firmware_text), "zmk %s", FIRMWARE_VERSION_STRING);
    return create_label(parent, firmware_text, x, y, lv_color_hex(0x90ee7e));
}

static void update_brightness_label(void) {
    static char brightness_text[16];
    uint8_t brightness = prospector_brightness_get_displayed();
    const char *mode = prospector_brightness_is_auto() ? "A" : "M";

    if (brightness_value == NULL) {
        return;
    }

    snprintk(brightness_text, sizeof(brightness_text), "%s %u%%", mode, brightness);
    lv_label_set_text(brightness_value, brightness_text);
}

static void update_key_count_label(void) {
    static char key_count_text[16];
    atomic_val_t raw_count = atomic_get(&key_press_count);
    uint32_t count = raw_count > 0 ? (uint32_t)raw_count : 0U;

    if (key_count_value == NULL) {
        return;
    }

    if (count < 10000) {
        snprintk(key_count_text, sizeof(key_count_text), "%u", count);
    } else if (count < 100000) {
        snprintk(key_count_text, sizeof(key_count_text), "%u.%uk", count / 1000U,
                 (count / 100U) % 10U);
    } else if (count < 1000000) {
        snprintk(key_count_text, sizeof(key_count_text), "%uk", count / 1000U);
    } else if (count < 10000000) {
        snprintk(key_count_text, sizeof(key_count_text), "%u.%uM", count / 1000000U,
                 (count / 100000U) % 10U);
    } else if (count < KEY_PRESS_COUNT_MAX) {
        snprintk(key_count_text, sizeof(key_count_text), "%uM", count / 1000000U);
    } else {
        snprintk(key_count_text, sizeof(key_count_text), "999M+");
    }

    lv_label_set_text(key_count_value, key_count_text);

    if (key_tug_marker != NULL) {
        int32_t balance = atomic_get(&key_tug_balance);
        uint32_t travel = KEY_TUG_TRACK_WIDTH - KEY_TUG_MARKER_WIDTH;
        int32_t x;

        if (balance < KEY_TUG_MIN) {
            balance = KEY_TUG_MIN;
        } else if (balance > KEY_TUG_MAX) {
            balance = KEY_TUG_MAX;
        }

        x = KEY_TUG_TRACK_X +
            ((balance - KEY_TUG_MIN) * travel) / (KEY_TUG_MAX - KEY_TUG_MIN);
        lv_obj_set_pos(key_tug_marker, x, KEY_TUG_TRACK_Y - 4);
    }
}

static void update_ambient_light_label(void) {
    static char ambient_text[16];
    int32_t light;

    if (ambient_light_value == NULL) {
        return;
    }

    if (!prospector_brightness_get_ambient_light(&light)) {
        lv_label_set_text(ambient_light_value, "N/A");
        return;
    }

    if (light > 999) {
        lv_label_set_text(ambient_light_value, "999+ lx");
        return;
    }

    if (light < 0) {
        light = 0;
    }

    snprintk(ambient_text, sizeof(ambient_text), "%d lx", light);
    lv_label_set_text(ambient_light_value, ambient_text);
}

static void update_memory_label(void) {
    static char memory_text[24];
    struct sys_memory_stats stats;
    uint32_t used_kib;
    uint32_t free_kib;

    if (memory_value == NULL) {
        return;
    }

#if defined(CONFIG_SYS_HEAP_RUNTIME_STATS) && defined(CONFIG_LV_Z_MEM_POOL_SYS_HEAP)
    lvgl_heap_stats(&stats);
    used_kib = stats.allocated_bytes / 1024U;
    free_kib = stats.free_bytes / 1024U;
    snprintk(memory_text, sizeof(memory_text), "%u/%uk", used_kib, free_kib);
    lv_label_set_text(memory_value, memory_text);
#else
    lv_label_set_text(memory_value, "N/A");
#endif
}

static void update_peripheral_labels(void) {
    static char rssi_text[DIAGNOSTICS_PERIPHERAL_ROWS][8];
    static char latency_text[DIAGNOSTICS_PERIPHERAL_ROWS][8];

    for (uint8_t i = 0; i < DIAGNOSTICS_PERIPHERAL_ROWS; i++) {
        bool rssi_known = atomic_get(&peripheral_rssi_known[i]);
        bool latency_known = atomic_get(&peripheral_latency_known[i]);
        int32_t rssi = atomic_get(&peripheral_rssi_dbm[i]);
        uint32_t latency_ms = atomic_get(&peripheral_latency_ms[i]);

        if (peripheral_rssi_values[i] != NULL) {
            if (rssi_known) {
                snprintk(rssi_text[i], sizeof(rssi_text[i]), "%lddB", (long)rssi);
                lv_label_set_text(peripheral_rssi_values[i], rssi_text[i]);
            } else {
                lv_label_set_text(peripheral_rssi_values[i], "--dB");
            }
        }

        if (peripheral_latency_values[i] != NULL) {
            if (latency_known) {
                if (latency_ms > 999U) {
                    latency_ms = 999U;
                }

                snprintk(latency_text[i], sizeof(latency_text[i]), "%lums", (long)latency_ms);
                lv_label_set_text(peripheral_latency_values[i], latency_text[i]);
            } else {
                lv_label_set_text(peripheral_latency_values[i], "--ms");
            }
        }
    }
}

static void reset_peripheral_metrics(uint8_t row) {
    atomic_set(&peripheral_rssi_known[row], false);
    atomic_set(&peripheral_rssi_dbm[row], 0);
    atomic_set(&peripheral_latency_known[row], false);
    atomic_set(&peripheral_latency_ms[row], 0);
}

static int read_conn_rssi(struct bt_conn *conn, int8_t *rssi) {
    struct bt_hci_cp_read_rssi *cp;
    struct bt_hci_rp_read_rssi *rp;
    struct net_buf *buf;
    struct net_buf *rsp = NULL;
    uint16_t handle;
    int err;

    err = bt_hci_get_conn_handle(conn, &handle);
    if (err < 0) {
        return err;
    }

    buf = bt_hci_cmd_create(BT_HCI_OP_READ_RSSI, sizeof(*cp));
    if (buf == NULL) {
        return -ENOMEM;
    }

    cp = net_buf_add(buf, sizeof(*cp));
    cp->handle = sys_cpu_to_le16(handle);

    err = bt_hci_cmd_send_sync(BT_HCI_OP_READ_RSSI, buf, &rsp);
    if (err < 0) {
        return err;
    }

    rp = (struct bt_hci_rp_read_rssi *)rsp->data;
    if (rp->status != 0) {
        err = -rp->status;
    } else {
        *rssi = rp->rssi;
    }

    net_buf_unref(rsp);
    return err;
}

static void sample_peripheral_metrics(void) {
    struct bt_conn *connections[DIAGNOSTICS_PERIPHERAL_ROWS] = {0};

    k_mutex_lock(&peripheral_connections_mutex, K_FOREVER);
    for (uint8_t i = 0; i < DIAGNOSTICS_PERIPHERAL_ROWS; i++) {
        if (peripheral_connections[i] != NULL) {
            connections[i] = bt_conn_ref(peripheral_connections[i]);
        }
    }
    k_mutex_unlock(&peripheral_connections_mutex);

    for (uint8_t i = 0; i < DIAGNOSTICS_PERIPHERAL_ROWS; i++) {
        struct bt_conn_info info;
        int8_t rssi;

        if (connections[i] == NULL) {
            reset_peripheral_metrics(i);
            continue;
        }

        if (bt_conn_get_info(connections[i], &info) != 0 || info.type != BT_CONN_TYPE_LE ||
            info.role != BT_CONN_ROLE_CENTRAL) {
            atomic_set(&peripheral_rssi_known[i], false);
            bt_conn_unref(connections[i]);
            continue;
        }

        int err = read_conn_rssi(connections[i], &rssi);
        if (err == 0) {
            atomic_set(&peripheral_rssi_dbm[i], rssi);
            atomic_set(&peripheral_rssi_known[i], true);
        } else {
            atomic_set(&peripheral_rssi_known[i], false);
        }

        bt_conn_unref(connections[i]);
    }
}

static void diagnostics_bt_connected(struct bt_conn *conn, uint8_t err) {
    struct bt_conn_info info;
    int slot;

    if (err != 0 || bt_conn_get_info(conn, &info) != 0 || info.type != BT_CONN_TYPE_LE ||
        info.role != BT_CONN_ROLE_CENTRAL) {
        return;
    }

    slot = zmk_ble_put_peripheral_addr(bt_conn_get_dst(conn));
    if (slot < 0 || slot >= DIAGNOSTICS_PERIPHERAL_ROWS) {
        return;
    }

    k_mutex_lock(&peripheral_connections_mutex, K_FOREVER);

    for (uint8_t i = 0; i < DIAGNOSTICS_PERIPHERAL_ROWS; i++) {
        if (peripheral_connections[i] == conn) {
            k_mutex_unlock(&peripheral_connections_mutex);
            return;
        }
    }

    if (peripheral_connections[slot] != NULL) {
        bt_conn_unref(peripheral_connections[slot]);
    }

    peripheral_connections[slot] = bt_conn_ref(conn);
    reset_peripheral_metrics(slot);

    k_mutex_unlock(&peripheral_connections_mutex);
}

static void diagnostics_bt_disconnected(struct bt_conn *conn, uint8_t reason) {
    ARG_UNUSED(reason);

    k_mutex_lock(&peripheral_connections_mutex, K_FOREVER);

    for (uint8_t i = 0; i < DIAGNOSTICS_PERIPHERAL_ROWS; i++) {
        if (peripheral_connections[i] == conn) {
            bt_conn_unref(peripheral_connections[i]);
            peripheral_connections[i] = NULL;
            reset_peripheral_metrics(i);
            break;
        }
    }

    k_mutex_unlock(&peripheral_connections_mutex);
}

BT_CONN_CB_DEFINE(prospector_diagnostics_bt_conn_cb) = {
    .connected = diagnostics_bt_connected,
    .disconnected = diagnostics_bt_disconnected,
};

static void update_uptime_label(void) {
    static char uptime_text[16];
    uint64_t uptime_minutes = k_uptime_get() / 60000U;
    uint32_t days = uptime_minutes / (24U * 60U);
    uint32_t hours = (uptime_minutes / 60U) % 24U;
    uint32_t minutes = uptime_minutes % 60U;

    if (uptime_value == NULL) {
        return;
    }

    if (days > 999U) {
        days = 999U;
        hours = 23U;
        minutes = 59U;
    }

    snprintk(uptime_text, sizeof(uptime_text), "%ud %02uh %02um", days, hours, minutes);
    lv_label_set_text(uptime_value, uptime_text);
}

static void diagnostics_tick_work_handler(struct k_work *work) {
    ARG_UNUSED(work);

    if (atomic_get(&diagnostics_visible) && zmk_display_is_initialized()) {
        sample_peripheral_metrics();
        k_work_submit_to_queue(zmk_display_work_q(), &diagnostics_update_work);
    }
}

static void diagnostics_update_work_handler(struct k_work *work) {
    ARG_UNUSED(work);

    if (!atomic_get(&diagnostics_visible)) {
        return;
    }

    decay_key_tug();
    update_uptime_label();
    update_key_count_label();
    update_brightness_label();
    update_ambient_light_label();
    update_memory_label();
    update_peripheral_labels();
    k_work_schedule(&diagnostics_tick_work, K_SECONDS(1));
}

lv_obj_t *prospector_diagnostics_page_create(lv_obj_t *parent) {
    lv_obj_t *page = lv_obj_create(parent);
    lv_obj_clear_flag(page, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(page, lv_pct(100), lv_pct(100));
    lv_obj_set_pos(page, 0, 0);
    lv_obj_set_style_bg_color(page, lv_color_hex(0x050505), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(page, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(page, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(page, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(page, 0, LV_PART_MAIN);

    create_centered_label(page, "Diagnostics", 6, lv_color_hex(0xffffff));

    lv_obj_t *rule = lv_obj_create(page);
    lv_obj_set_pos(rule, 7, 27);
    lv_obj_set_size(rule, 260, 2);
    lv_obj_set_style_bg_color(rule, lv_color_hex(0x38d6e8), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(rule, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(rule, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(rule, 0, LV_PART_MAIN);

    create_key_count_panel(page, 7, 34, 176);

    brightness_value =
        create_metric_panel(page, 190, 34, 76, "BRIGHT", "--", lv_color_hex(0xffd84a));
    ambient_light_value =
        create_metric_panel(page, 190, 80, 76, "ALS", "N/A", lv_color_hex(0xb88cff));
    memory_value = create_metric_panel(page, 190, 126, 76, "MEM", "N/A", lv_color_hex(0x70e8f0));

    lv_obj_t *ble_panel = create_panel(page, 7, 84, 176, 86);
    create_label(ble_panel, "PERIPHERALS", 8, 6, lv_color_hex(0xffffff));
    create_peripheral_row(ble_panel, 0, 30, "P1");
    create_peripheral_row(ble_panel, 1, 58, "P2");

    lv_obj_t *fw_panel = create_panel(page, 7, 175, 260, 38);
    create_label(fw_panel, "FIRMWARE", 8, 6, lv_color_hex(0x8a949c));
    create_right_label(fw_panel, "UPTIME", 6, lv_color_hex(0x8a949c));
    create_firmware_version_label(fw_panel, 8, 20);
    uptime_value = create_right_label(fw_panel, "0d 00h 00m", 20, lv_color_hex(0x90ee7e));
    update_uptime_label();
    update_key_count_label();
    update_brightness_label();
    update_ambient_light_label();
    update_memory_label();
    update_peripheral_labels();

    lv_obj_add_flag(page, LV_OBJ_FLAG_HIDDEN);

    return page;
}

void prospector_diagnostics_page_set_visible(bool visible) {
    atomic_set(&diagnostics_visible, visible);

    if (visible) {
        update_uptime_label();
        update_key_count_label();
        update_brightness_label();
        update_ambient_light_label();
        update_memory_label();
        update_peripheral_labels();
        k_work_schedule(&diagnostics_tick_work, K_SECONDS(1));
    } else {
        k_work_cancel_delayable(&diagnostics_tick_work);
    }
}

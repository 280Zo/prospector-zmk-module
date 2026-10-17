/* SPDX-License-Identifier: MIT */

#include "diagnostics_page.h"
#if defined(CONFIG_PROSPECTOR_POWER_STATUS)
#include <prospector/power_status.h>
static lv_obj_t *power_status_values[2];
#endif

/* Task 8.9: rows are drawn by on-screen side (row 0 = left). */
static uint8_t source_for_row(uint8_t row) {
#if defined(CONFIG_PROSPECTOR_POWER_STATUS)
    uint8_t source = prospector_source_for_position(row);
    return source < 2 ? source : row;
#else
    return row;
#endif
}
static bool sides_known(void) {
#if defined(CONFIG_PROSPECTOR_POWER_STATUS)
    return prospector_sides_known();
#else
    return false;
#endif
}

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
#include <zmk/ble.h>
#include <zmk/display.h>
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

#include <prospector/session_stats.h>

/* Task 8.15: a drop counts only if that half was used within this window; each half
 * deep-sleeps after 10 min idle, which disconnects exactly like a real dropout. */
#define DROP_ACTIVE_WINDOW_MS       (9 * 60 * 1000)
#define DROPS_SHOWN_MAX             9
#define DIAGNOSTICS_PERIPHERAL_ROWS 2
#define RECENT_POSITION_EVENTS      8

static lv_obj_t *uptime_value;
static lv_obj_t *brightness_value;
static lv_obj_t *ambient_light_value;
static lv_obj_t *ball_value;
static lv_obj_t *typos_value;
static lv_obj_t *drops_value;
static lv_obj_t *memory_value;
/* Task 8.10: live fill bar (used/total) with a tick at the peak since boot. */
static lv_obj_t *memory_bar_track;
static lv_obj_t *memory_bar_fill;
static lv_obj_t *memory_peak_marker;
#define MEMORY_BAR_X     8
#define MEMORY_BAR_Y     38
#define MEMORY_BAR_WIDTH 60
static lv_obj_t *peripheral_rssi_values[DIAGNOSTICS_PERIPHERAL_ROWS];
static lv_obj_t *peripheral_latency_values[DIAGNOSTICS_PERIPHERAL_ROWS];
static atomic_t diagnostics_visible;
static atomic_t peripheral_drops[2];
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
        uint8_t source;

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
    }

    return ZMK_EV_EVENT_BUBBLE;
}

ZMK_LISTENER(prospector_diagnostics_position_latency, diagnostics_position_state_changed_listener);
ZMK_SUBSCRIPTION(prospector_diagnostics_position_latency, zmk_position_state_changed);

static lv_obj_t *create_label(lv_obj_t *parent, const char *text, int x, int y, lv_color_t color) {
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

static lv_obj_t *create_right_label(lv_obj_t *parent, const char *text, int y, lv_color_t color) {
    lv_obj_t *label = lv_label_create(parent);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_font(label, DIAGNOSTICS_FONT, LV_PART_MAIN);
    lv_obj_set_style_text_color(label, color, LV_PART_MAIN);
    lv_obj_align(label, LV_ALIGN_TOP_RIGHT, -8, y);

    return label;
}

static lv_obj_t *create_rect(lv_obj_t *parent, int x, int y, int width, int height,
                             lv_color_t color) {
    lv_obj_t *rect = lv_obj_create(parent);
    lv_obj_remove_style_all(rect);
    lv_obj_remove_flag(rect, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_pos(rect, x, y);
    lv_obj_set_size(rect, width, height);
    lv_obj_set_style_bg_color(rect, color, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(rect, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(rect, 1, LV_PART_MAIN);
    return rect;
}

static lv_obj_t *create_panel(lv_obj_t *parent, int x, int y, int width, int height) {
    lv_obj_t *panel = lv_obj_create(parent);
    lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_pos(panel, x, y);
    lv_obj_set_size(panel, width, height);
    lv_obj_set_style_bg_color(panel, lv_color_hex(0x000000), LV_PART_MAIN);
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

/* Task 8.15: BALL / TYPOS / DROPS, columns 57/56/61 px (TYPOS never exceeds "100%"). */
static lv_obj_t *create_stat_column(lv_obj_t *panel, int left, int width, const char *label,
                                    uint32_t color) {
    lv_obj_t *title = create_label(panel, label, 0, 5, lv_color_hex(0x8a949c));
    lv_obj_set_width(title, width);
    lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_x(title, left);
    lv_obj_t *value = create_label(panel, "--", 0, 22, lv_color_hex(color));
    lv_obj_set_width(value, width);
    lv_obj_set_style_text_align(value, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_x(value, left);
    return value;
}

static void create_session_stats_panel(lv_obj_t *parent, int x, int y) {
    lv_obj_t *panel = create_panel(parent, x, y, 176, 42);

    ball_value = create_stat_column(panel, 1, 57, "BALL", 0x70e8f0);
    create_rect(panel, 58, 8, 1, 26, lv_color_hex(0x30343a));
    typos_value = create_stat_column(panel, 58, 56, "TYPOS", 0xffd84a);
    create_rect(panel, 114, 8, 1, 26, lv_color_hex(0x30343a));
    drops_value = create_stat_column(panel, 114, 61, "DROPS", 0x90ee7e);
}

static lv_obj_t *peripheral_side_labels[DIAGNOSTICS_PERIPHERAL_ROWS];

static void create_peripheral_row(lv_obj_t *parent, uint8_t row, int y, const char *side) {
    peripheral_side_labels[row] = create_label(parent, side, 8, y, lv_color_hex(0x70e8f0));
    peripheral_rssi_values[row] = create_label(parent, "--dB", 34, y, lv_color_hex(0x70e8f0));
    peripheral_latency_values[row] = create_label(parent, "--ms", 116, y, lv_color_hex(0xffffff));
#if defined(CONFIG_PROSPECTOR_POWER_STATUS)
    if (row < 2) {
        power_status_values[row] =
            create_label(parent, "OFF/0 r0 ?", 8, y + 16, lv_color_hex(0xffd84a));
        lv_obj_set_width(power_status_values[row], 160);
        lv_label_set_long_mode(power_status_values[row], LV_LABEL_LONG_DOT);
    }
#endif
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

static void format_drops(char *out, size_t size, uint8_t row) {
    atomic_val_t drops = atomic_get(&peripheral_drops[source_for_row(row)]);

    if (drops > DROPS_SHOWN_MAX) {
        snprintk(out, size, "%c9+", row == 0 ? 'L' : 'R');
    } else {
        snprintk(out, size, "%c%d", row == 0 ? 'L' : 'R', (int)drops);
    }
}

static void update_session_stats_labels(void) {
    static char ball_text[16], typos_text[16], drops_text[16];
    char left[8], right[8];
    struct prospector_session_stats stats;

    if (ball_value == NULL) {
        return;
    }
    prospector_session_stats_get(&stats);
    prospector_session_stats_format_distance(stats.ball_q8, ball_text, sizeof(ball_text));
    prospector_session_stats_format_share(stats.backspace_count, stats.key_count, typos_text,
                                          sizeof(typos_text));
    format_drops(left, sizeof(left), 0);
    format_drops(right, sizeof(right), 1);
    snprintk(drops_text, sizeof(drops_text), "%s %s", left, right);
    lv_label_set_text(ball_value, ball_text);
    lv_label_set_text(typos_value, typos_text);
    lv_label_set_text(drops_value, drops_text);
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

    if (memory_value == NULL) {
        return;
    }

#if defined(CONFIG_SYS_HEAP_RUNTIME_STATS) && defined(CONFIG_LV_Z_MEM_POOL_SYS_HEAP)
    /* LVGL's display heap: the only memory on the dongle that grows and shrinks at
     * runtime. Show used (to 0.1 KiB) of the configured pool size (whole KiB). */
    struct sys_memory_stats stats;
    const uint32_t total = CONFIG_LV_Z_MEM_POOL_SIZE;
    lvgl_heap_stats(&stats);
    uint32_t used = stats.allocated_bytes < total ? (uint32_t)stats.allocated_bytes : total;
    uint32_t peak = stats.max_allocated_bytes < total ? (uint32_t)stats.max_allocated_bytes : total;
    uint32_t used_tenths = (used * 10U + 512U) / 1024U;
    snprintk(memory_text, sizeof(memory_text), "%u.%u/%uk", (unsigned)(used_tenths / 10U),
             (unsigned)(used_tenths % 10U), (unsigned)(total / 1024U));
    lv_label_set_text(memory_value, memory_text);
    if (memory_bar_fill != NULL && memory_peak_marker != NULL) {
        lv_obj_set_width(memory_bar_fill, (int32_t)(MEMORY_BAR_WIDTH * used / total));
        int32_t peak_x = (int32_t)(MEMORY_BAR_WIDTH * peak / total) - 1;
        if (peak_x < 0) {
            peak_x = 0;
        } else if (peak_x > MEMORY_BAR_WIDTH - 1) {
            peak_x = MEMORY_BAR_WIDTH - 1;
        }
        lv_obj_set_x(memory_peak_marker, MEMORY_BAR_X + peak_x);
    }
#else
    lv_label_set_text(memory_value, "N/A");
#endif
}

static void update_peripheral_labels(void) {
#if defined(CONFIG_PROSPECTOR_POWER_STATUS)
    for (uint8_t i = 0; i < 2; i++) {
        if (!power_status_values[i])
            continue;
        struct prospector_power_diagnostic state =
            prospector_power_diagnostic_get(source_for_row(i));
        char text[40];
        prospector_power_diagnostic_format(&state, text, sizeof(text));
        lv_label_set_text(power_status_values[i], text);
    }
#endif
    static char rssi_text[DIAGNOSTICS_PERIPHERAL_ROWS][8];
    static char latency_text[DIAGNOSTICS_PERIPHERAL_ROWS][8];

    static const char *const slot_names[] = {"P1", "P2"};
    static const char *const side_names[] = {"L", "R"};
    bool by_side = sides_known();

    for (uint8_t i = 0; i < DIAGNOSTICS_PERIPHERAL_ROWS; i++) {
        uint8_t src = source_for_row(i);
        bool rssi_known = atomic_get(&peripheral_rssi_known[src]);
        bool latency_known = atomic_get(&peripheral_latency_known[src]);
        int32_t rssi = atomic_get(&peripheral_rssi_dbm[src]);
        uint32_t latency_ms = atomic_get(&peripheral_latency_ms[src]);

        if (peripheral_side_labels[i] != NULL && i < 2) {
            lv_label_set_text(peripheral_side_labels[i], by_side ? side_names[i] : slot_names[i]);
        }

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

/* A sleeping half disconnects like a dropout; count only halves used recently. */
static void count_drop(uint8_t source) {
    struct prospector_session_stats stats;

    prospector_session_stats_get(&stats);
    if (stats.source_presses[source] == 0 ||
        k_uptime_get() - stats.last_press_ms[source] >= DROP_ACTIVE_WINDOW_MS) {
        return;
    }
    if (atomic_get(&peripheral_drops[source]) <= DROPS_SHOWN_MAX) {
        atomic_inc(&peripheral_drops[source]);
    }
}

static void diagnostics_bt_disconnected(struct bt_conn *conn, uint8_t reason) {
    ARG_UNUSED(reason);

    k_mutex_lock(&peripheral_connections_mutex, K_FOREVER);

    for (uint8_t i = 0; i < DIAGNOSTICS_PERIPHERAL_ROWS; i++) {
        if (peripheral_connections[i] == conn) {
            count_drop(i);
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

    update_uptime_label();
    update_session_stats_labels();
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
    lv_obj_set_style_bg_color(page, lv_color_hex(0x000000), LV_PART_MAIN);
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

    create_session_stats_panel(page, 7, 34);

    brightness_value =
        create_metric_panel(page, 190, 34, 76, "BRIGHT", "--", lv_color_hex(0xffd84a));
    ambient_light_value =
        create_metric_panel(page, 190, 80, 76, "ALS", "N/A", lv_color_hex(0xb88cff));
    lv_obj_t *memory_panel = create_panel(page, 190, 126, 76, 46);
    create_label(memory_panel, "UI MEM", 8, 4, lv_color_hex(0x8a949c));
    memory_value = create_label(memory_panel, "N/A", 8, 19, lv_color_hex(0x70e8f0));
    memory_bar_track = create_rect(memory_panel, MEMORY_BAR_X, MEMORY_BAR_Y, MEMORY_BAR_WIDTH, 4,
                                   lv_color_hex(0x30343a));
    memory_bar_fill =
        create_rect(memory_panel, MEMORY_BAR_X, MEMORY_BAR_Y, 0, 4, lv_color_hex(0x70e8f0));
    memory_peak_marker =
        create_rect(memory_panel, MEMORY_BAR_X, MEMORY_BAR_Y - 2, 2, 8, lv_color_hex(0xffd84a));

    lv_obj_t *ble_panel = create_panel(page, 7, 84, 176, 86);
    create_label(ble_panel, "PERIPHERALS", 8, 6, lv_color_hex(0xffffff));
#if defined(CONFIG_PROSPECTOR_POWER_STATUS)
    create_peripheral_row(ble_panel, 0, 22, "P1");
    create_peripheral_row(ble_panel, 1, 54, "P2");
#else
    create_peripheral_row(ble_panel, 0, 30, "P1");
    create_peripheral_row(ble_panel, 1, 58, "P2");
#endif

    lv_obj_t *fw_panel = create_panel(page, 7, 175, 260, 38);
    create_label(fw_panel, "FIRMWARE", 8, 6, lv_color_hex(0x8a949c));
    create_right_label(fw_panel, "UPTIME", 6, lv_color_hex(0x8a949c));
    create_firmware_version_label(fw_panel, 8, 20);
    uptime_value = create_right_label(fw_panel, "0d 00h 00m", 20, lv_color_hex(0x90ee7e));
    update_uptime_label();
    update_session_stats_labels();
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
        update_session_stats_labels();
        update_brightness_label();
        update_ambient_light_label();
        update_memory_label();
        update_peripheral_labels();
        k_work_schedule(&diagnostics_tick_work, K_SECONDS(1));
    } else {
        k_work_cancel_delayable(&diagnostics_tick_work);
    }
}

#if defined(PROSPECTOR_RENDER_DIAGNOSTICS)
void prospector_diagnostics_page_render_seed(void) {
    atomic_set(&peripheral_rssi_known[0], true);
    atomic_set(&peripheral_rssi_dbm[0], -58);
    atomic_set(&peripheral_latency_known[0], true);
    atomic_set(&peripheral_latency_ms[0], 14);

    atomic_set(&peripheral_rssi_known[1], true);
    atomic_set(&peripheral_rssi_dbm[1], -64);
    atomic_set(&peripheral_latency_known[1], true);
    atomic_set(&peripheral_latency_ms[1], 18);

    update_uptime_label();
    update_session_stats_labels();
    /* Host-only visual fixture; runtime counters are owned by session_stats. */
    lv_label_set_text(ball_value, "12.4m");
    lv_label_set_text(typos_value, "6.2%");
    lv_label_set_text(drops_value, "L0 R2");
    update_brightness_label();
    update_ambient_light_label();
    update_memory_label();
    update_peripheral_labels();
}
#endif

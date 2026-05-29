/* SPDX-License-Identifier: MIT */

#include "diagnostics_page.h"

#include <stdbool.h>
#include <stdint.h>

#include <zephyr/app_version.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/atomic.h>
#include <zmk/display.h>
#include <zmk/event_manager.h>
#include <zmk/events/keycode_state_changed.h>

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

static lv_obj_t *uptime_value;
static lv_obj_t *brightness_value;
static lv_obj_t *ambient_light_value;
static lv_obj_t *key_count_value;
static atomic_t diagnostics_visible;
static atomic_t key_press_count;

static void diagnostics_tick_work_handler(struct k_work *work);
static void diagnostics_update_work_handler(struct k_work *work);

K_WORK_DELAYABLE_DEFINE(diagnostics_tick_work, diagnostics_tick_work_handler);
K_WORK_DEFINE(diagnostics_update_work, diagnostics_update_work_handler);

static int diagnostics_keycode_state_changed_listener(const zmk_event_t *eh) {
    const struct zmk_keycode_state_changed *ev = as_zmk_keycode_state_changed(eh);

    if (ev != NULL && ev->state) {
        atomic_val_t current;

        do {
            current = atomic_get(&key_press_count);
            if (current >= KEY_PRESS_COUNT_MAX) {
                return ZMK_EV_EVENT_BUBBLE;
            }
        } while (!atomic_cas(&key_press_count, current, current + 1));
    }

    return ZMK_EV_EVENT_BUBBLE;
}

ZMK_LISTENER(prospector_diagnostics_key_count, diagnostics_keycode_state_changed_listener);
ZMK_SUBSCRIPTION(prospector_diagnostics_key_count, zmk_keycode_state_changed);

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
}

static void create_ble_row(lv_obj_t *parent, int y, const char *side, const char *bars,
                           const char *rssi, const char *interval) {
    create_label(parent, side, 8, y, lv_color_hex(0x70e8f0));
    create_label(parent, bars, 30, y, lv_color_hex(0x70e8f0));
    create_label(parent, rssi, 74, y, lv_color_hex(0x70e8f0));
    create_label(parent, interval, 136, y, lv_color_hex(0xffffff));
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
    static char key_count_text[8];
    atomic_val_t count = atomic_get(&key_press_count);

    if (key_count_value == NULL) {
        return;
    }

    if (count < 10000) {
        snprintk(key_count_text, sizeof(key_count_text), "%ld", (long)count);
    } else if (count < 100000) {
        snprintk(key_count_text, sizeof(key_count_text), "%ld.%ldk", (long)(count / 1000),
                 (long)((count / 100) % 10));
    } else if (count < 1000000) {
        snprintk(key_count_text, sizeof(key_count_text), "%ldk", (long)(count / 1000));
    } else if (count < 10000000) {
        snprintk(key_count_text, sizeof(key_count_text), "%ld.%ldM", (long)(count / 1000000),
                 (long)((count / 100000) % 10));
    } else if (count < KEY_PRESS_COUNT_MAX) {
        snprintk(key_count_text, sizeof(key_count_text), "%ldM", (long)(count / 1000000));
    } else {
        snprintk(key_count_text, sizeof(key_count_text), "999M+");
    }

    lv_label_set_text(key_count_value, key_count_text);
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
        k_work_submit_to_queue(zmk_display_work_q(), &diagnostics_update_work);
    }
}

static void diagnostics_update_work_handler(struct k_work *work) {
    ARG_UNUSED(work);

    if (!atomic_get(&diagnostics_visible)) {
        return;
    }

    update_uptime_label();
    update_key_count_label();
    update_brightness_label();
    update_ambient_light_label();
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
    create_metric_panel(page, 190, 126, 76, "MEM", "--/--", lv_color_hex(0x70e8f0));

    lv_obj_t *ble_panel = create_panel(page, 7, 84, 176, 86);
    create_label(ble_panel, "PERIPHERALS", 8, 6, lv_color_hex(0xffffff));
    create_ble_row(ble_panel, 30, "L", "[---]", "-100 dBm", "99ms");
    create_ble_row(ble_panel, 58, "R", "[---]", "-100 dBm", "99ms");

    lv_obj_t *fw_panel = create_panel(page, 7, 175, 260, 38);
    create_label(fw_panel, "FIRMWARE", 8, 6, lv_color_hex(0x8a949c));
    create_right_label(fw_panel, "UPTIME", 6, lv_color_hex(0x8a949c));
    create_firmware_version_label(fw_panel, 8, 20);
    uptime_value = create_right_label(fw_panel, "0d 00h 00m", 20, lv_color_hex(0x90ee7e));
    update_uptime_label();
    update_key_count_label();
    update_brightness_label();
    update_ambient_light_label();

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
        k_work_schedule(&diagnostics_tick_work, K_SECONDS(1));
    } else {
        k_work_cancel_delayable(&diagnostics_tick_work);
    }
}

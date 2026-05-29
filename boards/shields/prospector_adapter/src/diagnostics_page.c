/* SPDX-License-Identifier: MIT */

#include "diagnostics_page.h"

#include <stdbool.h>
#include <stdint.h>

#include <zephyr/kernel.h>
#include <zephyr/sys/atomic.h>
#include <zmk/display.h>

#if LV_FONT_MONTSERRAT_14
#define DIAGNOSTICS_FONT (&lv_font_montserrat_14)
#else
#define DIAGNOSTICS_FONT LV_FONT_DEFAULT
#endif

static lv_obj_t *uptime_value;
static atomic_t diagnostics_visible;

static void uptime_tick_work_handler(struct k_work *work);
static void uptime_update_work_handler(struct k_work *work);

K_WORK_DELAYABLE_DEFINE(uptime_tick_work, uptime_tick_work_handler);
K_WORK_DEFINE(uptime_update_work, uptime_update_work_handler);

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

static void create_metric_panel(lv_obj_t *parent, int x, int y, int width, const char *label,
                                const char *value, lv_color_t value_color) {
    lv_obj_t *panel = create_panel(parent, x, y, width, 42);

    create_label(panel, label, 8, 6, lv_color_hex(0x8a949c));
    create_label(panel, value, 8, 22, value_color);
}

static void create_key_count_panel(lv_obj_t *parent, int x, int y, int width) {
    lv_obj_t *panel = create_panel(parent, x, y, width, 42);

    create_label(panel, "KEYS PRESSED", 8, 5, lv_color_hex(0x8a949c));
    create_label(panel, "000000", 8, 25, lv_color_hex(0x70e8f0));
}

static void create_ble_row(lv_obj_t *parent, int y, const char *side, const char *bars,
                           const char *rssi, const char *interval) {
    create_label(parent, side, 8, y, lv_color_hex(0x70e8f0));
    create_label(parent, bars, 30, y, lv_color_hex(0x70e8f0));
    create_label(parent, rssi, 74, y, lv_color_hex(0x70e8f0));
    create_label(parent, interval, 136, y, lv_color_hex(0xffffff));
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

static void uptime_tick_work_handler(struct k_work *work) {
    ARG_UNUSED(work);

    if (atomic_get(&diagnostics_visible) && zmk_display_is_initialized()) {
        k_work_submit_to_queue(zmk_display_work_q(), &uptime_update_work);
    }
}

static void uptime_update_work_handler(struct k_work *work) {
    ARG_UNUSED(work);

    if (!atomic_get(&diagnostics_visible)) {
        return;
    }

    update_uptime_label();
    k_work_schedule(&uptime_tick_work, K_MINUTES(1));
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

    create_metric_panel(page, 190, 34, 76, "BL", "auto 100%", lv_color_hex(0xffd84a));
    create_metric_panel(page, 190, 80, 76, "ALS", "N/A", lv_color_hex(0xb88cff));
    create_metric_panel(page, 190, 126, 76, "MEM", "--/--", lv_color_hex(0x70e8f0));

    lv_obj_t *ble_panel = create_panel(page, 7, 84, 176, 86);
    create_label(ble_panel, "PERIPHERALS", 8, 6, lv_color_hex(0xffffff));
    create_ble_row(ble_panel, 30, "L", "[---]", "-100 dBm", "99ms");
    create_ble_row(ble_panel, 58, "R", "[---]", "-100 dBm", "99ms");

    lv_obj_t *fw_panel = create_panel(page, 7, 175, 260, 38);
    create_label(fw_panel, "FIRMWARE", 8, 6, lv_color_hex(0x8a949c));
    create_right_label(fw_panel, "UPTIME", 6, lv_color_hex(0x8a949c));
    create_label(fw_panel, "zmk --", 8, 20, lv_color_hex(0x90ee7e));
    uptime_value = create_right_label(fw_panel, "0d 00h 00m", 20, lv_color_hex(0x90ee7e));
    update_uptime_label();

    lv_obj_add_flag(page, LV_OBJ_FLAG_HIDDEN);

    return page;
}

void prospector_diagnostics_page_set_visible(bool visible) {
    atomic_set(&diagnostics_visible, visible);

    if (visible) {
        update_uptime_label();
        k_work_schedule(&uptime_tick_work, K_MINUTES(1));
    } else {
        k_work_cancel_delayable(&uptime_tick_work);
    }
}

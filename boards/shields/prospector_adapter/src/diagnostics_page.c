/* SPDX-License-Identifier: MIT */

#include "diagnostics_page.h"

#include <stdint.h>

#include <zephyr/kernel.h>
#include <zmk/display.h>

static lv_obj_t *uptime_value;

static void uptime_tick_work_handler(struct k_work *work);
static void uptime_update_work_handler(struct k_work *work);

K_WORK_DELAYABLE_DEFINE(uptime_tick_work, uptime_tick_work_handler);
K_WORK_DEFINE(uptime_update_work, uptime_update_work_handler);

static lv_obj_t *create_row(lv_obj_t *parent, const char *label, const char *value, int y,
                            lv_color_t value_color) {
    lv_obj_t *label_obj = lv_label_create(parent);
    lv_label_set_text(label_obj, label);
    lv_obj_set_style_text_font(label_obj, LV_FONT_DEFAULT, LV_PART_MAIN);
    lv_obj_set_style_text_color(label_obj, lv_color_hex(0x8a949c), LV_PART_MAIN);
    lv_obj_set_pos(label_obj, 14, y);

    lv_obj_t *value_obj = lv_label_create(parent);
    lv_label_set_text(value_obj, value);
    lv_obj_set_style_text_font(value_obj, LV_FONT_DEFAULT, LV_PART_MAIN);
    lv_obj_set_style_text_color(value_obj, value_color, LV_PART_MAIN);
    lv_obj_align_to(value_obj, label_obj, LV_ALIGN_OUT_RIGHT_MID, 14, 0);

    return value_obj;
}

static void update_uptime_label(void) {
    static char uptime_text[16];
    uint64_t uptime_sec = k_uptime_get() / 1000U;
    uint32_t hours = uptime_sec / 3600U;
    uint32_t minutes = (uptime_sec % 3600U) / 60U;
    uint32_t seconds = uptime_sec % 60U;

    if (uptime_value == NULL) {
        return;
    }

    snprintk(uptime_text, sizeof(uptime_text), "%02u:%02u:%02u", hours, minutes, seconds);
    lv_label_set_text(uptime_value, uptime_text);
}

static void uptime_tick_work_handler(struct k_work *work) {
    ARG_UNUSED(work);

    if (zmk_display_is_initialized()) {
        k_work_submit_to_queue(zmk_display_work_q(), &uptime_update_work);
    }

    k_work_schedule(&uptime_tick_work, K_SECONDS(1));
}

static void uptime_update_work_handler(struct k_work *work) {
    ARG_UNUSED(work);

    update_uptime_label();
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

    lv_obj_t *title = lv_label_create(page);
    lv_label_set_text(title, "DIAGNOSTICS");
    lv_obj_set_style_text_font(title, LV_FONT_DEFAULT, LV_PART_MAIN);
    lv_obj_set_style_text_color(title, lv_color_hex(0xffffff), LV_PART_MAIN);
    lv_obj_set_pos(title, 14, 10);

    create_row(page, "PAGE", "diagnostics", 46, lv_color_hex(0x38ffb3));
    uptime_value = create_row(page, "UP", "--:--:--", 76, lv_color_hex(0xffffff));

    update_uptime_label();
    k_work_schedule(&uptime_tick_work, K_SECONDS(1));

    lv_obj_add_flag(page, LV_OBJ_FLAG_HIDDEN);
    return page;
}

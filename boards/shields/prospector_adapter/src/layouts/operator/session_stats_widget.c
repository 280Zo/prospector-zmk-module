/* SPDX-License-Identifier: MIT */
#include <zephyr/kernel.h>
#if IS_ENABLED(CONFIG_PROSPECTOR_OPERATOR_SESSION_STATS)
#include "session_stats_widget.h"
#include "display_colors.h"
#include <fonts.h>
#include <prospector/session_stats.h>
#if IS_ENABLED(CONFIG_PROSPECTOR_POWER_STATUS)
#include <prospector/power_status.h>
#endif

#define STATS_Y              7
#define BALANCE_TRACK_WIDTH  58
#define BALANCE_MARKER_WIDTH 6
#define BALANCE_RANGE        18
#define BALANCE_TRACK_X      ((280 - BALANCE_TRACK_WIDTH) / 2)
/* Marker (track y - 4, 12 px tall) centred on the stat digits' ink. */
#define BALANCE_TRACK_Y 16

static lv_obj_t *top_label, *top_value, *count_label, *count_value;
static lv_obj_t *balance_track, *balance_tick, *balance_marker;
static lv_timer_t *stats_timer;

static lv_obj_t *stats_text(lv_obj_t *parent, const char *text, int x, int y, const lv_font_t *font,
                            uint32_t color) {
    lv_obj_t *label = lv_label_create(parent);
    lv_obj_remove_flag(label, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_font(label, font, 0);
    lv_obj_set_style_text_color(label, lv_color_hex(color), 0);
    lv_obj_set_pos(label, x, y);
    return label;
}

static lv_obj_t *stats_rect(lv_obj_t *parent, int x, int y, int width, int height, uint32_t color) {
    lv_obj_t *rect = lv_obj_create(parent);
    lv_obj_remove_style_all(rect);
    lv_obj_remove_flag(rect, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_pos(rect, x, y);
    lv_obj_set_size(rect, width, height);
    lv_obj_set_style_bg_color(rect, lv_color_hex(color), 0);
    lv_obj_set_style_bg_opa(rect, LV_OPA_COVER, 0);
    return rect;
}

/* LVGL timer callbacks run with the display's LVGL handler, never input events. */
static void session_stats_widget_refresh(lv_timer_t *timer) {
    ARG_UNUSED(timer);
    struct prospector_session_stats stats;
    prospector_session_stats_get(&stats);
    char count[16];
    prospector_session_stats_format_count(stats.key_count, count, sizeof(count));
    /* FG_Medium_20 has no lowercase glyphs; its capital K matches the existing M. */
    for (char *c = count; *c; c++) {
        if (*c == 'k')
            *c = 'K';
        else if (*c == '+')
            *c = '\0'; /* Saturated "999M+" would reach the centred balance bar. */
    }
    lv_label_set_text(count_value, count);
    lv_label_set_text_fmt(top_value, "%u", stats.top_wpm);
    lv_obj_update_layout(count_value);
    lv_obj_set_x(count_value, lv_obj_get_x(count_label) - lv_obj_get_width(count_value) - 4);

    /* The tug is stored per source; mapping at render time follows late side reports. */
    int32_t balance = stats.tug;
#if IS_ENABLED(CONFIG_PROSPECTOR_POWER_STATUS)
    if (prospector_source_for_position(0) == 1)
        balance = -balance;
#endif
    if (balance < -BALANCE_RANGE)
        balance = -BALANCE_RANGE;
    else if (balance > BALANCE_RANGE)
        balance = BALANCE_RANGE;
    int32_t travel = BALANCE_TRACK_WIDTH - BALANCE_MARKER_WIDTH;
    lv_obj_set_pos(balance_marker,
                   BALANCE_TRACK_X + ((balance + BALANCE_RANGE) * travel) / (2 * BALANCE_RANGE),
                   BALANCE_TRACK_Y - 4);
}

void prospector_session_stats_widget_init(lv_obj_t *parent) {
    if (top_label != NULL)
        return;
    /* At the same y, the two-line Montserrat 10 stack and the FG_Medium_20 digits share
     * a vertical ink centre (measured from rendered pixels; asserted by the layout test). */
    /* x13/x42: AJ moved the TOP section right by half the T's width (3 px). */
    top_label = stats_text(parent, "TOP\nWPM", 13, STATS_Y, &lv_font_montserrat_10,
                           DISPLAY_COLOR_LABEL);
    lv_obj_set_style_text_align(top_label, LV_TEXT_ALIGN_CENTER, 0);
    top_value = stats_text(parent, "0", 42, STATS_Y, &FG_Medium_20, DISPLAY_COLOR_VALUE);
    count_label = stats_text(parent, "KEY\nPRESS", 0, STATS_Y, &lv_font_montserrat_10,
                             DISPLAY_COLOR_LABEL);
    lv_obj_set_style_text_align(count_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(count_label, LV_ALIGN_TOP_RIGHT, -10, STATS_Y);
    lv_obj_update_layout(count_label);
    count_value = stats_text(parent, "0", 0, STATS_Y, &FG_Medium_20, DISPLAY_COLOR_VALUE);

    /* Diagnostics key balance (create_key_count_panel), centred on the screen. */
    balance_track = stats_rect(parent, BALANCE_TRACK_X, BALANCE_TRACK_Y, BALANCE_TRACK_WIDTH, 4,
                               DISPLAY_COLOR_BALANCE_TRACK);
    lv_obj_set_style_radius(balance_track, 2, 0);
    balance_tick = stats_rect(parent, BALANCE_TRACK_X + BALANCE_TRACK_WIDTH / 2 - 1,
                              BALANCE_TRACK_Y - 3, 2, 10, DISPLAY_COLOR_BALANCE_TICK);
    lv_obj_set_style_radius(balance_tick, 1, 0);
    balance_marker =
        stats_rect(parent, 0, 0, BALANCE_MARKER_WIDTH, 12, DISPLAY_COLOR_BALANCE_MARKER);
    lv_obj_set_style_radius(balance_marker, 2, 0);
    session_stats_widget_refresh(NULL);
    stats_timer = lv_timer_create(session_stats_widget_refresh, 1000, NULL);
}

/* Theme switch: re-apply the stats row colours. */
void prospector_session_stats_widget_apply_palette(void) {
    if (top_label == NULL)
        return;
    lv_obj_set_style_text_color(top_label, lv_color_hex(DISPLAY_COLOR_LABEL), 0);
    lv_obj_set_style_text_color(count_label, lv_color_hex(DISPLAY_COLOR_LABEL), 0);
    lv_obj_set_style_text_color(top_value, lv_color_hex(DISPLAY_COLOR_VALUE), 0);
    lv_obj_set_style_text_color(count_value, lv_color_hex(DISPLAY_COLOR_VALUE), 0);
    lv_obj_set_style_bg_color(balance_track, lv_color_hex(DISPLAY_COLOR_BALANCE_TRACK), 0);
    lv_obj_set_style_bg_color(balance_tick, lv_color_hex(DISPLAY_COLOR_BALANCE_TICK), 0);
    lv_obj_set_style_bg_color(balance_marker, lv_color_hex(DISPLAY_COLOR_BALANCE_MARKER), 0);
}
#endif

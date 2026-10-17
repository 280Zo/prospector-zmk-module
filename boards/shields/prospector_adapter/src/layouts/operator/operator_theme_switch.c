/* SPDX-License-Identifier: MIT */
#include <stdbool.h>
#include <zephyr/kernel.h>
#include <zmk/display.h>

#include "../../brightness_toast.h"
#include "operator_theme.h"

/* Where the user is heading. Written by the key handler (or settings load), read once per
 * run by the display queue; presses always count from it, never from a theme that is
 * about to be replaced. requested_valid is false until the first request. */
static volatile size_t requested_theme;
static volatile bool requested_valid;
static volatile bool request_silent;

static void apply_all(void) {
    zmk_widget_wpm_meter_apply_palette();
    zmk_widget_layer_display_apply_palette();
#if !IS_ENABLED(CONFIG_PROSPECTOR_MODIFIER_TOAST)
    zmk_widget_modifier_indicator_apply_palette();
#endif
    zmk_widget_battery_circles_apply_palette();
    zmk_widget_output_apply_palette();
#if IS_ENABLED(CONFIG_PROSPECTOR_OPERATOR_SESSION_STATS)
    prospector_session_stats_widget_apply_palette();
    prospector_modifier_row_apply_palette();
#endif
    operator_status_screen_apply_palette();
}

static void theme_work_handler(struct k_work *work) {
    (void)work;
    bool silent = request_silent;
    size_t target = requested_theme;
    if (target == operator_theme_index())
        return;
    operator_theme_select(target);
    apply_all();
    if (!silent) {
        prospector_theme_toast_request(operator_theme_current()->name);
        operator_theme_store_schedule();
    }
}

K_WORK_DEFINE(operator_theme_work, theme_work_handler);

static void request(size_t index, bool silent) {
    if (index >= operator_theme_count())
        return;
    requested_theme = index;
    requested_valid = true;
    request_silent = silent;
    if (!zmk_display_is_initialized()) {
        operator_theme_select(index); /* widgets read it when they are created */
        if (!silent)
            operator_theme_store_schedule();
        return;
    }
    k_work_submit_to_queue(zmk_display_work_q(), &operator_theme_work);
}

void operator_theme_request(size_t index) { request(index, false); }

void operator_theme_restore(size_t index) { request(index, true); }

static size_t latest(void) { return requested_valid ? requested_theme : operator_theme_index(); }

void operator_theme_next(void) { operator_theme_request((latest() + 1) % operator_theme_count()); }

void operator_theme_prev(void) {
    size_t count = operator_theme_count();
    operator_theme_request((latest() + count - 1) % count);
}

/* SPDX-License-Identifier: MIT */
#pragma once

#include <stddef.h>

#include "display_colors.h"

/* Themes in cycle order: Default, Ember, Glacier, Synthwave, Sea Shells, GitHub. */
size_t operator_theme_count(void);
size_t operator_theme_index(void);
const struct operator_palette *operator_theme_at(size_t index);
/* Index of the theme chosen in the build configuration. */
size_t operator_theme_start_index(void);
/* Changes the current palette only; widgets are not recoloured. */
void operator_theme_select(size_t index);

#include <stdbool.h>
#include <stdint.h>

/* Startup theme from the saved choice: the saved index while the build's start theme is
 * unchanged and the index exists, otherwise the build's start theme. */
size_t operator_theme_resolve_start(bool saved, uint8_t saved_start, uint8_t saved_index,
                                    size_t build_start, size_t count);

/* Live switching (operator_theme_switch.c). Safe from any thread: the newest request
 * wins and is applied on the display work queue. Next/previous wrap around. */
void operator_theme_request(size_t index);
void operator_theme_next(void);
void operator_theme_prev(void);
/* Like a request, but without the name pop-up and without saving (boot restore). */
void operator_theme_restore(size_t index);

/* Saves the current theme a few seconds after the last switch (operator_theme_store.c). */
void operator_theme_store_schedule(void);

/* Recolour hooks, called only from the display work queue after a switch. */
void operator_status_screen_apply_palette(void);
void zmk_widget_wpm_meter_apply_palette(void);
void zmk_widget_layer_display_apply_palette(void);
void zmk_widget_modifier_indicator_apply_palette(void);
void zmk_widget_battery_circles_apply_palette(void);
void zmk_widget_output_apply_palette(void);
void prospector_session_stats_widget_apply_palette(void);
void prospector_modifier_row_apply_palette(void);

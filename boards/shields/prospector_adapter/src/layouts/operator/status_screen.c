#include <lvgl.h>

#include "background_texture.h"
#include "battery_circles.h"
#include "display_colors.h"
#include "layer_display.h"
#include "modifier_indicator.h"
#include "output.h"
#include "wpm_meter.h"
#if IS_ENABLED(CONFIG_PROSPECTOR_OPERATOR_SESSION_STATS)
#include "session_stats_widget.h"
#endif

#include <fonts.h>

#if !IS_ENABLED(CONFIG_PROSPECTOR_MODIFIER_TOAST)
static struct zmk_widget_modifier_indicator modifier_indicator_widget;
#endif
static struct zmk_widget_wpm_meter wpm_meter_widget;
static struct zmk_widget_layer_display layer_display_widget;
static struct zmk_widget_battery_circles battery_circles_widget;
static struct zmk_widget_output output_widget;

lv_obj_t *zmk_display_status_screen() {
    lv_obj_t *screen = lv_obj_create(NULL);
    lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(screen, lv_color_hex(DISPLAY_COLOR_BG), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(screen, 255, LV_PART_MAIN);
    operator_background_texture_apply(screen);

#if !IS_ENABLED(CONFIG_PROSPECTOR_MODIFIER_TOAST)
    zmk_widget_modifier_indicator_init(&modifier_indicator_widget, screen);
    lv_obj_set_pos(zmk_widget_modifier_indicator_obj(&modifier_indicator_widget), 25, 8);
#endif

    zmk_widget_wpm_meter_init(&wpm_meter_widget, screen);
#if IS_ENABLED(CONFIG_PROSPECTOR_MODIFIER_TOAST) &&                                                \
    !IS_ENABLED(CONFIG_PROSPECTOR_OPERATOR_SESSION_STATS)
    lv_obj_set_pos(zmk_widget_wpm_meter_obj(&wpm_meter_widget), 10, 12);
#else
    lv_obj_set_pos(zmk_widget_wpm_meter_obj(&wpm_meter_widget), 10, 42);
#endif

    zmk_widget_layer_display_init(&layer_display_widget, screen);
    lv_obj_set_pos(zmk_widget_layer_display_obj(&layer_display_widget), 10, 142);

    zmk_widget_battery_circles_init(&battery_circles_widget, screen);
    lv_obj_set_pos(zmk_widget_battery_circles_obj(&battery_circles_widget), 11, 170);

    zmk_widget_output_init(&output_widget, screen);
    lv_obj_set_pos(zmk_widget_output_obj(&output_widget), 148, 170);

#if IS_ENABLED(CONFIG_PROSPECTOR_OPERATOR_SESSION_STATS)
    prospector_session_stats_widget_init(screen);
#endif

    return screen;
}

/* SPDX-License-Identifier: MIT */
#include "brightness_toast.h"

#include <lvgl.h>
#include <stdio.h>
#include <string.h>
#include <zephyr/kernel.h>
#include <zmk/display.h>
#if defined(CONFIG_PROSPECTOR_OPERATOR_SESSION_STATS)
#include <modifier_order.h>
#endif

#define TOAST_DURATION_MS 1500
#define CHORD_RELEASE_MS  300

static lv_obj_t *toast;
static lv_obj_t *toast_label;
static uint32_t latest_sequence;
static uint8_t requested_target;
static bool requested_auto;
static int64_t expires_at;
static lv_obj_t *mode_row;
static lv_obj_t *word_badge;
static lv_obj_t *lock_badge;
static bool overlay_active = true;
static bool requested_word;
static bool requested_lock;
static bool chord_held;
static char requested_chord[64];
static int64_t chord_expires_at;
static bool modifier_cache_initialized;
K_MUTEX_DEFINE(toast_lock);

static void toast_work_handler(struct k_work *work);
static void modifier_state_request(const char *chord, bool caps_word, bool caps_lock, bool active,
                                   uint8_t mods);
K_WORK_DELAYABLE_DEFINE(toast_work, toast_work_handler);

#if defined(CONFIG_PROSPECTOR_OPERATOR_SESSION_STATS)
#ifndef OPERATOR_MOD_INACTIVE_COLOR
/* AJ chose the "halfway" dim (between the old grey and the unlit WPM bars). */
#define OPERATOR_MOD_INACTIVE_COLOR 0x34373a
#endif
static lv_obj_t *operator_row;
static lv_obj_t *operator_mod_labels[4];
static lv_obj_t *operator_caps_label;
static lv_obj_t *operator_brightness_label;
static bool operator_status_visible = true;
static uint8_t requested_mods, displayed_mods;
#endif

static void hide(lv_obj_t *obj) {
    if (obj != NULL)
        lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
}

static void create_primary(void) {
    if (toast == NULL) {
        toast = lv_obj_create(lv_layer_sys());
        lv_obj_remove_flag(toast, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_size(toast, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
        lv_obj_set_style_bg_color(toast, lv_color_hex(0x080808), 0);
        lv_obj_set_style_bg_opa(toast, LV_OPA_COVER, 0);
        lv_obj_set_style_border_color(toast, lv_color_hex(0x30343a), 0);
        lv_obj_set_style_border_width(toast, 1, 0);
        lv_obj_set_style_radius(toast, 6, 0);
        lv_obj_set_style_pad_all(toast, 6, 0);
        toast_label = lv_label_create(toast);
        lv_obj_remove_flag(toast_label, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_style_text_color(toast_label, lv_color_hex(0xffd84a), 0);
        lv_obj_set_style_text_font(toast_label, LV_FONT_DEFAULT, 0);
        lv_obj_set_style_max_width(toast_label, 246, 0);
        lv_label_set_long_mode(toast_label, LV_LABEL_LONG_WRAP);
    }
}

static lv_obj_t *create_badge(const char *text) {
    lv_obj_t *badge = lv_obj_create(mode_row);
    lv_obj_remove_flag(badge, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(badge, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_color(badge, lv_color_hex(0x080808), 0);
    lv_obj_set_style_bg_opa(badge, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(badge, lv_color_hex(0x30343a), 0);
    lv_obj_set_style_border_width(badge, 1, 0);
    lv_obj_set_style_radius(badge, 6, 0);
    lv_obj_set_style_pad_all(badge, 3, 0);
    lv_obj_t *label = lv_label_create(badge);
    lv_obj_remove_flag(label, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_text_font(label, LV_FONT_DEFAULT, 0);
    lv_obj_set_style_text_color(label, lv_color_hex(0xffbf00), 0);
    lv_obj_set_style_max_width(label, 118, 0);
    lv_label_set_long_mode(label, LV_LABEL_LONG_WRAP);
    lv_label_set_text(label, text);
    return badge;
}

static void render_modes(bool primary_visible) {
    if (!requested_word && !requested_lock) {
        hide(mode_row);
        return;
    }
    if (!mode_row) {
        mode_row = lv_obj_create(lv_layer_sys());
        lv_obj_remove_flag(mode_row, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_size(mode_row, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
        lv_obj_set_style_bg_opa(mode_row, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(mode_row, 0, 0);
        lv_obj_set_style_pad_all(mode_row, 0, 0);
        lv_obj_set_style_pad_column(mode_row, 4, 0);
        lv_obj_set_flex_flow(mode_row, LV_FLEX_FLOW_ROW);
        word_badge = create_badge("CAPS WORD");
        lock_badge = create_badge("CAPS LOCK");
    }
    if (requested_word)
        lv_obj_remove_flag(word_badge, LV_OBJ_FLAG_HIDDEN);
    else
        hide(word_badge);
    if (requested_lock)
        lv_obj_remove_flag(lock_badge, LV_OBJ_FLAG_HIDDEN);
    else
        hide(lock_badge);
    lv_obj_remove_flag(mode_row, LV_OBJ_FLAG_HIDDEN);
    lv_obj_update_layout(lv_layer_sys());
    int y = primary_visible ? 12 + lv_obj_get_height(toast) : 6;
    lv_obj_align(mode_row, LV_ALIGN_TOP_RIGHT, -6, y);
    lv_obj_move_foreground(mode_row);
}

#if defined(CONFIG_PROSPECTOR_OPERATOR_SESSION_STATS)
static void render_operator_row(int64_t remaining, int64_t grace) {
    if (!operator_row) {
        operator_row = lv_obj_create(lv_layer_sys());
        lv_obj_remove_style_all(operator_row);
        lv_obj_remove_flag(operator_row, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_size(operator_row, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
        lv_obj_set_flex_flow(operator_row, LV_FLEX_FLOW_ROW);
        lv_obj_set_style_pad_column(operator_row, 5, 0);
        for (int i = 0; i < 4; i++) {
            operator_mod_labels[i] = lv_label_create(operator_row);
            lv_obj_set_style_text_font(operator_mod_labels[i], &lv_font_montserrat_14, 0);
            lv_obj_remove_flag(operator_mod_labels[i],
                               LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
            lv_label_set_text(operator_mod_labels[i], modifier_order_get_text(i));
        }
        operator_caps_label = lv_label_create(operator_row);
        lv_obj_set_style_text_font(operator_caps_label, &lv_font_montserrat_14, 0);
        lv_obj_remove_flag(operator_caps_label, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
        /* One fixed slot for both Caps texts, so switching never moves the row. */
        lv_point_t word_size, lock_size;
        lv_text_get_size(&word_size, "CAPS WORD", &lv_font_montserrat_14, 0, 0, LV_COORD_MAX,
                         LV_TEXT_FLAG_NONE);
        lv_text_get_size(&lock_size, "CAPS LOCK", &lv_font_montserrat_14, 0, 0, LV_COORD_MAX,
                         LV_TEXT_FLAG_NONE);
        lv_obj_set_width(operator_caps_label,
                         word_size.x > lock_size.x ? word_size.x : lock_size.x);
        lv_obj_set_style_text_align(operator_caps_label, LV_TEXT_ALIGN_CENTER, 0);
        operator_brightness_label = lv_label_create(lv_layer_sys());
        lv_obj_set_style_text_font(operator_brightness_label, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(operator_brightness_label, lv_color_hex(0xffd84a), 0);
        lv_obj_remove_flag(operator_brightness_label,
                           LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    }
    if (requested_mods != 0)
        displayed_mods = requested_mods;
    else if (!chord_held && grace <= 0)
        displayed_mods = 0;
    const uint8_t masks[] = {0x88, 0x44, 0x11, 0x22}; /* GUI, ALT, CTRL, SHIFT; both sides. */
    for (int i = 0; i < 4; i++) {
        bool held = (displayed_mods & masks[modifier_order_get(i)]) != 0;
        lv_obj_set_style_text_color(operator_mod_labels[i],
                                    lv_color_hex(held ? 0xb1e5f0 : OPERATOR_MOD_INACTIVE_COLOR), 0);
    }
    lv_label_set_text(operator_caps_label, requested_lock ? "CAPS LOCK" : "CAPS WORD");
    lv_obj_set_style_text_color(
        operator_caps_label,
        lv_color_hex(requested_lock || requested_word ? 0xffbf00 : OPERATOR_MOD_INACTIVE_COLOR), 0);
    if (remaining > 0) {
        hide(operator_row);
        if (requested_auto)
            lv_label_set_text(operator_brightness_label, "Auto");
        else
            lv_label_set_text_fmt(operator_brightness_label, "%u%%", requested_target);
        lv_obj_align(operator_brightness_label, LV_ALIGN_TOP_MID, 0, 151);
        lv_obj_remove_flag(operator_brightness_label, LV_OBJ_FLAG_HIDDEN);
        lv_obj_move_foreground(operator_brightness_label);
    } else {
        hide(operator_brightness_label);
        lv_obj_align(operator_row, LV_ALIGN_TOP_MID, 0, 151);
        lv_obj_remove_flag(operator_row, LV_OBJ_FLAG_HIDDEN);
        lv_obj_move_foreground(operator_row);
    }
}

void prospector_modifier_row_request(uint8_t mods, bool caps_word, bool caps_lock, bool active) {
    const uint8_t masks[] = {0x88, 0x44, 0x11, 0x22};
    char chord[64] = "";
    size_t used = 0;
    for (int i = 0; i < 4; i++) {
        if (!(mods & masks[modifier_order_get(i)]))
            continue;
        int n = snprintf(chord + used, sizeof(chord) - used, "%s%s", used ? " + " : "",
                         modifier_order_get_text(i));
        if (n < 0 || (size_t)n >= sizeof(chord) - used) {
            chord[used] = '\0';
            break;
        }
        used += n;
    }
    modifier_state_request(chord, caps_word, caps_lock, active, mods);
}

void prospector_modifier_row_set_status_visible(bool visible) {
    k_mutex_lock(&toast_lock, K_FOREVER);
    operator_status_visible = visible;
    if (zmk_display_is_initialized())
        k_work_reschedule_for_queue(zmk_display_work_q(), &toast_work, K_MSEC(0));
    k_mutex_unlock(&toast_lock);
}
#endif

/* Only this display-queue handler touches LVGL. Holding the toast lock prevents
 * newer input/commands from losing their reschedule to an old expiry. */
static void toast_work_handler(struct k_work *work) {
    ARG_UNUSED(work);
    k_mutex_lock(&toast_lock, K_FOREVER);
    if (!zmk_display_is_initialized() || !overlay_active) {
        hide(toast);
        hide(mode_row);
#if defined(CONFIG_PROSPECTOR_OPERATOR_SESSION_STATS)
        hide(operator_row);
        hide(operator_brightness_label);
#endif
        k_mutex_unlock(&toast_lock);
        return;
    }
    int64_t now = k_uptime_get();
    int64_t remaining = expires_at - now;
    int64_t grace = chord_expires_at - now;
    if (!chord_held && grace <= 0)
        requested_chord[0] = '\0';
    bool primary_visible = remaining > 0 || requested_chord[0] != '\0';
#if defined(CONFIG_PROSPECTOR_OPERATOR_SESSION_STATS)
    if (operator_status_visible) {
        hide(toast);
        hide(mode_row);
        render_operator_row(remaining, grace);
    } else {
        hide(operator_row);
        hide(operator_brightness_label);
#endif
        if (primary_visible) {
            create_primary();
            if (remaining > 0) {
                if (requested_auto)
                    lv_label_set_text(toast_label, "Auto");
                else
                    lv_label_set_text_fmt(toast_label, "%u%%", requested_target);
                lv_obj_set_style_text_color(toast_label, lv_color_hex(0xffd84a), 0);
            } else {
                lv_label_set_text(toast_label, requested_chord);
                lv_obj_set_style_text_color(toast_label, lv_color_hex(0xb1e5f0), 0);
            }
            lv_obj_align(toast, LV_ALIGN_TOP_RIGHT, -6, 6);
            lv_obj_remove_flag(toast, LV_OBJ_FLAG_HIDDEN);
            lv_obj_move_foreground(toast);
        } else
            hide(toast);
        render_modes(primary_visible);
#if defined(CONFIG_PROSPECTOR_OPERATOR_SESSION_STATS)
    }
#endif
    int64_t next = remaining > 0 ? remaining : 0;
    if (!chord_held && grace > 0 && (next == 0 || grace < next))
        next = grace;
    if (next > 0)
        k_work_reschedule_for_queue(zmk_display_work_q(), &toast_work, K_MSEC(next));
    k_mutex_unlock(&toast_lock);
}

void prospector_brightness_toast_request(uint32_t sequence, uint8_t target, bool automatic,
                                         bool show) {
    k_mutex_lock(&toast_lock, K_FOREVER);
    /* An idle/wake or newer command may publish before an older caller does. */
    if ((int32_t)(sequence - latest_sequence) <= 0) {
        k_mutex_unlock(&toast_lock);
        return;
    }
    latest_sequence = sequence;
    requested_target = target;
    requested_auto = automatic;
    bool initialized = zmk_display_is_initialized();
    overlay_active = show;
    expires_at = show && initialized ? k_uptime_get() + TOAST_DURATION_MS : 0;
    if (!show) {
        requested_chord[0] = '\0';
        chord_held = false;
        chord_expires_at = 0;
    }
    if (initialized) {
        k_work_reschedule_for_queue(zmk_display_work_q(), &toast_work, K_MSEC(0));
    }
    k_mutex_unlock(&toast_lock);
}

void prospector_modifier_toast_request(const char *chord, bool caps_word, bool caps_lock,
                                       bool active) {
    modifier_state_request(chord, caps_word, caps_lock, active, 0);
}

static void modifier_state_request(const char *chord, bool caps_word, bool caps_lock, bool active,
                                   uint8_t mods) {
    k_mutex_lock(&toast_lock, K_FOREVER);
    bool initialized = zmk_display_is_initialized();
    bool held = active && chord != NULL && chord[0] != '\0';
    bool same_chord = held ? chord_held && strcmp(requested_chord, chord) == 0 : !chord_held;
#if defined(CONFIG_PROSPECTOR_OPERATOR_SESSION_STATS)
    uint8_t next_mods = active ? mods : 0;
    same_chord = same_chord && requested_mods == next_mods;
#else
    ARG_UNUSED(mods);
#endif
    /* Normal typing emits key events too. Don't allocate labels/redraw or disturb
     * an existing expiry unless the visible state (or startup readiness) changed. */
    if (requested_word == caps_word && requested_lock == caps_lock && overlay_active == active &&
        same_chord && modifier_cache_initialized == initialized) {
        k_mutex_unlock(&toast_lock);
        return;
    }
    modifier_cache_initialized = initialized;
#if defined(CONFIG_PROSPECTOR_OPERATOR_SESSION_STATS)
    requested_mods = next_mods;
    if (!active)
        displayed_mods = 0;
#endif
    requested_word = caps_word;
    requested_lock = caps_lock;
    overlay_active = active;
    if (!active) {
        expires_at = 0;
        chord_expires_at = 0;
        requested_chord[0] = '\0';
        chord_held = false;
    } else if (chord != NULL && chord[0] != '\0') {
        snprintf(requested_chord, sizeof(requested_chord), "%s", chord);
        chord_held = true;
        chord_expires_at = 0;
    } else if (chord_held) {
        chord_held = false;
        chord_expires_at = k_uptime_get() + CHORD_RELEASE_MS;
    }
    if (initialized)
        k_work_reschedule_for_queue(zmk_display_work_q(), &toast_work, K_MSEC(0));
    k_mutex_unlock(&toast_lock);
}

void prospector_modifier_toast_invalidate_chord(void) {
    k_mutex_lock(&toast_lock, K_FOREVER);
    requested_chord[0] = '\0';
    chord_held = false;
    chord_expires_at = 0;
#if defined(CONFIG_PROSPECTOR_OPERATOR_SESSION_STATS)
    requested_mods = 0;
    displayed_mods = 0;
#endif
    if (zmk_display_is_initialized())
        k_work_reschedule_for_queue(zmk_display_work_q(), &toast_work, K_MSEC(0));
    k_mutex_unlock(&toast_lock);
}

/* SPDX-License-Identifier: MIT */
#include <stdio.h>
#include <zephyr/kernel.h>
#include <zmk/activity.h>
#include <zmk/endpoints.h>
#include <zmk/event_manager.h>
#include <zmk/events/activity_state_changed.h>
#include <zmk/events/endpoint_changed.h>
#include <zmk/events/hid_indicators_changed.h>
#include <zmk/events/keycode_state_changed.h>
#include <zmk/hid.h>
#include <zmk/hid_indicators.h>
#ifdef CONFIG_DT_HAS_ZMK_BEHAVIOR_CAPS_WORD_ENABLED
#include <zmk/events/caps_word_state_changed.h>
#endif
#include "brightness_toast.h"
#include <dt-bindings/zmk/hid_indicators.h>
#include <modifier_order.h>
#include <prospector/modifier_toast.h>

static bool frontend_ready;
static bool caps_word_active;
static bool modifier_context_valid;
K_MUTEX_DEFINE(modifier_state_lock);

static void publish_current_state(void) {
    if (!frontend_ready)
        return;
    bool connected = zmk_endpoint_is_connected();
    /* Show explicitly held modifiers, as the original header did. Mod-morph
     * masks can clear AFTER its final key event with no change notification;
     * following report masks would silently lose a still-held Shift. Implicit
     * Caps Word Shift is not part of this explicit state. */
    zmk_mod_flags_t mods = connected && modifier_context_valid ? zmk_hid_get_explicit_mods() : 0;
#if !defined(CONFIG_PROSPECTOR_OPERATOR_SESSION_STATS)
    const zmk_mod_flags_t masks[MOD_TYPE_COUNT] = {MOD_LGUI | MOD_RGUI, MOD_LALT | MOD_RALT,
                                                   MOD_LCTL | MOD_RCTL, MOD_LSFT | MOD_RSFT};
    char chord[64] = "";
    size_t used = 0;
    for (int i = 0; i < MOD_TYPE_COUNT; i++) {
        enum modifier_type type = modifier_order_get(i);
        if (!(mods & masks[type]))
            continue;
        int n = snprintf(chord + used, sizeof(chord) - used, "%s%s", used ? " + " : "",
                         modifier_order_get_text(i));
        if (n < 0 || (size_t)n >= sizeof(chord) - used) {
            chord[used] = '\0';
            break;
        }
        used += n;
    }
#endif
    bool caps_lock =
        connected && (zmk_hid_indicators_get_current_profile() & HID_INDICATOR_CAPS_LOCK) != 0;
#if defined(CONFIG_PROSPECTOR_OPERATOR_SESSION_STATS)
    prospector_modifier_row_request(mods, caps_word_active, caps_lock,
                                    zmk_activity_get_state() == ZMK_ACTIVITY_ACTIVE);
#else
    prospector_modifier_toast_request(chord, caps_word_active, caps_lock,
                                      zmk_activity_get_state() == ZMK_ACTIVITY_ACTIVE);
#endif
}

static int modifier_toast_listener(const zmk_event_t *eh) {
    k_mutex_lock(&modifier_state_lock, K_FOREVER);
    if (as_zmk_endpoint_changed(eh) != NULL) {
        /* A disconnect/profile switch is not a physical release. Invalidate
         * immediately and wait for fresh input in the selected endpoint; LED
         * updates must not resurrect its old explicit bookkeeping. */
        modifier_context_valid = false;
        if (frontend_ready)
            prospector_modifier_toast_invalidate_chord();
    } else if (as_zmk_keycode_state_changed(eh) != NULL) {
        modifier_context_valid = zmk_endpoint_is_connected();
    }
#ifdef CONFIG_DT_HAS_ZMK_BEHAVIOR_CAPS_WORD_ENABLED
    const struct zmk_caps_word_state_changed *word = as_zmk_caps_word_state_changed(eh);
    if (word != NULL)
        caps_word_active = word->active;
#endif
    publish_current_state();
    k_mutex_unlock(&modifier_state_lock);
    return ZMK_EV_EVENT_BUBBLE;
}

void prospector_modifier_toast_init(void) {
    k_mutex_lock(&modifier_state_lock, K_FOREVER);
    frontend_ready = true;
    modifier_context_valid = zmk_endpoint_is_connected();
    publish_current_state();
    k_mutex_unlock(&modifier_state_lock);
}

ZMK_LISTENER(prospector_modifier_toast, modifier_toast_listener);
ZMK_SUBSCRIPTION(prospector_modifier_toast, zmk_keycode_state_changed);
ZMK_SUBSCRIPTION(prospector_modifier_toast, zmk_hid_indicators_changed);
ZMK_SUBSCRIPTION(prospector_modifier_toast, zmk_endpoint_changed);
ZMK_SUBSCRIPTION(prospector_modifier_toast, zmk_activity_state_changed);
#ifdef CONFIG_DT_HAS_ZMK_BEHAVIOR_CAPS_WORD_ENABLED
ZMK_SUBSCRIPTION(prospector_modifier_toast, zmk_caps_word_state_changed);
#endif

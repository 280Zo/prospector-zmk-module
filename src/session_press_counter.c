/* SPDX-License-Identifier: MIT */
/* Compiled into ZMK's app library (see module CMakeLists.txt) so this subscription
 * precedes combo.c, which consumes the presses that complete a combo. */
#include <prospector/session_stats.h>
#include <zmk/event_manager.h>
#include <zmk/events/position_state_changed.h>

static int session_press_listener(const zmk_event_t *eh) {
    const struct zmk_position_state_changed *ev = as_zmk_position_state_changed(eh);
    if (ev != NULL && ev->state)
        prospector_session_stats_note_press(ev->source, ev->position, ev->timestamp);
    return ZMK_EV_EVENT_BUBBLE;
}

ZMK_LISTENER(prospector_session_press, session_press_listener);
ZMK_SUBSCRIPTION(prospector_session_press, zmk_position_state_changed);

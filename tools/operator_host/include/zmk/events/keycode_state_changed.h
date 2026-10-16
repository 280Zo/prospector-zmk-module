#pragma once
#include <stdbool.h>
#include <stdint.h>
#include <zmk/event_manager.h>

/* Mirrors ZMK app/include/zmk/events/keycode_state_changed.h. */
struct zmk_keycode_state_changed {
    uint16_t usage_page;
    uint32_t keycode;
    uint8_t implicit_modifiers;
    uint8_t explicit_modifiers;
    bool state;
    int64_t timestamp;
};

static inline const struct zmk_keycode_state_changed *
as_zmk_keycode_state_changed(const zmk_event_t *eh) {
    return (const struct zmk_keycode_state_changed *)eh;
}

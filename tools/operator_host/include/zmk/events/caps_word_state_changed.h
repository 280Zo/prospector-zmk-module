#pragma once
#include <stdbool.h>
#include <zmk/event_manager.h>
struct zmk_caps_word_state_changed { bool active; };
static inline const struct zmk_caps_word_state_changed *as_zmk_caps_word_state_changed(const zmk_event_t *eh) { return (const struct zmk_caps_word_state_changed *)eh; }

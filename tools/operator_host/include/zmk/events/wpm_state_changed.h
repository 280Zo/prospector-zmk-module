#pragma once
#include <zmk/event_manager.h>
struct zmk_wpm_state_changed { int state; };
static inline const struct zmk_wpm_state_changed *as_zmk_wpm_state_changed(const zmk_event_t *eh) { return (const struct zmk_wpm_state_changed *)eh; }

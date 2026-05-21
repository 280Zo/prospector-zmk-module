#pragma once
#include <stdint.h>
#include <zmk/event_manager.h>
struct zmk_ble_active_profile_changed { uint8_t index; };
static inline const struct zmk_ble_active_profile_changed *as_zmk_ble_active_profile_changed(const zmk_event_t *eh) { return (const struct zmk_ble_active_profile_changed *)eh; }

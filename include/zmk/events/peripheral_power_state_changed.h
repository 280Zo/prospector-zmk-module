/* SPDX-License-Identifier: MIT */
#pragma once
#include <prospector/power_status.h>
#include <zmk/event_manager.h>
struct zmk_peripheral_power_state_changed {
    uint8_t source;
    enum prospector_power_state state;
};
ZMK_EVENT_DECLARE(zmk_peripheral_power_state_changed);

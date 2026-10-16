#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <zmk/event_manager.h>

struct zmk_position_state_changed {
    uint8_t source;
    uint32_t position;
    bool state;
    int64_t timestamp;
};

static inline const struct zmk_position_state_changed *
as_zmk_position_state_changed(const zmk_event_t *eh) {
    return (const struct zmk_position_state_changed *)eh;
}

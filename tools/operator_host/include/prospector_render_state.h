#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <zmk/endpoints.h>
#include <zmk/hid.h>

void host_state_set_wpm(int wpm);
void host_state_set_mods(zmk_mod_flags_t mods);
void host_state_set_caps_word(bool active);
void host_state_set_layer(int index, const char *name);
void host_state_set_endpoint(enum zmk_transport transport);
void host_state_set_profile(uint8_t profile);
void host_modifier_order_set(const char *order);

struct battery_update_state {
    uint8_t source;
    uint8_t level;
};

struct connection_update_state {
    uint8_t source;
    bool connected;
};

void battery_circles_battery_update_cb(struct battery_update_state state);
void battery_circles_connection_update_cb(struct connection_update_state state);

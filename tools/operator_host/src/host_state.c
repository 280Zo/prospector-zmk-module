#include <prospector_render_state.h>

#include <string.h>
#include <zmk/ble.h>
#include <zmk/keymap.h>
#include <zmk/wpm.h>

static int current_wpm;
static zmk_mod_flags_t current_mods;
static int current_layer;
static char current_layer_name[64] = "Base";
static enum zmk_transport current_transport = ZMK_TRANSPORT_USB;
static uint8_t current_profile;

void host_state_set_wpm(int wpm) {
    current_wpm = wpm;
}

void host_state_set_mods(zmk_mod_flags_t mods) {
    current_mods = mods;
}

void host_state_set_caps_word(bool active) {
    (void)active;
}

void host_state_set_layer(int index, const char *name) {
    current_layer = index;
    if (name && *name) {
        strncpy(current_layer_name, name, sizeof(current_layer_name) - 1);
        current_layer_name[sizeof(current_layer_name) - 1] = '\0';
    }
}

void host_state_set_endpoint(enum zmk_transport transport) {
    current_transport = transport;
}

void host_state_set_profile(uint8_t profile) {
    current_profile = profile;
}

int zmk_wpm_get_state(void) {
    return current_wpm;
}

zmk_mod_flags_t zmk_hid_get_explicit_mods(void) {
    return current_mods;
}

int zmk_keymap_highest_layer_active(void) {
    return current_layer;
}

int zmk_keymap_layer_index_to_id(int index) {
    return index;
}

const char *zmk_keymap_layer_name(int id) {
    (void)id;
    return current_layer_name;
}

uint8_t zmk_ble_active_profile_index(void) {
    return current_profile;
}

bool zmk_ble_profile_is_connected(uint8_t index) {
    (void)index;
    return true;
}

bool zmk_ble_profile_is_open(uint8_t index) {
    (void)index;
    return true;
}

struct zmk_endpoint_instance zmk_endpoint_get_selected(void) {
    return (struct zmk_endpoint_instance){.transport = current_transport};
}

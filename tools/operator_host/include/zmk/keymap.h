#pragma once

#ifndef ZMK_KEYMAP_LAYERS_LEN
#define ZMK_KEYMAP_LAYERS_LEN 5
#endif

int zmk_keymap_highest_layer_active(void);
int zmk_keymap_layer_index_to_id(int index);
const char *zmk_keymap_layer_name(int id);

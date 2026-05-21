#pragma once

#include <stdint.h>

typedef uint8_t zmk_mod_flags_t;

#define MOD_LGUI 0x01
#define MOD_RGUI 0x02
#define MOD_LALT 0x04
#define MOD_RALT 0x08
#define MOD_LCTL 0x10
#define MOD_RCTL 0x20
#define MOD_LSFT 0x40
#define MOD_RSFT 0x80

zmk_mod_flags_t zmk_hid_get_explicit_mods(void);

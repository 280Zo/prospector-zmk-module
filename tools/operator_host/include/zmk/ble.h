#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifndef ZMK_BLE_PROFILE_COUNT
#define ZMK_BLE_PROFILE_COUNT 5
#endif

uint8_t zmk_ble_active_profile_index(void);
bool zmk_ble_profile_is_connected(uint8_t index);
bool zmk_ble_profile_is_open(uint8_t index);

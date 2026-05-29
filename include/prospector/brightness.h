/* SPDX-License-Identifier: MIT */

#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <zephyr/types.h>

int prospector_brightness_auto_on(void);
int prospector_brightness_manual_on(void);
int prospector_brightness_toggle_auto(void);
int prospector_brightness_manual_step(int direction);
int prospector_brightness_set_manual(uint8_t brightness);
bool prospector_brightness_is_auto(void);
uint8_t prospector_brightness_get_displayed(void);
uint8_t prospector_brightness_get_target(void);
bool prospector_brightness_get_ambient_light(int32_t *value);

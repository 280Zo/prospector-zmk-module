#pragma once

#include <stdbool.h>
#include <stdint.h>

uint8_t prospector_brightness_get_displayed(void);
bool prospector_brightness_is_auto(void);
bool prospector_brightness_get_ambient_light(int32_t *lux);

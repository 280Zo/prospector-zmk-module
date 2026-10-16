/* SPDX-License-Identifier: MIT */
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Protocol v1: exactly one byte; a new incompatible format requires a new UUID. */
enum prospector_power_state {
    PROSPECTOR_POWER_UNKNOWN = 0,
    PROSPECTOR_POWER_BATTERY = 1,
    PROSPECTOR_POWER_USB = 2,
};
#define PROSPECTOR_POWER_SERVICE_UUID                                                              \
    BT_UUID_128_ENCODE(0xdc67801d, 0xc543, 0x4e4b, 0x9935, 0x0295d70692f8)
#define PROSPECTOR_POWER_STATE_UUID                                                                \
    BT_UUID_128_ENCODE(0xdc67801e, 0xc543, 0x4e4b, 0x9935, 0x0295d70692f8)

enum prospector_power_state prospector_power_get(uint8_t source);

/* Task 8.9: each half reports which side it is (one byte), so the dongle can
 * place halves by side instead of by pairing order. */
enum prospector_split_side {
    PROSPECTOR_SIDE_UNKNOWN = 0,
    PROSPECTOR_SIDE_LEFT = 1,
    PROSPECTOR_SIDE_RIGHT = 2,
};
#define PROSPECTOR_SIDE_UUID BT_UUID_128_ENCODE(0xdc67801f, 0xc543, 0x4e4b, 0x9935, 0x0295d70692f8)

/* Display position (0 = left, 1 = right) for a peripheral slot, and the inverse.
 * Uses reported sides when they are valid and distinct; otherwise slot order. */
uint8_t prospector_position_for_source(uint8_t source);
uint8_t prospector_source_for_position(uint8_t position);
/* True when positions come from reported sides rather than slot order. */
bool prospector_sides_known(void);

enum prospector_power_stage {
    PROSPECTOR_POWER_OFF,
    PROSPECTOR_POWER_SEC,
    PROSPECTOR_POWER_SVC,
    PROSPECTOR_POWER_CHR,
    PROSPECTOR_POWER_CCC,
    PROSPECTOR_POWER_SUB,
    PROSPECTOR_POWER_READ,
    PROSPECTOR_POWER_LIVE,
};
struct prospector_power_diagnostic {
    enum prospector_power_stage stage;
    enum prospector_power_state state;
    int error;
    uint8_t retries;
    uint8_t security;
};
struct prospector_power_diagnostic prospector_power_diagnostic_get(uint8_t source);
void prospector_power_diagnostic_format(const struct prospector_power_diagnostic *state, char *text,
                                        size_t size);

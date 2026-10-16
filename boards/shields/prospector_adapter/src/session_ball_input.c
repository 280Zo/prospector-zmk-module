/* SPDX-License-Identifier: MIT */
/* On the central, each zmk,input-split device re-reports a peripheral's pointer events.
 * Zephyr runs input callbacks in linker name order on one shared event, and ZMK's
 * input_handler_* rewrite it in place (scalers, scroll mappers). The leading underscore
 * in the callback name sorts BALL first, so it sees the sensor's raw motion; the Task 8.15
 * build helper verifies that order. Mouse keys use their own devices and never reach this. */
#include <prospector/session_stats.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/input/input.h>

#define DT_DRV_COMPAT zmk_input_split

#define PROSPECTOR_BALL_INPUT(n)                                                                   \
    static void prospector_ball_input_##n(struct input_event *evt, void *user_data) {              \
        ARG_UNUSED(user_data);                                                                     \
        prospector_session_stats_note_motion(n, evt->type, evt->code, evt->value, evt->sync);      \
    }                                                                                              \
    INPUT_CALLBACK_DEFINE_NAMED(DEVICE_DT_INST_GET(n), prospector_ball_input_##n, NULL,            \
                                _prospector_ball_input_##n);

DT_INST_FOREACH_STATUS_OKAY(PROSPECTOR_BALL_INPUT)

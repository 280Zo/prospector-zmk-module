/* SPDX-License-Identifier: MIT */
#define DT_DRV_COMPAT zmk_behavior_prospector_top_reset

#include <drivers/behavior.h>
#include <zephyr/device.h>
#include <prospector/session_stats.h>
#if IS_ENABLED(CONFIG_ZMK_BEHAVIOR_METADATA)
#include <zmk/behavior.h>
#endif

#if DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT)

static int behavior_prospector_top_reset_init(const struct device *dev) { return 0; }

static int on_keymap_binding_pressed(struct zmk_behavior_binding *binding,
                                     struct zmk_behavior_binding_event event) {
#if IS_ENABLED(CONFIG_PROSPECTOR_OPERATOR_SESSION_STATS)
    prospector_session_stats_reset_top_wpm();
#endif
    return ZMK_BEHAVIOR_OPAQUE;
}

static int on_keymap_binding_released(struct zmk_behavior_binding *binding,
                                      struct zmk_behavior_binding_event event) {
    return ZMK_BEHAVIOR_OPAQUE;
}

static const struct behavior_driver_api behavior_prospector_top_reset_driver_api = {
    .binding_pressed = on_keymap_binding_pressed,
    .binding_released = on_keymap_binding_released,
    .locality = BEHAVIOR_LOCALITY_CENTRAL,
#if IS_ENABLED(CONFIG_ZMK_BEHAVIOR_METADATA)
    .get_parameter_metadata = zmk_behavior_get_empty_param_metadata,
#endif
};

BEHAVIOR_DT_INST_DEFINE(0, behavior_prospector_top_reset_init, NULL, NULL, NULL, POST_KERNEL,
                        CONFIG_KERNEL_INIT_PRIORITY_DEFAULT,
                        &behavior_prospector_top_reset_driver_api);

#endif

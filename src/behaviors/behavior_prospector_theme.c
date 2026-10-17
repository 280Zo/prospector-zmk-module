/*
 * SPDX-License-Identifier: MIT
 */

#define DT_DRV_COMPAT zmk_behavior_prospector_theme

#include <drivers/behavior.h>
#include <zephyr/device.h>
#include <zephyr/logging/log.h>

#include <dt-bindings/prospector/theme.h>
#include <prospector/operator_theme.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#if DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT)

#if IS_ENABLED(CONFIG_ZMK_BEHAVIOR_METADATA)

static const struct behavior_parameter_value_metadata theme_values[] = {
    {
        .display_name = "Next Theme",
        .type = BEHAVIOR_PARAMETER_VALUE_TYPE_VALUE,
        .value = PTH_NEXT_CMD,
    },
    {
        .display_name = "Previous Theme",
        .type = BEHAVIOR_PARAMETER_VALUE_TYPE_VALUE,
        .value = PTH_PREV_CMD,
    },
};

static const struct behavior_parameter_metadata_set theme_set = {
    .param1_values = theme_values,
    .param1_values_len = ARRAY_SIZE(theme_values),
};

static const struct behavior_parameter_metadata metadata = {
    .sets_len = 1,
    .sets = &theme_set,
};

#endif

static int behavior_prospector_theme_init(const struct device *dev) { return 0; }

static int on_keymap_binding_pressed(struct zmk_behavior_binding *binding,
                                     struct zmk_behavior_binding_event event) {
#if IS_ENABLED(CONFIG_PROSPECTOR_STATUS_SCREEN_OPERATOR)
    switch (binding->param1) {
    case PTH_NEXT_CMD:
        operator_theme_next();
        return ZMK_BEHAVIOR_OPAQUE;
    case PTH_PREV_CMD:
        operator_theme_prev();
        return ZMK_BEHAVIOR_OPAQUE;
    default:
        LOG_ERR("Unknown Prospector theme command: %d", binding->param1);
        return -ENOTSUP;
    }
#else
    return -ENOTSUP; /* themes exist only in the Operator layout */
#endif
}

static int on_keymap_binding_released(struct zmk_behavior_binding *binding,
                                      struct zmk_behavior_binding_event event) {
    return ZMK_BEHAVIOR_OPAQUE;
}

static const struct behavior_driver_api behavior_prospector_theme_driver_api = {
    .binding_pressed = on_keymap_binding_pressed,
    .binding_released = on_keymap_binding_released,
    .locality = BEHAVIOR_LOCALITY_GLOBAL,
#if IS_ENABLED(CONFIG_ZMK_BEHAVIOR_METADATA)
    .parameter_metadata = &metadata,
#endif
};

BEHAVIOR_DT_INST_DEFINE(0, behavior_prospector_theme_init, NULL, NULL, NULL, POST_KERNEL,
                        CONFIG_KERNEL_INIT_PRIORITY_DEFAULT, &behavior_prospector_theme_driver_api);

#endif

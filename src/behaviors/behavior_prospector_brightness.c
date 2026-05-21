/*
 * SPDX-License-Identifier: MIT
 */

#define DT_DRV_COMPAT zmk_behavior_prospector_brightness

#include <drivers/behavior.h>
#include <zephyr/device.h>
#include <zephyr/logging/log.h>

#include <dt-bindings/prospector/brightness.h>
#include <prospector/brightness.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#if DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT)

#if IS_ENABLED(CONFIG_ZMK_BEHAVIOR_METADATA)

static const struct behavior_parameter_value_metadata no_arg_values[] = {
    {
        .display_name = "Auto Brightness",
        .type = BEHAVIOR_PARAMETER_VALUE_TYPE_VALUE,
        .value = PBL_AUTO_CMD,
    },
    {
        .display_name = "Manual Brightness",
        .type = BEHAVIOR_PARAMETER_VALUE_TYPE_VALUE,
        .value = PBL_MANUAL_CMD,
    },
    {
        .display_name = "Toggle Auto Brightness",
        .type = BEHAVIOR_PARAMETER_VALUE_TYPE_VALUE,
        .value = PBL_TOG_CMD,
    },
    {
        .display_name = "Increase Brightness",
        .type = BEHAVIOR_PARAMETER_VALUE_TYPE_VALUE,
        .value = PBL_INC_CMD,
    },
    {
        .display_name = "Decrease Brightness",
        .type = BEHAVIOR_PARAMETER_VALUE_TYPE_VALUE,
        .value = PBL_DEC_CMD,
    },
};

static const struct behavior_parameter_value_metadata one_arg_p1_values[] = {
    {
        .display_name = "Set Brightness",
        .type = BEHAVIOR_PARAMETER_VALUE_TYPE_VALUE,
        .value = PBL_SET_CMD,
    },
};

static const struct behavior_parameter_value_metadata one_arg_p2_values[] = {
    {
        .display_name = "Brightness",
        .type = BEHAVIOR_PARAMETER_VALUE_TYPE_RANGE,
        .range =
            {
                .min = 1,
                .max = 100,
            },
    },
};

static const struct behavior_parameter_metadata_set no_args_set = {
    .param1_values = no_arg_values,
    .param1_values_len = ARRAY_SIZE(no_arg_values),
};

static const struct behavior_parameter_metadata_set one_arg_set = {
    .param1_values = one_arg_p1_values,
    .param1_values_len = ARRAY_SIZE(one_arg_p1_values),
    .param2_values = one_arg_p2_values,
    .param2_values_len = ARRAY_SIZE(one_arg_p2_values),
};

static const struct behavior_parameter_metadata_set sets[] = {no_args_set, one_arg_set};

static const struct behavior_parameter_metadata metadata = {
    .sets_len = ARRAY_SIZE(sets),
    .sets = sets,
};

#endif

static int behavior_prospector_brightness_init(const struct device *dev) { return 0; }

static int on_keymap_binding_pressed(struct zmk_behavior_binding *binding,
                                     struct zmk_behavior_binding_event event) {
    switch (binding->param1) {
    case PBL_AUTO_CMD:
        return prospector_brightness_auto_on();
    case PBL_MANUAL_CMD:
        return prospector_brightness_manual_on();
    case PBL_TOG_CMD:
        return prospector_brightness_toggle_auto();
    case PBL_INC_CMD:
        return prospector_brightness_manual_step(1);
    case PBL_DEC_CMD:
        return prospector_brightness_manual_step(-1);
    case PBL_SET_CMD:
        return prospector_brightness_set_manual(binding->param2);
    default:
        LOG_ERR("Unknown Prospector brightness command: %d", binding->param1);
        return -ENOTSUP;
    }
}

static int on_keymap_binding_released(struct zmk_behavior_binding *binding,
                                      struct zmk_behavior_binding_event event) {
    return ZMK_BEHAVIOR_OPAQUE;
}

static const struct behavior_driver_api behavior_prospector_brightness_driver_api = {
    .binding_pressed = on_keymap_binding_pressed,
    .binding_released = on_keymap_binding_released,
    .locality = BEHAVIOR_LOCALITY_GLOBAL,
#if IS_ENABLED(CONFIG_ZMK_BEHAVIOR_METADATA)
    .parameter_metadata = &metadata,
#endif
};

BEHAVIOR_DT_INST_DEFINE(0, behavior_prospector_brightness_init, NULL, NULL, NULL, POST_KERNEL,
                        CONFIG_KERNEL_INIT_PRIORITY_DEFAULT,
                        &behavior_prospector_brightness_driver_api);

#endif

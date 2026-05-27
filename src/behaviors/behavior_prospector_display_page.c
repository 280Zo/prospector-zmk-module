/*
 * SPDX-License-Identifier: MIT
 */

#define DT_DRV_COMPAT zmk_behavior_prospector_display_page

#include <drivers/behavior.h>
#include <zephyr/device.h>
#include <zephyr/logging/log.h>

#include <dt-bindings/prospector/display_page.h>
#include <prospector/display_pages.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#if DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT)

#if IS_ENABLED(CONFIG_ZMK_BEHAVIOR_METADATA)

static const struct behavior_parameter_value_metadata page_values[] = {
    {
        .display_name = "Status Page",
        .type = BEHAVIOR_PARAMETER_VALUE_TYPE_VALUE,
        .value = PDP_STATUS_CMD,
    },
    {
        .display_name = "Diagnostics Page",
        .type = BEHAVIOR_PARAMETER_VALUE_TYPE_VALUE,
        .value = PDP_DIAGNOSTICS_CMD,
    },
    {
        .display_name = "Next Page",
        .type = BEHAVIOR_PARAMETER_VALUE_TYPE_VALUE,
        .value = PDP_NEXT_CMD,
    },
    {
        .display_name = "Previous Page",
        .type = BEHAVIOR_PARAMETER_VALUE_TYPE_VALUE,
        .value = PDP_PREV_CMD,
    },
};

static const struct behavior_parameter_metadata_set page_set = {
    .param1_values = page_values,
    .param1_values_len = ARRAY_SIZE(page_values),
};

static const struct behavior_parameter_metadata metadata = {
    .sets_len = 1,
    .sets = &page_set,
};

#endif

static int behavior_prospector_display_page_init(const struct device *dev) { return 0; }

static int on_keymap_binding_pressed(struct zmk_behavior_binding *binding,
                                     struct zmk_behavior_binding_event event) {
    switch (binding->param1) {
    case PDP_STATUS_CMD:
    case PDP_DIAGNOSTICS_CMD:
        return prospector_display_page_set(binding->param1);
    case PDP_NEXT_CMD:
        return prospector_display_page_next();
    case PDP_PREV_CMD:
        return prospector_display_page_prev();
    default:
        LOG_ERR("Unknown Prospector display page command: %d", binding->param1);
        return -ENOTSUP;
    }
}

static int on_keymap_binding_released(struct zmk_behavior_binding *binding,
                                      struct zmk_behavior_binding_event event) {
    return ZMK_BEHAVIOR_OPAQUE;
}

static const struct behavior_driver_api behavior_prospector_display_page_driver_api = {
    .binding_pressed = on_keymap_binding_pressed,
    .binding_released = on_keymap_binding_released,
    .locality = BEHAVIOR_LOCALITY_GLOBAL,
#if IS_ENABLED(CONFIG_ZMK_BEHAVIOR_METADATA)
    .parameter_metadata = &metadata,
#endif
};

BEHAVIOR_DT_INST_DEFINE(0, behavior_prospector_display_page_init, NULL, NULL, NULL, POST_KERNEL,
                        CONFIG_KERNEL_INIT_PRIORITY_DEFAULT,
                        &behavior_prospector_display_page_driver_api);

#endif

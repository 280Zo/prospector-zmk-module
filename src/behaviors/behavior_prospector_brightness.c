/*
 * SPDX-License-Identifier: MIT
 */

#define DT_DRV_COMPAT zmk_behavior_prospector_brightness

#include <drivers/behavior.h>
#include <zephyr/device.h>
#include <zephyr/kernel.h>
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

/* A separate queue keeps the existing synchronous PWM fades off the system
 * workqueue (which also services Bluetooth). Only one step is ever pending. */
static struct k_work_q brightness_repeat_queue;
K_THREAD_STACK_DEFINE(brightness_repeat_stack, 1024);
K_MUTEX_DEFINE(brightness_repeat_lock);

struct held_brightness_key {
    bool pressed;
    uint32_t position;
    uint8_t source;
};

static struct held_brightness_key held_keys[2]; /* increase, decrease */
static int64_t next_repeat_at;

static uint8_t binding_source(struct zmk_behavior_binding_event event) {
#if IS_ENABLED(CONFIG_ZMK_SPLIT)
    return event.source;
#else
    return 0;
#endif
}

static int held_direction(void) {
    return held_keys[0].pressed == held_keys[1].pressed ? 0 : held_keys[0].pressed ? 1 : -1;
}

static void brightness_repeat_handler(struct k_work *work);
K_WORK_DELAYABLE_DEFINE(brightness_repeat_work, brightness_repeat_handler);

static int schedule_repeat(int64_t started, int delay_ms) {
    /* If a fade or scheduling took longer, skip missed intervals rather than
     * replaying a burst of stale adjustments. */
    next_repeat_at = started + delay_ms;
    int64_t remaining = next_repeat_at - k_uptime_get();
    int rc = k_work_reschedule_for_queue(&brightness_repeat_queue, &brightness_repeat_work,
                                         K_MSEC(MAX(remaining, 0)));
    if (rc < 0) {
        LOG_ERR("Failed to schedule brightness repeat: %d", rc);
    }
    return rc < 0 ? rc : 0;
}

static void brightness_repeat_handler(struct k_work *work) {
    k_mutex_lock(&brightness_repeat_lock, K_FOREVER);
    int direction = held_direction();
    /* Cancellation cannot revoke a dispatched handler waiting on this lock.
     * Such old work must honor the current press/resume deadline. */
    if (direction && k_uptime_get() < next_repeat_at) {
        schedule_repeat(next_repeat_at, 0);
        k_mutex_unlock(&brightness_repeat_lock);
        return;
    }
    uint8_t target = prospector_brightness_get_target();
    if (direction && ((direction > 0 && target < 100) || (direction < 0 && target > 1))) {
        int64_t started = k_uptime_get();
        int rc = prospector_brightness_manual_step(direction);
        if (rc == 0) {
            schedule_repeat(started, 100);
        } else {
            LOG_ERR("Failed to repeat brightness step: %d", rc);
        }
    }
    k_mutex_unlock(&brightness_repeat_lock);
}

static int brightness_key_pressed(struct zmk_behavior_binding_event event, int index) {
    k_mutex_lock(&brightness_repeat_lock, K_FOREVER);
    /* One physical binding per direction. Ignore duplicate presses instead of
     * overwriting its owner and allowing an unrelated release to cancel it. */
    if (held_keys[index].pressed) {
        k_mutex_unlock(&brightness_repeat_lock);
        return ZMK_BEHAVIOR_OPAQUE;
    }
    held_keys[index] = (struct held_brightness_key){
        .pressed = true, .position = event.position, .source = binding_source(event)};
    k_work_cancel_delayable(&brightness_repeat_work);
    int direction = held_direction();
    int rc = 0;
    if (direction) {
        int64_t started = k_uptime_get();
        rc = prospector_brightness_manual_step(direction);
        if (!rc) {
            rc = schedule_repeat(started, 300);
        }
    }
    k_mutex_unlock(&brightness_repeat_lock);
    return rc;
}

static int behavior_prospector_brightness_init(const struct device *dev) {
    k_work_queue_start(&brightness_repeat_queue, brightness_repeat_stack,
                       K_THREAD_STACK_SIZEOF(brightness_repeat_stack), 10, NULL);
    return 0;
}

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
        return brightness_key_pressed(event, 0);
    case PBL_DEC_CMD:
        return brightness_key_pressed(event, 1);
    case PBL_SET_CMD:
        return prospector_brightness_set_manual(binding->param2);
    default:
        LOG_ERR("Unknown Prospector brightness command: %d", binding->param1);
        return -ENOTSUP;
    }
}

static int on_keymap_binding_released(struct zmk_behavior_binding *binding,
                                      struct zmk_behavior_binding_event event) {
    if (binding->param1 == PBL_INC_CMD || binding->param1 == PBL_DEC_CMD) {
        int index = binding->param1 == PBL_INC_CMD ? 0 : 1;
        k_mutex_lock(&brightness_repeat_lock, K_FOREVER);
        if (held_keys[index].pressed && held_keys[index].position == event.position &&
            held_keys[index].source == binding_source(event)) {
            held_keys[index].pressed = false;
            k_work_cancel_delayable(&brightness_repeat_work);
            if (held_direction()) {
                schedule_repeat(k_uptime_get(), 100);
            }
        }
        k_mutex_unlock(&brightness_repeat_lock);
    }
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

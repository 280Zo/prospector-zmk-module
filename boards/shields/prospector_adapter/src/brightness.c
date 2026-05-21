#include <zephyr/device.h>
#include <zephyr/drivers/led.h>
#include <zephyr/drivers/pwm.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>

#include <prospector/brightness.h>
#include <zephyr/logging/log.h>
#include <zmk/event_manager.h>
#include <zmk/events/activity_state_changed.h>
LOG_MODULE_REGISTER(als, 4);

static const struct device *pwm_leds_dev = DEVICE_DT_GET_ONE(pwm_leds);
#define DISP_BL DT_NODE_CHILD_IDX(DT_NODELABEL(disp_bl))

static int set_backlight_brightness(uint8_t brightness) {
    int rc = led_set_brightness(pwm_leds_dev, DISP_BL, brightness);
    if (rc != 0) {
        LOG_ERR("Failed to set brightness: %d", rc);
    }

    return rc;
}

#ifdef CONFIG_PROSPECTOR_USE_AMBIENT_LIGHT_SENSOR
#define PROSPECTOR_DEFAULT_BRIGHTNESS 100
#define PROSPECTOR_DEFAULT_AUTO_MODE true
#else
#define PROSPECTOR_DEFAULT_BRIGHTNESS CONFIG_PROSPECTOR_FIXED_BRIGHTNESS
#define PROSPECTOR_DEFAULT_AUTO_MODE false
#endif

static uint8_t target_brightness = PROSPECTOR_DEFAULT_BRIGHTNESS;
static uint8_t displayed_brightness = PROSPECTOR_DEFAULT_BRIGHTNESS;
static bool backlight_active = true;
static bool auto_brightness_enabled = PROSPECTOR_DEFAULT_AUTO_MODE;
K_MUTEX_DEFINE(brightness_lock);

#define PWM_MIN 1   // Minimum PWM duty cycle (%) - keep display visible
#define PWM_MAX 100 // Maximum PWM duty cycle (%)

#define FADE_STEP 1
#define FADE_SLEEP_BRIGHTEN_MS 3
#define FADE_SLEEP_DARKEN_MS 10

static uint8_t clamp_brightness(uint8_t brightness) { return CLAMP(brightness, PWM_MIN, PWM_MAX); }

static void set_displayed_brightness(uint8_t brightness) {
    k_mutex_lock(&brightness_lock, K_FOREVER);
    displayed_brightness = brightness;
    k_mutex_unlock(&brightness_lock);
}

static int bl_fade(uint8_t source, uint8_t target) {
    bool increasing = target > source;
    uint8_t brightness = source;

    if (source == target) {
        set_displayed_brightness(target);
        return 0;
    }

    while ((increasing && brightness < target) || (!increasing && brightness > target)) {
        int rc;

        if (increasing) {
            brightness = MIN(brightness + FADE_STEP, target);
        } else {
            brightness = MAX(brightness - FADE_STEP, target);
        }

        rc = set_backlight_brightness(brightness);
        if (rc != 0) {
            return rc;
        }

        set_displayed_brightness(brightness);
        k_msleep(increasing ? FADE_SLEEP_BRIGHTEN_MS : FADE_SLEEP_DARKEN_MS);
    }

    return 0;
}

static int set_target_brightness(uint8_t brightness, bool manual_mode) {
    uint8_t source;
    uint8_t target = clamp_brightness(brightness);
    bool active;

    k_mutex_lock(&brightness_lock, K_FOREVER);
    if (manual_mode) {
        auto_brightness_enabled = false;
    }
    target_brightness = target;
    source = displayed_brightness;
    active = backlight_active;
    k_mutex_unlock(&brightness_lock);

    if (!active) {
        return 0;
    }

    return bl_fade(source, target);
}

int prospector_brightness_auto_on(void) {
#ifdef CONFIG_PROSPECTOR_USE_AMBIENT_LIGHT_SENSOR
    k_mutex_lock(&brightness_lock, K_FOREVER);
    auto_brightness_enabled = true;
    k_mutex_unlock(&brightness_lock);
    return 0;
#else
    return -ENOTSUP;
#endif
}

int prospector_brightness_manual_on(void) {
    k_mutex_lock(&brightness_lock, K_FOREVER);
    auto_brightness_enabled = false;
    k_mutex_unlock(&brightness_lock);
    return 0;
}

int prospector_brightness_toggle_auto(void) {
#ifdef CONFIG_PROSPECTOR_USE_AMBIENT_LIGHT_SENSOR
    k_mutex_lock(&brightness_lock, K_FOREVER);
    auto_brightness_enabled = !auto_brightness_enabled;
    k_mutex_unlock(&brightness_lock);
    return 0;
#else
    return -ENOTSUP;
#endif
}

int prospector_brightness_set_manual(uint8_t brightness) {
    return set_target_brightness(brightness, true);
}

int prospector_brightness_manual_step(int direction) {
    uint8_t next;

    k_mutex_lock(&brightness_lock, K_FOREVER);
    if (direction > 0) {
        next = MIN(target_brightness + CONFIG_PROSPECTOR_BRIGHTNESS_STEP, PWM_MAX);
    } else {
        next = MAX(target_brightness - CONFIG_PROSPECTOR_BRIGHTNESS_STEP, PWM_MIN);
    }
    k_mutex_unlock(&brightness_lock);

    return set_target_brightness(next, true);
}

bool prospector_brightness_is_auto(void) {
    bool enabled;

    k_mutex_lock(&brightness_lock, K_FOREVER);
    enabled = auto_brightness_enabled;
    k_mutex_unlock(&brightness_lock);

    return enabled;
}

#ifdef CONFIG_PROSPECTOR_USE_AMBIENT_LIGHT_SENSOR

#define SENSOR_MIN 0   // Minimum sensor reading
#define SENSOR_MAX 100 // Maximum sensor reading

#define FADE_THRESHOLD 10

#define NORMAL_SAMPLE_SLEEP_MS 100

#define BURST_SAMPLE_SLEEP_MS 30
#define BURST_SAMPLE_TIMEOUT 10
#define BURST_SAMPLE_CONSECUTIVE 3

uint8_t map_light_to_pwm(int32_t sensor_reading) {
    // Handle invalid/error readings
    if (sensor_reading < SENSOR_MIN) {
        return PWM_MIN; // Default to minimum brightness on error
    }

    // Clamp to maximum
    if (sensor_reading > SENSOR_MAX) {
        sensor_reading = SENSOR_MAX;
    }

    // Linear mapping
    uint8_t pwm_value = (uint8_t)(PWM_MIN + ((PWM_MAX - PWM_MIN) * (sensor_reading - SENSOR_MIN)) /
                                                (SENSOR_MAX - SENSOR_MIN));

    return pwm_value;
}

extern void als_thread(void *d0, void *d1, void *d2) {
    ARG_UNUSED(d0);
    ARG_UNUSED(d1);
    ARG_UNUSED(d2);

    const struct device *dev;
    struct sensor_value intensity;
    uint8_t mapped_brightness;

    dev = DEVICE_DT_GET_ONE(avago_apds9960);
    if (!device_is_ready(dev)) {
        printk("sensor: device not ready.\n");
    }

    // led_set_brightness(pwm_leds_dev, DISP_BL, 100);

    while (1) {

        k_msleep(NORMAL_SAMPLE_SLEEP_MS);

        uint8_t compare_brightness;
        bool active;
        bool auto_enabled;

        k_mutex_lock(&brightness_lock, K_FOREVER);
        active = backlight_active;
        auto_enabled = auto_brightness_enabled;
        compare_brightness = target_brightness;
        k_mutex_unlock(&brightness_lock);

        if (!active || !auto_enabled) {
            continue;
        }

        int rc = sensor_sample_fetch(dev);
        if (rc != 0) {
            LOG_ERR("sensor_sample_fetch failed: %d", rc);
            continue;
        }

        rc = sensor_channel_get(dev, SENSOR_CHAN_LIGHT, &intensity);
        if (rc != 0) {
            LOG_ERR("Cannot read ALS data: %d", rc);
            continue;
        }

        // LOG_INF("ambient light intensity %d", intensity.val1);

        mapped_brightness = map_light_to_pwm(intensity.val1);
        // LOG_INF("NORMAL: mapped PWM duty cycle %d\n", mapped_brightness);

        if (abs(mapped_brightness - compare_brightness) > FADE_THRESHOLD) {
            uint8_t integrator = 0;

            for (int i = 0; i < BURST_SAMPLE_TIMEOUT; i++) {
                uint8_t burst_compare_brightness;

                k_msleep(BURST_SAMPLE_SLEEP_MS);

                rc = sensor_sample_fetch(dev);
                if (rc != 0) {
                    LOG_ERR("sensor_sample_fetch failed: %d", rc);
                    continue;
                }
                rc = sensor_channel_get(dev, SENSOR_CHAN_LIGHT, &intensity);
                if (rc != 0) {
                    LOG_ERR("Cannot read ALS data: %d", rc);
                    continue;
                }

                mapped_brightness = map_light_to_pwm(intensity.val1);
                // LOG_INF("BURST: mapped PWM duty cycle %d\n", mapped_brightness);

                k_mutex_lock(&brightness_lock, K_FOREVER);
                active = backlight_active;
                auto_enabled = auto_brightness_enabled;
                burst_compare_brightness = target_brightness;
                k_mutex_unlock(&brightness_lock);

                if (!active || !auto_enabled) {
                    break;
                }

                if (abs(mapped_brightness - burst_compare_brightness) > FADE_THRESHOLD) {
                    integrator++;
                    // printk("integrator at: %d", integrator);
                    if (integrator >= BURST_SAMPLE_CONSECUTIVE) {
                        rc = set_target_brightness(mapped_brightness, false);
                        if (rc != 0) {
                            LOG_ERR("Failed to fade brightness: %d", rc);
                        }
                        // LOG_INF("SETTING NEW BRIGHTNESS: %d", mapped_brightness);
                        break;
                    }
                }
            }
        }
        // led_set_brightness(pwm_leds_dev, DISP_BL,
        // map_light_to_pwm(intensity.val1));
    }
}

static int als_brightness_activity_listener(const zmk_event_t *eh) {
    struct zmk_activity_state_changed *ev = as_zmk_activity_state_changed(eh);

    if (ev == NULL) {
        return -ENOTSUP;
    }

    switch (ev->state) {
    case ZMK_ACTIVITY_ACTIVE: {
        k_mutex_lock(&brightness_lock, K_FOREVER);
        backlight_active = true;
        uint8_t restore_brightness = target_brightness;
        k_mutex_unlock(&brightness_lock);
        return bl_fade(0, restore_brightness);
    }
    case ZMK_ACTIVITY_IDLE:
    case ZMK_ACTIVITY_SLEEP: {
        k_mutex_lock(&brightness_lock, K_FOREVER);
        uint8_t source_brightness = displayed_brightness;
        backlight_active = false;
        k_mutex_unlock(&brightness_lock);

        return bl_fade(source_brightness, 0);
    }
    default:
        return -EINVAL;
    }
}

ZMK_LISTENER(prospector_als_brightness, als_brightness_activity_listener);
ZMK_SUBSCRIPTION(prospector_als_brightness, zmk_activity_state_changed);

K_THREAD_DEFINE(als_tid, 1024, als_thread, NULL, NULL, NULL, K_LOWEST_APPLICATION_THREAD_PRIO, 0,
                0);

#else

static int fixed_brightness_activity_listener(const zmk_event_t *eh) {
    struct zmk_activity_state_changed *ev = as_zmk_activity_state_changed(eh);

    if (ev == NULL) {
        return -ENOTSUP;
    }

    switch (ev->state) {
    case ZMK_ACTIVITY_ACTIVE: {
        k_mutex_lock(&brightness_lock, K_FOREVER);
        backlight_active = true;
        uint8_t restore_brightness = target_brightness;
        k_mutex_unlock(&brightness_lock);
        return bl_fade(0, restore_brightness);
    }
    case ZMK_ACTIVITY_IDLE:
    case ZMK_ACTIVITY_SLEEP: {
        k_mutex_lock(&brightness_lock, K_FOREVER);
        uint8_t source_brightness = displayed_brightness;
        backlight_active = false;
        k_mutex_unlock(&brightness_lock);
        return bl_fade(source_brightness, 0);
    }
    default:
        return -EINVAL;
    }
}

ZMK_LISTENER(prospector_fixed_brightness, fixed_brightness_activity_listener);
ZMK_SUBSCRIPTION(prospector_fixed_brightness, zmk_activity_state_changed);

static int init_fixed_brightness(void) {
    return set_backlight_brightness(CONFIG_PROSPECTOR_FIXED_BRIGHTNESS);
}

SYS_INIT(init_fixed_brightness, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);

#endif

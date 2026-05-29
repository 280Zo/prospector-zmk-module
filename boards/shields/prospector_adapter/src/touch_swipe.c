#include <stdlib.h>

#include <zephyr/device.h>
#include <zephyr/dt-bindings/input/input-event-codes.h>
#include <zephyr/input/input.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <prospector/display_pages.h>

LOG_MODULE_REGISTER(prospector_touch_swipe, LOG_LEVEL_INF);

#define CST816S_NODE DT_NODELABEL(cst816s)

#define SWIPE_MIN_DISTANCE_PX 35
#define SWIPE_MAX_DURATION_MS 800

enum swipe_action {
    SWIPE_ACTION_NONE,
    SWIPE_ACTION_NEXT,
    SWIPE_ACTION_PREV,
};

static int32_t latest_x;
static int32_t latest_y;
static int32_t start_x;
static int32_t start_y;
static int32_t last_x;
static int32_t last_y;
static int64_t start_time;
static bool touch_active;
static enum swipe_action pending_action;

static void swipe_work_handler(struct k_work *work) {
    ARG_UNUSED(work);

    enum swipe_action action = pending_action;

    pending_action = SWIPE_ACTION_NONE;

    if (action == SWIPE_ACTION_NEXT) {
        prospector_display_page_next();
    } else if (action == SWIPE_ACTION_PREV) {
        prospector_display_page_prev();
    }
}

K_WORK_DEFINE(swipe_work, swipe_work_handler);

static void queue_swipe_action(enum swipe_action action) {
    pending_action = action;
    k_work_submit(&swipe_work);
}

static void handle_touch_release(void) {
    if (!touch_active) {
        return;
    }

    int32_t dy = last_y - start_y;
    int64_t duration = k_uptime_get() - start_time;
    int32_t abs_dy = abs(dy);

    touch_active = false;

    if (duration > SWIPE_MAX_DURATION_MS) {
        return;
    }

    /*
     * The Prospector mounts the display sideways, so the panel's raw Y axis maps
     * to the display's horizontal page-swipe axis.
     */
    int32_t page_delta = dy;
    int32_t page_abs_delta = abs_dy;

    if (page_abs_delta < SWIPE_MIN_DISTANCE_PX) {
        return;
    }

    LOG_INF("Prospector touch swipe: %s", page_delta < 0 ? "next" : "prev");
    queue_swipe_action(page_delta < 0 ? SWIPE_ACTION_NEXT : SWIPE_ACTION_PREV);
}

static void touch_input_cb(struct input_event *evt, void *user_data) {
    ARG_UNUSED(user_data);

    bool is_touch = evt->dev == DEVICE_DT_GET(CST816S_NODE);

    if (!is_touch) {
        return;
    }

    if (evt->type == INPUT_EV_ABS && evt->code == INPUT_ABS_X) {
        latest_x = evt->value;
        if (touch_active) {
            last_x = latest_x;
        }
        return;
    }

    if (evt->type == INPUT_EV_ABS && evt->code == INPUT_ABS_Y) {
        latest_y = evt->value;
        if (touch_active) {
            last_y = latest_y;
        }
        return;
    }

    if (evt->type != INPUT_EV_KEY || evt->code != INPUT_BTN_TOUCH) {
        return;
    }

    if (evt->value) {
        if (!touch_active) {
            start_x = latest_x;
            start_y = latest_y;
            last_x = latest_x;
            last_y = latest_y;
            start_time = k_uptime_get();
            touch_active = true;
        }
    } else {
        handle_touch_release();
    }
}

INPUT_CALLBACK_DEFINE(NULL, touch_input_cb, NULL);

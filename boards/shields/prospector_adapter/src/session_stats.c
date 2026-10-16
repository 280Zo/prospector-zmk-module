/* SPDX-License-Identifier: MIT */
#include <dt-bindings/zmk/hid_usage.h>
#include <dt-bindings/zmk/hid_usage_pages.h>
#include <prospector/session_stats.h>
#include <zephyr/dt-bindings/input/input-event-codes.h>
#include <zephyr/kernel.h>
#include <zmk/event_manager.h>
#include <zmk/events/keycode_state_changed.h>
#if IS_ENABLED(CONFIG_ZMK_WPM)
#include <zmk/events/wpm_state_changed.h>
#endif

#define SESSION_COUNT_MAX 999999999U
/* Same pull/range as the Diagnostics key balance. */
#define SESSION_TUG_PULL 3
#define SESSION_TUG_MAX  18
#if defined(CONFIG_PROSPECTOR_BALL_CPI)
#define SESSION_BALL_CPI CONFIG_PROSPECTOR_BALL_CPI
#else
#define SESSION_BALL_CPI 800
#endif
static struct prospector_session_stats session_stats;
K_MUTEX_DEFINE(session_stats_lock);

void prospector_session_stats_get(struct prospector_session_stats *out) {
    k_mutex_lock(&session_stats_lock, K_FOREVER);
    *out = session_stats;
    k_mutex_unlock(&session_stats_lock);
}

void prospector_session_stats_format_count(uint32_t count, char *out, size_t size) {
    if (size == 0 || out == NULL)
        return;
    if (count < 10000U) {
        snprintk(out, size, "%u", (unsigned)count);
    } else if (count < 100000U) {
        snprintk(out, size, "%u.%uk", (unsigned)(count / 1000U), (unsigned)((count / 100U) % 10U));
    } else if (count < 1000000U) {
        snprintk(out, size, "%uk", (unsigned)(count / 1000U));
    } else if (count < 10000000U) {
        snprintk(out, size, "%u.%uM", (unsigned)(count / 1000000U),
                 (unsigned)((count / 100000U) % 10U));
    } else if (count < SESSION_COUNT_MAX) {
        snprintk(out, size, "%uM", (unsigned)(count / 1000000U));
    } else {
        snprintk(out, size, "999M+");
    }
}

void prospector_session_stats_format_distance(uint64_t ball_q8, char *out, size_t size) {
    if (size == 0 || out == NULL)
        return;
    /* 1 count = 0.0254 m / CPI, so tenths of a metre = counts * 254 / (CPI * 1000). */
    uint64_t counts = ball_q8 / 256U;
    uint64_t tenths = counts * 254U / ((uint64_t)SESSION_BALL_CPI * 1000U);
    uint64_t metres = tenths / 10U;
    if (tenths < 10000U) {
        snprintk(out, size, "%u.%um", (unsigned)(tenths / 10U), (unsigned)(tenths % 10U));
    } else if (metres < 100000U) {
        snprintk(out, size, "%u.%ukm", (unsigned)(metres / 1000U),
                 (unsigned)((metres / 100U) % 10U));
    } else {
        uint64_t km = metres / 1000U;
        snprintk(out, size, "%ukm", (unsigned)(km > 99999U ? 99999U : km));
    }
}

void prospector_session_stats_format_share(uint32_t part, uint32_t total, char *out, size_t size) {
    if (size == 0 || out == NULL)
        return;
    if (total == 0) {
        snprintk(out, size, "--");
        return;
    }
    uint64_t tenths = ((uint64_t)part * 1000U + total / 2U) / total;
    if (tenths >= 1000U)
        snprintk(out, size, "100%%");
    else
        snprintk(out, size, "%u.%u%%", (unsigned)(tenths / 10U), (unsigned)(tenths % 10U));
}

/* Integer square root, for travel in 1/256 counts without libm. */
static uint64_t isqrt64(uint64_t value) {
    uint64_t root = 0, bit = 1ULL << 62;
    while (bit > value)
        bit >>= 2;
    while (bit != 0) {
        if (value >= root + bit) {
            value -= root + bit;
            root = (root >> 1) + bit;
        } else {
            root >>= 1;
        }
        bit >>= 2;
    }
    return root;
}

static struct {
    int32_t dx, dy;
} pending_motion[2];

void prospector_session_stats_note_motion(uint8_t device, uint16_t type, uint16_t code,
                                          int32_t value, bool sync) {
    if (device >= 2 || type != INPUT_EV_REL)
        return;
    k_mutex_lock(&session_stats_lock, K_FOREVER);
    if (code == INPUT_REL_X)
        pending_motion[device].dx += value;
    else if (code == INPUT_REL_Y)
        pending_motion[device].dy += value;
    if (sync && (pending_motion[device].dx != 0 || pending_motion[device].dy != 0)) {
        int64_t dx = pending_motion[device].dx, dy = pending_motion[device].dy;
        uint64_t step = isqrt64((uint64_t)(dx * dx + dy * dy) << 16);
        if (session_stats.ball_q8 <= UINT64_MAX - step)
            session_stats.ball_q8 += step;
        pending_motion[device].dx = 0;
        pending_motion[device].dy = 0;
    }
    k_mutex_unlock(&session_stats_lock);
}

static int session_keycode_listener(const zmk_event_t *eh) {
    const struct zmk_keycode_state_changed *ev = as_zmk_keycode_state_changed(eh);
    if (ev != NULL && ev->state) {
        bool backspace = ev->usage_page == HID_USAGE_KEY &&
                         ev->keycode == HID_USAGE_KEY_KEYBOARD_DELETE_BACKSPACE;
        k_mutex_lock(&session_stats_lock, K_FOREVER);
        if (session_stats.key_count < SESSION_COUNT_MAX)
            session_stats.key_count++;
        if (backspace && session_stats.backspace_count < SESSION_COUNT_MAX)
            session_stats.backspace_count++;
        k_mutex_unlock(&session_stats_lock);
    }
    return ZMK_EV_EVENT_BUBBLE;
}

/* Combo re-raises captured presses with their original data, so one physical press
 * can pass the early press listener more than once. Recent presses identify repeats. */
#define RECENT_PRESSES 16
static struct {
    int64_t timestamp;
    uint32_t position;
    uint8_t source;
    bool used;
} recent_presses[RECENT_PRESSES];
static uint8_t recent_press_next;

static void tug_decay_handler(struct k_work *work);
K_WORK_DELAYABLE_DEFINE(tug_decay_work, tug_decay_handler);

void prospector_session_stats_note_press(uint8_t source, uint32_t position, int64_t timestamp) {
    if (source >= 2)
        return;
    k_mutex_lock(&session_stats_lock, K_FOREVER);
    for (int i = 0; i < RECENT_PRESSES; i++) {
        if (recent_presses[i].used && recent_presses[i].source == source &&
            recent_presses[i].position == position && recent_presses[i].timestamp == timestamp) {
            k_mutex_unlock(&session_stats_lock);
            return;
        }
    }
    recent_presses[recent_press_next].timestamp = timestamp;
    recent_presses[recent_press_next].position = position;
    recent_presses[recent_press_next].source = source;
    recent_presses[recent_press_next].used = true;
    recent_press_next = (recent_press_next + 1) % RECENT_PRESSES;
    if (session_stats.source_presses[source] < SESSION_COUNT_MAX)
        session_stats.source_presses[source]++;
    session_stats.last_press_ms[source] = timestamp;
    int tug = session_stats.tug + (source == 0 ? -SESSION_TUG_PULL : SESSION_TUG_PULL);
    session_stats.tug = tug < -SESSION_TUG_MAX  ? -SESSION_TUG_MAX
                        : tug > SESSION_TUG_MAX ? SESSION_TUG_MAX
                                                : tug;
    k_mutex_unlock(&session_stats_lock);
    /* Already-scheduled work keeps its deadline: one decay step per second, as before. */
    k_work_schedule(&tug_decay_work, K_SECONDS(1));
}

/* Owned here, not by a page, so every screen shows the same lean and it keeps
 * settling while no balance is visible. Stops once centred. */
static void tug_decay_handler(struct k_work *work) {
    ARG_UNUSED(work);
    k_mutex_lock(&session_stats_lock, K_FOREVER);
    if (session_stats.tug > 0)
        session_stats.tug--;
    else if (session_stats.tug < 0)
        session_stats.tug++;
    bool settled = session_stats.tug == 0;
    k_mutex_unlock(&session_stats_lock);
    if (!settled)
        k_work_schedule(&tug_decay_work, K_SECONDS(1));
}

#if IS_ENABLED(CONFIG_ZMK_WPM)
static int session_wpm_listener(const zmk_event_t *eh) {
    const struct zmk_wpm_state_changed *ev = as_zmk_wpm_state_changed(eh);
    if (ev != NULL && ev->state >= 0) {
        uint8_t value = ev->state > UINT8_MAX ? UINT8_MAX : (uint8_t)ev->state;
        k_mutex_lock(&session_stats_lock, K_FOREVER);
        if (value > session_stats.top_wpm)
            session_stats.top_wpm = value;
        k_mutex_unlock(&session_stats_lock);
    }
    return ZMK_EV_EVENT_BUBBLE;
}
#endif

ZMK_LISTENER(prospector_session_key_count, session_keycode_listener);
ZMK_SUBSCRIPTION(prospector_session_key_count, zmk_keycode_state_changed);
#if IS_ENABLED(CONFIG_ZMK_WPM)
ZMK_LISTENER(prospector_session_wpm, session_wpm_listener);
ZMK_SUBSCRIPTION(prospector_session_wpm, zmk_wpm_state_changed);
#endif

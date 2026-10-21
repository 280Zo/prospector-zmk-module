#include <lvgl.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/util.h>
#include <zmk/display.h>

#include <prospector/display_pages.h>
#if IS_ENABLED(CONFIG_PROSPECTOR_OPERATOR_SESSION_STATS)
#include "brightness_toast.h"
#endif
#if IS_ENABLED(CONFIG_PROSPECTOR_MODIFIER_TOAST)
#include <prospector/modifier_toast.h>
#endif

#if defined(CONFIG_PROSPECTOR_ENABLE_DIAGNOSTICS_PAGE)
#include "diagnostics_page.h"
#endif
#if defined(CONFIG_PROSPECTOR_ENABLE_AVATAR_PAGE)
#include "avatar_page.h"
#endif

LOG_MODULE_REGISTER(prospector_display_pages, LOG_LEVEL_INF);

#define zmk_display_status_screen prospector_theme_status_screen

#if defined(CONFIG_PROSPECTOR_STATUS_SCREEN_CLASSIC)
#include "layouts/classic/status_screen.c"
#elif defined(CONFIG_PROSPECTOR_STATUS_SCREEN_RADII)
#include "layouts/radii/status_screen.c"
#elif defined(CONFIG_PROSPECTOR_STATUS_SCREEN_FIELD)
#include "layouts/field/status_screen.c"
#elif defined(CONFIG_PROSPECTOR_STATUS_SCREEN_OPERATOR)
#include "layouts/operator/status_screen.c"
#else
#error "No status screen layout selected"
#endif

#undef zmk_display_status_screen

enum prospector_display_page {
    PROSPECTOR_DISPLAY_PAGE_STATUS,
    PROSPECTOR_DISPLAY_PAGE_DIAGNOSTICS,
    PROSPECTOR_DISPLAY_PAGE_AVATAR,
    PROSPECTOR_DISPLAY_PAGE_COUNT,
};

static lv_obj_t *status_page;
#if defined(CONFIG_PROSPECTOR_ENABLE_DIAGNOSTICS_PAGE)
static lv_obj_t *diagnostics_page;
#endif
#if defined(CONFIG_PROSPECTOR_ENABLE_AVATAR_PAGE)
static lv_obj_t *avatar_page;
#endif
static uint8_t active_page;
static uint8_t requested_page;

#define PROSPECTOR_AVATAR_OVERLAY_ONLY                                                             \
    (IS_ENABLED(CONFIG_PROSPECTOR_ENABLE_AVATAR_PAGE) &&                                           \
     IS_ENABLED(CONFIG_PROSPECTOR_ENABLE_HIDDEN_EXTRA) &&                                          \
     !IS_ENABLED(CONFIG_PROSPECTOR_AVATAR_SHOW_KEYBOARD_KNIGHT))

static void apply_page(uint8_t page);

static void apply_requested_page(void) { apply_page(requested_page); }

static void page_switch_work_handler(struct k_work *work) { apply_requested_page(); }

K_WORK_DEFINE(page_switch_work, page_switch_work_handler);

static void apply_page(uint8_t page) {
    if (status_page == NULL) {
        return;
    }

    page %= PROSPECTOR_DISPLAY_PAGE_COUNT;

#if defined(CONFIG_PROSPECTOR_ENABLE_AVATAR_PAGE)
    /* The hidden extra plays to the end: no page key may leave it. */
    if (page != PROSPECTOR_DISPLAY_PAGE_AVATAR && prospector_avatar_page_extra_active()) {
        return;
    }
#endif

    if (page == PROSPECTOR_DISPLAY_PAGE_DIAGNOSTICS &&
        !IS_ENABLED(CONFIG_PROSPECTOR_ENABLE_DIAGNOSTICS_PAGE)) {
        page = PROSPECTOR_DISPLAY_PAGE_STATUS;
    } else if (page == PROSPECTOR_DISPLAY_PAGE_AVATAR &&
               !IS_ENABLED(CONFIG_PROSPECTOR_ENABLE_AVATAR_PAGE)) {
        page = PROSPECTOR_DISPLAY_PAGE_STATUS;
    }

#if defined(CONFIG_PROSPECTOR_ENABLE_DIAGNOSTICS_PAGE)
    if (diagnostics_page != NULL) {
        lv_obj_add_flag(diagnostics_page, LV_OBJ_FLAG_HIDDEN);
        prospector_diagnostics_page_set_visible(false);
    }

    if (page == PROSPECTOR_DISPLAY_PAGE_DIAGNOSTICS) {
        if (diagnostics_page == NULL) {
            diagnostics_page = prospector_diagnostics_page_create(lv_layer_top());
        }

        if (diagnostics_page != NULL) {
            lv_obj_clear_flag(diagnostics_page, LV_OBJ_FLAG_HIDDEN);
            lv_obj_move_foreground(diagnostics_page);
            prospector_diagnostics_page_set_visible(true);
        } else {
            page = PROSPECTOR_DISPLAY_PAGE_STATUS;
        }
    }
#endif

#if defined(CONFIG_PROSPECTOR_ENABLE_AVATAR_PAGE)
    if (avatar_page != NULL) {
        lv_obj_add_flag(avatar_page, LV_OBJ_FLAG_HIDDEN);
        prospector_avatar_page_set_visible(false);
    }

    if (PROSPECTOR_AVATAR_OVERLAY_ONLY || page == PROSPECTOR_DISPLAY_PAGE_AVATAR) {
        if (avatar_page == NULL) {
            avatar_page = prospector_avatar_page_create(lv_layer_top());
        }

        if (avatar_page != NULL) {
            lv_obj_clear_flag(avatar_page, LV_OBJ_FLAG_HIDDEN);
            lv_obj_move_foreground(avatar_page);
            prospector_avatar_page_set_visible(true);
        } else {
            page = PROSPECTOR_DISPLAY_PAGE_STATUS;
        }
    }
#endif

    active_page = page;
#if IS_ENABLED(CONFIG_PROSPECTOR_OPERATOR_SESSION_STATS)
    prospector_modifier_row_set_status_visible(page == PROSPECTOR_DISPLAY_PAGE_STATUS &&
                                               !PROSPECTOR_AVATAR_OVERLAY_ONLY);
#endif
    LOG_INF("Prospector page applied: %u", active_page);
}

static int schedule_page_switch(uint8_t page) {
    requested_page = page % PROSPECTOR_DISPLAY_PAGE_COUNT;

    if (!zmk_display_is_initialized()) {
        return 0;
    }

    k_work_submit_to_queue(zmk_display_work_q(), &page_switch_work);

    return 0;
}

int prospector_display_page_set(uint8_t page) { return schedule_page_switch(page); }

static uint8_t page_after_next(uint8_t page) {
    switch (page) {
    case PROSPECTOR_DISPLAY_PAGE_STATUS:
#if defined(CONFIG_PROSPECTOR_ENABLE_DIAGNOSTICS_PAGE)
        return PROSPECTOR_DISPLAY_PAGE_DIAGNOSTICS;
#elif defined(CONFIG_PROSPECTOR_ENABLE_AVATAR_PAGE)
        return PROSPECTOR_DISPLAY_PAGE_AVATAR;
#else
        return page;
#endif
    case PROSPECTOR_DISPLAY_PAGE_DIAGNOSTICS:
#if defined(CONFIG_PROSPECTOR_ENABLE_AVATAR_PAGE)
        return PROSPECTOR_DISPLAY_PAGE_AVATAR;
#else
        return PROSPECTOR_DISPLAY_PAGE_STATUS;
#endif
    default:
        return PROSPECTOR_DISPLAY_PAGE_STATUS;
    }
}

static uint8_t page_after_prev(uint8_t page) {
    switch (page) {
    case PROSPECTOR_DISPLAY_PAGE_STATUS:
#if defined(CONFIG_PROSPECTOR_ENABLE_AVATAR_PAGE)
        return PROSPECTOR_DISPLAY_PAGE_AVATAR;
#elif defined(CONFIG_PROSPECTOR_ENABLE_DIAGNOSTICS_PAGE)
        return PROSPECTOR_DISPLAY_PAGE_DIAGNOSTICS;
#else
        return page;
#endif
    case PROSPECTOR_DISPLAY_PAGE_AVATAR:
#if defined(CONFIG_PROSPECTOR_ENABLE_DIAGNOSTICS_PAGE)
        return PROSPECTOR_DISPLAY_PAGE_DIAGNOSTICS;
#else
        return PROSPECTOR_DISPLAY_PAGE_STATUS;
#endif
    default:
        return PROSPECTOR_DISPLAY_PAGE_STATUS;
    }
}

/* Next/previous from keys and swipes run in order on the display queue. Each one counts
 * toward the hidden extra; a press it consumes does not change page. */
#define NAV_QUEUE_SIZE 8
static bool nav_queue[NAV_QUEUE_SIZE]; /* true = next */
static uint8_t nav_head, nav_count;
K_MUTEX_DEFINE(nav_lock);

static void nav_work_handler(struct k_work *work) {
    ARG_UNUSED(work);
    for (;;) {
        k_mutex_lock(&nav_lock, K_FOREVER);
        if (nav_count == 0) {
            k_mutex_unlock(&nav_lock);
            return;
        }
        bool next = nav_queue[nav_head];
        nav_head = (nav_head + 1) % NAV_QUEUE_SIZE;
        nav_count--;
        k_mutex_unlock(&nav_lock);

        if (prospector_display_page_note_swipe() ||
            (PROSPECTOR_AVATAR_OVERLAY_ONLY &&
             !IS_ENABLED(CONFIG_PROSPECTOR_ENABLE_DIAGNOSTICS_PAGE))) {
            continue;
        }
        /* Not via requested_page: a Status press queued behind this must still win. */
        apply_page(next ? page_after_next(active_page) : page_after_prev(active_page));
    }
}

K_WORK_DEFINE(nav_work, nav_work_handler);

static int queue_navigation(bool next) {
    if (!zmk_display_is_initialized()) {
        return 0;
    }
    k_mutex_lock(&nav_lock, K_FOREVER);
    if (nav_count < NAV_QUEUE_SIZE) { /* A full queue drops the newest press. */
        nav_queue[(nav_head + nav_count) % NAV_QUEUE_SIZE] = next;
        nav_count++;
    }
    k_mutex_unlock(&nav_lock);
    k_work_submit_to_queue(zmk_display_work_q(), &nav_work);
    return 0;
}

int prospector_display_page_next(void) { return queue_navigation(true); }

int prospector_display_page_prev(void) { return queue_navigation(false); }

int prospector_display_page_toggle(void) {
    if (PROSPECTOR_AVATAR_OVERLAY_ONLY && !IS_ENABLED(CONFIG_PROSPECTOR_ENABLE_DIAGNOSTICS_PAGE)) {
        return schedule_page_switch(PROSPECTOR_DISPLAY_PAGE_STATUS);
    }

#if defined(CONFIG_PROSPECTOR_ENABLE_DIAGNOSTICS_PAGE)
    uint8_t next_page = active_page == PROSPECTOR_DISPLAY_PAGE_STATUS
                            ? PROSPECTOR_DISPLAY_PAGE_DIAGNOSTICS
                            : PROSPECTOR_DISPLAY_PAGE_STATUS;

    return schedule_page_switch(next_page);
#elif defined(CONFIG_PROSPECTOR_ENABLE_AVATAR_PAGE)
    uint8_t next_page = active_page == PROSPECTOR_DISPLAY_PAGE_STATUS
                            ? PROSPECTOR_DISPLAY_PAGE_AVATAR
                            : PROSPECTOR_DISPLAY_PAGE_STATUS;

    return schedule_page_switch(next_page);
#else
    return schedule_page_switch(PROSPECTOR_DISPLAY_PAGE_STATUS);
#endif
}

bool prospector_display_page_note_swipe(void) {
#if defined(CONFIG_PROSPECTOR_ENABLE_AVATAR_PAGE)
    if (prospector_avatar_page_note_swipe()) {
        schedule_page_switch(PROSPECTOR_DISPLAY_PAGE_AVATAR);
        return true;
    }
#endif

    return false;
}

lv_obj_t *zmk_display_status_screen() {
    status_page = prospector_theme_status_screen();
    lv_obj_clear_flag(status_page, LV_OBJ_FLAG_SCROLLABLE);

    requested_page = PROSPECTOR_DISPLAY_PAGE_STATUS;
    active_page = PROSPECTOR_DISPLAY_PAGE_STATUS;
    apply_requested_page();

#if IS_ENABLED(CONFIG_PROSPECTOR_MODIFIER_TOAST)
    prospector_modifier_toast_init();
#endif
    return status_page;
}

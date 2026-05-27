#include <lvgl.h>
#include <zephyr/kernel.h>
#include <zmk/display.h>

#include <prospector/display_pages.h>

#include "diagnostics_page.h"

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
    PROSPECTOR_DISPLAY_PAGE_COUNT,
};

static lv_obj_t *status_page;
static lv_obj_t *diagnostics_page;
static uint8_t active_page;
static uint8_t requested_page;

static void apply_requested_page(void);

static void page_switch_work_handler(struct k_work *work) { apply_requested_page(); }

K_WORK_DEFINE(page_switch_work, page_switch_work_handler);

static void apply_requested_page(void) {
    if (status_page == NULL) {
        return;
    }

    uint8_t page = requested_page % PROSPECTOR_DISPLAY_PAGE_COUNT;

    if (page == PROSPECTOR_DISPLAY_PAGE_DIAGNOSTICS) {
        if (diagnostics_page == NULL) {
            diagnostics_page = prospector_diagnostics_page_create(status_page);
        }

        lv_obj_clear_flag(diagnostics_page, LV_OBJ_FLAG_HIDDEN);
        lv_obj_move_foreground(diagnostics_page);
    } else if (diagnostics_page != NULL) {
        lv_obj_add_flag(diagnostics_page, LV_OBJ_FLAG_HIDDEN);
    }

    active_page = page;
}

static int schedule_page_switch(uint8_t page) {
    requested_page = page % PROSPECTOR_DISPLAY_PAGE_COUNT;

    if (zmk_display_is_initialized()) {
        k_work_submit_to_queue(zmk_display_work_q(), &page_switch_work);
    }

    return 0;
}

int prospector_display_page_set(uint8_t page) { return schedule_page_switch(page); }

int prospector_display_page_next(void) {
    return schedule_page_switch((active_page + 1) % PROSPECTOR_DISPLAY_PAGE_COUNT);
}

int prospector_display_page_prev(void) {
    return schedule_page_switch((active_page + PROSPECTOR_DISPLAY_PAGE_COUNT - 1) %
                                PROSPECTOR_DISPLAY_PAGE_COUNT);
}

lv_obj_t *zmk_display_status_screen() {
    status_page = prospector_theme_status_screen();
    lv_obj_clear_flag(status_page, LV_OBJ_FLAG_SCROLLABLE);

    requested_page = PROSPECTOR_DISPLAY_PAGE_STATUS;
    active_page = PROSPECTOR_DISPLAY_PAGE_STATUS;
    apply_requested_page();

    return status_page;
}

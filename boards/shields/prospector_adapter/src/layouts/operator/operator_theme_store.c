/* SPDX-License-Identifier: MIT */
#include <string.h>
#include <zephyr/kernel.h>

#include "operator_theme.h"

#if IS_ENABLED(CONFIG_SETTINGS)
#include <zephyr/settings/settings.h>

/* Saving waits for a pause in switching so cycling through themes writes flash once. */
#define SAVE_DELAY_MS 3000

/* The build's start theme is stored with the choice so a build that changes the start
 * theme takes effect instead of the old choice. */
struct stored_theme {
    uint8_t start;
    uint8_t index;
};

static void save_work_handler(struct k_work *work) {
    (void)work;
    struct stored_theme stored = {
        .start = (uint8_t)operator_theme_start_index(),
        .index = (uint8_t)operator_theme_index(),
    };
    settings_save_one("prospector/theme/state", &stored, sizeof(stored));
}

static K_WORK_DELAYABLE_DEFINE(save_work, save_work_handler);

void operator_theme_store_schedule(void) { k_work_reschedule(&save_work, K_MSEC(SAVE_DELAY_MS)); }

static int theme_settings_set(const char *name, size_t len, settings_read_cb read_cb,
                              void *cb_arg) {
    if (strcmp(name, "state") != 0)
        return -ENOENT;
    struct stored_theme stored;
    if (len != sizeof(stored) || read_cb(cb_arg, &stored, sizeof(stored)) != sizeof(stored))
        return -EINVAL;
    operator_theme_restore(operator_theme_resolve_start(true, stored.start, stored.index,
                                                        operator_theme_start_index(),
                                                        operator_theme_count()));
    return 0;
}

SETTINGS_STATIC_HANDLER_DEFINE(prospector_operator_theme, "prospector/theme", NULL,
                               theme_settings_set, NULL, NULL);
#else
void operator_theme_store_schedule(void) {}
#endif

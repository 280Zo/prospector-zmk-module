/* SPDX-License-Identifier: MIT */
#include <string.h>
#include <zephyr/kernel.h>

#include "operator_theme.h"

#if IS_ENABLED(CONFIG_SETTINGS)
#include <zephyr/settings/settings.h>

/* Saving waits for a pause in switching so cycling through themes writes flash once. */
#define SAVE_DELAY_MS 3000

static void save_work_handler(struct k_work *work) {
    (void)work;
    uint8_t stored[OPERATOR_THEME_STATE_SIZE];
    operator_theme_encode_state(stored);
    settings_save_one("prospector/theme/state", stored, sizeof(stored));
}

static K_WORK_DELAYABLE_DEFINE(save_work, save_work_handler);

void operator_theme_store_schedule(void) { k_work_reschedule(&save_work, K_MSEC(SAVE_DELAY_MS)); }

static int theme_settings_set(const char *name, size_t len, settings_read_cb read_cb,
                              void *cb_arg) {
    if (strcmp(name, "state") != 0)
        return -ENOENT;
    /* Legacy positional records cannot safely identify a selected/reordered registry. */
    if (len == 2)
        return 0;
    uint8_t stored[OPERATOR_THEME_STATE_SIZE];
    if (len != sizeof(stored) || read_cb(cb_arg, stored, sizeof(stored)) != sizeof(stored))
        return -EINVAL;
    operator_theme_restore(operator_theme_resolve_state(stored, sizeof(stored)));
    return 0;
}

SETTINGS_STATIC_HANDLER_DEFINE(prospector_operator_theme, "prospector/theme", NULL,
                               theme_settings_set, NULL, NULL);
#else
void operator_theme_store_schedule(void) {}
#endif

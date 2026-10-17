/* SPDX-License-Identifier: MIT */
#pragma once

#include <stdbool.h>
#include <stdint.h>

/* Sequence is captured under the controller lock; publish only after unlocking.
 * show=false invalidates both visible and pending feedback on activity changes. */
void prospector_brightness_toast_request(uint32_t sequence, uint8_t target, bool automatic,
                                         bool show);

/* Cache current connected modifier/mode state; rendering stays on display queue. */
void prospector_modifier_toast_request(const char *chord, bool caps_word, bool caps_lock,
                                       bool active);
/* Disconnect/profile invalidation must bypass ordinary release grace. */
void prospector_modifier_toast_invalidate_chord(void);

/* Theme name in the floating toast for 1.5 s on every page; a brightness change
 * replaces it. The name is copied. */
void prospector_theme_toast_request(const char *name);

#if defined(CONFIG_PROSPECTOR_OPERATOR_SESSION_STATS)
void prospector_modifier_row_request(uint8_t mods, bool caps_word, bool caps_lock, bool active);
/* Display-queue page switch; persistent row belongs only to Operator status. */
void prospector_modifier_row_set_status_visible(bool visible);
#endif

/* SPDX-License-Identifier: MIT */

#pragma once

#include <stdbool.h>

#include <lvgl.h>

lv_obj_t *prospector_avatar_page_create(lv_obj_t *parent);
void prospector_avatar_page_set_visible(bool visible);
bool prospector_avatar_page_note_swipe(void);
/* True while the hidden extra animation is playing. */
bool prospector_avatar_page_extra_active(void);

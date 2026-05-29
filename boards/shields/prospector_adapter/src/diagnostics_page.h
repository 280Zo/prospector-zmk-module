/* SPDX-License-Identifier: MIT */

#pragma once

#include <stdbool.h>

#include <lvgl.h>

lv_obj_t *prospector_diagnostics_page_create(lv_obj_t *parent);
void prospector_diagnostics_page_set_visible(bool visible);

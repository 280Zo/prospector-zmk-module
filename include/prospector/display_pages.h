/* SPDX-License-Identifier: MIT */

#pragma once

#include <stdbool.h>
#include <stdint.h>

int prospector_display_page_set(uint8_t page);
int prospector_display_page_next(void);
int prospector_display_page_prev(void);
int prospector_display_page_toggle(void);
bool prospector_display_page_note_swipe(void);

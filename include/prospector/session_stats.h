/* SPDX-License-Identifier: MIT */
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

struct prospector_session_stats {
    uint32_t key_count;
    uint32_t source_presses[2];
    uint8_t top_wpm;
    /* Recent-typing lean, -18..18 like Diagnostics; negative = source 0. */
    int8_t tug;
    /* Backspace keycode presses (subset of key_count). */
    uint32_t backspace_count;
    /* Raw split-trackball travel in 1/256 sensor counts. */
    uint64_t ball_q8;
    /* Event time of each half's latest physical press; 0 = none since boot. */
    int64_t last_press_ms[2];
};

/* Boot-local counters, independent of display construction and visibility. */
void prospector_session_stats_get(struct prospector_session_stats *out);
void prospector_session_stats_format_count(uint32_t count, char *out, size_t size);
/* Physical press from a split source; repeats of the same event are ignored. */
void prospector_session_stats_note_press(uint8_t source, uint32_t position, int64_t timestamp);
/* Raw input event from split pointing device `device` (0..1); X/Y combine at sync. */
void prospector_session_stats_note_motion(uint8_t device, uint16_t type, uint16_t code,
                                          int32_t value, bool sync);
/* Travel at CONFIG_PROSPECTOR_BALL_CPI: "12.4m", "12.3km", "123km". */
void prospector_session_stats_format_distance(uint64_t ball_q8, char *out, size_t size);
/* part/total to one decimal: "--" when total is 0, "6.2%", "100%". */
void prospector_session_stats_format_share(uint32_t part, uint32_t total, char *out, size_t size);

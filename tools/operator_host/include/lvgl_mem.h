#pragma once

#include <zephyr/sys/mem_stats.h>

static inline void lvgl_heap_stats(struct sys_memory_stats *stats) {
    stats->allocated_bytes = 20 * 1024;
    stats->free_bytes = 9 * 1024;
    stats->max_allocated_bytes = 24 * 1024;
}

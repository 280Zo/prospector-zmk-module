#pragma once

#include <stddef.h>

/* Mirrors Zephyr include/zephyr/sys/mem_stats.h. */
struct sys_memory_stats {
    size_t free_bytes;
    size_t allocated_bytes;
    size_t max_allocated_bytes;
};

/* Deterministic host-only sample values; never linked into firmware. */
#include <prospector/session_stats.h>
#include <stdio.h>

void prospector_session_stats_get(struct prospector_session_stats *out) {
    *out = (struct prospector_session_stats){.key_count = 12345, .top_wpm = 96, .tug = 6};
}
void prospector_session_stats_format_count(uint32_t count, char *out, size_t size) {
    if (count >= 10000)
        snprintf(out, size, "%u.%uK", count / 1000, count / 100 % 10);
    else
        snprintf(out, size, "%u", count);
}

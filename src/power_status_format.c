/* SPDX-License-Identifier: MIT */
#include <prospector/power_status.h>
#include <stdio.h>

void prospector_power_diagnostic_format(const struct prospector_power_diagnostic *state, char *text,
                                        size_t size) {
    static const char *const names[] = {"OFF", "SEC", "SVC", "CHR", "CCC", "SUB", "READ", "LIVE"};
    const char *name =
        (unsigned)state->stage < sizeof(names) / sizeof(names[0]) ? names[state->stage] : "?";
    const char *vbus = state->state == PROSPECTOR_POWER_USB       ? "V1"
                       : state->state == PROSPECTOR_POWER_BATTERY ? "V0"
                                                                  : "?";
    if (state->stage == PROSPECTOR_POWER_SEC)
        snprintf(text, size, "%s L%u/%d r%u %s", name, state->security, state->error,
                 state->retries, vbus);
    else
        snprintf(text, size, "%s/%d r%u %s", name, state->error, state->retries, vbus);
}

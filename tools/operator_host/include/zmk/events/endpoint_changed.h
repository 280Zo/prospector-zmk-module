#pragma once
#include <zmk/event_manager.h>
struct zmk_endpoint_changed { int unused; };
static inline const struct zmk_endpoint_changed *as_zmk_endpoint_changed(const zmk_event_t *eh) { return (const struct zmk_endpoint_changed *)eh; }

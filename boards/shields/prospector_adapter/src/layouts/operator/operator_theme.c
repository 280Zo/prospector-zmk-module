/* SPDX-License-Identifier: MIT */
#include "operator_theme.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#if defined(OPERATOR_THEME_REGISTRY_VERSION)
#if OPERATOR_THEME_REGISTRY_VERSION != 1
#error "Unsupported generated Operator theme registry version"
#endif
#if defined(CONFIG_PROSPECTOR_OPERATOR_CUSTOM_THEME)
#error "Generated theme selection replaces legacy Custom config; remove the old fragment"
#endif
struct operator_theme_entry {
    const char *id;
    struct operator_palette palette;
};
static const struct operator_theme_entry registry[] = {OPERATOR_THEME_REGISTRY_ENTRIES};
#define THEME_COUNT (sizeof(registry) / sizeof(registry[0]))
_Static_assert(THEME_COUNT > 0, "Select at least one Operator theme");
_Static_assert(OPERATOR_THEME_REGISTRY_START < THEME_COUNT, "Invalid registry startup theme");
#else

static const struct operator_palette builtin[] = {
    OPERATOR_PALETTE_DEFAULT,   OPERATOR_PALETTE_EMBER,      OPERATOR_PALETTE_GLACIER,
    OPERATOR_PALETTE_SYNTHWAVE, OPERATOR_PALETTE_SEA_SHELLS, OPERATOR_PALETTE_GITHUB,
};
#define BUILTIN_COUNT (sizeof(builtin) / sizeof(builtin[0]))

#if defined(CONFIG_PROSPECTOR_OPERATOR_CUSTOM_THEME)
#if !defined(CONFIG_PROSPECTOR_OPERATOR_CUSTOM_BG)
#error "Custom theme: set CONFIG_PROSPECTOR_OPERATOR_CUSTOM_BG in dongle_prospector_operator_theme_custom.conf"
#endif
#if !defined(CONFIG_PROSPECTOR_OPERATOR_CUSTOM_TEXT)
#error "Custom theme: set CONFIG_PROSPECTOR_OPERATOR_CUSTOM_TEXT in dongle_prospector_operator_theme_custom.conf"
#endif
#if !defined(CONFIG_PROSPECTOR_OPERATOR_CUSTOM_MUTED)
#error "Custom theme: set CONFIG_PROSPECTOR_OPERATOR_CUSTOM_MUTED in dongle_prospector_operator_theme_custom.conf"
#endif
#if !defined(CONFIG_PROSPECTOR_OPERATOR_CUSTOM_OUTLINE)
#error "Custom theme: set CONFIG_PROSPECTOR_OPERATOR_CUSTOM_OUTLINE in dongle_prospector_operator_theme_custom.conf"
#endif
#if !defined(CONFIG_PROSPECTOR_OPERATOR_CUSTOM_ACCENT)
#error "Custom theme: set CONFIG_PROSPECTOR_OPERATOR_CUSTOM_ACCENT in dongle_prospector_operator_theme_custom.conf"
#endif
#if !defined(CONFIG_PROSPECTOR_OPERATOR_CUSTOM_INFO)
#error "Custom theme: set CONFIG_PROSPECTOR_OPERATOR_CUSTOM_INFO in dongle_prospector_operator_theme_custom.conf"
#endif
#if !defined(CONFIG_PROSPECTOR_OPERATOR_CUSTOM_MARKER)
#error "Custom theme: set CONFIG_PROSPECTOR_OPERATOR_CUSTOM_MARKER in dongle_prospector_operator_theme_custom.conf"
#endif
#if !defined(CONFIG_PROSPECTOR_OPERATOR_CUSTOM_CAPS)
#error "Custom theme: set CONFIG_PROSPECTOR_OPERATOR_CUSTOM_CAPS in dongle_prospector_operator_theme_custom.conf"
#endif
#if !defined(CONFIG_PROSPECTOR_OPERATOR_CUSTOM_BRIGHT)
#error "Custom theme: set CONFIG_PROSPECTOR_OPERATOR_CUSTOM_BRIGHT in dongle_prospector_operator_theme_custom.conf"
#endif
#if !defined(CONFIG_PROSPECTOR_OPERATOR_CUSTOM_BATTERY)
#error "Custom theme: set CONFIG_PROSPECTOR_OPERATOR_CUSTOM_BATTERY in dongle_prospector_operator_theme_custom.conf"
#endif
#if !defined(CONFIG_PROSPECTOR_OPERATOR_CUSTOM_LOW)
#error "Custom theme: set CONFIG_PROSPECTOR_OPERATOR_CUSTOM_LOW in dongle_prospector_operator_theme_custom.conf"
#endif
#if !defined(CONFIG_PROSPECTOR_OPERATOR_CUSTOM_USB)
#error "Custom theme: set CONFIG_PROSPECTOR_OPERATOR_CUSTOM_USB in dongle_prospector_operator_theme_custom.conf"
#endif
#if !defined(CONFIG_PROSPECTOR_OPERATOR_CUSTOM_BLE)
#error "Custom theme: set CONFIG_PROSPECTOR_OPERATOR_CUSTOM_BLE in dongle_prospector_operator_theme_custom.conf"
#endif
#if !defined(CONFIG_PROSPECTOR_OPERATOR_CUSTOM_SLOT)
#error "Custom theme: set CONFIG_PROSPECTOR_OPERATOR_CUSTOM_SLOT in dongle_prospector_operator_theme_custom.conf"
#endif

#define CUSTOM_BG CONFIG_PROSPECTOR_OPERATOR_CUSTOM_BG
#if ((((CUSTOM_BG) >> 16 & 0xff) * 2 + ((CUSTOM_BG) >> 8 & 0xff) * 5 + ((CUSTOM_BG) & 0xff)) / 8) > 0x20
#error "Custom theme: CONFIG_PROSPECTOR_OPERATOR_CUSTOM_BG must be a dark colour (0x000000 recommended)"
#endif

#define THEME_COUNT (BUILTIN_COUNT + 1)

/* sRGB channel to linear light, scaled to 0..65535 (WCAG 2 definition). */
static const uint16_t srgb_linear[256] = {
    0, 20, 40, 60, 80, 99, 119, 139, 159, 179, 199, 219,
    241, 264, 288, 313, 340, 367, 396, 427, 458, 491, 526, 562,
    599, 637, 677, 718, 761, 805, 851, 898, 947, 997, 1048, 1101,
    1156, 1212, 1270, 1330, 1391, 1453, 1517, 1583, 1651, 1720, 1790, 1863,
    1937, 2013, 2090, 2170, 2250, 2333, 2418, 2504, 2592, 2681, 2773, 2866,
    2961, 3058, 3157, 3258, 3360, 3464, 3570, 3678, 3788, 3900, 4014, 4129,
    4247, 4366, 4488, 4611, 4736, 4864, 4993, 5124, 5257, 5392, 5530, 5669,
    5810, 5953, 6099, 6246, 6395, 6547, 6700, 6856, 7014, 7174, 7335, 7500,
    7666, 7834, 8004, 8177, 8352, 8528, 8708, 8889, 9072, 9258, 9445, 9635,
    9828, 10022, 10219, 10417, 10619, 10822, 11028, 11235, 11446, 11658, 11873, 12090,
    12309, 12530, 12754, 12980, 13209, 13440, 13673, 13909, 14146, 14387, 14629, 14874,
    15122, 15371, 15623, 15878, 16135, 16394, 16656, 16920, 17187, 17456, 17727, 18001,
    18277, 18556, 18837, 19121, 19407, 19696, 19987, 20281, 20577, 20876, 21177, 21481,
    21787, 22096, 22407, 22721, 23038, 23357, 23678, 24002, 24329, 24658, 24990, 25325,
    25662, 26001, 26344, 26688, 27036, 27386, 27739, 28094, 28452, 28813, 29176, 29542,
    29911, 30282, 30656, 31033, 31412, 31794, 32179, 32567, 32957, 33350, 33745, 34143,
    34544, 34948, 35355, 35764, 36176, 36591, 37008, 37429, 37852, 38278, 38706, 39138,
    39572, 40009, 40449, 40891, 41337, 41785, 42236, 42690, 43147, 43606, 44069, 44534,
    45002, 45473, 45947, 46423, 46903, 47385, 47871, 48359, 48850, 49344, 49841, 50341,
    50844, 51349, 51858, 52369, 52884, 53401, 53921, 54445, 54971, 55500, 56032, 56567,
    57105, 57646, 58190, 58737, 59287, 59840, 60396, 60955, 61517, 62082, 62650, 63221,
    63795, 64372, 64952, 65535
};

/* Relative luminance x 655350000 (weights x 10000, channels x 65535). */
static uint32_t luminance(uint32_t c) {
    return 2126u * srgb_linear[c >> 16 & 0xff] + 7152u * srgb_linear[c >> 8 & 0xff] +
           722u * srgb_linear[c & 0xff];
}

/* WCAG contrast of a and b as a fraction (lighter + 0.05) / (darker + 0.05). */
struct ratio {
    uint64_t num, den;
};

static struct ratio contrast(uint32_t a, uint32_t b) {
    const uint64_t k = 32767500u; /* 0.05 on the same scale */
    uint64_t la = luminance(a), lb = luminance(b);
    return la > lb ? (struct ratio){la + k, lb + k} : (struct ratio){lb + k, la + k};
}

static bool greater(struct ratio x, struct ratio y) { return x.num * y.den > y.num * x.den; }

static struct ratio smaller(struct ratio x, struct ratio y) { return greater(x, y) ? y : x; }

/* Per channel a + (b - a) * percent / 100, rounded half up. */
static uint32_t mix(uint32_t a, uint32_t b, uint32_t percent) {
    uint32_t out = 0;
    for (int shift = 16; shift >= 0; shift -= 8) {
        uint32_t x = a >> shift & 0xff, y = b >> shift & 0xff;
        out |= ((x * (100 - percent) + y * percent + 50) / 100) << shift;
    }
    return out;
}

static struct operator_palette custom;
static bool custom_ready;

static const struct operator_palette *custom_palette(void) {
    if (custom_ready)
        return &custom;
    const uint32_t bg = CONFIG_PROSPECTOR_OPERATOR_CUSTOM_BG;
    const uint32_t text = CONFIG_PROSPECTOR_OPERATOR_CUSTOM_TEXT;
    const uint32_t muted = CONFIG_PROSPECTOR_OPERATOR_CUSTOM_MUTED;
    const uint32_t info = CONFIG_PROSPECTOR_OPERATOR_CUSTOM_INFO;
    const uint32_t caps = CONFIG_PROSPECTOR_OPERATOR_CUSTOM_CAPS;
    const uint32_t accent = CONFIG_PROSPECTOR_OPERATOR_CUSTOM_ACCENT;
    const uint32_t battery = CONFIG_PROSPECTOR_OPERATOR_CUSTOM_BATTERY;
    const uint32_t low = CONFIG_PROSPECTOR_OPERATOR_CUSTOM_LOW;
    const uint32_t usb = CONFIG_PROSPECTOR_OPERATOR_CUSTOM_USB;
    const uint32_t ble = CONFIG_PROSPECTOR_OPERATOR_CUSTOM_BLE;
    const uint32_t slot = CONFIG_PROSPECTOR_OPERATOR_CUSTOM_SLOT;
    struct ratio battery_bg = smaller(contrast(bg, battery), contrast(bg, low));
    struct ratio battery_text = smaller(contrast(text, battery), contrast(text, low));
    custom = (struct operator_palette){
        .name = "Custom",
        .label_box_radius = 0,
        .label_box_opacity = 255,
        .bg = bg,
        .label = muted,
        .value = info,
        .balance_track = CONFIG_PROSPECTOR_OPERATOR_CUSTOM_OUTLINE,
        .balance_tick = muted,
        .balance_marker = CONFIG_PROSPECTOR_OPERATOR_CUSTOM_MARKER,
        .mod_row_active = info,
        .mod_row_inactive = mix(bg, muted, 35),
        .caps = caps,
        .brightness = CONFIG_PROSPECTOR_OPERATOR_CUSTOM_BRIGHT,
        .wpm_peak = mix(bg, text, 30),
        .battery_number = greater(battery_text, battery_bg) ? text : bg,
        .slot_active_text = greater(contrast(text, slot), contrast(bg, slot)) ? text : bg,
        .slot_inactive_text = bg,
        .usb_icon = bg,
        .mod_active = info,
        .mod_inactive = mix(bg, muted, 35),
        .mod_separator = muted,
        .mod_caps_word = caps,
        .wpm_bar_active = accent,
        .wpm_bar_inactive = mix(bg, text, 10),
        .wpm_text = accent,
        .layer_text = text,
        .layer_dot_active = mix(text, bg, 8),
        .layer_dot_inactive = mix(bg, muted, 55),
        .battery_fill = battery,
        .battery_ring = mix(bg, battery, 35),
        .battery_bg = mix(bg, text, 30),
        .battery_label = text,
        .battery_disconnected_fill = mix(bg, muted, 30),
        .battery_disconnected_ring = mix(bg, muted, 15),
        .battery_disconnected_label = bg,
        .battery_low_fill = low,
        .battery_low_ring = mix(bg, low, 35),
        .usb_active_bg = usb,
        .usb_inactive_bg = mix(bg, usb, 25),
        .ble_active_bg = ble,
        .ble_inactive_bg = mix(bg, ble, 25),
        .output_active_text = bg,
        .output_inactive_text = slot,
        .slot_active_bg = slot,
        .slot_inactive_bg = mix(bg, slot, 30),
        .slot_text = text,
        .texture = mix(bg, muted, 22),
        .texture_faint = mix(bg, muted, 12),
    };
    custom_ready = true;
    return &custom;
}
#else
#define THEME_COUNT BUILTIN_COUNT
#endif

#endif /* generated registry / legacy defaults */

#if defined(OPERATOR_THEME_REGISTRY_VERSION)
#define START_INDEX OPERATOR_THEME_REGISTRY_START
#elif defined(CONFIG_PROSPECTOR_OPERATOR_THEME_EMBER)
#define START_INDEX 1
#elif defined(CONFIG_PROSPECTOR_OPERATOR_THEME_GLACIER)
#define START_INDEX 2
#elif defined(CONFIG_PROSPECTOR_OPERATOR_THEME_SYNTHWAVE)
#define START_INDEX 3
#elif defined(CONFIG_PROSPECTOR_OPERATOR_THEME_SEA_SHELLS)
#define START_INDEX 4
#elif defined(CONFIG_PROSPECTOR_OPERATOR_THEME_GITHUB)
#define START_INDEX 5
#elif defined(CONFIG_PROSPECTOR_OPERATOR_THEME_CUSTOM)
#define START_INDEX 6
#else
#define START_INDEX 0
#endif

static size_t current_theme = START_INDEX;

size_t operator_theme_count(void) { return THEME_COUNT; }

size_t operator_theme_index(void) { return current_theme; }

size_t operator_theme_start_index(void) { return START_INDEX; }

const struct operator_palette *operator_theme_at(size_t index) {
#if defined(OPERATOR_THEME_REGISTRY_VERSION)
    return &registry[index < THEME_COUNT ? index : 0].palette;
#else
#if defined(CONFIG_PROSPECTOR_OPERATOR_CUSTOM_THEME)
    if (index == BUILTIN_COUNT)
        return custom_palette();
#endif
    return &builtin[index < BUILTIN_COUNT ? index : 0];
#endif
}

const char *operator_theme_id_at(size_t index) {
#if defined(OPERATOR_THEME_REGISTRY_VERSION)
    return registry[index < THEME_COUNT ? index : 0].id;
#else
    static const char *const ids[] = {"default",
                                      "ember",
                                      "glacier",
                                      "synthwave",
                                      "sea_shells",
                                      "github"
#if defined(CONFIG_PROSPECTOR_OPERATOR_CUSTOM_THEME)
                                      ,
                                      "custom"
#endif
    };
    return ids[index < THEME_COUNT ? index : 0];
#endif
}

bool operator_theme_find_id(const char *id, size_t *index) {
    if (!id || !index)
        return false;
    for (size_t i = 0; i < THEME_COUNT; ++i) {
        if (strcmp(id, operator_theme_id_at(i)) == 0) {
            *index = i;
            return true;
        }
    }
    return false;
}

void operator_theme_encode_state(uint8_t state[OPERATOR_THEME_STATE_SIZE]) {
    memset(state, 0, OPERATOR_THEME_STATE_SIZE);
    state[0] = 1;
    const char *start = operator_theme_id_at(START_INDEX);
    const char *current = operator_theme_id_at(current_theme);
    if (strlen(start) >= OPERATOR_THEME_ID_SIZE || strlen(current) >= OPERATOR_THEME_ID_SIZE) {
        state[0] = 0;
        return;
    }
    memcpy(state + 1, start, strlen(start));
    memcpy(state + 1 + OPERATOR_THEME_ID_SIZE, current, strlen(current));
}

size_t operator_theme_resolve_state(const uint8_t *state, size_t len) {
    if (!state || len != OPERATOR_THEME_STATE_SIZE || state[0] != 1)
        return START_INDEX;
    const char *start = (const char *)(state + 1);
    const char *current = start + OPERATOR_THEME_ID_SIZE;
    if (!memchr(start, 0, OPERATOR_THEME_ID_SIZE) || !memchr(current, 0, OPERATOR_THEME_ID_SIZE) ||
        strcmp(start, operator_theme_id_at(START_INDEX)) != 0)
        return START_INDEX;
    size_t index;
    return operator_theme_find_id(current, &index) ? index : START_INDEX;
}

const struct operator_palette *operator_theme_current(void) {
    return operator_theme_at(current_theme);
}

void operator_theme_select(size_t index) {
    if (index < THEME_COUNT)
        current_theme = index;
}

size_t operator_theme_resolve_start(bool saved, uint8_t saved_start, uint8_t saved_index,
                                    size_t build_start, size_t count) {
    if (!saved || saved_start != build_start || saved_index >= count)
        return build_start;
    return saved_index;
}

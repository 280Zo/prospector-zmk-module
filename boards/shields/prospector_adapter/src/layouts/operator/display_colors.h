#pragma once

#include <stdint.h>

/* Operator colour roles. Each theme is a constant palette; DISPLAY_COLOR_<ROLE> reads
 * the palette selected at runtime (operator_theme.c), so a theme can change while the
 * dongle runs. The Default palette is the original Operator look. Every theme uses a
 * true black background: near-black tints look washed out on the ST7789V panel. Dim
 * shades are blends of each palette's original colours. GitHub uses the GitHub Dark
 * High Contrast palette. */

struct operator_palette {
    const char *name;
    uint32_t bg;
    uint32_t label;
    uint32_t value;
    uint32_t balance_track;
    uint32_t balance_tick;
    uint32_t balance_marker;
    uint32_t mod_row_active;
    uint32_t mod_row_inactive;
    uint32_t caps;
    uint32_t brightness;
    uint32_t wpm_peak;
    uint32_t battery_number;
    uint32_t slot_active_text;
    uint32_t slot_inactive_text;
    uint32_t usb_icon;
    uint32_t mod_active;
    uint32_t mod_inactive;
    uint32_t mod_separator;
    uint32_t mod_caps_word;
    uint32_t wpm_bar_active;
    uint32_t wpm_bar_inactive;
    uint32_t wpm_text;
    uint32_t layer_text;
    uint32_t layer_dot_active;
    uint32_t layer_dot_inactive;
    uint32_t battery_fill;
    uint32_t battery_ring;
    uint32_t battery_bg;
    uint32_t battery_label;
    uint32_t battery_disconnected_fill;
    uint32_t battery_disconnected_ring;
    uint32_t battery_disconnected_label;
    uint32_t battery_low_fill;
    uint32_t battery_low_ring;
    uint32_t usb_active_bg;
    uint32_t usb_inactive_bg;
    uint32_t ble_active_bg;
    uint32_t ble_inactive_bg;
    uint32_t output_active_text;
    uint32_t output_inactive_text;
    uint32_t slot_active_bg;
    uint32_t slot_inactive_bg;
    uint32_t slot_text;
    uint32_t texture;
    uint32_t texture_faint;
};

#define OPERATOR_PALETTE_DEFAULT                                                               \
    {                                                                                          \
        .name = "Default",                                                                     \
        .bg = 0x000000,                                                                        \
        .label = 0x8a949c,                                                                     \
        .value = 0xb1e5f0,                                                                     \
        .balance_track = 0x30343a,                                                             \
        .balance_tick = 0x8a949c,                                                              \
        .balance_marker = 0x70e8f0,                                                            \
        .mod_row_active = 0xb1e5f0,                                                            \
        .mod_row_inactive = 0x34373a,                                                          \
        .caps = 0xffbf00,                                                                      \
        .brightness = 0xffd84a,                                                                \
        .wpm_peak = 0x505050,                                                                  \
        .battery_number = 0x000000,                                                            \
        .slot_active_text = 0x000000,                                                          \
        .slot_inactive_text = 0x000000,                                                        \
        .usb_icon = 0x000000,                                                                  \
        .mod_active = 0xb1e5f0,                                                                \
        .mod_inactive = 0x3b527c,                                                              \
        .mod_separator = 0x606060,                                                             \
        .mod_caps_word = 0xffbf00,                                                             \
        .wpm_bar_active = 0xc2526a,                                                            \
        .wpm_bar_inactive = 0x242424,                                                          \
        .wpm_text = 0xc2526a,                                                                  \
        .layer_text = 0xffffff,                                                                \
        .layer_dot_active = 0xe0e0e0,                                                          \
        .layer_dot_inactive = 0x575757,                                                        \
        .battery_fill = 0x54806c,                                                              \
        .battery_ring = 0x2a4036,                                                              \
        .battery_bg = 0x505050,                                                                \
        .battery_label = 0xffffff,                                                             \
        .battery_disconnected_fill = 0x383c42,                                                 \
        .battery_disconnected_ring = 0x282c30,                                                 \
        .battery_disconnected_label = 0x000000,                                                \
        .battery_low_fill = 0xc08040,                                                          \
        .battery_low_ring = 0x584028,                                                          \
        .usb_active_bg = 0xb9b9a7,                                                             \
        .usb_inactive_bg = 0x4f4f40,                                                           \
        .ble_active_bg = 0x569fa7,                                                             \
        .ble_inactive_bg = 0x353f40,                                                           \
        .output_active_text = 0x000000,                                                        \
        .output_inactive_text = 0x7b7d93,                                                      \
        .slot_active_bg = 0x7b7d93,                                                            \
        .slot_inactive_bg = 0x353640,                                                          \
        .slot_text = 0xffffff,                                                                 \
        .texture = 0x1e2122,                                                                   \
        .texture_faint = 0x111213,                                                             \
    }

#define OPERATOR_PALETTE_EMBER                                                                 \
    {                                                                                          \
        .name = "Ember",                                                                       \
        .bg = 0x000000,                                                                        \
        .label = 0x8f7a69,                                                                     \
        .value = 0xffcf99,                                                                     \
        .balance_track = 0x3a2a20,                                                             \
        .balance_tick = 0x8f7a69,                                                              \
        .balance_marker = 0xff9f43,                                                            \
        .mod_row_active = 0xffcf99,                                                            \
        .mod_row_inactive = 0x3a3129,                                                          \
        .caps = 0xffd166,                                                                      \
        .brightness = 0xffd166,                                                                \
        .wpm_peak = 0x554f49,                                                                  \
        .battery_number = 0x000000,                                                            \
        .slot_active_text = 0x000000,                                                          \
        .slot_inactive_text = 0x000000,                                                        \
        .usb_icon = 0x000000,                                                                  \
        .mod_active = 0xffcf99,                                                                \
        .mod_inactive = 0x3a3129,                                                              \
        .mod_separator = 0x8f7a69,                                                             \
        .mod_caps_word = 0xffd166,                                                             \
        .wpm_bar_active = 0xff6b35,                                                            \
        .wpm_bar_inactive = 0x24201d,                                                          \
        .wpm_text = 0xff6b35,                                                                  \
        .layer_text = 0xfff1e2,                                                                \
        .layer_dot_active = 0xecded0,                                                          \
        .layer_dot_inactive = 0x54473d,                                                        \
        .battery_fill = 0x9cc069,                                                              \
        .battery_ring = 0x3e4929,                                                              \
        .battery_bg = 0x554f49,                                                                \
        .battery_label = 0xfff1e2,                                                             \
        .battery_disconnected_fill = 0x332b24,                                                 \
        .battery_disconnected_ring = 0x201a16,                                                 \
        .battery_disconnected_label = 0x000000,                                                \
        .battery_low_fill = 0xe63946,                                                          \
        .battery_low_ring = 0x581a1d,                                                          \
        .usb_active_bg = 0xe9c46a,                                                             \
        .usb_inactive_bg = 0x433820,                                                           \
        .ble_active_bg = 0xf4a261,                                                             \
        .ble_inactive_bg = 0x462f1e,                                                           \
        .output_active_text = 0x000000,                                                        \
        .output_inactive_text = 0xa0826a,                                                      \
        .slot_active_bg = 0xa0826a,                                                            \
        .slot_inactive_bg = 0x382d25,                                                          \
        .slot_text = 0xfff1e2,                                                                 \
        .texture = 0x29221d,                                                                   \
        .texture_faint = 0x1c1713,                                                             \
    }

#define OPERATOR_PALETTE_GLACIER                                                               \
    {                                                                                          \
        .name = "Glacier",                                                                     \
        .bg = 0x000000,                                                                        \
        .label = 0x7890a6,                                                                     \
        .value = 0xc8f1ff,                                                                     \
        .balance_track = 0x1f2b3a,                                                             \
        .balance_tick = 0x7890a6,                                                              \
        .balance_marker = 0x8be0ff,                                                            \
        .mod_row_active = 0xc8f1ff,                                                            \
        .mod_row_inactive = 0x2d3843,                                                          \
        .caps = 0xfde68a,                                                                      \
        .brightness = 0xfde68a,                                                                \
        .wpm_peak = 0x4b5056,                                                                  \
        .battery_number = 0x000000,                                                            \
        .slot_active_text = 0x000000,                                                          \
        .slot_inactive_text = 0x000000,                                                        \
        .usb_icon = 0x000000,                                                                  \
        .mod_active = 0xc8f1ff,                                                                \
        .mod_inactive = 0x2d3843,                                                              \
        .mod_separator = 0x7890a6,                                                             \
        .mod_caps_word = 0xfde68a,                                                             \
        .wpm_bar_active = 0x5ec8ff,                                                            \
        .wpm_bar_inactive = 0x1c2025,                                                          \
        .wpm_text = 0x5ec8ff,                                                                  \
        .layer_text = 0xeef7ff,                                                                \
        .layer_dot_active = 0xdbe4ec,                                                          \
        .layer_dot_inactive = 0x445361,                                                        \
        .battery_fill = 0x6ee7b7,                                                              \
        .battery_ring = 0x2a5648,                                                              \
        .battery_bg = 0x4b5056,                                                                \
        .battery_label = 0xeef7ff,                                                             \
        .battery_disconnected_fill = 0x28313b,                                                 \
        .battery_disconnected_ring = 0x161c24,                                                 \
        .battery_disconnected_label = 0x000000,                                                \
        .battery_low_fill = 0xff8a65,                                                          \
        .battery_low_ring = 0x5c362c,                                                          \
        .usb_active_bg = 0xcbd5e1,                                                             \
        .usb_inactive_bg = 0x363b42,                                                           \
        .ble_active_bg = 0x38bdf8,                                                             \
        .ble_inactive_bg = 0x123548,                                                           \
        .output_active_text = 0x000000,                                                        \
        .output_inactive_text = 0x64748b,                                                      \
        .slot_active_bg = 0x64748b,                                                            \
        .slot_inactive_bg = 0x222833,                                                          \
        .slot_text = 0xeef7ff,                                                                 \
        .texture = 0x1e262f,                                                                   \
        .texture_faint = 0x13181f,                                                             \
    }

#define OPERATOR_PALETTE_SYNTHWAVE                                                             \
    {                                                                                          \
        .name = "Synthwave",                                                                   \
        .bg = 0x000000,                                                                        \
        .label = 0x8a7fa8,                                                                     \
        .value = 0x05d9e8,                                                                     \
        .balance_track = 0x2e2148,                                                             \
        .balance_tick = 0x8a7fa8,                                                              \
        .balance_marker = 0x01fff4,                                                            \
        .mod_row_active = 0x05d9e8,                                                            \
        .mod_row_inactive = 0x373149,                                                          \
        .caps = 0xffd319,                                                                      \
        .brightness = 0xffd319,                                                                \
        .wpm_peak = 0x514b5c,                                                                  \
        .battery_number = 0x000000,                                                            \
        .slot_active_text = 0xf5e9ff,                                                          \
        .slot_inactive_text = 0x000000,                                                        \
        .usb_icon = 0x000000,                                                                  \
        .mod_active = 0x05d9e8,                                                                \
        .mod_inactive = 0x373149,                                                              \
        .mod_separator = 0x8a7fa8,                                                             \
        .mod_caps_word = 0xffd319,                                                             \
        .wpm_bar_active = 0xff2a6d,                                                            \
        .wpm_bar_inactive = 0x221e2d,                                                          \
        .wpm_text = 0xff2a6d,                                                                  \
        .layer_text = 0xf5e9ff,                                                                \
        .layer_dot_active = 0xe2d7ec,                                                          \
        .layer_dot_inactive = 0x514966,                                                        \
        .battery_fill = 0x4ade80,                                                              \
        .battery_ring = 0x21523b,                                                              \
        .battery_bg = 0x514b5c,                                                                \
        .battery_label = 0xf5e9ff,                                                             \
        .battery_disconnected_fill = 0x312b42,                                                 \
        .battery_disconnected_ring = 0x1e192c,                                                 \
        .battery_disconnected_label = 0x000000,                                                \
        .battery_low_fill = 0xffb000,                                                          \
        .battery_low_ring = 0x60420e,                                                          \
        .usb_active_bg = 0xd1f7ff,                                                             \
        .usb_inactive_bg = 0x3c4350,                                                           \
        .ble_active_bg = 0xb967ff,                                                             \
        .ble_inactive_bg = 0x361f50,                                                           \
        .output_active_text = 0x000000,                                                        \
        .output_inactive_text = 0x6b5b95,                                                      \
        .slot_active_bg = 0x6b5b95,                                                            \
        .slot_inactive_bg = 0x28203c,                                                          \
        .slot_text = 0xf5e9ff,                                                                 \
        .texture = 0x272136,                                                                   \
        .texture_faint = 0x1a1528,                                                             \
    }

#define OPERATOR_PALETTE_SEA_SHELLS                                                            \
    {                                                                                          \
        .name = "Sea Shells",                                                                  \
        .bg = 0x000000,                                                                        \
        .label = 0x628d98,                                                                     \
        .value = 0xfdd39f,                                                                     \
        .balance_track = 0x17384c,                                                             \
        .balance_tick = 0x628d98,                                                              \
        .balance_marker = 0xfca02f,                                                            \
        .mod_row_active = 0xfdd39f,                                                            \
        .mod_row_inactive = 0x283e47,                                                          \
        .caps = 0xfca02f,                                                                      \
        .brightness = 0xfdd39f,                                                                \
        .wpm_peak = 0x525251,                                                                  \
        .battery_number = 0x000000,                                                            \
        .slot_active_text = 0x000000,                                                          \
        .slot_inactive_text = 0x000000,                                                        \
        .usb_icon = 0x000000,                                                                  \
        .mod_active = 0xfdd39f,                                                                \
        .mod_inactive = 0x283e47,                                                              \
        .mod_separator = 0x628d98,                                                             \
        .mod_caps_word = 0xfca02f,                                                             \
        .wpm_bar_active = 0xd15123,                                                            \
        .wpm_bar_inactive = 0x22292d,                                                          \
        .wpm_text = 0xd15123,                                                                  \
        .layer_text = 0xfee4ce,                                                                \
        .layer_dot_active = 0xead3c0,                                                          \
        .layer_dot_inactive = 0x3a5760,                                                        \
        .battery_fill = 0x50a3b5,                                                              \
        .battery_ring = 0x224651,                                                              \
        .battery_bg = 0x525251,                                                                \
        .battery_label = 0xfee4ce,                                                             \
        .battery_disconnected_fill = 0x243840,                                                 \
        .battery_disconnected_ring = 0x16262e,                                                 \
        .battery_disconnected_label = 0x000000,                                                \
        .battery_low_fill = 0xd48678,                                                          \
        .battery_low_ring = 0x503c3c,                                                          \
        .usb_active_bg = 0xdeb88d,                                                             \
        .usb_inactive_bg = 0x3e3d38,                                                           \
        .ble_active_bg = 0x1bbcdd,                                                             \
        .ble_inactive_bg = 0x0e3e4c,                                                           \
        .output_active_text = 0x000000,                                                        \
        .output_inactive_text = 0x628d98,                                                      \
        .slot_active_bg = 0x628d98,                                                            \
        .slot_inactive_bg = 0x243840,                                                          \
        .slot_text = 0xfee4ce,                                                                 \
        .texture = 0x1d2f36,                                                                   \
        .texture_faint = 0x14232a,                                                             \
    }

#define OPERATOR_PALETTE_GITHUB                                                                \
    {                                                                                          \
        .name = "GitHub",                                                                      \
        .bg = 0x000000,                                                                        \
        .label = 0x9ea7b3,                                                                     \
        .value = 0x71b7ff,                                                                     \
        .balance_track = 0x3a404b,                                                             \
        .balance_tick = 0x9ea7b3,                                                              \
        .balance_marker = 0x91cbff,                                                            \
        .mod_row_active = 0x71b7ff,                                                            \
        .mod_row_inactive = 0x3e4249,                                                          \
        .caps = 0xf0b72f,                                                                      \
        .brightness = 0xf7c843,                                                                \
        .wpm_peak = 0x4f5155,                                                                  \
        .battery_number = 0x000000,                                                            \
        .slot_active_text = 0xf0f3f6,                                                          \
        .slot_inactive_text = 0x000000,                                                        \
        .usb_icon = 0x000000,                                                                  \
        .mod_active = 0x71b7ff,                                                                \
        .mod_inactive = 0x3e4249,                                                              \
        .mod_separator = 0x9ea7b3,                                                             \
        .mod_caps_word = 0xf0b72f,                                                             \
        .wpm_bar_active = 0x26cd4d,                                                            \
        .wpm_bar_inactive = 0x212327,                                                          \
        .wpm_text = 0x26cd4d,                                                                  \
        .layer_text = 0xf0f3f6,                                                                \
        .layer_dot_active = 0xdee1e4,                                                          \
        .layer_dot_inactive = 0x5b616a,                                                        \
        .battery_fill = 0x4ae168,                                                              \
        .battery_ring = 0x20572f,                                                              \
        .battery_bg = 0x4f5155,                                                                \
        .battery_label = 0xf0f3f6,                                                             \
        .battery_disconnected_fill = 0x363a41,                                                 \
        .battery_disconnected_ring = 0x202328,                                                 \
        .battery_disconnected_label = 0x000000,                                                \
        .battery_low_fill = 0xff9492,                                                          \
        .battery_low_ring = 0x603c3e,                                                          \
        .usb_active_bg = 0xd9dee3,                                                             \
        .usb_inactive_bg = 0x3e4045,                                                           \
        .ble_active_bg = 0x71b7ff,                                                             \
        .ble_inactive_bg = 0x24374c,                                                           \
        .output_active_text = 0x000000,                                                        \
        .output_inactive_text = 0x2f6fd0,                                                      \
        .slot_active_bg = 0x2f6fd0,                                                            \
        .slot_inactive_bg = 0x152a4a,                                                          \
        .slot_text = 0xf0f3f6,                                                                 \
        .texture = 0x2b2e34,                                                                   \
        .texture_faint = 0x1c1f24,                                                             \
    }

const struct operator_palette *operator_theme_current(void);

#define DISPLAY_COLOR_BG (operator_theme_current()->bg)
#define DISPLAY_COLOR_LABEL (operator_theme_current()->label)
#define DISPLAY_COLOR_VALUE (operator_theme_current()->value)
#define DISPLAY_COLOR_BALANCE_TRACK (operator_theme_current()->balance_track)
#define DISPLAY_COLOR_BALANCE_TICK (operator_theme_current()->balance_tick)
#define DISPLAY_COLOR_BALANCE_MARKER (operator_theme_current()->balance_marker)
#define DISPLAY_COLOR_MOD_ROW_ACTIVE (operator_theme_current()->mod_row_active)
#define DISPLAY_COLOR_MOD_ROW_INACTIVE (operator_theme_current()->mod_row_inactive)
#define DISPLAY_COLOR_CAPS (operator_theme_current()->caps)
#define DISPLAY_COLOR_BRIGHTNESS (operator_theme_current()->brightness)
#define DISPLAY_COLOR_WPM_PEAK (operator_theme_current()->wpm_peak)
#define DISPLAY_COLOR_BATTERY_NUMBER (operator_theme_current()->battery_number)
#define DISPLAY_COLOR_SLOT_ACTIVE_TEXT (operator_theme_current()->slot_active_text)
#define DISPLAY_COLOR_SLOT_INACTIVE_TEXT (operator_theme_current()->slot_inactive_text)
#define DISPLAY_COLOR_USB_ICON (operator_theme_current()->usb_icon)
#define DISPLAY_COLOR_MOD_ACTIVE (operator_theme_current()->mod_active)
#define DISPLAY_COLOR_MOD_INACTIVE (operator_theme_current()->mod_inactive)
#define DISPLAY_COLOR_MOD_SEPARATOR (operator_theme_current()->mod_separator)
#define DISPLAY_COLOR_MOD_CAPS_WORD (operator_theme_current()->mod_caps_word)
#define DISPLAY_COLOR_WPM_BAR_ACTIVE (operator_theme_current()->wpm_bar_active)
#define DISPLAY_COLOR_WPM_BAR_INACTIVE (operator_theme_current()->wpm_bar_inactive)
#define DISPLAY_COLOR_WPM_TEXT (operator_theme_current()->wpm_text)
#define DISPLAY_COLOR_LAYER_TEXT (operator_theme_current()->layer_text)
#define DISPLAY_COLOR_LAYER_DOT_ACTIVE (operator_theme_current()->layer_dot_active)
#define DISPLAY_COLOR_LAYER_DOT_INACTIVE (operator_theme_current()->layer_dot_inactive)
#define DISPLAY_COLOR_BATTERY_FILL (operator_theme_current()->battery_fill)
#define DISPLAY_COLOR_BATTERY_RING (operator_theme_current()->battery_ring)
#define DISPLAY_COLOR_BATTERY_BG (operator_theme_current()->battery_bg)
#define DISPLAY_COLOR_BATTERY_LABEL (operator_theme_current()->battery_label)
#define DISPLAY_COLOR_BATTERY_DISCONNECTED_FILL \
    (operator_theme_current()->battery_disconnected_fill)
#define DISPLAY_COLOR_BATTERY_DISCONNECTED_RING \
    (operator_theme_current()->battery_disconnected_ring)
#define DISPLAY_COLOR_BATTERY_DISCONNECTED_LABEL \
    (operator_theme_current()->battery_disconnected_label)
#define DISPLAY_COLOR_BATTERY_LOW_FILL (operator_theme_current()->battery_low_fill)
#define DISPLAY_COLOR_BATTERY_LOW_RING (operator_theme_current()->battery_low_ring)
#define DISPLAY_COLOR_USB_ACTIVE_BG (operator_theme_current()->usb_active_bg)
#define DISPLAY_COLOR_USB_INACTIVE_BG (operator_theme_current()->usb_inactive_bg)
#define DISPLAY_COLOR_BLE_ACTIVE_BG (operator_theme_current()->ble_active_bg)
#define DISPLAY_COLOR_BLE_INACTIVE_BG (operator_theme_current()->ble_inactive_bg)
#define DISPLAY_COLOR_OUTPUT_ACTIVE_TEXT (operator_theme_current()->output_active_text)
#define DISPLAY_COLOR_OUTPUT_INACTIVE_TEXT (operator_theme_current()->output_inactive_text)
#define DISPLAY_COLOR_SLOT_ACTIVE_BG (operator_theme_current()->slot_active_bg)
#define DISPLAY_COLOR_SLOT_INACTIVE_BG (operator_theme_current()->slot_inactive_bg)
#define DISPLAY_COLOR_SLOT_TEXT (operator_theme_current()->slot_text)
#define DISPLAY_COLOR_TEXTURE (operator_theme_current()->texture)
#define DISPLAY_COLOR_TEXTURE_FAINT (operator_theme_current()->texture_faint)

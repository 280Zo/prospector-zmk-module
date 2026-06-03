/* SPDX-License-Identifier: MIT */

#include "avatar_page.h"

#if defined(CONFIG_PROSPECTOR_AVATAR_USE_PIXEL_ART)
#include "images/avatar_pixel_art.h"
#endif

#if defined(CONFIG_PROSPECTOR_AVATAR_USE_IMAGE)
#include "images/avatar_knight_scene.h"
#endif

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#include <zephyr/kernel.h>
#include <zephyr/sys/atomic.h>
#include <zephyr/sys/util.h>
#include <zmk/display.h>
#include <zmk/event_manager.h>
#include <zmk/events/wpm_state_changed.h>
#include <zmk/wpm.h>

#if LV_FONT_MONTSERRAT_14
#define AVATAR_FONT (&lv_font_montserrat_14)
#else
#define AVATAR_FONT LV_FONT_DEFAULT
#endif

#define AVATAR_FAST_WPM 45
#define AVATAR_DIZZY_SWIPES 5
#define AVATAR_SWIPE_STREAK_MS 2500
#define AVATAR_DIZZY_MS 4000
#define AVATAR_IMAGE_X 52
#define AVATAR_IMAGE_Y 43
#define AVATAR_PIXEL_ART_SCALE 2
#define AVATAR_PIXEL_ART_X 8
#define AVATAR_PIXEL_ART_Y 52

enum avatar_mood {
    AVATAR_MOOD_SLEEP,
    AVATAR_MOOD_GUARD,
    AVATAR_MOOD_CHARGE,
    AVATAR_MOOD_DIZZY,
};

static lv_obj_t *avatar_image;
static lv_obj_t *avatar_page_obj;
static lv_obj_t *avatar_status_label;
static lv_obj_t *avatar_wpm_label;
static lv_obj_t *sleep_bubble;
static lv_obj_t *sleep_bubble_small;
static lv_obj_t *sleep_bubble_tiny;
static lv_obj_t *spark_1;
static lv_obj_t *spark_2;
static lv_obj_t *spark_3;

#if defined(CONFIG_PROSPECTOR_AVATAR_USE_PIXEL_ART)
static uint8_t avatar_pixel_art_frame;
#endif

static atomic_t avatar_visible;
static atomic_t avatar_wpm;
static atomic_t avatar_swipe_streak;
static int64_t last_swipe_at;
static int64_t dizzy_until;
static uint8_t frame;

static void avatar_tick_work_handler(struct k_work *work);
static void avatar_render_work_handler(struct k_work *work);

K_WORK_DELAYABLE_DEFINE(avatar_tick_work, avatar_tick_work_handler);
K_WORK_DEFINE(avatar_render_work, avatar_render_work_handler);

static lv_obj_t *create_block(lv_obj_t *parent, int x, int y, int width, int height,
                              lv_color_t color) {
    lv_obj_t *block = lv_obj_create(parent);
    lv_obj_clear_flag(block, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_pos(block, x, y);
    lv_obj_set_size(block, width, height);
    lv_obj_set_style_bg_color(block, color, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(block, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(block, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(block, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(block, 0, LV_PART_MAIN);
    return block;
}

#if defined(CONFIG_PROSPECTOR_AVATAR_USE_PIXEL_ART)
static uint8_t avatar_pixel_art_get_px(uint16_t x, uint16_t y) {
    size_t frame_offset = (size_t)avatar_pixel_art_frame * AVATAR_PIXEL_ART_FRAME_DATA_SIZE * 2U;
    size_t index = frame_offset + (size_t)y * AVATAR_PIXEL_ART_WIDTH + x;
    uint8_t packed = avatar_pixel_data[index / 2U];

    return (index & 1U) == 0U ? packed >> 4 : packed & 0x0f;
}

static void avatar_pixel_art_draw_cb(lv_event_t *e) {
    lv_obj_t *obj = lv_event_get_target(e);
    lv_layer_t *layer = lv_event_get_layer(e);

    lv_area_t obj_coords;
    lv_obj_get_coords(obj, &obj_coords);

    lv_draw_rect_dsc_t rect_dsc;
    lv_draw_rect_dsc_init(&rect_dsc);
    rect_dsc.bg_opa = LV_OPA_COVER;
    rect_dsc.border_width = 0;
    rect_dsc.radius = 0;

    for (uint16_t y = 0; y < AVATAR_PIXEL_ART_HEIGHT; y++) {
        uint16_t run_x = 0;
        uint8_t run_color = AVATAR_PIXEL_ART_TRANSPARENT;

        for (uint16_t x = 0; x <= AVATAR_PIXEL_ART_WIDTH; x++) {
            uint8_t color = x < AVATAR_PIXEL_ART_WIDTH
                                ? avatar_pixel_art_get_px(x, y)
                                : AVATAR_PIXEL_ART_TRANSPARENT;

            if (x == 0) {
                run_x = 0;
                run_color = color;
                continue;
            }

            if (color == run_color) {
                continue;
            }

            if (run_color != AVATAR_PIXEL_ART_TRANSPARENT) {
                lv_area_t area = {
                    .x1 = obj_coords.x1 + run_x * AVATAR_PIXEL_ART_SCALE,
                    .y1 = obj_coords.y1 + y * AVATAR_PIXEL_ART_SCALE,
                    .x2 = obj_coords.x1 + x * AVATAR_PIXEL_ART_SCALE - 1,
                    .y2 = obj_coords.y1 + (y + 1) * AVATAR_PIXEL_ART_SCALE - 1,
                };

                rect_dsc.bg_color = lv_color_hex(avatar_pixel_palette[run_color]);
                lv_draw_rect(layer, &rect_dsc, &area);
            }

            run_x = x;
            run_color = color;
        }
    }
}

static lv_obj_t *create_avatar_primitive(lv_obj_t *parent) {
    lv_obj_t *scene = lv_obj_create(parent);
    lv_obj_remove_style_all(scene);
    lv_obj_clear_flag(scene, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(scene, AVATAR_PIXEL_ART_WIDTH * AVATAR_PIXEL_ART_SCALE,
                    AVATAR_PIXEL_ART_HEIGHT * AVATAR_PIXEL_ART_SCALE);
    lv_obj_set_style_bg_opa(scene, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_add_event_cb(scene, avatar_pixel_art_draw_cb, LV_EVENT_DRAW_MAIN, NULL);

    return scene;
}
#else
static lv_obj_t *create_avatar_primitive(lv_obj_t *parent) {
    lv_obj_t *scene = lv_obj_create(parent);
    lv_obj_clear_flag(scene, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(scene, 176, 154);
    lv_obj_set_style_bg_opa(scene, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(scene, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(scene, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(scene, 0, LV_PART_MAIN);

    create_block(scene, 39, 0, 96, 5, lv_color_hex(0x2c1f55));
    create_block(scene, 23, 5, 128, 7, lv_color_hex(0x251844));
    create_block(scene, 14, 12, 146, 12, lv_color_hex(0x1a1230));
    create_block(scene, 8, 24, 158, 89, lv_color_hex(0x120c23));
    create_block(scene, 31, 125, 111, 8, lv_color_hex(0x281b49));
    create_block(scene, 50, 133, 72, 5, lv_color_hex(0x33215c));

    create_block(scene, 22, 24, 6, 65, lv_color_hex(0xe8dcc4));
    create_block(scene, 28, 17, 12, 74, lv_color_hex(0xf7ead0));
    create_block(scene, 40, 25, 8, 63, lv_color_hex(0xd0b68c));
    create_block(scene, 18, 88, 42, 8, lv_color_hex(0x5b412c));
    create_block(scene, 10, 94, 34, 8, lv_color_hex(0xb17736));

    create_block(scene, 53, 29, 59, 19, lv_color_hex(0x252b3a));
    create_block(scene, 61, 17, 51, 15, lv_color_hex(0x596071));
    create_block(scene, 58, 47, 52, 25, lv_color_hex(0x0f1420));
    create_block(scene, 67, 55, 8, 12, lv_color_hex(0xf4e759));
    create_block(scene, 79, 55, 8, 12, lv_color_hex(0x44f2c8));
    create_block(scene, 91, 55, 8, 12, lv_color_hex(0xc855ff));
    create_block(scene, 108, 29, 13, 31, lv_color_hex(0x4f3375));

    create_block(scene, 52, 73, 58, 28, lv_color_hex(0x172030));
    create_block(scene, 57, 78, 45, 19, lv_color_hex(0x263348));
    create_block(scene, 111, 71, 43, 17, lv_color_hex(0x5a237a));
    create_block(scene, 132, 83, 42, 14, lv_color_hex(0x6d2d8e));
    create_block(scene, 74, 100, 16, 27, lv_color_hex(0x121a29));
    create_block(scene, 96, 100, 16, 27, lv_color_hex(0x121a29));
    create_block(scene, 55, 127, 36, 9, lv_color_hex(0x2b3142));
    create_block(scene, 91, 127, 41, 9, lv_color_hex(0x2b3142));

    create_block(scene, 100, 54, 28, 28, lv_color_hex(0x433126));
    create_block(scene, 105, 59, 18, 18, lv_color_hex(0x9d6b38));
    create_block(scene, 81, 84, 27, 27, lv_color_hex(0x111827));
    create_block(scene, 101, 84, 27, 28, lv_color_hex(0x101724));
    create_block(scene, 122, 84, 28, 27, lv_color_hex(0x0f1622));

    create_block(scene, 36, 42, 5, 13, lv_color_hex(0xffd95d));
    create_block(scene, 33, 45, 11, 9, lv_color_hex(0xf47a2d));
    create_block(scene, 156, 35, 5, 15, lv_color_hex(0xffd95d));
    create_block(scene, 153, 38, 11, 10, lv_color_hex(0xf47a2d));

    return scene;
}
#endif

static bool ensure_avatar_image(void) {
    if (avatar_image != NULL) {
        return true;
    }

#if defined(CONFIG_PROSPECTOR_AVATAR_USE_IMAGE)
    avatar_image = lv_image_create(avatar_page_obj);
    if (avatar_image == NULL) {
        return false;
    }

    lv_image_set_src(avatar_image, &avatar_knight_scene);
#else
    avatar_image = create_avatar_primitive(avatar_page_obj);
    if (avatar_image == NULL) {
        return false;
    }
#endif

#if defined(CONFIG_PROSPECTOR_AVATAR_USE_PIXEL_ART) && !defined(CONFIG_PROSPECTOR_AVATAR_USE_IMAGE)
    lv_obj_set_pos(avatar_image, AVATAR_PIXEL_ART_X, AVATAR_PIXEL_ART_Y);
#else
    lv_obj_set_pos(avatar_image, AVATAR_IMAGE_X, AVATAR_IMAGE_Y);
#endif
    lv_obj_move_background(avatar_image);
    return true;
}

static lv_obj_t *create_label(lv_obj_t *parent, const char *text, int x, int y,
                              lv_color_t color) {
    lv_obj_t *label = lv_label_create(parent);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_font(label, AVATAR_FONT, LV_PART_MAIN);
    lv_obj_set_style_text_color(label, color, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(label, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_pos(label, x, y);
    return label;
}

static void set_hidden(lv_obj_t *obj, bool hidden) {
    if (hidden) {
        lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_clear_flag(obj, LV_OBJ_FLAG_HIDDEN);
    }
}

static lv_obj_t *create_round_bubble(lv_obj_t *parent, int x, int y, int size) {
    lv_obj_t *bubble = lv_obj_create(parent);
    lv_obj_clear_flag(bubble, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_pos(bubble, x, y);
    lv_obj_set_size(bubble, size, size);
    lv_obj_set_style_bg_color(bubble, lv_color_hex(0x111827), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(bubble, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_color(bubble, lv_color_hex(0x8ac6ff), LV_PART_MAIN);
    lv_obj_set_style_border_width(bubble, 2, LV_PART_MAIN);
    lv_obj_set_style_radius(bubble, size / 2, LV_PART_MAIN);
    lv_obj_set_style_pad_all(bubble, 0, LV_PART_MAIN);
    return bubble;
}

static enum avatar_mood get_avatar_mood(int64_t now, int wpm) {
    if (now < dizzy_until) {
        return AVATAR_MOOD_DIZZY;
    }

    if (wpm == 0) {
        return AVATAR_MOOD_SLEEP;
    }

    if (wpm >= AVATAR_FAST_WPM) {
        return AVATAR_MOOD_CHARGE;
    }

    return AVATAR_MOOD_GUARD;
}

static void render_avatar(void) {
    if (!ensure_avatar_image()) {
        return;
    }

    int wpm = atomic_get(&avatar_wpm);
    enum avatar_mood mood = get_avatar_mood(k_uptime_get(), wpm);
    bool step = (frame & 1U) != 0U;
    const char *status = "guard";
    int dx = 0;
    int dy = 0;

#if defined(CONFIG_PROSPECTOR_AVATAR_USE_PIXEL_ART)
    avatar_pixel_art_frame = 0;
#endif

    if (mood == AVATAR_MOOD_SLEEP) {
        status = "sleep";
    } else if (mood == AVATAR_MOOD_GUARD) {
        dy = step ? 1 : 0;
    } else if (mood == AVATAR_MOOD_CHARGE) {
        status = "charge";
#if defined(CONFIG_PROSPECTOR_AVATAR_USE_PIXEL_ART) && !defined(CONFIG_PROSPECTOR_AVATAR_USE_IMAGE)
        avatar_pixel_art_frame = 1U + (frame % (AVATAR_PIXEL_ART_FRAME_COUNT - 1U));
#else
        dx = step ? 2 : 0;
#endif
    } else if (mood == AVATAR_MOOD_DIZZY) {
        status = "dizzy";
        dx = step ? 4 : -4;
        dy = step ? -1 : 1;
    }

#if defined(CONFIG_PROSPECTOR_AVATAR_USE_PIXEL_ART) && !defined(CONFIG_PROSPECTOR_AVATAR_USE_IMAGE)
    lv_obj_set_pos(avatar_image, AVATAR_PIXEL_ART_X + dx, AVATAR_PIXEL_ART_Y + dy);
    lv_obj_invalidate(avatar_image);
#else
    lv_obj_set_pos(avatar_image, AVATAR_IMAGE_X + dx, AVATAR_IMAGE_Y + dy);
#endif

    char wpm_text[12];
    snprintk(wpm_text, sizeof(wpm_text), "%d wpm", wpm);
    lv_label_set_text(avatar_status_label, status);
    lv_label_set_text(avatar_wpm_label, wpm_text);
    lv_obj_align(avatar_wpm_label, LV_ALIGN_TOP_RIGHT, -10, 6);

    bool sparks_hidden = mood != AVATAR_MOOD_DIZZY;
    set_hidden(sleep_bubble, mood != AVATAR_MOOD_SLEEP);
    set_hidden(sleep_bubble_small, mood != AVATAR_MOOD_SLEEP);
    set_hidden(sleep_bubble_tiny, mood != AVATAR_MOOD_SLEEP);
    set_hidden(spark_1, sparks_hidden);
    set_hidden(spark_2, sparks_hidden);
    set_hidden(spark_3, sparks_hidden);
}

static void avatar_tick_work_handler(struct k_work *work) {
    ARG_UNUSED(work);

    if (atomic_get(&avatar_visible) && zmk_display_is_initialized()) {
        k_work_submit_to_queue(zmk_display_work_q(), &avatar_render_work);
    }
}

static void avatar_render_work_handler(struct k_work *work) {
    ARG_UNUSED(work);

    if (!atomic_get(&avatar_visible)) {
        return;
    }

    frame++;
    render_avatar();

    int wpm = atomic_get(&avatar_wpm);
    enum avatar_mood mood = get_avatar_mood(k_uptime_get(), wpm);
    k_timeout_t delay = mood == AVATAR_MOOD_CHARGE || mood == AVATAR_MOOD_DIZZY ? K_MSEC(180)
                                                                                : K_MSEC(700);

    k_work_schedule(&avatar_tick_work, delay);
}

static int avatar_wpm_state_changed_listener(const zmk_event_t *eh) {
    const struct zmk_wpm_state_changed *ev = as_zmk_wpm_state_changed(eh);

    if (ev == NULL || ev->state < 0 || ev->state > 300) {
        return ZMK_EV_EVENT_BUBBLE;
    }

    atomic_set(&avatar_wpm, ev->state);

    if (atomic_get(&avatar_visible) && zmk_display_is_initialized()) {
        k_work_submit_to_queue(zmk_display_work_q(), &avatar_render_work);
    }

    return ZMK_EV_EVENT_BUBBLE;
}

ZMK_LISTENER(prospector_avatar_wpm, avatar_wpm_state_changed_listener);
ZMK_SUBSCRIPTION(prospector_avatar_wpm, zmk_wpm_state_changed);

lv_obj_t *prospector_avatar_page_create(lv_obj_t *parent) {
    lv_obj_t *page = lv_obj_create(parent);
    avatar_page_obj = page;

    lv_obj_clear_flag(page, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(page, lv_pct(100), lv_pct(100));
    lv_obj_set_pos(page, 0, 0);
    lv_obj_set_style_bg_color(page, lv_color_hex(0x05060d), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(page, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(page, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(page, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(page, 0, LV_PART_MAIN);

    avatar_status_label = create_label(page, "sleep", 14, 6, lv_color_hex(0x8ac6ff));
    avatar_wpm_label = create_label(page, "0 wpm", 0, 6, lv_color_hex(0xffffff));
    lv_obj_align(avatar_wpm_label, LV_ALIGN_TOP_RIGHT, -10, 6);

    sleep_bubble = lv_obj_create(page);
    lv_obj_clear_flag(sleep_bubble, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_pos(sleep_bubble, 116, 34);
    lv_obj_set_size(sleep_bubble, 48, 26);
    lv_obj_set_style_bg_color(sleep_bubble, lv_color_hex(0x111827), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(sleep_bubble, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_color(sleep_bubble, lv_color_hex(0x8ac6ff), LV_PART_MAIN);
    lv_obj_set_style_border_width(sleep_bubble, 2, LV_PART_MAIN);
    lv_obj_set_style_radius(sleep_bubble, 13, LV_PART_MAIN);
    lv_obj_set_style_pad_all(sleep_bubble, 0, LV_PART_MAIN);
    create_label(sleep_bubble, "Zzz", 8, 2, lv_color_hex(0xffffff));
    sleep_bubble_small = create_round_bubble(page, 154, 58, 14);
    sleep_bubble_tiny = create_round_bubble(page, 144, 74, 9);

    spark_1 = create_block(page, 76, 38, 7, 7, lv_color_hex(0xffd84a));
    spark_2 = create_block(page, 220, 62, 7, 7, lv_color_hex(0xffd84a));
    spark_3 = create_block(page, 232, 144, 7, 7, lv_color_hex(0xffd84a));
    set_hidden(spark_1, true);
    set_hidden(spark_2, true);
    set_hidden(spark_3, true);
    set_hidden(sleep_bubble, true);
    set_hidden(sleep_bubble_small, true);
    set_hidden(sleep_bubble_tiny, true);

    atomic_set(&avatar_wpm, zmk_wpm_get_state());
    lv_obj_add_flag(page, LV_OBJ_FLAG_HIDDEN);

    return page;
}

void prospector_avatar_page_set_visible(bool visible) {
    atomic_set(&avatar_visible, visible);

    if (visible) {
        if (zmk_display_is_initialized()) {
            k_work_submit_to_queue(zmk_display_work_q(), &avatar_render_work);
        }
    } else {
        k_work_cancel_delayable(&avatar_tick_work);
    }
}

void prospector_avatar_page_note_swipe(void) {
    int64_t now = k_uptime_get();

    if (now - last_swipe_at > AVATAR_SWIPE_STREAK_MS) {
        atomic_set(&avatar_swipe_streak, 1);
    } else {
        atomic_inc(&avatar_swipe_streak);
    }

    last_swipe_at = now;

    if (atomic_get(&avatar_swipe_streak) >= AVATAR_DIZZY_SWIPES) {
        dizzy_until = now + AVATAR_DIZZY_MS;
    }

    if (atomic_get(&avatar_visible) && zmk_display_is_initialized()) {
        k_work_submit_to_queue(zmk_display_work_q(), &avatar_render_work);
    }
}

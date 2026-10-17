/* SPDX-License-Identifier: MIT */
#include "background_texture.h"
#include "display_colors.h"

#if defined(CONFIG_PROSPECTOR_OPERATOR_TEXTURE_DOTS) ||                                            \
    defined(CONFIG_PROSPECTOR_OPERATOR_TEXTURE_FINE_GRID) ||                                       \
    defined(CONFIG_PROSPECTOR_OPERATOR_TEXTURE_HATCH)
#define TEXTURE_DRAWN 1
#endif

#if defined(TEXTURE_DRAWN)

/* First value >= min on the grid offset + k * pitch. */
static int32_t first_on_grid(int32_t min, int32_t offset, int32_t pitch) {
    int32_t r = (offset - min) % pitch;
    return min + (r < 0 ? r + pitch : r);
}

#if !defined(CONFIG_PROSPECTOR_OPERATOR_TEXTURE_HATCH)
static void fill(lv_layer_t *layer, lv_draw_rect_dsc_t *dsc, int32_t x1, int32_t y1, int32_t x2,
                 int32_t y2) {
    lv_area_t area = {x1, y1, x2, y2};
    lv_draw_rect(layer, dsc, &area);
}
#endif

/* Drawn after the screen background and before its children; only the area being
 * redrawn is visited, so small widget updates stay cheap. */
static void texture_draw_cb(lv_event_t *e) {
    lv_obj_t *screen = lv_event_get_target(e);
    lv_layer_t *layer = lv_event_get_layer(e);
    lv_area_t coords, clip = layer->_clip_area;
    lv_obj_get_coords(screen, &coords);
    clip.x1 = LV_MAX(clip.x1, coords.x1);
    clip.y1 = LV_MAX(clip.y1, coords.y1);
    clip.x2 = LV_MIN(clip.x2, coords.x2);
    clip.y2 = LV_MIN(clip.y2, coords.y2);
    if (clip.x1 > clip.x2 || clip.y1 > clip.y2)
        return;
#if !defined(CONFIG_PROSPECTOR_OPERATOR_TEXTURE_HATCH)
    lv_draw_rect_dsc_t dsc;
    lv_draw_rect_dsc_init(&dsc);
    dsc.bg_opa = LV_OPA_COVER;
#endif
#if defined(CONFIG_PROSPECTOR_OPERATOR_TEXTURE_DOTS)
    dsc.bg_color = lv_color_hex(DISPLAY_COLOR_TEXTURE);
    for (int32_t y = first_on_grid(clip.y1, coords.y1 + 3, 8); y <= clip.y2; y += 8)
        for (int32_t x = first_on_grid(clip.x1, coords.x1 + 3, 8); x <= clip.x2; x += 8)
            fill(layer, &dsc, x, y, x, y);
#elif defined(CONFIG_PROSPECTOR_OPERATOR_TEXTURE_FINE_GRID)
    dsc.bg_color = lv_color_hex(DISPLAY_COLOR_TEXTURE_FAINT);
    for (int32_t x = first_on_grid(clip.x1, coords.x1 + 3, 6); x <= clip.x2; x += 6)
        fill(layer, &dsc, x, clip.y1, x, clip.y2);
    for (int32_t y = first_on_grid(clip.y1, coords.y1 + 3, 6); y <= clip.y2; y += 6)
        fill(layer, &dsc, clip.x1, y, clip.x2, y);
#else
    /* 45 degree lines through (coords.x1 + 3 + 9k, coords.y1), going down-right. */
    lv_draw_line_dsc_t line;
    lv_draw_line_dsc_init(&line);
    line.color = lv_color_hex(DISPLAY_COLOR_TEXTURE);
    line.width = 1;
    int32_t height = lv_area_get_height(&coords);
    int32_t x_start = clip.x1 - (clip.y2 - coords.y1) - 1;
    int32_t x_end = clip.x2 - (clip.y1 - coords.y1) + 1;
    for (int32_t x = first_on_grid(x_start, coords.x1 + 3, 9); x <= x_end; x += 9) {
        line.p1 = (lv_point_precise_t){x, coords.y1};
        line.p2 = (lv_point_precise_t){x + height, coords.y1 + height};
        lv_draw_line(layer, &line);
    }
#endif
}
#endif

void operator_background_texture_apply(lv_obj_t *screen) {
#if defined(TEXTURE_DRAWN)
    lv_obj_add_event_cb(screen, texture_draw_cb, LV_EVENT_DRAW_MAIN_END, NULL);
#else
    LV_UNUSED(screen);
#endif
}

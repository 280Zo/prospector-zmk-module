#include <lvgl.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

lv_obj_t *prospector_diagnostics_page_create(lv_obj_t *parent);
void prospector_diagnostics_page_render_seed(void);

static uint32_t ticks;
static int64_t uptime_ms = 0;

uint32_t custom_tick_get(void) {
    return ticks;
}

int64_t k_uptime_get(void) {
    return uptime_ms;
}

uint8_t prospector_brightness_get_displayed(void) {
    return 82;
}

bool prospector_brightness_is_auto(void) {
    return true;
}

bool prospector_brightness_get_ambient_light(int32_t *lux) {
    *lux = 146;
    return true;
}

static const char *arg_value(int *index, int argc, char **argv) {
    if (*index + 1 >= argc) {
        fprintf(stderr, "Missing value for %s\n", argv[*index]);
        exit(2);
    }
    *index += 1;
    return argv[*index];
}

int main(int argc, char **argv) {
    const char *output = "diagnostics.raw";

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--output") == 0) {
            output = arg_value(&i, argc, argv);
        } else if (strcmp(argv[i], "--uptime-minutes") == 0) {
            uptime_ms = (int64_t)atoi(arg_value(&i, argc, argv)) * 60000;
        }
    }

    lv_init();
    lv_tick_set_cb(custom_tick_get);
    lv_display_t *display = lv_display_create(280, 240);
    static uint8_t draw_buf[280 * 240 * 4];
    lv_display_set_buffers(display, draw_buf, NULL, sizeof(draw_buf), LV_DISPLAY_RENDER_MODE_DIRECT);

    lv_obj_t *screen = lv_obj_create(NULL);
    lv_obj_set_size(screen, 280, 240);
    lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_screen_load(screen);

    lv_obj_t *page = prospector_diagnostics_page_create(screen);
    lv_obj_clear_flag(page, LV_OBJ_FLAG_HIDDEN);
    prospector_diagnostics_page_render_seed();

    lv_obj_update_layout(screen);

    lv_draw_buf_t *snapshot = lv_snapshot_take(screen, LV_COLOR_FORMAT_ARGB8888);
    if (!snapshot) {
        fprintf(stderr, "Failed to capture LVGL snapshot\n");
        return 1;
    }

    FILE *fp = fopen(output, "wb");
    if (!fp) {
        perror(output);
        return 1;
    }
    fwrite("LVRAW8888\n", 1, 10, fp);
    fprintf(fp, "%u %u %u\n", snapshot->header.w, snapshot->header.h, snapshot->header.stride);
    fwrite(snapshot->data, 1, snapshot->data_size, fp);
    fclose(fp);
    lv_draw_buf_destroy(snapshot);
    return 0;
}

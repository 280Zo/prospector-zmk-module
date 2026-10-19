#include "brightness_toast.h"
#include <lvgl.h>
#include <prospector_render_state.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zephyr/kernel.h>
#include <zmk/events/caps_word_state_changed.h>

lv_obj_t *zmk_display_status_screen(void);
int widget_wpm_meter_cb(const zmk_event_t *eh);
static uint32_t ticks;

uint32_t custom_tick_get(void) { return ticks; }

static void flush(lv_display_t *display, const lv_area_t *area, uint8_t *pixels) {
    (void)area;
    (void)pixels;
    lv_display_flush_ready(display);
}

static zmk_mod_flags_t parse_mods(const char *mods) {
    zmk_mod_flags_t flags = 0;
    if (!mods) {
        return flags;
    }

    char buf[128];
    strncpy(buf, mods, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';

    for (char *part = strtok(buf, ","); part; part = strtok(NULL, ",")) {
        while (*part == ' ') {
            part++;
        }
        if (strcasecmp(part, "gui") == 0 || strcasecmp(part, "win") == 0 ||
            strcasecmp(part, "cmd") == 0 || strcasecmp(part, "g") == 0) {
            flags |= MOD_LGUI;
        } else if (strcasecmp(part, "alt") == 0 || strcasecmp(part, "a") == 0) {
            flags |= MOD_LALT;
        } else if (strcasecmp(part, "ctrl") == 0 || strcasecmp(part, "control") == 0 ||
                   strcasecmp(part, "c") == 0) {
            flags |= MOD_LCTL;
        } else if (strcasecmp(part, "shift") == 0 || strcasecmp(part, "s") == 0) {
            flags |= MOD_LSFT;
        }
    }
    return flags;
}

static bool parse_bool(const char *value) {
    return value && (strcasecmp(value, "1") == 0 || strcasecmp(value, "true") == 0 ||
                     strcasecmp(value, "yes") == 0 || strcasecmp(value, "y") == 0 ||
                     strcasecmp(value, "connected") == 0 || strcasecmp(value, "on") == 0);
}

static void apply_batteries(const char *levels, const char *connected) {
    char level_buf[128];
    char conn_buf[128];
    strncpy(level_buf, levels ? levels : "", sizeof(level_buf) - 1);
    level_buf[sizeof(level_buf) - 1] = '\0';
    strncpy(conn_buf, connected ? connected : "", sizeof(conn_buf) - 1);
    conn_buf[sizeof(conn_buf) - 1] = '\0';

    char *conn_save = NULL;
    char *conn_part = strtok_r(conn_buf, ",", &conn_save);
    char *level_save = NULL;
    int index = 0;
    for (char *level_part = strtok_r(level_buf, ",", &level_save); level_part;
         level_part = strtok_r(NULL, ",", &level_save)) {
        bool is_connected = conn_part ? parse_bool(conn_part) : true;
        int level = atoi(level_part);
        battery_circles_connection_update_cb(
            (struct connection_update_state){.source = (uint8_t)index, .connected = is_connected});
        battery_circles_battery_update_cb(
            (struct battery_update_state){.source = (uint8_t)index, .level = (uint8_t)level});
        if (conn_part) {
            conn_part = strtok_r(NULL, ",", &conn_save);
        }
        index++;
    }
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
    const char *output = "operator.raw";
    const char *mods = "shift";
    const char *order = "GACS";
    const char *layer = "Base";
    const char *batteries = "65,11";
    const char *connected = "yes,yes";
    int wpm = 67;
    int active_layer = 0;
    int profile = 1;
    bool caps_word = false;
    enum zmk_transport transport = ZMK_TRANSPORT_USB;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--output") == 0) {
            output = arg_value(&i, argc, argv);
        } else if (strcmp(argv[i], "--wpm") == 0) {
            wpm = atoi(arg_value(&i, argc, argv));
        } else if (strcmp(argv[i], "--mods") == 0) {
            mods = arg_value(&i, argc, argv);
        } else if (strcmp(argv[i], "--modifier-order") == 0) {
            order = arg_value(&i, argc, argv);
        } else if (strcmp(argv[i], "--caps-word") == 0) {
            caps_word = true;
        } else if (strcmp(argv[i], "--layer") == 0) {
            layer = arg_value(&i, argc, argv);
        } else if (strcmp(argv[i], "--active-layer") == 0) {
            active_layer = atoi(arg_value(&i, argc, argv));
        } else if (strcmp(argv[i], "--batteries") == 0) {
            batteries = arg_value(&i, argc, argv);
        } else if (strcmp(argv[i], "--connected") == 0) {
            connected = arg_value(&i, argc, argv);
        } else if (strcmp(argv[i], "--transport") == 0) {
            transport = strcmp(arg_value(&i, argc, argv), "ble") == 0 ? ZMK_TRANSPORT_BLE
                                                                      : ZMK_TRANSPORT_USB;
        } else if (strcmp(argv[i], "--profile") == 0) {
            profile = atoi(arg_value(&i, argc, argv));
        }
    }

    lv_init();
    lv_tick_set_cb(custom_tick_get);
    lv_display_t *display = lv_display_create(280, 240);
    lv_display_set_color_format(display, LV_COLOR_FORMAT_ARGB8888);
    lv_display_set_flush_cb(display, flush);
    static uint8_t draw_buf[280 * 240 * 4];
    lv_display_set_buffers(display, draw_buf, NULL, sizeof(draw_buf),
                           LV_DISPLAY_RENDER_MODE_DIRECT);

    host_state_set_wpm(wpm);
    host_state_set_mods(parse_mods(mods));
    host_state_set_layer(active_layer, layer);
    host_state_set_endpoint(transport);
    host_state_set_profile((uint8_t)(profile - 1));
    host_modifier_order_set(order);

    lv_obj_t *screen = zmk_display_status_screen();
    lv_obj_set_size(screen, 280, 240);
    lv_screen_load(screen);

    /* Warm the actual smoothing/history code, then settle at the requested WPM. */
    for (int sample = 0; sample < 48; ++sample) {
        host_state_set_wpm(15 + sample % 12 * 5);
        widget_wpm_meter_cb(NULL);
        ticks += 40;
        host_work_advance(40);
    }
    host_state_set_wpm(wpm);
    widget_wpm_meter_cb(NULL);
    prospector_brightness_toast_request(1, 80, false, true);
    prospector_modifier_row_request(parse_mods(mods), caps_word, false, true);
    for (int i = 0; i < 60; ++i) {
        ticks += 33;
        host_work_advance(33);
        lv_timer_handler();
    }
    apply_batteries(batteries, connected);

    lv_obj_update_layout(screen);

    /* Refresh the full display so sys/top-layer modifier and brightness widgets are included. */
    lv_refr_now(display);

    FILE *fp = fopen(output, "wb");
    if (!fp) {
        perror(output);
        return 1;
    }
    fwrite("LVRAW8888\n", 1, 10, fp);
    fprintf(fp, "280 240 1120\n");
    fwrite(draw_buf, 1, sizeof(draw_buf), fp);
    fclose(fp);
    return 0;
}

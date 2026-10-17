#include "tetris_app.h"

#include <stdint.h>
#include <string.h>

#include "bsp_battery.h"
#include "bsp_button.h"
#include "bsp_display.h"
#include "bsp_pins.h"
#include "esp_log.h"
#include "esp_sleep.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "key_guard.h"
#include "lvgl.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "tetris_audio.h"
#include "tetris_input.h"
#include "tetris_menu.h"
#include "tetris_model.h"
#include "tetris_save.h"
#include "tetris_settings.h"
#include "tetris_strings.h"

LV_FONT_DECLARE(app_font_14);
LV_FONT_DECLARE(app_font_20);

_Static_assert((int)BSP_BTN_UP == (int)TETRIS_BTN_UP, "button index mismatch");
_Static_assert((int)BSP_BTN_DOWN == (int)TETRIS_BTN_DOWN, "button index mismatch");
_Static_assert((int)BSP_BTN_OK == (int)TETRIS_BTN_OK, "button index mismatch");
_Static_assert((int)BSP_BTN_PRESS == (int)TETRIS_BTN_PRESS, "button event mismatch");
_Static_assert((int)BSP_BTN_CLICK == (int)TETRIS_BTN_CLICK, "button event mismatch");
_Static_assert((int)BSP_BTN_DOUBLE == (int)TETRIS_BTN_DOUBLE, "button event mismatch");
_Static_assert((int)BSP_BTN_LONG == (int)TETRIS_BTN_LONG, "button event mismatch");
_Static_assert((int)BSP_BTN_RELEASE == (int)TETRIS_BTN_RELEASE, "button event mismatch");
_Static_assert(TETRIS_SAVE_ENCODED_SIZE <= 255, "save message length must fit uint8_t");

#define COLOR_BG_TOP     0x1C1030
#define COLOR_BG_BOTTOM  0x0B0714
#define COLOR_CARD       0x171022
#define COLOR_CARD_DARK  0x0E0918
#define COLOR_CARD_SEL   0x241834
#define COLOR_BORDER     0x352A4D
#define COLOR_TEXT       0xF4ECFF
#define COLOR_DIM        0x9A8AB5
#define COLOR_AMBER      0xFFC857
#define COLOR_ROSE       0xFF6B8A
#define COLOR_MINT       0x7CE3B0
#define COLOR_INPUT_ERR  0xFF6B8A
#define COLOR_CELEBRATE  0x1E1530

#define PITCH            11
#define CELL             9
#define BOARD_CANVAS_W   (TETRIS_BOARD_WIDTH * PITCH)
#define BOARD_CANVAS_H   (TETRIS_BOARD_HEIGHT * PITCH)
#define NEXT_CANVAS_W    (4 * PITCH)
#define NEXT_CANVAS_H    (4 * PITCH)
#define BOARD_X          8
#define BOARD_Y          58

#define GAME_TIMER_MS    20
#define FAST_DROP_MS     45
#define FLASH_MS         110
#define CELEBRATE_MS     2500
#define LEVEL_BANNER_MS  2500
#define SAVE_INTERVAL_MS 8000
#define KEY_QUEUE_DEPTH  16
#define BATTERY_TASK_MS  30000
#define NVS_NAMESPACE    "tetris"
#define NVS_KEY_BEST     "best"
#define NVS_KEY_SAVE     "save"
#define NVS_KEY_SOUND    "sound"
#define NVS_KEY_VOLUME   "vol"
#define DEFAULT_VOLUME   55U

static const uint32_t PALETTE[] = {
    0x0B0714, // 0 棋盘背景
    0x63E6E2, // 1 I
    0xFFD166, // 2 O
    0xC9A2FF, // 3 T
    0x7CE3B0, // 4 S
    0xFF6B8A, // 5 Z
    0x7B9CFF, // 6 J
    0xFFA45B, // 7 L
    0x493A63, // 8 落点轮廓
    0xF4ECFF, // 9 高光边
    0x120C1E, // 10 阴影边
    0x2A1E3D, // 11 网格点
    0x000000, 0x000000, 0x000000, 0x000000,
};

LV_DRAW_BUF_DEFINE_STATIC(s_board_buf, BOARD_CANVAS_W, BOARD_CANVAS_H, LV_COLOR_FORMAT_I4);
LV_DRAW_BUF_DEFINE_STATIC(s_next_buf, NEXT_CANVAS_W, NEXT_CANVAS_H, LV_COLOR_FORMAT_I4);

typedef struct {
    uint8_t btn;
    uint8_t ev;
} key_event_t;

// 存档任务的消息:len=0 表示删除存档。
typedef struct {
    uint8_t len;
    uint8_t data[TETRIS_SAVE_ENCODED_SIZE];
} save_msg_t;

static const char *TAG = "tetris";

static const lv_point_precise_t SMILE_POINTS[] = {
    { 12, 40 }, { 16, 46 }, { 23, 51 }, { 32, 53 },
    { 41, 51 }, { 48, 46 }, { 52, 40 },
};

static tetris_model_t s_model;
static tetris_model_t s_saved_model;
static tetris_input_state_t s_input;
static QueueHandle_t s_key_queue;
static QueueHandle_t s_save_queue;
static bool s_input_available;

static bool s_in_menu = true;
static bool s_in_settings;
static bool s_settings_from_game;
static bool s_save_available;
static bool s_save_pending;
static bool s_sound_on = true;
static uint8_t s_volume = DEFAULT_VOLUME;

static lv_obj_t *s_screen;
static lv_obj_t *s_menu_screen;
static lv_obj_t *s_settings_screen;

static lv_obj_t *s_board_canvas;
static lv_obj_t *s_next_canvas;
static lv_obj_t *s_score_value;
static lv_obj_t *s_lines_value;
static lv_obj_t *s_level_value;
static lv_obj_t *s_best_value;
static lv_obj_t *s_battery_label;
static lv_obj_t *s_battery_fill;
static lv_obj_t *s_menu_battery_label;
static lv_obj_t *s_menu_battery_fill;
static lv_obj_t *s_settings_battery_label;
static lv_obj_t *s_settings_battery_fill;
static lv_obj_t *s_controls_label;
static lv_obj_t *s_flash_overlay;
static lv_obj_t *s_pause_overlay;
static lv_obj_t *s_over_overlay;
static lv_obj_t *s_over_score;
static lv_obj_t *s_record_label;

static lv_obj_t *s_menu_items[3];
static lv_obj_t *s_menu_item_labels[3];
static lv_obj_t *s_menu_continue_detail;
static lv_obj_t *s_menu_hint;
static tetris_menu_t s_start_menu;

static lv_obj_t *s_settings_sound_value;
static lv_obj_t *s_settings_volume_value;
static lv_obj_t *s_settings_volume_fill;
static tetris_settings_state_t s_settings;

static lv_obj_t *s_pause_rows[3];
static lv_obj_t *s_pause_labels[3];
static tetris_menu_t s_pause_menu;

static lv_obj_t *s_over_rows[2];
static lv_obj_t *s_over_labels[2];
static tetris_menu_t s_over_menu;

static lv_obj_t *s_celebrate_card;
static uint64_t s_celebrate_until_ms;
static lv_obj_t *s_level_banner;
static uint64_t s_level_banner_until_ms;

static uint64_t s_last_gravity_ms;
static uint64_t s_last_save_ms;
static uint64_t s_flash_until_ms;
static bool s_fast_drop;
static uint32_t s_best_score;
static volatile int s_battery_soc = -1;
static int s_battery_shown = -999;

// 深睡按键唤醒时按键仍被按住,守卫在松手前吞掉事件(见 key_guard.h)。
#define KEY_GUARD_MAX_US (10LL * 1000 * 1000)
static key_guard_t s_key_guard;

static uint64_t app_now_ms(void) {
    return (uint64_t)esp_timer_get_time() / 1000ULL;
}

// ---------------------------------------------------------------------------
// 纯数据与持久化
// ---------------------------------------------------------------------------

static uint32_t best_load(void) {
    nvs_handle_t handle;
    uint32_t value = 0;
    if (nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle) == ESP_OK) {
        (void)nvs_get_u32(handle, NVS_KEY_BEST, &value);
        nvs_close(handle);
    }
    return value;
}

static void best_save(uint32_t value) {
    nvs_handle_t handle;
    if (nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle) != ESP_OK) return;
    if (nvs_set_u32(handle, NVS_KEY_BEST, value) == ESP_OK) {
        (void)nvs_commit(handle);
    }
    nvs_close(handle);
}

// 声音设置默认开启、音量 55%,与音频模块的默认值一致。
static void settings_load(void) {
    nvs_handle_t handle;
    if (nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle) != ESP_OK) return;
    uint8_t sound = 1;
    uint8_t volume = (uint8_t)DEFAULT_VOLUME;
    if (nvs_get_u8(handle, NVS_KEY_SOUND, &sound) == ESP_OK) {
        s_sound_on = sound != 0;
    }
    if (nvs_get_u8(handle, NVS_KEY_VOLUME, &volume) == ESP_OK) {
        s_volume = volume > 100 ? 100 : volume;
    }
    nvs_close(handle);
}

static void settings_save(void) {
    nvs_handle_t handle;
    if (nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle) != ESP_OK) return;
    esp_err_t err = nvs_set_u8(handle, NVS_KEY_SOUND, s_sound_on ? 1 : 0);
    if (err == ESP_OK) err = nvs_set_u8(handle, NVS_KEY_VOLUME, s_volume);
    if (err == ESP_OK) (void)nvs_commit(handle);
    nvs_close(handle);
}

static void write_save_nvs(const uint8_t *data, size_t len) {
    nvs_handle_t handle;
    if (nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle) != ESP_OK) {
        ESP_LOGW(TAG, "存档写入失败:无法打开 NVS");
        return;
    }
    esp_err_t err = nvs_set_blob(handle, NVS_KEY_SAVE, data, len);
    if (err == ESP_OK) err = nvs_commit(handle);
    nvs_close(handle);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "存档写入失败: %s", esp_err_to_name(err));
    }
}

static void erase_save_nvs(void) {
    nvs_handle_t handle;
    if (nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle) != ESP_OK) return;
    (void)nvs_erase_key(handle, NVS_KEY_SAVE);
    (void)nvs_commit(handle);
    nvs_close(handle);
}

static void save_task(void *arg) {
    (void)arg;
    for (;;) {
        save_msg_t msg;
        if (xQueueReceive(s_save_queue, &msg, portMAX_DELAY) != pdTRUE) continue;
        if (msg.len == 0) {
            erase_save_nvs();
        } else {
            write_save_nvs(msg.data, msg.len);
        }
    }
}

// 队列深度为 1,用覆盖写保证"最新状态"(尤其是结束时的删除)不会被旧请求压住。
static void request_save(void) {
    save_msg_t msg;
    const size_t len = tetris_save_encode(&s_model, msg.data, sizeof(msg.data));
    if (len == 0) return;
    msg.len = (uint8_t)len;
    if (s_save_queue) {
        (void)xQueueOverwrite(s_save_queue, &msg);
    } else {
        write_save_nvs(msg.data, len);
    }
}

static void request_erase(void) {
    save_msg_t msg;
    memset(&msg, 0, sizeof(msg));
    msg.len = 0;
    if (s_save_queue) {
        (void)xQueueOverwrite(s_save_queue, &msg);
    } else {
        erase_save_nvs();
    }
}

static bool load_saved_game(tetris_model_t *model) {
    nvs_handle_t handle;
    if (nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle) != ESP_OK) return false;

    size_t len = 0;
    bool ok = false;
    if (nvs_get_blob(handle, NVS_KEY_SAVE, NULL, &len) == ESP_OK &&
        len == TETRIS_SAVE_ENCODED_SIZE) {
        uint8_t blob[TETRIS_SAVE_ENCODED_SIZE];
        if (nvs_get_blob(handle, NVS_KEY_SAVE, blob, &len) == ESP_OK) {
            ok = tetris_save_decode(blob, len, model);
        }
    }
    nvs_close(handle);
    return ok;
}

static void battery_task(void *arg) {
    (void)arg;
    for (;;) {
        s_battery_soc = bsp_battery_soc();
        vTaskDelay(pdMS_TO_TICKS(BATTERY_TASK_MS));
    }
}

// ---------------------------------------------------------------------------
// 画布绘制
// ---------------------------------------------------------------------------

static void canvas_clear(lv_obj_t *canvas) {
    lv_draw_buf_t *buf = lv_canvas_get_draw_buf(canvas);
    if (!buf || !buf->data) return;
    // I4 缓冲区开头是调色板,只能清像素区,否则颜色表会被清零成全透明。
    const uint32_t palette_bytes =
        (uint32_t)LV_COLOR_INDEXED_PALETTE_SIZE(buf->header.cf) * sizeof(lv_color32_t);
    const uint32_t pixel_bytes = buf->header.stride * buf->header.h;
    if (buf->data_size < palette_bytes + pixel_bytes) return;
    memset(buf->data + palette_bytes, 0, pixel_bytes);
}

static void canvas_px(lv_obj_t *canvas, int x, int y, uint8_t index) {
    lv_draw_buf_t *buf = lv_canvas_get_draw_buf(canvas);
    if (!buf) return;
    uint8_t *data = lv_draw_buf_goto_xy(buf, x, y);
    if (!data) return;
    const uint8_t shift = (uint8_t)(4 - 4 * (x & 1));
    *data = (uint8_t)((*data & ~(0x0FU << shift)) | ((index & 0x0FU) << shift));
}

static void canvas_set_palette(lv_obj_t *canvas) {
    for (uint8_t i = 0; i < 16; i++) {
        lv_canvas_set_palette(canvas, i,
                              lv_color_to_32(lv_color_hex(PALETTE[i]), LV_OPA_COVER));
    }
}

static void draw_cell_block(lv_obj_t *canvas, int cell_x, int cell_y,
                            uint8_t color, bool ghost) {
    const int start_x = cell_x * PITCH + 1;
    const int start_y = cell_y * PITCH + 1;

    if (ghost) {
        for (int i = 0; i < CELL; i++) {
            const bool corner = (i == 0 || i == CELL - 1);
            if (!corner) {
                canvas_px(canvas, start_x + i, start_y, 8);
                canvas_px(canvas, start_x + i, start_y + CELL - 1, 8);
                canvas_px(canvas, start_x, start_y + i, 8);
                canvas_px(canvas, start_x + CELL - 1, start_y + i, 8);
            }
        }
        return;
    }

    for (int y = 0; y < CELL; y++) {
        for (int x = 0; x < CELL; x++) {
            if ((x == 0 || x == CELL - 1) && (y == 0 || y == CELL - 1)) continue;
            uint8_t pixel = color;
            if (x == 0 || y == 0) {
                pixel = 9;
            } else if (x == CELL - 1 || y == CELL - 1) {
                pixel = 10;
            }
            canvas_px(canvas, start_x + x, start_y + y, pixel);
        }
    }
}

static void draw_dot_grid(lv_obj_t *canvas) {
    for (int y = 0; y < TETRIS_BOARD_HEIGHT; y++) {
        for (int x = 0; x < TETRIS_BOARD_WIDTH; x++) {
            canvas_px(canvas, x * PITCH + 5, y * PITCH + 5, 11);
        }
    }
}

static void draw_piece_at(lv_obj_t *canvas, tetris_piece_t piece, uint8_t rotation,
                          int origin_x, int origin_y, bool ghost,
                          int min_x, int min_y, int max_x, int max_y) {
    for (int py = 0; py < 4; py++) {
        for (int px = 0; px < 4; px++) {
            if (!tetris_piece_cell(piece, rotation, px, py)) continue;
            const int x = origin_x + px;
            const int y = origin_y + py;
            if (x < min_x || x >= max_x || y < min_y || y >= max_y) continue;
            const uint8_t color = ghost ? 8 : (uint8_t)piece;
            draw_cell_block(canvas, x, y, color, ghost);
        }
    }
}

static void render_board(void) {
    if (!s_board_canvas) return;
    canvas_clear(s_board_canvas);
    draw_dot_grid(s_board_canvas);

    for (int y = 0; y < TETRIS_BOARD_HEIGHT; y++) {
        for (int x = 0; x < TETRIS_BOARD_WIDTH; x++) {
            if (s_model.board[y][x] != 0) {
                draw_cell_block(s_board_canvas, x, y, s_model.board[y][x], false);
            }
        }
    }

    if (s_model.state != TETRIS_GAME_OVER) {
        if (s_model.state == TETRIS_PLAYING && tetris_model_ghost_enabled(&s_model)) {
            const int ghost_y = tetris_model_ghost_y(&s_model);
            if (ghost_y != s_model.y) {
                draw_piece_at(s_board_canvas, s_model.piece, s_model.rotation,
                              s_model.x, ghost_y, true,
                              0, 0, TETRIS_BOARD_WIDTH, TETRIS_BOARD_HEIGHT);
            }
        }
        draw_piece_at(s_board_canvas, s_model.piece, s_model.rotation,
                      s_model.x, s_model.y, false,
                      0, 0, TETRIS_BOARD_WIDTH, TETRIS_BOARD_HEIGHT);
    }

    lv_obj_invalidate(s_board_canvas);
}

static void render_next(void) {
    if (!s_next_canvas) return;
    canvas_clear(s_next_canvas);

    int min_x = 4, min_y = 4, max_x = -1, max_y = -1;
    for (int py = 0; py < 4; py++) {
        for (int px = 0; px < 4; px++) {
            if (!tetris_piece_cell(s_model.next_piece, 0, px, py)) continue;
            if (px < min_x) min_x = px;
            if (px > max_x) max_x = px;
            if (py < min_y) min_y = py;
            if (py > max_y) max_y = py;
        }
    }
    if (max_x < min_x || max_y < min_y) {
        lv_obj_invalidate(s_next_canvas);
        return;
    }
    const int width = max_x - min_x + 1;
    const int height = max_y - min_y + 1;
    const int offset_x = (4 - width) / 2 - min_x;
    const int offset_y = (4 - height) / 2 - min_y;

    draw_piece_at(s_next_canvas, s_model.next_piece, 0, offset_x, offset_y, false,
                  0, 0, 4, 4);
    lv_obj_invalidate(s_next_canvas);
}

// ---------------------------------------------------------------------------
// 界面创建
// ---------------------------------------------------------------------------

static lv_obj_t *make_label(lv_obj_t *parent, const lv_font_t *font, uint32_t color,
                            int x, int y, int width, lv_text_align_t align,
                            const char *text) {
    lv_obj_t *label = lv_label_create(parent);
    lv_obj_set_style_text_font(label, font, 0);
    lv_obj_set_style_text_color(label, lv_color_hex(color), 0);
    lv_obj_set_pos(label, x, y);
    if (width > 0) {
        lv_obj_set_width(label, width);
        lv_obj_set_style_text_align(label, align, 0);
    }
    lv_label_set_text(label, text);
    return label;
}

static lv_obj_t *make_panel(lv_obj_t *parent, int x, int y, int w, int h,
                            int radius, uint32_t bg, uint32_t border, int border_width) {
    lv_obj_t *panel = lv_obj_create(parent);
    lv_obj_set_pos(panel, x, y);
    lv_obj_set_size(panel, w, h);
    lv_obj_set_style_bg_color(panel, lv_color_hex(bg), 0);
    lv_obj_set_style_bg_opa(panel, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(panel, lv_color_hex(border), 0);
    lv_obj_set_style_border_width(panel, border_width, 0);
    lv_obj_set_style_radius(panel, radius, 0);
    lv_obj_set_style_pad_all(panel, 0, 0);
    lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);
    return panel;
}

static lv_obj_t *make_screen(void) {
    lv_obj_t *screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screen, lv_color_hex(COLOR_BG_TOP), 0);
    lv_obj_set_style_bg_grad_color(screen, lv_color_hex(COLOR_BG_BOTTOM), 0);
    lv_obj_set_style_bg_grad_dir(screen, LV_GRAD_DIR_VER, 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(screen, 0, 0);
    lv_obj_set_style_pad_all(screen, 0, 0);
    lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
    return screen;
}

static lv_obj_t *make_overlay_root(lv_obj_t *parent) {
    lv_obj_t *overlay = lv_obj_create(parent);
    lv_obj_set_pos(overlay, 0, 0);
    lv_obj_set_size(overlay, 240, 320);
    lv_obj_set_style_bg_color(overlay, lv_color_hex(0x05030A), 0);
    lv_obj_set_style_bg_opa(overlay, LV_OPA_70, 0);
    lv_obj_set_style_border_width(overlay, 0, 0);
    lv_obj_set_style_radius(overlay, 0, 0);
    lv_obj_set_style_pad_all(overlay, 0, 0);
    lv_obj_clear_flag(overlay, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(overlay, LV_OBJ_FLAG_HIDDEN);
    return overlay;
}

static void make_battery_widget(lv_obj_t *parent, lv_obj_t **fill_out,
                                lv_obj_t **label_out) {
    lv_obj_t *body = make_panel(parent, 168, 14, 26, 11, 2,
                                COLOR_CARD_DARK, COLOR_DIM, 1);
    lv_obj_t *fill = lv_obj_create(body);
    lv_obj_set_pos(fill, 0, 0);
    lv_obj_set_size(fill, 0, 9);
    lv_obj_set_style_bg_color(fill, lv_color_hex(COLOR_MINT), 0);
    lv_obj_set_style_bg_opa(fill, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(fill, 0, 0);
    lv_obj_set_style_radius(fill, 1, 0);
    lv_obj_set_style_pad_all(fill, 0, 0);
    lv_obj_clear_flag(fill, LV_OBJ_FLAG_SCROLLABLE);

    *fill_out = fill;
    *label_out = make_label(parent, &app_font_14, COLOR_DIM,
                            198, 8, 34, LV_TEXT_ALIGN_RIGHT, "--");
}

static void style_choice(lv_obj_t *row, lv_obj_t *label, bool selected,
                         bool enabled) {
    lv_obj_set_style_border_color(row,
                                  lv_color_hex(selected ? COLOR_AMBER : COLOR_BORDER), 0);
    lv_obj_set_style_border_width(row, selected ? 2 : 1, 0);
    lv_obj_set_style_bg_color(row,
                              lv_color_hex(selected ? COLOR_CARD_SEL : COLOR_CARD_DARK), 0);
    const uint32_t text_color =
        selected ? COLOR_AMBER : (enabled ? COLOR_TEXT : COLOR_DIM);
    lv_obj_set_style_text_color(label, lv_color_hex(text_color), 0);
}

static void build_menu_ui(void) {
    s_menu_screen = make_screen();

    lv_obj_t *accent = lv_obj_create(s_menu_screen);
    lv_obj_set_pos(accent, 20, 24);
    lv_obj_set_size(accent, 4, 24);
    lv_obj_set_style_bg_color(accent, lv_color_hex(COLOR_AMBER), 0);
    lv_obj_set_style_bg_opa(accent, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(accent, 0, 0);
    lv_obj_set_style_radius(accent, 2, 0);
    lv_obj_set_style_pad_all(accent, 0, 0);

    make_label(s_menu_screen, &app_font_20, COLOR_TEXT, 30, 22, 0,
               LV_TEXT_ALIGN_LEFT, TETRIS_STR_TITLE);
    make_battery_widget(s_menu_screen, &s_menu_battery_fill, &s_menu_battery_label);

    static const uint32_t DECOR[] = { 0x63E6E2, 0xFFD166, 0xC9A2FF, 0xFF6B8A };
    for (int i = 0; i < 4; i++) {
        make_panel(s_menu_screen, 69 + i * 28, 64, 18, 18, 4,
                   DECOR[i], DECOR[i], 0);
    }

    s_menu_items[0] = make_panel(s_menu_screen, 20, 92, 200, 48, 10,
                                 COLOR_CARD, COLOR_BORDER, 1);
    s_menu_item_labels[0] = make_label(s_menu_items[0], &app_font_20, COLOR_TEXT,
                                       0, 11, 196, LV_TEXT_ALIGN_CENTER,
                                       TETRIS_STR_MENU_NEW);

    s_menu_items[1] = make_panel(s_menu_screen, 20, 150, 200, 58, 10,
                                 COLOR_CARD, COLOR_BORDER, 1);
    s_menu_item_labels[1] = make_label(s_menu_items[1], &app_font_20, COLOR_TEXT,
                                       0, 5, 196, LV_TEXT_ALIGN_CENTER,
                                       TETRIS_STR_MENU_CONTINUE);
    s_menu_continue_detail = make_label(s_menu_items[1], &app_font_14, COLOR_DIM,
                                         0, 32, 196, LV_TEXT_ALIGN_CENTER, "");

    s_menu_items[2] = make_panel(s_menu_screen, 20, 216, 200, 48, 10,
                                 COLOR_CARD, COLOR_BORDER, 1);
    s_menu_item_labels[2] = make_label(s_menu_items[2], &app_font_20, COLOR_TEXT,
                                       0, 11, 196, LV_TEXT_ALIGN_CENTER,
                                       TETRIS_STR_SETTINGS);

    s_menu_hint = make_label(s_menu_screen, &app_font_14, COLOR_DIM,
                             0, 280, 240, LV_TEXT_ALIGN_CENTER,
                             TETRIS_STR_MENU_HINT);
    if (!s_input_available) {
        lv_label_set_text(s_menu_hint, TETRIS_STR_INPUT_ERROR);
        lv_obj_set_style_text_color(s_menu_hint, lv_color_hex(COLOR_INPUT_ERR), 0);
    }
}

static void build_settings_ui(void) {
    s_settings_screen = make_screen();

    lv_obj_t *accent = lv_obj_create(s_settings_screen);
    lv_obj_set_pos(accent, 20, 24);
    lv_obj_set_size(accent, 4, 24);
    lv_obj_set_style_bg_color(accent, lv_color_hex(COLOR_AMBER), 0);
    lv_obj_set_style_bg_opa(accent, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(accent, 0, 0);
    lv_obj_set_style_radius(accent, 2, 0);
    lv_obj_set_style_pad_all(accent, 0, 0);

    make_label(s_settings_screen, &app_font_20, COLOR_TEXT, 30, 22, 0,
               LV_TEXT_ALIGN_LEFT, TETRIS_STR_SETTINGS);
    make_battery_widget(s_settings_screen, &s_settings_battery_fill,
                        &s_settings_battery_label);

    lv_obj_t *sound_card = make_panel(s_settings_screen, 20, 84, 200, 64, 10,
                                      COLOR_CARD, COLOR_BORDER, 1);
    make_label(sound_card, &app_font_20, COLOR_TEXT, 16, 19, 0,
               LV_TEXT_ALIGN_LEFT, TETRIS_STR_SOUND);
    s_settings_sound_value = make_label(sound_card, &app_font_20, COLOR_MINT,
                                        0, 19, 184, LV_TEXT_ALIGN_RIGHT,
                                        TETRIS_STR_ON);

    lv_obj_t *volume_card = make_panel(s_settings_screen, 20, 160, 200, 76, 10,
                                       COLOR_CARD, COLOR_BORDER, 1);
    make_label(volume_card, &app_font_20, COLOR_TEXT, 16, 10, 0,
               LV_TEXT_ALIGN_LEFT, TETRIS_STR_VOLUME);
    s_settings_volume_value = make_label(volume_card, &app_font_20, COLOR_AMBER,
                                         0, 10, 184, LV_TEXT_ALIGN_RIGHT, "55");
    lv_obj_t *bar = make_panel(volume_card, 16, 50, 168, 10, 5,
                               COLOR_CARD_SEL, COLOR_BORDER, 0);
    s_settings_volume_fill = make_panel(bar, 0, 0, 92, 10, 5,
                                        COLOR_MINT, COLOR_MINT, 0);

    make_label(s_settings_screen, &app_font_14, COLOR_DIM, 0, 252, 240,
               LV_TEXT_ALIGN_CENTER, TETRIS_STR_SETTINGS_HINT1);
    make_label(s_settings_screen, &app_font_14, COLOR_DIM, 0, 278, 240,
               LV_TEXT_ALIGN_CENTER, TETRIS_STR_SETTINGS_HINT2);
}

static void build_game_ui(void) {
    s_screen = make_screen();

    lv_obj_t *accent = lv_obj_create(s_screen);
    lv_obj_set_pos(accent, 10, 11);
    lv_obj_set_size(accent, 4, 24);
    lv_obj_set_style_bg_color(accent, lv_color_hex(COLOR_AMBER), 0);
    lv_obj_set_style_bg_opa(accent, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(accent, 0, 0);
    lv_obj_set_style_radius(accent, 2, 0);
    lv_obj_set_style_pad_all(accent, 0, 0);

    make_label(s_screen, &app_font_20, COLOR_TEXT, 20, 9, 0,
               LV_TEXT_ALIGN_LEFT, TETRIS_STR_TITLE);
    make_battery_widget(s_screen, &s_battery_fill, &s_battery_label);

    make_panel(s_screen, 4, 52, 118, 232, 10, COLOR_CARD, COLOR_BORDER, 2);
    make_panel(s_screen, 126, 52, 106, 232, 10, COLOR_CARD, COLOR_BORDER, 2);
    make_panel(s_screen, 132, 80, 94, 58, 8, COLOR_CARD_DARK, COLOR_BORDER, 1);

    LV_DRAW_BUF_INIT_STATIC(s_board_buf);
    s_board_canvas = lv_canvas_create(s_screen);
    lv_canvas_set_draw_buf(s_board_canvas, &s_board_buf);
    canvas_set_palette(s_board_canvas);
    lv_obj_set_pos(s_board_canvas, BOARD_X, BOARD_Y);

    LV_DRAW_BUF_INIT_STATIC(s_next_buf);
    s_next_canvas = lv_canvas_create(s_screen);
    lv_canvas_set_draw_buf(s_next_canvas, &s_next_buf);
    canvas_set_palette(s_next_canvas);
    lv_obj_set_pos(s_next_canvas, 157, 87);

    make_label(s_screen, &app_font_14, COLOR_DIM, 136, 62, 0,
               LV_TEXT_ALIGN_LEFT, TETRIS_STR_NEXT);
    make_label(s_screen, &app_font_14, COLOR_DIM, 136, 146, 0,
               LV_TEXT_ALIGN_LEFT, TETRIS_STR_SCORE);

    s_score_value = make_label(s_screen, &app_font_20, COLOR_AMBER,
                               136, 160, 90, LV_TEXT_ALIGN_LEFT, "0");
    make_label(s_screen, &app_font_14, COLOR_DIM, 136, 192, 0,
               LV_TEXT_ALIGN_LEFT, TETRIS_STR_LINES);
    s_lines_value = make_label(s_screen, &app_font_14, COLOR_TEXT,
                               178, 192, 48, LV_TEXT_ALIGN_RIGHT, "0");

    make_label(s_screen, &app_font_14, COLOR_DIM, 136, 214, 0,
               LV_TEXT_ALIGN_LEFT, TETRIS_STR_LEVEL);
    s_level_value = make_label(s_screen, &app_font_14, COLOR_TEXT,
                               178, 214, 48, LV_TEXT_ALIGN_RIGHT, "1");

    make_label(s_screen, &app_font_14, COLOR_DIM, 136, 236, 0,
               LV_TEXT_ALIGN_LEFT, TETRIS_STR_BEST);
    s_best_value = make_label(s_screen, &app_font_14, COLOR_TEXT,
                              178, 236, 48, LV_TEXT_ALIGN_RIGHT, "0");

    make_label(s_screen, &app_font_14, COLOR_DIM, 136, 260, 90,
               LV_TEXT_ALIGN_LEFT, TETRIS_STR_PAUSE_HINT);

    s_controls_label = make_label(s_screen, &app_font_14, COLOR_DIM,
                                  8, 292, 224, LV_TEXT_ALIGN_CENTER,
                                  TETRIS_STR_CONTROLS);
    if (!s_input_available) {
        lv_label_set_text(s_controls_label, TETRIS_STR_INPUT_ERROR);
        lv_obj_set_style_text_color(s_controls_label, lv_color_hex(COLOR_INPUT_ERR), 0);
    }

    s_flash_overlay = make_panel(s_screen, BOARD_X, BOARD_Y,
                                 BOARD_CANVAS_W, BOARD_CANVAS_H, 0,
                                 0xFFFFFF, 0xFFFFFF, 0);
    lv_obj_set_style_bg_opa(s_flash_overlay, LV_OPA_30, 0);
    lv_obj_add_flag(s_flash_overlay, LV_OBJ_FLAG_HIDDEN);

    // 四行消除庆祝贴纸:盖在右侧信息栏上,数秒后自动消失。
    s_celebrate_card = make_panel(s_screen, 128, 56, 102, 224, 12,
                                  COLOR_CELEBRATE, COLOR_AMBER, 2);
    make_label(s_celebrate_card, &app_font_20, COLOR_AMBER, 0, 12, 98,
               LV_TEXT_ALIGN_CENTER, TETRIS_STR_CELEBRATE);

    lv_obj_t *face = lv_obj_create(s_celebrate_card);
    lv_obj_set_pos(face, 19, 54);
    lv_obj_set_size(face, 64, 64);
    lv_obj_set_style_radius(face, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(face, lv_color_hex(COLOR_AMBER), 0);
    lv_obj_set_style_bg_opa(face, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(face, 0, 0);
    lv_obj_clear_flag(face, LV_OBJ_FLAG_SCROLLABLE);

    for (int i = 0; i < 2; i++) {
        lv_obj_t *eye = lv_obj_create(face);
        lv_obj_set_pos(eye, i == 0 ? 18 : 39, 20);
        lv_obj_set_size(eye, 7, 7);
        lv_obj_set_style_radius(eye, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_color(eye, lv_color_hex(0x2A1E3D), 0);
        lv_obj_set_style_bg_opa(eye, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(eye, 0, 0);
        lv_obj_clear_flag(eye, LV_OBJ_FLAG_SCROLLABLE);
    }

    lv_obj_t *smile = lv_line_create(face);
    lv_obj_set_pos(smile, 0, 0);
    lv_line_set_points(smile, SMILE_POINTS, sizeof(SMILE_POINTS) / sizeof(SMILE_POINTS[0]));
    lv_obj_set_style_line_width(smile, 4, 0);
    lv_obj_set_style_line_color(smile, lv_color_hex(0x2A1E3D), 0);
    lv_obj_set_style_line_rounded(smile, true, 0);

    make_label(s_celebrate_card, &app_font_14, COLOR_TEXT, 0, 140, 98,
               LV_TEXT_ALIGN_CENTER, TETRIS_STR_CELEBRATE_SUB);
    make_label(s_celebrate_card, &app_font_20, COLOR_MINT, 0, 170, 98,
               LV_TEXT_ALIGN_CENTER, TETRIS_STR_CELEBRATE_CHEER);
    lv_obj_add_flag(s_celebrate_card, LV_OBJ_FLAG_HIDDEN);

    // 难度提升提示:盖在右侧信息栏上,不遮挡棋盘,数秒后自动消失。
    s_level_banner = make_panel(s_screen, 126, 146, 106, 124, 12,
                                COLOR_CARD, COLOR_AMBER, 2);
    make_label(s_level_banner, &app_font_20, COLOR_AMBER, 0, 8, 106,
               LV_TEXT_ALIGN_CENTER, TETRIS_STR_DIFFICULTY_UP);
    make_label(s_level_banner, &app_font_14, COLOR_TEXT, 0, 42, 106,
               LV_TEXT_ALIGN_CENTER, TETRIS_STR_SPEED_UP);
    make_label(s_level_banner, &app_font_14, COLOR_TEXT, 0, 62, 106,
               LV_TEXT_ALIGN_CENTER, TETRIS_STR_GHOST_OFF);
    make_label(s_level_banner, &app_font_14, COLOR_MINT, 0, 88, 106,
               LV_TEXT_ALIGN_CENTER, TETRIS_STR_GARBAGE_ADD);
    lv_obj_add_flag(s_level_banner, LV_OBJ_FLAG_HIDDEN);

    s_pause_overlay = make_overlay_root(s_screen);
    lv_obj_t *pause_card = make_panel(s_pause_overlay, 24, 62, 192, 194, 12,
                                      COLOR_CARD, COLOR_AMBER, 2);
    make_label(pause_card, &app_font_20, COLOR_AMBER, 0, 10, 188,
               LV_TEXT_ALIGN_CENTER, TETRIS_STR_PAUSED);
    static const char *const PAUSE_TEXT[3] = {
        TETRIS_STR_MENU_CONTINUE, TETRIS_STR_SETTINGS, TETRIS_STR_MENU_EXIT,
    };
    for (int i = 0; i < 3; i++) {
        s_pause_rows[i] = make_panel(pause_card, 16, 44 + i * 38, 160, 32, 8,
                                     COLOR_CARD_DARK, COLOR_BORDER, 1);
        s_pause_labels[i] = make_label(s_pause_rows[i], &app_font_14, COLOR_TEXT,
                                       0, 8, 156, LV_TEXT_ALIGN_CENTER,
                                       PAUSE_TEXT[i]);
    }
    make_label(pause_card, &app_font_14, COLOR_DIM, 0, 162, 188,
               LV_TEXT_ALIGN_CENTER, TETRIS_STR_MENU_HINT);

    s_over_overlay = make_overlay_root(s_screen);
    lv_obj_t *over_card = make_panel(s_over_overlay, 24, 76, 192, 168, 12,
                                     COLOR_CARD, COLOR_ROSE, 2);
    make_label(over_card, &app_font_20, COLOR_ROSE, 0, 8, 188,
               LV_TEXT_ALIGN_CENTER, TETRIS_STR_GAME_OVER);
    s_over_score = make_label(over_card, &app_font_20, COLOR_TEXT, 0, 36, 188,
                              LV_TEXT_ALIGN_CENTER, "0");
    s_record_label = make_label(over_card, &app_font_14, COLOR_AMBER, 0, 66, 188,
                                LV_TEXT_ALIGN_CENTER, TETRIS_STR_NEW_RECORD);
    lv_obj_add_flag(s_record_label, LV_OBJ_FLAG_HIDDEN);
    static const char *const OVER_TEXT[2] = {
        TETRIS_STR_MENU_AGAIN, TETRIS_STR_MENU_BACK,
    };
    for (int i = 0; i < 2; i++) {
        s_over_rows[i] = make_panel(over_card, 20, 88 + i * 36, 152, 32, 8,
                                    COLOR_CARD_DARK, COLOR_BORDER, 1);
        s_over_labels[i] = make_label(s_over_rows[i], &app_font_14, COLOR_TEXT,
                                      0, 8, 148, LV_TEXT_ALIGN_CENTER,
                                      OVER_TEXT[i]);
    }
}

// ---------------------------------------------------------------------------
// 状态刷新
// ---------------------------------------------------------------------------

static void update_stats(void) {
    lv_label_set_text_fmt(s_score_value, "%lu", (unsigned long)s_model.score);
    lv_label_set_text_fmt(s_lines_value, "%u", (unsigned)s_model.lines);
    lv_label_set_text_fmt(s_level_value, "%u", (unsigned)s_model.level);
    lv_label_set_text_fmt(s_best_value, "%lu", (unsigned long)s_best_score);
}

static void update_start_menu(void) {
    const bool has_save = s_save_available;
    for (int i = 0; i < 3; i++) {
        style_choice(s_menu_items[i], s_menu_item_labels[i],
                     s_start_menu.selected == i, i == 0 || i == 2 || has_save);
    }
    if (has_save) {
        lv_label_set_text_fmt(s_menu_continue_detail, "%s %lu  %s %u",
                              TETRIS_STR_SCORE, (unsigned long)s_saved_model.score,
                              TETRIS_STR_LINES, (unsigned)s_saved_model.lines);
    } else {
        lv_label_set_text(s_menu_continue_detail, TETRIS_STR_MENU_NO_SAVE);
    }
}

static void update_pause_menu(void) {
    for (int i = 0; i < 3; i++) {
        style_choice(s_pause_rows[i], s_pause_labels[i],
                     s_pause_menu.selected == i, true);
    }
}

static void update_settings_ui(void) {
    lv_label_set_text(s_settings_sound_value,
                      s_sound_on ? TETRIS_STR_ON : TETRIS_STR_OFF);
    lv_obj_set_style_text_color(s_settings_sound_value,
                                lv_color_hex(s_sound_on ? COLOR_MINT : COLOR_ROSE), 0);
    lv_label_set_text_fmt(s_settings_volume_value, "%u", (unsigned)s_volume);
    lv_obj_set_style_text_color(s_settings_volume_value,
                                lv_color_hex(s_sound_on ? COLOR_AMBER : COLOR_DIM), 0);
    lv_obj_set_width(s_settings_volume_fill, (168 * s_volume) / 100);
    lv_obj_set_style_bg_opa(s_settings_volume_fill,
                            s_sound_on ? LV_OPA_COVER : LV_OPA_30, 0);
}

static void update_over_menu(void) {
    for (int i = 0; i < 2; i++) {
        style_choice(s_over_rows[i], s_over_labels[i],
                     s_over_menu.selected == i, true);
    }
}

static void update_overlays(void) {
    if (!s_in_menu && s_model.state == TETRIS_PAUSED) {
        lv_obj_remove_flag(s_pause_overlay, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(s_pause_overlay, LV_OBJ_FLAG_HIDDEN);
    }
    if (!s_in_menu && s_model.state == TETRIS_GAME_OVER) {
        lv_obj_remove_flag(s_over_overlay, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(s_over_overlay, LV_OBJ_FLAG_HIDDEN);
    }
}

static void hide_celebration(void) {
    s_celebrate_until_ms = 0;
    if (s_celebrate_card) lv_obj_add_flag(s_celebrate_card, LV_OBJ_FLAG_HIDDEN);
}

static void show_celebration(void) {
    s_celebrate_until_ms = app_now_ms() + CELEBRATE_MS;
    lv_obj_remove_flag(s_celebrate_card, LV_OBJ_FLAG_HIDDEN);
}

static void hide_level_banner(void) {
    s_level_banner_until_ms = 0;
    if (s_level_banner) lv_obj_add_flag(s_level_banner, LV_OBJ_FLAG_HIDDEN);
}

static void show_level_banner(void) {
    s_level_banner_until_ms = app_now_ms() + LEVEL_BANNER_MS;
    lv_obj_remove_flag(s_level_banner, LV_OBJ_FLAG_HIDDEN);
}

static void refresh_all(void) {
    render_board();
    render_next();
    update_stats();
    update_overlays();
}

static void battery_apply(lv_obj_t *label, lv_obj_t *fill) {
    if (!label || !fill) return;
    if (s_battery_soc < 0) {
        lv_label_set_text(label, "--");
        lv_obj_set_width(fill, 0);
        return;
    }

    lv_label_set_text_fmt(label, "%d%%", s_battery_soc);
    int width = (24 * s_battery_soc) / 100;
    if (width < 1 && s_battery_soc > 0) width = 1;
    lv_obj_set_width(fill, width);

    uint32_t color = COLOR_MINT;
    if (s_battery_soc <= 20) {
        color = COLOR_ROSE;
    } else if (s_battery_soc <= 50) {
        color = COLOR_AMBER;
    }
    lv_obj_set_style_bg_color(fill, lv_color_hex(color), 0);
}

static void update_battery(void) {
    if (s_battery_soc == s_battery_shown) return;
    s_battery_shown = s_battery_soc;
    battery_apply(s_battery_label, s_battery_fill);
    battery_apply(s_menu_battery_label, s_menu_battery_fill);
    battery_apply(s_settings_battery_label, s_settings_battery_fill);
}

static void log_font_coverage(const lv_font_t *font, const char *name) {
    uint32_t codepoints[64];
    int missing = 0;
    for (size_t i = 0; i < TETRIS_UI_STRING_COUNT; i++) {
        const size_t count = tetris_utf8_decode(TETRIS_UI_STRINGS[i],
                                                codepoints, 64);
        for (size_t j = 0; j < count && j < 64; j++) {
            lv_font_glyph_dsc_t dsc;
            if (!lv_font_get_glyph_dsc(font, &dsc, codepoints[j], 0) ||
                dsc.is_placeholder) {
                ESP_LOGW(TAG, "%s 缺少字形 U+%04lX", name,
                         (unsigned long)codepoints[j]);
                missing++;
            }
        }
    }
    if (missing == 0) ESP_LOGI(TAG, "%s 中文字形覆盖检查通过", name);
}

// ---------------------------------------------------------------------------
// 界面切换
// ---------------------------------------------------------------------------

static void show_menu(void) {
    s_in_menu = true;
    s_in_settings = false;
    tetris_menu_init(&s_start_menu, 3, s_save_available ? 1 : 0);
    update_start_menu();
    hide_celebration();
    hide_level_banner();
    if (s_flash_overlay) lv_obj_add_flag(s_flash_overlay, LV_OBJ_FLAG_HIDDEN);
    s_flash_until_ms = 0;
    update_overlays();
    lv_screen_load(s_menu_screen);
}

static void enter_settings(bool from_game) {
    s_in_settings = true;
    s_settings_from_game = from_game;
    tetris_settings_reset(&s_settings);
    update_settings_ui();
    tetris_audio_play(TETRIS_SOUND_MENU_CONFIRM);
    lv_screen_load(s_settings_screen);
}

static void leave_settings(void) {
    s_in_settings = false;
    tetris_settings_reset(&s_settings);
    lv_screen_load(s_settings_from_game ? s_screen : s_menu_screen);
    tetris_audio_play(TETRIS_SOUND_MENU_CONFIRM);
}

static void change_volume(bool up) {
    const uint8_t next = tetris_settings_volume_step(s_volume, up);
    if (next == s_volume) return;
    s_volume = next;
    tetris_audio_set_volume(s_volume);
    settings_save();
    update_settings_ui();
}

static void start_new_game(void) {
    tetris_model_init(&s_model, (uint32_t)app_now_ms());
    tetris_input_reset(&s_input);
    s_fast_drop = false;
    s_flash_until_ms = 0;
    s_last_gravity_ms = app_now_ms();
    s_last_save_ms = s_last_gravity_ms;
    s_save_pending = false;
    hide_celebration();
    hide_level_banner();
    lv_obj_add_flag(s_flash_overlay, LV_OBJ_FLAG_HIDDEN);
    request_save();
    s_saved_model = s_model;
    s_save_available = true;
    s_in_menu = false;
    refresh_all();
    lv_screen_load(s_screen);
    tetris_audio_play(TETRIS_SOUND_MENU_CONFIRM);
}

static void continue_game(void) {
    if (!s_save_available) {
        tetris_audio_play(TETRIS_SOUND_ERROR);
        return;
    }
    s_model = s_saved_model;
    s_model.state = TETRIS_PLAYING;
    tetris_input_reset(&s_input);
    s_fast_drop = false;
    s_flash_until_ms = 0;
    s_last_gravity_ms = app_now_ms();
    s_last_save_ms = s_last_gravity_ms;
    s_save_pending = false;
    hide_celebration();
    hide_level_banner();
    lv_obj_add_flag(s_flash_overlay, LV_OBJ_FLAG_HIDDEN);
    s_in_menu = false;
    refresh_all();
    lv_screen_load(s_screen);
    tetris_audio_play(TETRIS_SOUND_MENU_CONFIRM);
}

static void enter_pause(void) {
    if (s_model.state != TETRIS_PLAYING) return;
    s_model.state = TETRIS_PAUSED;
    s_fast_drop = false;
    s_flash_until_ms = 0;
    hide_level_banner();
    lv_obj_add_flag(s_flash_overlay, LV_OBJ_FLAG_HIDDEN);
    request_save();
    s_saved_model = s_model;
    s_save_available = true;
    s_save_pending = false;
    s_last_save_ms = app_now_ms();
    tetris_menu_init(&s_pause_menu, 3, 0);
    update_pause_menu();
    update_overlays();
}

static void resume_game(void) {
    if (s_model.state != TETRIS_PAUSED) return;
    s_model.state = TETRIS_PLAYING;
    tetris_input_reset(&s_input);
    s_last_gravity_ms = app_now_ms();
    update_overlays();
    tetris_audio_play(TETRIS_SOUND_MENU_CONFIRM);
}

static void exit_to_menu(void) {
    request_save();
    s_saved_model = s_model;
    s_save_available = true;
    s_save_pending = false;
    s_last_save_ms = app_now_ms();
    s_fast_drop = false;
    tetris_audio_play(TETRIS_SOUND_MENU_CONFIRM);
    show_menu();
}

// ---------------------------------------------------------------------------
// 游戏事件与输入
// ---------------------------------------------------------------------------

static void handle_game_event(tetris_event_t event, uint16_t cleared,
                              uint8_t level_before) {
    switch (event) {
    case TETRIS_EVENT_MOVED:
        render_board();
        break;
    case TETRIS_EVENT_LOCKED:
        render_board();
        render_next();
        update_stats();
        s_save_pending = true;
        tetris_audio_play(TETRIS_SOUND_LOCK);
        break;
    case TETRIS_EVENT_LINE_CLEAR:
        s_flash_until_ms = app_now_ms() + FLASH_MS;
        lv_obj_remove_flag(s_flash_overlay, LV_OBJ_FLAG_HIDDEN);
        refresh_all();
        s_save_pending = true;
        if (cleared >= 4) {
            tetris_audio_play(TETRIS_SOUND_TETRIS);
        } else {
            tetris_audio_play(TETRIS_SOUND_LINE_CLEAR);
        }
        if (s_model.level > level_before) {
            // 难度提示优先于庆祝贴纸,避免两块卡片重叠。
            hide_celebration();
            show_level_banner();
            tetris_audio_play(TETRIS_SOUND_LEVEL_UP);
            ESP_LOGI(TAG, "难度提升: 行数=%u 难度=%u 下落间隔=%lu ms 落点预览=%d",
                     (unsigned)s_model.lines, (unsigned)s_model.level,
                     (unsigned long)tetris_model_drop_interval_ms(&s_model),
                     tetris_model_ghost_enabled(&s_model) ? 1 : 0);
        } else if (cleared >= 4) {
            show_celebration();
        }
        break;
    case TETRIS_EVENT_GAME_OVER: {
        s_fast_drop = false;
        hide_level_banner();
        request_erase();
        s_save_available = false;
        s_save_pending = false;
        memset(&s_saved_model, 0, sizeof(s_saved_model));
        const bool new_record = s_model.score > s_best_score;
        if (new_record) {
            s_best_score = s_model.score;
            best_save(s_best_score);
        }
        lv_label_set_text_fmt(s_over_score, "%s %lu", TETRIS_STR_FINAL_SCORE,
                              (unsigned long)s_model.score);
        if (new_record) {
            lv_obj_remove_flag(s_record_label, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(s_record_label, LV_OBJ_FLAG_HIDDEN);
        }
        tetris_menu_init(&s_over_menu, 2, 0);
        update_over_menu();
        refresh_all();
        tetris_audio_play(TETRIS_SOUND_GAME_OVER);
        break;
    }
    case TETRIS_EVENT_NONE:
    default:
        break;
    }
}

static void apply_action(tetris_action_t action) {
    switch (action) {
    case TETRIS_ACTION_MOVE_LEFT:
        if (tetris_model_move(&s_model, -1)) render_board();
        break;
    case TETRIS_ACTION_MOVE_RIGHT:
        if (tetris_model_move(&s_model, 1)) render_board();
        break;
    case TETRIS_ACTION_ROTATE:
        if (tetris_model_rotate(&s_model)) render_board();
        break;
    case TETRIS_ACTION_FAST_DROP_START:
        if (s_model.state == TETRIS_PLAYING) {
            s_fast_drop = true;
            s_last_gravity_ms = app_now_ms();
        }
        break;
    case TETRIS_ACTION_FAST_DROP_STOP:
        s_fast_drop = false;
        s_last_gravity_ms = app_now_ms();
        break;
    case TETRIS_ACTION_PAUSE_TOGGLE:
        enter_pause();
        break;
    case TETRIS_ACTION_NONE:
    default:
        break;
    }
}

static void handle_key(const key_event_t *key) {
    const tetris_button_t btn = (tetris_button_t)key->btn;
    const tetris_button_event_t ev = (tetris_button_event_t)key->ev;

    if (s_in_settings) {
        switch (tetris_settings_handle(&s_settings, btn, ev, s_sound_on)) {
        case TETRIS_SETTINGS_ACTION_TOGGLE_SOUND:
            s_sound_on = !s_sound_on;
            tetris_audio_set_enabled(s_sound_on);
            settings_save();
            update_settings_ui();
            if (s_sound_on) tetris_audio_play(TETRIS_SOUND_MENU_CONFIRM);
            break;
        case TETRIS_SETTINGS_ACTION_VOLUME_UP:
            change_volume(true);
            break;
        case TETRIS_SETTINGS_ACTION_VOLUME_DOWN:
            change_volume(false);
            break;
        case TETRIS_SETTINGS_ACTION_EXIT:
            leave_settings();
            break;
        default:
            break;
        }
        return;
    }

    if (s_in_menu) {
        const tetris_menu_action_t action = tetris_menu_handle(&s_start_menu, btn, ev);
        if (action == TETRIS_MENU_ACTION_PREV || action == TETRIS_MENU_ACTION_NEXT) {
            update_start_menu();
        } else if (action == TETRIS_MENU_ACTION_SELECT) {
            if (s_start_menu.selected == 0) {
                start_new_game();
            } else if (s_start_menu.selected == 1) {
                continue_game();
            } else {
                enter_settings(false);
            }
        }
        return;
    }

    if (s_model.state == TETRIS_PLAYING) {
        apply_action(tetris_input_handle(&s_input, btn, ev, s_model.state));
        return;
    }

    if (s_model.state == TETRIS_PAUSED) {
        if (btn == TETRIS_BTN_UP && ev == TETRIS_BTN_LONG) {
            resume_game();
            return;
        }
        const tetris_menu_action_t action = tetris_menu_handle(&s_pause_menu, btn, ev);
        if (action == TETRIS_MENU_ACTION_PREV || action == TETRIS_MENU_ACTION_NEXT) {
            update_pause_menu();
        } else if (action == TETRIS_MENU_ACTION_SELECT) {
            if (s_pause_menu.selected == 0) {
                resume_game();
            } else if (s_pause_menu.selected == 1) {
                enter_settings(true);
            } else {
                exit_to_menu();
            }
        }
        return;
    }

    // TETRIS_GAME_OVER
    const tetris_menu_action_t action = tetris_menu_handle(&s_over_menu, btn, ev);
    if (action == TETRIS_MENU_ACTION_PREV || action == TETRIS_MENU_ACTION_NEXT) {
        update_over_menu();
    } else if (action == TETRIS_MENU_ACTION_SELECT) {
        if (s_over_menu.selected == 0) {
            start_new_game();
        } else {
            show_menu();
        }
    }
}

static void game_timer_cb(lv_timer_t *timer) {
    (void)timer;

    key_event_t key;
    while (s_key_queue && xQueueReceive(s_key_queue, &key, 0) == pdTRUE) {
        if (key.btn > (uint8_t)TETRIS_BTN_OK ||
            key.ev > (uint8_t)TETRIS_BTN_RELEASE) {
            continue;
        }
        handle_key(&key);
    }

    const uint64_t now = app_now_ms();
    if (s_flash_until_ms != 0 && now >= s_flash_until_ms) {
        s_flash_until_ms = 0;
        lv_obj_add_flag(s_flash_overlay, LV_OBJ_FLAG_HIDDEN);
    }
    if (s_celebrate_until_ms != 0 && now >= s_celebrate_until_ms) {
        hide_celebration();
    }
    if (s_level_banner_until_ms != 0 && now >= s_level_banner_until_ms) {
        hide_level_banner();
    }

    if (!s_in_menu && s_model.state == TETRIS_PLAYING) {
        const uint32_t interval = s_fast_drop
            ? FAST_DROP_MS
            : tetris_model_drop_interval_ms(&s_model);
        if (now - s_last_gravity_ms >= interval) {
            s_last_gravity_ms = now;
            const uint16_t lines_before = s_model.lines;
            const uint8_t level_before = s_model.level;
            const tetris_event_t event = tetris_model_step(&s_model);
            handle_game_event(event,
                              (uint16_t)(s_model.lines - lines_before),
                              level_before);
        }
    }

    if (s_save_pending && now - s_last_save_ms >= SAVE_INTERVAL_MS) {
        s_save_pending = false;
        s_last_save_ms = now;
        request_save();
    }

    update_battery();
}

static void on_button(bsp_btn_t btn, bsp_btn_ev_t ev, void *user) {
    (void)user;
    // 先消费守卫再检查队列就绪:release 可能早于队列可用,不能在检查处丢掉。
    const bool was_armed = s_key_guard.armed;
    if (key_guard_consume(&s_key_guard, ev == BSP_BTN_RELEASE, esp_timer_get_time())) {
        if (was_armed && !s_key_guard.armed) {
            ESP_LOGI(TAG, "唤醒键已松手,按键恢复响应");
        }
        return;
    }
    if (!s_key_queue) return;
    if (ev == BSP_BTN_PRESS) {
        tetris_audio_play(TETRIS_SOUND_KEY);
    }
    const key_event_t key = { .btn = (uint8_t)btn, .ev = (uint8_t)ev };
    (void)xQueueSend(s_key_queue, &key, 0);
}

// ---------------------------------------------------------------------------
// 启动
// ---------------------------------------------------------------------------

void tetris_app_start(void) {
    esp_err_t nvs_err = nvs_flash_init();
    if (nvs_err == ESP_ERR_NVS_NO_FREE_PAGES ||
        nvs_err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "NVS 不可用(%s),最高分和存档只在本次运行内保留",
                 esp_err_to_name(nvs_err));
    } else if (nvs_err != ESP_OK) {
        ESP_LOGW(TAG, "NVS 初始化失败: %s", esp_err_to_name(nvs_err));
    }
    s_best_score = best_load();
    settings_load();
    s_save_available = load_saved_game(&s_saved_model);
    if (s_save_available) {
        ESP_LOGI(TAG, "发现未完成存档:分数=%lu 行数=%u",
                 (unsigned long)s_saved_model.score,
                 (unsigned)s_saved_model.lines);
    }

    s_battery_soc = -1;
    if (bsp_battery_init() == ESP_OK) {
        if (xTaskCreate(battery_task, "tetris_batt", 3072, NULL, 3, NULL) != pdPASS) {
            ESP_LOGW(TAG, "电量采样任务创建失败");
            s_battery_soc = -1;
        }
    } else {
        ESP_LOGW(TAG, "电量计不可用,界面显示 --");
    }

    const uint32_t wake_causes = esp_sleep_get_wakeup_causes();
    if (wake_causes & (1u << ESP_SLEEP_WAKEUP_GPIO)) {
        key_guard_arm(&s_key_guard, esp_timer_get_time(), KEY_GUARD_MAX_US);
        ESP_LOGI(TAG, "按键唤醒:先屏蔽按键动作,直到那颗键松手");
    }

    s_key_queue = xQueueCreate(KEY_QUEUE_DEPTH, sizeof(key_event_t));
    const esp_err_t button_err = bsp_button_init(on_button, NULL);
    s_input_available = s_key_queue != NULL && button_err == ESP_OK;
    if (!s_input_available) {
        ESP_LOGE(TAG, "按键不可用(queue=%p, err=%s)", (void *)s_key_queue,
                 esp_err_to_name(button_err));
    } else if (s_key_guard.armed) {
        // 唤醒键可能在按键驱动启动前已松手,组件不会产生 release;按电压同步撤防。
        const bool released = bsp_button_read_mv() >= BSP_BTN_MV_RELEASED_MIN;
        key_guard_sync_released(&s_key_guard, released);
        if (!s_key_guard.armed) {
            ESP_LOGI(TAG, "唤醒键在按键驱动启动前已松手,提前撤防");
        }
    }

    // NVS 写入放在独立任务,避免消行/自动存档时卡住 LVGL 任务。
    s_save_queue = xQueueCreate(1, sizeof(save_msg_t));
    if (!s_save_queue ||
        xTaskCreate(save_task, "tetris_save", 3072, NULL, 2, NULL) != pdPASS) {
        ESP_LOGW(TAG, "存档任务创建失败,存档改为立即写入");
        if (s_save_queue) {
            vQueueDelete(s_save_queue);
            s_save_queue = NULL;
        }
    }

    tetris_audio_start();
    tetris_audio_set_enabled(s_sound_on);
    tetris_audio_set_volume(s_volume);

    tetris_model_init(&s_model, (uint32_t)app_now_ms());
    tetris_input_reset(&s_input);
    tetris_menu_init(&s_start_menu, 3, 0);
    tetris_menu_init(&s_pause_menu, 3, 0);
    tetris_menu_init(&s_over_menu, 2, 0);
    s_last_gravity_ms = app_now_ms();
    s_last_save_ms = s_last_gravity_ms;

    if (!bsp_lvgl_lock(1000)) {
        ESP_LOGE(TAG, "LVGL 加锁超时,界面未创建");
        return;
    }
    build_game_ui();
    build_menu_ui();
    build_settings_ui();
    refresh_all();
    update_start_menu();
    update_pause_menu();
    update_over_menu();
    update_settings_ui();
    show_menu();
    log_font_coverage(&app_font_14, "14px");
    log_font_coverage(&app_font_20, "20px");
    lv_timer_create(game_timer_cb, GAME_TIMER_MS, NULL);
    lv_mem_monitor_t memory;
    lv_mem_monitor(&memory);
    bsp_lvgl_unlock();

    ESP_LOGI(TAG, "俄罗斯方块就绪:按键=%d 最高分=%lu 存档=%d LVGL剩余=%u字节",
             s_input_available, (unsigned long)s_best_score, s_save_available,
             (unsigned)memory.free_size);
}

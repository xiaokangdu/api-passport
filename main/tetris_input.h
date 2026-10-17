#pragma once

#include <stdbool.h>

#include "tetris_model.h"

// 与 bsp_btn_t / bsp_btn_ev_t 的取值一一对应;应用层用 _Static_assert 校验,
// 这里不直接包含 ESP-IDF 头文件,便于在主机上做纯逻辑测试。
typedef enum {
    TETRIS_BTN_UP = 0,
    TETRIS_BTN_DOWN,
    TETRIS_BTN_OK,
} tetris_button_t;

typedef enum {
    TETRIS_BTN_PRESS = 0,
    TETRIS_BTN_CLICK,
    TETRIS_BTN_DOUBLE,
    TETRIS_BTN_LONG,
    TETRIS_BTN_RELEASE,
} tetris_button_event_t;

typedef enum {
    TETRIS_ACTION_NONE = 0,
    TETRIS_ACTION_MOVE_LEFT,
    TETRIS_ACTION_MOVE_RIGHT,
    TETRIS_ACTION_ROTATE,
    TETRIS_ACTION_FAST_DROP_START,
    TETRIS_ACTION_FAST_DROP_STOP,
    TETRIS_ACTION_PAUSE_TOGGLE,
} tetris_action_t;

typedef struct {
    bool ok_down;
    bool ok_long;
} tetris_input_state_t;

void tetris_input_reset(tetris_input_state_t *state);

// 本模块只映射进行中的游戏操作;暂停、结束和开始界面由 tetris_menu 处理。
// 上键按下 = 左移;下键按下 = 右移;确定键短按 = 旋转;
// 确定键长按 = 加速下落,松手停止;上键长按 = 暂停。
// 方向键用 PRESS 而不是 CLICK,避免 180ms 的单击判定延迟。
tetris_action_t tetris_input_handle(tetris_input_state_t *state,
                                    tetris_button_t btn,
                                    tetris_button_event_t event,
                                    tetris_state_t game_state);

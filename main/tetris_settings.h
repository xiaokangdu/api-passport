#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "tetris_input.h"

#define TETRIS_VOLUME_STEP 5U

// 声音设置页的按键语义:确定键短按切换声音开关,声音开启时上/下调整音量,
// 长按确定返回。纯逻辑,便于主机测试。
typedef enum {
    TETRIS_SETTINGS_ACTION_NONE = 0,
    TETRIS_SETTINGS_ACTION_TOGGLE_SOUND,
    TETRIS_SETTINGS_ACTION_VOLUME_UP,
    TETRIS_SETTINGS_ACTION_VOLUME_DOWN,
    TETRIS_SETTINGS_ACTION_EXIT,
} tetris_settings_action_t;

typedef struct {
    bool ok_down;
    bool ok_long;
} tetris_settings_state_t;

void tetris_settings_reset(tetris_settings_state_t *state);

tetris_settings_action_t tetris_settings_handle(tetris_settings_state_t *state,
                                                tetris_button_t btn,
                                                tetris_button_event_t event,
                                                bool sound_on);

// 按 5% 步进调整并夹在 0..100。
uint8_t tetris_settings_volume_step(uint8_t volume, bool up);

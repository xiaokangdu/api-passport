#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "tetris_input.h"

// 通用列表菜单:上/下短按移动选择,确定键松手确认。
// 纯逻辑,不依赖 LVGL,便于主机测试。
typedef enum {
    TETRIS_MENU_ACTION_NONE = 0,
    TETRIS_MENU_ACTION_PREV,
    TETRIS_MENU_ACTION_NEXT,
    TETRIS_MENU_ACTION_SELECT,
} tetris_menu_action_t;

typedef struct {
    uint8_t count;
    uint8_t selected;
    bool ok_down;
} tetris_menu_t;

// count 至少为 1;selected 超出范围时夹到合法值。
void tetris_menu_init(tetris_menu_t *menu, uint8_t count, uint8_t selected);

// 上键/下键的 PRESS 循环移动选择;确定键 PRESS 只记录按下状态,
// RELEASE 且此前记录过按下时返回 SELECT。长按与重复事件被忽略。
tetris_menu_action_t tetris_menu_handle(tetris_menu_t *menu,
                                        tetris_button_t btn,
                                        tetris_button_event_t event);

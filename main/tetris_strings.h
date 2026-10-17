#pragma once

#include <stddef.h>
#include <stdint.h>

#define TETRIS_STR_TITLE          "俄罗斯方块"
#define TETRIS_STR_NEXT           "下一个"
#define TETRIS_STR_SCORE          "分数"
#define TETRIS_STR_LINES          "行数"
#define TETRIS_STR_LEVEL          "难度"
#define TETRIS_STR_BEST           "最高分"
#define TETRIS_STR_PAUSE_HINT     "长按上键暂停"
#define TETRIS_STR_CONTROLS       "上/下移动  确定旋转  长按加速"
#define TETRIS_STR_PAUSED         "已暂停"
#define TETRIS_STR_GAME_OVER      "游戏结束"
#define TETRIS_STR_FINAL_SCORE    "得分"
#define TETRIS_STR_NEW_RECORD     "新纪录！"
#define TETRIS_STR_INPUT_ERROR    "按键不可用"
#define TETRIS_STR_MENU_NEW       "开始新游戏"
#define TETRIS_STR_MENU_CONTINUE  "继续游戏"
#define TETRIS_STR_MENU_NO_SAVE   "暂无存档"
#define TETRIS_STR_MENU_HINT      "上/下选择  确定确认"
#define TETRIS_STR_MENU_EXIT      "退出游戏"
#define TETRIS_STR_MENU_BACK      "返回菜单"
#define TETRIS_STR_MENU_AGAIN     "再来一局"
#define TETRIS_STR_CELEBRATE      "恭喜！"
#define TETRIS_STR_CELEBRATE_SUB  "四行消除"
#define TETRIS_STR_CELEBRATE_CHEER "太棒了"
#define TETRIS_STR_SETTINGS       "设置"
#define TETRIS_STR_SOUND          "声音"
#define TETRIS_STR_VOLUME         "音量"
#define TETRIS_STR_ON             "开启"
#define TETRIS_STR_OFF            "关闭"
#define TETRIS_STR_SETTINGS_HINT1 "确定键开关声音"
#define TETRIS_STR_SETTINGS_HINT2 "上/下调节音量  长按确定返回"
#define TETRIS_STR_DIFFICULTY_UP  "难度提升！"
#define TETRIS_STR_SPEED_UP       "速度加快"
#define TETRIS_STR_GHOST_OFF      "关闭落点提示"
#define TETRIS_STR_GARBAGE_ADD    "新增障碍行"

extern const char *const TETRIS_UI_STRINGS[];
extern const size_t TETRIS_UI_STRING_COUNT;

// 返回解码出的 Unicode 码点数量;out 为 NULL 时只计数。
// 非法或截断的 UTF-8 序列会被跳过,不会越界读取。
size_t tetris_utf8_decode(const char *text, uint32_t *out, size_t out_cap);

// 输入状态机 host 测试:不依赖 ESP-IDF,直接验证进行中的三键映射与长按语义。
#include <assert.h>
#include <stdio.h>

#include "tetris_input.h"

static tetris_action_t handle(tetris_input_state_t *state, tetris_button_t btn,
                              tetris_button_event_t event, tetris_state_t game) {
    return tetris_input_handle(state, btn, event, game);
}

int main(void) {
    tetris_input_state_t s;
    tetris_input_reset(&s);
    assert(!s.ok_down);
    assert(!s.ok_long);

    // 上键短按在游戏中左移,其他状态无效。
    assert(handle(&s, TETRIS_BTN_UP, TETRIS_BTN_PRESS, TETRIS_PLAYING) ==
           TETRIS_ACTION_MOVE_LEFT);
    assert(handle(&s, TETRIS_BTN_UP, TETRIS_BTN_PRESS, TETRIS_PAUSED) ==
           TETRIS_ACTION_NONE);
    assert(handle(&s, TETRIS_BTN_UP, TETRIS_BTN_PRESS, TETRIS_GAME_OVER) ==
           TETRIS_ACTION_NONE);
    assert(handle(&s, TETRIS_BTN_UP, TETRIS_BTN_RELEASE, TETRIS_PLAYING) ==
           TETRIS_ACTION_NONE);

    // 上键长按在游戏中暂停;暂停后的长按由菜单层处理。
    assert(handle(&s, TETRIS_BTN_UP, TETRIS_BTN_LONG, TETRIS_PLAYING) ==
           TETRIS_ACTION_PAUSE_TOGGLE);
    assert(handle(&s, TETRIS_BTN_UP, TETRIS_BTN_LONG, TETRIS_PAUSED) ==
           TETRIS_ACTION_NONE);
    assert(handle(&s, TETRIS_BTN_UP, TETRIS_BTN_LONG, TETRIS_GAME_OVER) ==
           TETRIS_ACTION_NONE);

    // 下键短按在游戏中右移,其他状态无效。
    assert(handle(&s, TETRIS_BTN_DOWN, TETRIS_BTN_PRESS, TETRIS_PLAYING) ==
           TETRIS_ACTION_MOVE_RIGHT);
    assert(handle(&s, TETRIS_BTN_DOWN, TETRIS_BTN_PRESS, TETRIS_PAUSED) ==
           TETRIS_ACTION_NONE);
    assert(handle(&s, TETRIS_BTN_DOWN, TETRIS_BTN_PRESS, TETRIS_GAME_OVER) ==
           TETRIS_ACTION_NONE);
    assert(handle(&s, TETRIS_BTN_DOWN, TETRIS_BTN_RELEASE, TETRIS_PLAYING) ==
           TETRIS_ACTION_NONE);

    // 确定键短按:按下不动作,松手旋转。
    assert(handle(&s, TETRIS_BTN_OK, TETRIS_BTN_PRESS, TETRIS_PLAYING) ==
           TETRIS_ACTION_NONE);
    assert(s.ok_down);
    assert(handle(&s, TETRIS_BTN_OK, TETRIS_BTN_RELEASE, TETRIS_PLAYING) ==
           TETRIS_ACTION_ROTATE);
    assert(!s.ok_down);
    assert(!s.ok_long);

    // 确定键长按:开始加速下落;松手停止加速且不旋转。
    assert(handle(&s, TETRIS_BTN_OK, TETRIS_BTN_PRESS, TETRIS_PLAYING) ==
           TETRIS_ACTION_NONE);
    assert(handle(&s, TETRIS_BTN_OK, TETRIS_BTN_LONG, TETRIS_PLAYING) ==
           TETRIS_ACTION_FAST_DROP_START);
    assert(s.ok_down);
    assert(s.ok_long);
    assert(handle(&s, TETRIS_BTN_OK, TETRIS_BTN_RELEASE, TETRIS_PLAYING) ==
           TETRIS_ACTION_FAST_DROP_STOP);
    assert(!s.ok_down);
    assert(!s.ok_long);

    // 没有收到按下事件的长按不应触发动作。
    tetris_input_reset(&s);
    assert(handle(&s, TETRIS_BTN_OK, TETRIS_BTN_LONG, TETRIS_PLAYING) ==
           TETRIS_ACTION_NONE);

    // 暂停/结束状态不记录确定键按下;这些界面由菜单层处理。
    assert(handle(&s, TETRIS_BTN_OK, TETRIS_BTN_PRESS, TETRIS_PAUSED) ==
           TETRIS_ACTION_NONE);
    assert(!s.ok_down);
    assert(handle(&s, TETRIS_BTN_OK, TETRIS_BTN_RELEASE, TETRIS_PAUSED) ==
           TETRIS_ACTION_NONE);
    assert(handle(&s, TETRIS_BTN_OK, TETRIS_BTN_PRESS, TETRIS_GAME_OVER) ==
           TETRIS_ACTION_NONE);
    assert(handle(&s, TETRIS_BTN_OK, TETRIS_BTN_RELEASE, TETRIS_GAME_OVER) ==
           TETRIS_ACTION_NONE);

    // 游戏中按住确定后状态切到暂停:松手只清理状态,不产生动作。
    tetris_input_reset(&s);
    assert(handle(&s, TETRIS_BTN_OK, TETRIS_BTN_PRESS, TETRIS_PLAYING) ==
           TETRIS_ACTION_NONE);
    assert(s.ok_down);
    assert(handle(&s, TETRIS_BTN_OK, TETRIS_BTN_RELEASE, TETRIS_PAUSED) ==
           TETRIS_ACTION_NONE);
    assert(!s.ok_down);

    // 多余的 CLICK/DOUBLE 事件不产生动作。
    assert(handle(&s, TETRIS_BTN_UP, TETRIS_BTN_CLICK, TETRIS_PLAYING) ==
           TETRIS_ACTION_NONE);
    assert(handle(&s, TETRIS_BTN_DOWN, TETRIS_BTN_DOUBLE, TETRIS_PLAYING) ==
           TETRIS_ACTION_NONE);
    assert(handle(&s, TETRIS_BTN_OK, TETRIS_BTN_CLICK, TETRIS_PLAYING) ==
           TETRIS_ACTION_NONE);
    assert(handle(&s, TETRIS_BTN_OK, TETRIS_BTN_DOUBLE, TETRIS_PLAYING) ==
           TETRIS_ACTION_NONE);

    // 未知按键编号被忽略。
    assert(handle(&s, (tetris_button_t)9, TETRIS_BTN_PRESS, TETRIS_PLAYING) ==
           TETRIS_ACTION_NONE);

    puts("tetris input tests: PASS");
    return 0;
}

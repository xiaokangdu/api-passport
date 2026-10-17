// 声音设置页 host 测试:开关切换、音量步进、长按返回与无效事件。
#include <assert.h>
#include <stdio.h>

#include "tetris_settings.h"

static tetris_settings_action_t handle(tetris_settings_state_t *state,
                                       tetris_button_t btn,
                                       tetris_button_event_t event,
                                       bool sound_on) {
    return tetris_settings_handle(state, btn, event, sound_on);
}

int main(void) {
    tetris_settings_state_t s;
    tetris_settings_reset(&s);
    assert(!s.ok_down);
    assert(!s.ok_long);

    // 声音开启:上下键调整音量。
    assert(handle(&s, TETRIS_BTN_UP, TETRIS_BTN_PRESS, true) ==
           TETRIS_SETTINGS_ACTION_VOLUME_UP);
    assert(handle(&s, TETRIS_BTN_DOWN, TETRIS_BTN_PRESS, true) ==
           TETRIS_SETTINGS_ACTION_VOLUME_DOWN);
    // 声音关闭:上下键无效。
    assert(handle(&s, TETRIS_BTN_UP, TETRIS_BTN_PRESS, false) ==
           TETRIS_SETTINGS_ACTION_NONE);
    assert(handle(&s, TETRIS_BTN_DOWN, TETRIS_BTN_PRESS, false) ==
           TETRIS_SETTINGS_ACTION_NONE);
    // 方向键长按/松手不产生动作。
    assert(handle(&s, TETRIS_BTN_UP, TETRIS_BTN_LONG, true) ==
           TETRIS_SETTINGS_ACTION_NONE);
    assert(handle(&s, TETRIS_BTN_DOWN, TETRIS_BTN_RELEASE, true) ==
           TETRIS_SETTINGS_ACTION_NONE);

    // 确定键短按在松手时切换声音。
    assert(handle(&s, TETRIS_BTN_OK, TETRIS_BTN_PRESS, true) ==
           TETRIS_SETTINGS_ACTION_NONE);
    assert(s.ok_down);
    assert(handle(&s, TETRIS_BTN_OK, TETRIS_BTN_RELEASE, true) ==
           TETRIS_SETTINGS_ACTION_TOGGLE_SOUND);
    assert(!s.ok_down);

    // 没有按下事件的松手不切换。
    assert(handle(&s, TETRIS_BTN_OK, TETRIS_BTN_RELEASE, true) ==
           TETRIS_SETTINGS_ACTION_NONE);

    // 长按确定立即返回;松手不再切换声音。
    assert(handle(&s, TETRIS_BTN_OK, TETRIS_BTN_PRESS, true) ==
           TETRIS_SETTINGS_ACTION_NONE);
    assert(handle(&s, TETRIS_BTN_OK, TETRIS_BTN_LONG, true) ==
           TETRIS_SETTINGS_ACTION_EXIT);
    assert(handle(&s, TETRIS_BTN_OK, TETRIS_BTN_RELEASE, true) ==
           TETRIS_SETTINGS_ACTION_NONE);

    // 未按下时的长按不返回。
    tetris_settings_reset(&s);
    assert(handle(&s, TETRIS_BTN_OK, TETRIS_BTN_LONG, true) ==
           TETRIS_SETTINGS_ACTION_NONE);

    // 音量 5% 步进并在 0..100 夹紧。
    assert(tetris_settings_volume_step(55, true) == 60);
    assert(tetris_settings_volume_step(55, false) == 50);
    assert(tetris_settings_volume_step(100, true) == 100);
    assert(tetris_settings_volume_step(97, true) == 100);
    assert(tetris_settings_volume_step(0, false) == 0);
    assert(tetris_settings_volume_step(3, false) == 0);
    assert(tetris_settings_volume_step(0, true) == TETRIS_VOLUME_STEP);

    // 未知按键与空指针安全。
    assert(handle(&s, (tetris_button_t)9, TETRIS_BTN_PRESS, true) ==
           TETRIS_SETTINGS_ACTION_NONE);
    assert(tetris_settings_handle(NULL, TETRIS_BTN_UP, TETRIS_BTN_PRESS, true) ==
           TETRIS_SETTINGS_ACTION_NONE);
    tetris_settings_reset(NULL);

    puts("tetris settings tests: PASS");
    return 0;
}

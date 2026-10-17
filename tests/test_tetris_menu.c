// 列表菜单逻辑 host 测试:循环选择、确定键松手确认、无效事件忽略。
#include <assert.h>
#include <stdio.h>

#include "tetris_menu.h"

int main(void) {
    tetris_menu_t menu;

    // 两个选项:上下循环移动。
    tetris_menu_init(&menu, 2, 0);
    assert(menu.count == 2);
    assert(menu.selected == 0);
    assert(!menu.ok_down);

    assert(tetris_menu_handle(&menu, TETRIS_BTN_DOWN, TETRIS_BTN_PRESS) ==
           TETRIS_MENU_ACTION_NEXT);
    assert(menu.selected == 1);
    assert(tetris_menu_handle(&menu, TETRIS_BTN_DOWN, TETRIS_BTN_PRESS) ==
           TETRIS_MENU_ACTION_NEXT);
    assert(menu.selected == 0);
    assert(tetris_menu_handle(&menu, TETRIS_BTN_UP, TETRIS_BTN_PRESS) ==
           TETRIS_MENU_ACTION_PREV);
    assert(menu.selected == 1);
    assert(tetris_menu_handle(&menu, TETRIS_BTN_UP, TETRIS_BTN_PRESS) ==
           TETRIS_MENU_ACTION_PREV);
    assert(menu.selected == 0);

    // 确定键:按下不确认,松手才返回 SELECT。
    assert(tetris_menu_handle(&menu, TETRIS_BTN_OK, TETRIS_BTN_PRESS) ==
           TETRIS_MENU_ACTION_NONE);
    assert(menu.ok_down);
    assert(tetris_menu_handle(&menu, TETRIS_BTN_OK, TETRIS_BTN_RELEASE) ==
           TETRIS_MENU_ACTION_SELECT);
    assert(!menu.ok_down);

    // 没看到按下事件的松手不确认(例如长按加速结束后松手)。
    assert(tetris_menu_handle(&menu, TETRIS_BTN_OK, TETRIS_BTN_RELEASE) ==
           TETRIS_MENU_ACTION_NONE);

    // 长按、单击、双击事件不改变选择也不确认。
    assert(tetris_menu_handle(&menu, TETRIS_BTN_OK, TETRIS_BTN_LONG) ==
           TETRIS_MENU_ACTION_NONE);
    assert(tetris_menu_handle(&menu, TETRIS_BTN_OK, TETRIS_BTN_CLICK) ==
           TETRIS_MENU_ACTION_NONE);
    assert(tetris_menu_handle(&menu, TETRIS_BTN_OK, TETRIS_BTN_DOUBLE) ==
           TETRIS_MENU_ACTION_NONE);
    assert(tetris_menu_handle(&menu, TETRIS_BTN_UP, TETRIS_BTN_RELEASE) ==
           TETRIS_MENU_ACTION_NONE);
    assert(tetris_menu_handle(&menu, TETRIS_BTN_DOWN, TETRIS_BTN_LONG) ==
           TETRIS_MENU_ACTION_NONE);
    assert(menu.selected == 0);

    // 只有一个选项时移动仍停留在 0,确认照常工作。
    tetris_menu_init(&menu, 1, 0);
    assert(tetris_menu_handle(&menu, TETRIS_BTN_UP, TETRIS_BTN_PRESS) ==
           TETRIS_MENU_ACTION_PREV);
    assert(menu.selected == 0);
    assert(tetris_menu_handle(&menu, TETRIS_BTN_DOWN, TETRIS_BTN_PRESS) ==
           TETRIS_MENU_ACTION_NEXT);
    assert(menu.selected == 0);
    assert(tetris_menu_handle(&menu, TETRIS_BTN_OK, TETRIS_BTN_PRESS) ==
           TETRIS_MENU_ACTION_NONE);
    assert(tetris_menu_handle(&menu, TETRIS_BTN_OK, TETRIS_BTN_RELEASE) ==
           TETRIS_MENU_ACTION_SELECT);

    // 非法选项夹到 0;count=0 视为 1;未知按键忽略。
    tetris_menu_init(&menu, 2, 9);
    assert(menu.selected == 0);
    tetris_menu_init(&menu, 0, 0);
    assert(menu.count == 1);
    assert(menu.selected == 0);
    assert(tetris_menu_handle(&menu, (tetris_button_t)9, TETRIS_BTN_PRESS) ==
           TETRIS_MENU_ACTION_NONE);

    // 菜单指针为空时安全返回。
    assert(tetris_menu_handle(NULL, TETRIS_BTN_UP, TETRIS_BTN_PRESS) ==
           TETRIS_MENU_ACTION_NONE);
    tetris_menu_init(NULL, 2, 0);

    puts("tetris menu tests: PASS");
    return 0;
}

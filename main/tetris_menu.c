#include "tetris_menu.h"

#include <stddef.h>

void tetris_menu_init(tetris_menu_t *menu, uint8_t count, uint8_t selected) {
    if (!menu) return;
    menu->count = count == 0 ? 1 : count;
    menu->selected = selected < menu->count ? selected : 0;
    menu->ok_down = false;
}

tetris_menu_action_t tetris_menu_handle(tetris_menu_t *menu,
                                        tetris_button_t btn,
                                        tetris_button_event_t event) {
    if (!menu) return TETRIS_MENU_ACTION_NONE;

    switch (btn) {
    case TETRIS_BTN_UP:
        if (event != TETRIS_BTN_PRESS) return TETRIS_MENU_ACTION_NONE;
        menu->selected = (uint8_t)((menu->selected + menu->count - 1U) % menu->count);
        return TETRIS_MENU_ACTION_PREV;

    case TETRIS_BTN_DOWN:
        if (event != TETRIS_BTN_PRESS) return TETRIS_MENU_ACTION_NONE;
        menu->selected = (uint8_t)((menu->selected + 1U) % menu->count);
        return TETRIS_MENU_ACTION_NEXT;

    case TETRIS_BTN_OK:
        if (event == TETRIS_BTN_PRESS) {
            menu->ok_down = true;
            return TETRIS_MENU_ACTION_NONE;
        }
        if (event == TETRIS_BTN_RELEASE && menu->ok_down) {
            menu->ok_down = false;
            return TETRIS_MENU_ACTION_SELECT;
        }
        return TETRIS_MENU_ACTION_NONE;

    default:
        return TETRIS_MENU_ACTION_NONE;
    }
}

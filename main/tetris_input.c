#include "tetris_input.h"

void tetris_input_reset(tetris_input_state_t *state) {
    state->ok_down = false;
    state->ok_long = false;
}

tetris_action_t tetris_input_handle(tetris_input_state_t *state,
                                    tetris_button_t btn,
                                    tetris_button_event_t event,
                                    tetris_state_t game_state) {
    switch (btn) {
    case TETRIS_BTN_UP:
        if (game_state != TETRIS_PLAYING) return TETRIS_ACTION_NONE;
        if (event == TETRIS_BTN_PRESS) return TETRIS_ACTION_MOVE_LEFT;
        if (event == TETRIS_BTN_LONG) return TETRIS_ACTION_PAUSE_TOGGLE;
        return TETRIS_ACTION_NONE;

    case TETRIS_BTN_DOWN:
        if (event == TETRIS_BTN_PRESS && game_state == TETRIS_PLAYING) {
            return TETRIS_ACTION_MOVE_RIGHT;
        }
        return TETRIS_ACTION_NONE;

    case TETRIS_BTN_OK:
        if (event == TETRIS_BTN_PRESS) {
            if (game_state == TETRIS_PLAYING) {
                state->ok_down = true;
                state->ok_long = false;
            }
            return TETRIS_ACTION_NONE;
        }
        if (event == TETRIS_BTN_LONG && state->ok_down &&
            game_state == TETRIS_PLAYING) {
            state->ok_long = true;
            return TETRIS_ACTION_FAST_DROP_START;
        }
        if (event == TETRIS_BTN_RELEASE) {
            if (!state->ok_down) return TETRIS_ACTION_NONE;
            const bool was_long = state->ok_long;
            state->ok_down = false;
            state->ok_long = false;
            if (game_state != TETRIS_PLAYING) return TETRIS_ACTION_NONE;
            return was_long ? TETRIS_ACTION_FAST_DROP_STOP : TETRIS_ACTION_ROTATE;
        }
        return TETRIS_ACTION_NONE;

    default:
        return TETRIS_ACTION_NONE;
    }
}

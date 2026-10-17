#include "tetris_settings.h"

#include <stddef.h>

void tetris_settings_reset(tetris_settings_state_t *state) {
    if (!state) return;
    state->ok_down = false;
    state->ok_long = false;
}

tetris_settings_action_t tetris_settings_handle(tetris_settings_state_t *state,
                                                tetris_button_t btn,
                                                tetris_button_event_t event,
                                                bool sound_on) {
    if (!state) return TETRIS_SETTINGS_ACTION_NONE;

    switch (btn) {
    case TETRIS_BTN_UP:
        if (event == TETRIS_BTN_PRESS && sound_on) {
            return TETRIS_SETTINGS_ACTION_VOLUME_UP;
        }
        return TETRIS_SETTINGS_ACTION_NONE;

    case TETRIS_BTN_DOWN:
        if (event == TETRIS_BTN_PRESS && sound_on) {
            return TETRIS_SETTINGS_ACTION_VOLUME_DOWN;
        }
        return TETRIS_SETTINGS_ACTION_NONE;

    case TETRIS_BTN_OK:
        if (event == TETRIS_BTN_PRESS) {
            state->ok_down = true;
            state->ok_long = false;
            return TETRIS_SETTINGS_ACTION_NONE;
        }
        if (event == TETRIS_BTN_LONG && state->ok_down) {
            state->ok_long = true;
            return TETRIS_SETTINGS_ACTION_EXIT;
        }
        if (event == TETRIS_BTN_RELEASE) {
            if (!state->ok_down) return TETRIS_SETTINGS_ACTION_NONE;
            const bool was_long = state->ok_long;
            state->ok_down = false;
            state->ok_long = false;
            return was_long ? TETRIS_SETTINGS_ACTION_NONE
                            : TETRIS_SETTINGS_ACTION_TOGGLE_SOUND;
        }
        return TETRIS_SETTINGS_ACTION_NONE;

    default:
        return TETRIS_SETTINGS_ACTION_NONE;
    }
}

uint8_t tetris_settings_volume_step(uint8_t volume, bool up) {
    if (up) {
        const unsigned next = (unsigned)volume + TETRIS_VOLUME_STEP;
        return (uint8_t)(next > 100U ? 100U : next);
    }
    return (uint8_t)(volume >= TETRIS_VOLUME_STEP ? volume - TETRIS_VOLUME_STEP : 0U);
}

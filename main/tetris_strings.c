#include "tetris_strings.h"

#include <stdbool.h>

const char *const TETRIS_UI_STRINGS[] = {
    TETRIS_STR_TITLE,
    TETRIS_STR_NEXT,
    TETRIS_STR_SCORE,
    TETRIS_STR_LINES,
    TETRIS_STR_LEVEL,
    TETRIS_STR_BEST,
    TETRIS_STR_PAUSE_HINT,
    TETRIS_STR_CONTROLS,
    TETRIS_STR_PAUSED,
    TETRIS_STR_GAME_OVER,
    TETRIS_STR_FINAL_SCORE,
    TETRIS_STR_NEW_RECORD,
    TETRIS_STR_INPUT_ERROR,
    TETRIS_STR_MENU_NEW,
    TETRIS_STR_MENU_CONTINUE,
    TETRIS_STR_MENU_NO_SAVE,
    TETRIS_STR_MENU_HINT,
    TETRIS_STR_MENU_EXIT,
    TETRIS_STR_MENU_BACK,
    TETRIS_STR_MENU_AGAIN,
    TETRIS_STR_CELEBRATE,
    TETRIS_STR_CELEBRATE_SUB,
    TETRIS_STR_CELEBRATE_CHEER,
    TETRIS_STR_SETTINGS,
    TETRIS_STR_SOUND,
    TETRIS_STR_VOLUME,
    TETRIS_STR_ON,
    TETRIS_STR_OFF,
    TETRIS_STR_SETTINGS_HINT1,
    TETRIS_STR_SETTINGS_HINT2,
    TETRIS_STR_DIFFICULTY_UP,
    TETRIS_STR_SPEED_UP,
    TETRIS_STR_GHOST_OFF,
    TETRIS_STR_GARBAGE_ADD,
};

const size_t TETRIS_UI_STRING_COUNT =
    sizeof(TETRIS_UI_STRINGS) / sizeof(TETRIS_UI_STRINGS[0]);

size_t tetris_utf8_decode(const char *text, uint32_t *out, size_t out_cap) {
    size_t count = 0;
    const unsigned char *p = (const unsigned char *)text;
    while (p && *p) {
        uint32_t cp;
        int extra;
        if (*p < 0x80) {
            cp = *p;
            extra = 0;
        } else if ((*p & 0xE0) == 0xC0) {
            cp = (uint32_t)(*p & 0x1F);
            extra = 1;
        } else if ((*p & 0xF0) == 0xE0) {
            cp = (uint32_t)(*p & 0x0F);
            extra = 2;
        } else if ((*p & 0xF8) == 0xF0) {
            cp = (uint32_t)(*p & 0x07);
            extra = 3;
        } else {
            p++;
            continue;
        }
        p++;

        bool valid = true;
        for (int i = 0; i < extra; i++) {
            if (p[i] == 0 || (p[i] & 0xC0) != 0x80) {
                valid = false;
                break;
            }
            cp = (cp << 6) | (uint32_t)(p[i] & 0x3F);
        }
        if (!valid) continue;
        p += extra;

        if (cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF)) continue;
        if (extra > 0 && cp < 0x80) continue;  // 过长编码
        if (out && count < out_cap) out[count] = cp;
        count++;
    }
    return count;
}

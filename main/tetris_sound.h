#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// 纯波形合成:不依赖 ESP-IDF/LVGL,音频任务按块调用 render 后写入 codec。
typedef enum {
    TETRIS_SOUND_KEY = 0,
    TETRIS_SOUND_LOCK,
    TETRIS_SOUND_LINE_CLEAR,
    TETRIS_SOUND_LEVEL_UP,
    TETRIS_SOUND_TETRIS,
    TETRIS_SOUND_MENU_CONFIRM,
    TETRIS_SOUND_GAME_OVER,
    TETRIS_SOUND_ERROR,
    TETRIS_SOUND_COUNT,
} tetris_sound_t;

typedef struct {
    uint8_t sound;
    uint8_t note;
    uint8_t decay;
    uint32_t remaining;
    uint32_t total;
    uint32_t phase;
    uint32_t step;
    uint32_t amp;
    uint32_t rate;
    bool active;
} tetris_sound_player_t;

// sample_rate 为 0 或 sound 越界时返回 false。
bool tetris_sound_start(tetris_sound_player_t *player,
                        tetris_sound_t sound, uint32_t sample_rate);

// 生成最多 max_frames 个 16 位单声道采样,返回实际帧数。
// 返回 0 表示该音效已播放完毕;可重复调用直到返回 0。
size_t tetris_sound_render(tetris_sound_player_t *player,
                           int16_t *out, size_t max_frames);

#include "tetris_sound.h"

#include <string.h>

typedef struct {
    uint16_t freq_hz;
    uint16_t ms;
    uint16_t amp;
    uint8_t decay;   // 音符结束时衰减到的幅度百分比,0 表示完全淡出
} sound_note_t;

typedef struct {
    const sound_note_t *notes;
    uint8_t count;
} sound_def_t;

// 短促按键音。
static const sound_note_t KEY_NOTES[] = {
    { 1800, 18, 3500, 40 },
};
// 落地闷响:两段下行低频。
static const sound_note_t LOCK_NOTES[] = {
    { 180, 45, 7000, 40 },
    { 120, 55, 6000, 60 },
};
// 消行上行琶音,最后一段长尾音淡出。
static const sound_note_t LINE_CLEAR_NOTES[] = {
    { 660, 70, 6000, 30 },
    { 880, 70, 6500, 30 },
    { 1100, 70, 7000, 40 },
    { 1320, 260, 7000, 0 },
};
static const sound_note_t LEVEL_UP_NOTES[] = {
    { 523, 80, 6000, 20 },
    { 659, 80, 6500, 20 },
    { 784, 80, 7000, 20 },
    { 1047, 220, 7500, 0 },
};
// 四行消除庆祝旋律,带长尾音。
static const sound_note_t TETRIS_NOTES[] = {
    { 523, 90, 6500, 10 },
    { 659, 90, 7000, 10 },
    { 784, 90, 7500, 10 },
    { 1047, 90, 8000, 10 },
    { 1319, 120, 8000, 20 },
    { 1568, 380, 8000, 0 },
};
static const sound_note_t MENU_CONFIRM_NOTES[] = {
    { 880, 50, 5500, 20 },
    { 1320, 90, 6000, 20 },
};
static const sound_note_t GAME_OVER_NOTES[] = {
    { 392, 130, 6000, 20 },
    { 330, 150, 6000, 30 },
    { 262, 180, 6500, 40 },
    { 196, 380, 7000, 0 },
};
static const sound_note_t ERROR_NOTES[] = {
    { 220, 90, 6000, 20 },
    { 165, 150, 6500, 50 },
};

#define DEF(name) { name, (uint8_t)(sizeof(name) / sizeof((name)[0])) }

static const sound_def_t SOUNDS[TETRIS_SOUND_COUNT] = {
    [TETRIS_SOUND_KEY] = DEF(KEY_NOTES),
    [TETRIS_SOUND_LOCK] = DEF(LOCK_NOTES),
    [TETRIS_SOUND_LINE_CLEAR] = DEF(LINE_CLEAR_NOTES),
    [TETRIS_SOUND_LEVEL_UP] = DEF(LEVEL_UP_NOTES),
    [TETRIS_SOUND_TETRIS] = DEF(TETRIS_NOTES),
    [TETRIS_SOUND_MENU_CONFIRM] = DEF(MENU_CONFIRM_NOTES),
    [TETRIS_SOUND_GAME_OVER] = DEF(GAME_OVER_NOTES),
    [TETRIS_SOUND_ERROR] = DEF(ERROR_NOTES),
};

// 32 点正弦表,按 int8 幅度存储。
static const int8_t SINE[32] = {
    0, 25, 49, 71, 90, 106, 117, 125,
    127, 125, 117, 106, 90, 71, 49, 25,
    0, -25, -49, -71, -90, -106, -117, -125,
    -127, -125, -117, -106, -90, -71, -49, -25,
};

bool tetris_sound_start(tetris_sound_player_t *player,
                        tetris_sound_t sound, uint32_t sample_rate) {
    if (!player || sample_rate == 0) return false;
    if ((unsigned)sound >= (unsigned)TETRIS_SOUND_COUNT) return false;
    memset(player, 0, sizeof(*player));
    player->sound = (uint8_t)sound;
    player->rate = sample_rate;
    player->active = true;
    return true;
}

size_t tetris_sound_render(tetris_sound_player_t *player,
                           int16_t *out, size_t max_frames) {
    if (!player || !player->active || !out || max_frames == 0) return 0;
    const sound_def_t *def = &SOUNDS[player->sound];
    size_t produced = 0;

    while (produced < max_frames) {
        if (player->remaining == 0) {
            if (player->note >= def->count) {
                player->active = false;
                break;
            }
            const sound_note_t *note = &def->notes[player->note++];
            player->total = ((uint32_t)note->ms * player->rate) / 1000U;
            if (player->total == 0) player->total = 1;
            player->remaining = player->total;
            player->step = ((uint32_t)note->freq_hz << 16) / player->rate;
            player->amp = note->amp;
            player->decay = note->decay;
            player->phase = 0;
        }

        const uint32_t elapsed = player->total - player->remaining;
        const uint32_t attack = player->rate / 500U;           // 2 ms
        const uint32_t release = (player->rate * 3U) / 1000U;  // 3 ms
        uint32_t gain = 65536U;
        if (attack > 0 && elapsed < attack) {
            gain = (uint32_t)(((uint64_t)gain * elapsed) / attack);
        }
        if (release > 0 && player->remaining < release) {
            gain = (uint32_t)(((uint64_t)gain * player->remaining) / release);
        }
        const uint32_t decay_floor = ((uint32_t)player->decay * 65536U) / 100U;
        const uint32_t decay_gain = 65536U -
            (uint32_t)(((uint64_t)(65536U - decay_floor) * elapsed) / player->total);
        gain = (uint32_t)(((uint64_t)gain * decay_gain) >> 16);

        const int32_t wave = SINE[(player->phase >> 11) & 31U];
        int32_t value = (wave * (int32_t)player->amp) / 127;
        value = (int32_t)(((int64_t)value * (int32_t)gain) >> 16);

        out[produced++] = (int16_t)value;
        player->phase = (player->phase + player->step) & 0xFFFFU;
        player->remaining--;
    }
    return produced;
}

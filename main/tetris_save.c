#include "tetris_save.h"

#include <string.h>

#define SAVE_MAGIC      "TTRS"
#define SAVE_VERSION    1U

#define OFF_MAGIC       0
#define OFF_VERSION     4
#define OFF_PIECE       5
#define OFF_NEXT        6
#define OFF_ROTATION    7
#define OFF_X           8
#define OFF_Y           9
#define OFF_LINES       10
#define OFF_SCORE       12
#define OFF_LEVEL       16
#define OFF_BAG_INDEX   17
#define OFF_RNG         18
#define OFF_BAG         22
#define OFF_BOARD       29
#define OFF_CRC         (OFF_BOARD + TETRIS_BOARD_HEIGHT * TETRIS_BOARD_WIDTH)
#define OFF_END         (OFF_CRC + 4)

_Static_assert(OFF_END == TETRIS_SAVE_ENCODED_SIZE, "save layout size mismatch");

static void put_u16(uint8_t *out, uint16_t value) {
    out[0] = (uint8_t)(value & 0xFFU);
    out[1] = (uint8_t)(value >> 8);
}

static uint16_t get_u16(const uint8_t *data) {
    return (uint16_t)((uint16_t)data[0] | ((uint16_t)data[1] << 8));
}

static void put_u32(uint8_t *out, uint32_t value) {
    out[0] = (uint8_t)(value & 0xFFU);
    out[1] = (uint8_t)((value >> 8) & 0xFFU);
    out[2] = (uint8_t)((value >> 16) & 0xFFU);
    out[3] = (uint8_t)(value >> 24);
}

static uint32_t get_u32(const uint8_t *data) {
    return (uint32_t)data[0] |
           ((uint32_t)data[1] << 8) |
           ((uint32_t)data[2] << 16) |
           ((uint32_t)data[3] << 24);
}

uint32_t tetris_save_crc32(const uint8_t *data, size_t len) {
    uint32_t crc = 0xFFFFFFFFU;
    for (size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (int bit = 0; bit < 8; bit++) {
            crc = (crc & 1U) ? ((crc >> 1) ^ 0xEDB88320U) : (crc >> 1);
        }
    }
    return ~crc;
}

static bool model_valid(const tetris_model_t *model) {
    if (!model) return false;
    if (model->piece < TETRIS_PIECE_I || model->piece > TETRIS_PIECE_L) return false;
    if (model->next_piece < TETRIS_PIECE_I || model->next_piece > TETRIS_PIECE_L) {
        return false;
    }
    if (model->rotation > 3) return false;
    if (model->x < -3 || model->x > TETRIS_BOARD_WIDTH - 1) return false;
    if (model->y < 0 || model->y >= TETRIS_BOARD_HEIGHT) return false;
    if (model->level == 0) return false;
    if (model->bag_index > 7) return false;
    for (int i = 0; i < 7; i++) {
        if (model->bag[i] > TETRIS_PIECE_L) return false;
    }
    for (int y = 0; y < TETRIS_BOARD_HEIGHT; y++) {
        for (int x = 0; x < TETRIS_BOARD_WIDTH; x++) {
            if (model->board[y][x] > TETRIS_PIECE_L) return false;
        }
    }
    return true;
}

size_t tetris_save_encode(const tetris_model_t *model, uint8_t *out, size_t cap) {
    if (!model || !out || cap < TETRIS_SAVE_ENCODED_SIZE) return 0;
    if (model->state == TETRIS_GAME_OVER || !model_valid(model)) return 0;

    memset(out, 0, TETRIS_SAVE_ENCODED_SIZE);
    memcpy(out + OFF_MAGIC, SAVE_MAGIC, 4);
    out[OFF_VERSION] = SAVE_VERSION;
    out[OFF_PIECE] = (uint8_t)model->piece;
    out[OFF_NEXT] = (uint8_t)model->next_piece;
    out[OFF_ROTATION] = model->rotation;
    out[OFF_X] = (uint8_t)model->x;
    out[OFF_Y] = (uint8_t)model->y;
    put_u16(out + OFF_LINES, model->lines);
    put_u32(out + OFF_SCORE, model->score);
    // 难度由累计行数推导,写入归一键,避免旧存档里的等级与行数不一致。
    out[OFF_LEVEL] = tetris_model_level_for_lines(model->lines);
    out[OFF_BAG_INDEX] = model->bag_index;
    put_u32(out + OFF_RNG, model->rng);
    memcpy(out + OFF_BAG, model->bag, 7);
    for (int y = 0; y < TETRIS_BOARD_HEIGHT; y++) {
        memcpy(out + OFF_BOARD + y * TETRIS_BOARD_WIDTH,
               model->board[y], TETRIS_BOARD_WIDTH);
    }
    put_u32(out + OFF_CRC, tetris_save_crc32(out, OFF_CRC));
    return TETRIS_SAVE_ENCODED_SIZE;
}

bool tetris_save_decode(const uint8_t *data, size_t len, tetris_model_t *out) {
    if (!data || !out || len != TETRIS_SAVE_ENCODED_SIZE) return false;
    if (memcmp(data + OFF_MAGIC, SAVE_MAGIC, 4) != 0) return false;
    if (data[OFF_VERSION] != SAVE_VERSION) return false;
    if (get_u32(data + OFF_CRC) != tetris_save_crc32(data, OFF_CRC)) return false;

    tetris_model_t model;
    memset(&model, 0, sizeof(model));
    model.piece = (tetris_piece_t)data[OFF_PIECE];
    model.next_piece = (tetris_piece_t)data[OFF_NEXT];
    model.rotation = data[OFF_ROTATION];
    model.x = (int8_t)data[OFF_X];
    model.y = (int8_t)data[OFF_Y];
    model.lines = get_u16(data + OFF_LINES);
    model.score = get_u32(data + OFF_SCORE);
    if (data[OFF_LEVEL] == 0) return false;
    model.level = tetris_model_level_for_lines(model.lines);
    model.bag_index = data[OFF_BAG_INDEX];
    model.rng = get_u32(data + OFF_RNG);
    memcpy(model.bag, data + OFF_BAG, 7);
    for (int y = 0; y < TETRIS_BOARD_HEIGHT; y++) {
        memcpy(model.board[y], data + OFF_BOARD + y * TETRIS_BOARD_WIDTH,
               TETRIS_BOARD_WIDTH);
    }
    model.state = TETRIS_PLAYING;

    if (!model_valid(&model)) return false;
    *out = model;
    return true;
}

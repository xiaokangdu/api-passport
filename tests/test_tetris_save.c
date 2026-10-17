// 单存档槽序列化 host 测试:往返一致、CRC/版本/长度校验、字段非法拒绝。
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "tetris_save.h"

#define OFF_MAGIC     0
#define OFF_VERSION   4
#define OFF_PIECE     5
#define OFF_NEXT      6
#define OFF_ROTATION  7
#define OFF_X         8
#define OFF_Y         9
#define OFF_LINES     10
#define OFF_SCORE     12
#define OFF_LEVEL     16
#define OFF_BAG_INDEX 17
#define OFF_RNG       18
#define OFF_BAG       22
#define OFF_BOARD     29
#define OFF_CRC       229

static void fix_crc(uint8_t *blob) {
    const uint32_t crc = tetris_save_crc32(blob, OFF_CRC);
    blob[OFF_CRC + 0] = (uint8_t)(crc & 0xFFU);
    blob[OFF_CRC + 1] = (uint8_t)((crc >> 8) & 0xFFU);
    blob[OFF_CRC + 2] = (uint8_t)((crc >> 16) & 0xFFU);
    blob[OFF_CRC + 3] = (uint8_t)(crc >> 24);
}

static void make_model(tetris_model_t *model) {
    tetris_model_init(model, 12345);
    model->piece = TETRIS_PIECE_T;
    model->next_piece = TETRIS_PIECE_L;
    model->rotation = 2;
    model->x = 4;
    model->y = 7;
    model->score = 123456;
    model->lines = 42;
    model->level = tetris_model_level_for_lines(model->lines);
    model->rng = 0xDEADBEEFU;
    model->bag[0] = 1;
    model->bag[1] = 2;
    model->bag[2] = 3;
    model->bag[3] = 4;
    model->bag[4] = 5;
    model->bag[5] = 6;
    model->bag[6] = 7;
    model->bag_index = 3;
    model->board[0][0] = TETRIS_PIECE_I;
    model->board[5][9] = TETRIS_PIECE_Z;
    model->board[19][3] = TETRIS_PIECE_O;
}

static void assert_same_model(const tetris_model_t *a, const tetris_model_t *b) {
    assert(a->piece == b->piece);
    assert(a->next_piece == b->next_piece);
    assert(a->rotation == b->rotation);
    assert(a->x == b->x);
    assert(a->y == b->y);
    assert(a->score == b->score);
    assert(a->lines == b->lines);
    assert(a->level == b->level);
    assert(a->rng == b->rng);
    assert(memcmp(a->bag, b->bag, sizeof(a->bag)) == 0);
    assert(a->bag_index == b->bag_index);
    assert(memcmp(a->board, b->board, sizeof(a->board)) == 0);
}

static void test_round_trip(void) {
    tetris_model_t model;
    make_model(&model);
    model.state = TETRIS_PLAYING;

    uint8_t blob[TETRIS_SAVE_ENCODED_SIZE];
    assert(tetris_save_encode(&model, blob, sizeof(blob)) == TETRIS_SAVE_ENCODED_SIZE);
    assert(memcmp(blob + OFF_MAGIC, "TTRS", 4) == 0);
    assert(blob[OFF_VERSION] == 1);

    tetris_model_t loaded;
    memset(&loaded, 0xA5, sizeof(loaded));
    assert(tetris_save_decode(blob, sizeof(blob), &loaded));
    assert_same_model(&model, &loaded);
    assert(loaded.state == TETRIS_PLAYING);

    // 暂停状态也能保存,读回后统一恢复为进行中。
    model.state = TETRIS_PAUSED;
    assert(tetris_save_encode(&model, blob, sizeof(blob)) == TETRIS_SAVE_ENCODED_SIZE);
    assert(tetris_save_decode(blob, sizeof(blob), &loaded));
    assert(loaded.state == TETRIS_PLAYING);
}

static void test_encode_rejects(void) {
    tetris_model_t model;
    make_model(&model);
    uint8_t blob[TETRIS_SAVE_ENCODED_SIZE];

    model.state = TETRIS_GAME_OVER;
    assert(tetris_save_encode(&model, blob, sizeof(blob)) == 0);

    model.state = TETRIS_PLAYING;
    assert(tetris_save_encode(NULL, blob, sizeof(blob)) == 0);
    assert(tetris_save_encode(&model, NULL, sizeof(blob)) == 0);
    assert(tetris_save_encode(&model, blob, sizeof(blob) - 1) == 0);

    // 字段越界不生成存档,避免把坏数据写进 NVS。
    model.board[3][3] = TETRIS_PIECE_L + 1;
    assert(tetris_save_encode(&model, blob, sizeof(blob)) == 0);
    model.board[3][3] = 0;
    model.rotation = 4;
    assert(tetris_save_encode(&model, blob, sizeof(blob)) == 0);
    model.rotation = 0;
    model.bag_index = 8;
    assert(tetris_save_encode(&model, blob, sizeof(blob)) == 0);
}

static void test_decode_rejects(void) {
    tetris_model_t model;
    make_model(&model);
    model.state = TETRIS_PLAYING;

    uint8_t good[TETRIS_SAVE_ENCODED_SIZE];
    assert(tetris_save_encode(&model, good, sizeof(good)) == TETRIS_SAVE_ENCODED_SIZE);

    tetris_model_t out;
    memset(&out, 0x5A, sizeof(out));
    const tetris_model_t sentinel = out;

    // 长度、空指针、magic、版本、CRC。
    assert(!tetris_save_decode(good, sizeof(good) - 1, &out));
    assert(!tetris_save_decode(good, sizeof(good) + 1, &out));
    assert(!tetris_save_decode(NULL, sizeof(good), &out));
    assert(!tetris_save_decode(good, sizeof(good), NULL));

    uint8_t bad[TETRIS_SAVE_ENCODED_SIZE];

    memcpy(bad, good, sizeof(bad));
    bad[OFF_MAGIC] = 'X';
    assert(!tetris_save_decode(bad, sizeof(bad), &out));

    memcpy(bad, good, sizeof(bad));
    bad[OFF_VERSION] = 2;
    fix_crc(bad);
    assert(!tetris_save_decode(bad, sizeof(bad), &out));

    memcpy(bad, good, sizeof(bad));
    bad[OFF_SCORE] ^= 0x01;
    assert(!tetris_save_decode(bad, sizeof(bad), &out));

    // CRC 正确但字段非法时同样拒绝。
    const struct {
        size_t offset;
        uint8_t value;
    } cases[] = {
        { OFF_PIECE, 0 },
        { OFF_PIECE, TETRIS_PIECE_L + 1 },
        { OFF_NEXT, 0 },
        { OFF_ROTATION, 4 },
        { OFF_X, 10 },
        { OFF_Y, TETRIS_BOARD_HEIGHT },
        { OFF_LEVEL, 0 },
        { OFF_BAG_INDEX, 8 },
        { OFF_BAG, 8 },
        { OFF_BOARD + 7, 8 },
    };
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        memcpy(bad, good, sizeof(bad));
        bad[cases[i].offset] = cases[i].value;
        fix_crc(bad);
        assert(!tetris_save_decode(bad, sizeof(bad), &out));
    }

    // 所有失败路径都不应改动调用方的模型。
    assert(memcmp(&out, &sentinel, sizeof(out)) == 0);

    // 重新计算 CRC 后的原始数据仍然可读。
    memcpy(bad, good, sizeof(bad));
    fix_crc(bad);
    assert(tetris_save_decode(bad, sizeof(bad), &out));
    assert_same_model(&model, &out);

    // 等级由累计行数推导,存档里的过期等级不会带进游戏。
    memcpy(bad, good, sizeof(bad));
    bad[OFF_LEVEL] = 9;
    fix_crc(bad);
    assert(tetris_save_decode(bad, sizeof(bad), &out));
    assert(out.level == tetris_model_level_for_lines(out.lines));
}

static void test_crc32_reference(void) {
    // CRC-32/ISO-HDLC 标准校验值。
    assert(tetris_save_crc32((const uint8_t *)"123456789", 9) == 0xCBF43926U);
    assert(tetris_save_crc32(NULL, 0) == 0x00000000U);
}

int main(void) {
    test_round_trip();
    test_encode_rejects();
    test_decode_rejects();
    test_crc32_reference();
    puts("tetris save tests: PASS");
    return 0;
}

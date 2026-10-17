// 游戏模型 host 测试:旋转/碰撞/消行/计分/等级/暂停/结束,不依赖 SDK 与 LVGL。
#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "tetris_model.h"

#define W TETRIS_BOARD_WIDTH
#define H TETRIS_BOARD_HEIGHT

static void clear_board(tetris_model_t *model) {
    memset(model->board, 0, sizeof(model->board));
}

static int min_occupied_x(tetris_piece_t piece, uint8_t rotation) {
    int min_x = 4;
    for (int y = 0; y < 4; y++) {
        for (int x = 0; x < 4; x++) {
            if (tetris_piece_cell(piece, rotation, x, y) && x < min_x) min_x = x;
        }
    }
    return min_x;
}

static int max_occupied_x(tetris_piece_t piece, uint8_t rotation) {
    int max_x = -1;
    for (int y = 0; y < 4; y++) {
        for (int x = 0; x < 4; x++) {
            if (tetris_piece_cell(piece, rotation, x, y) && x > max_x) max_x = x;
        }
    }
    return max_x;
}

static void test_piece_geometry(void) {
    for (int p = TETRIS_PIECE_I; p <= TETRIS_PIECE_L; p++) {
        for (int rot = 0; rot < 4; rot++) {
            int cells = 0;
            for (int y = 0; y < 4; y++) {
                for (int x = 0; x < 4; x++) {
                    if (tetris_piece_cell((tetris_piece_t)p, (uint8_t)rot, x, y)) cells++;
                }
            }
            assert(cells == 4);
        }
        // 旋转按 4 取模;越界坐标一律为空。
        for (int y = 0; y < 4; y++) {
            for (int x = 0; x < 4; x++) {
                assert(tetris_piece_cell((tetris_piece_t)p, 0, x, y) ==
                       tetris_piece_cell((tetris_piece_t)p, 4, x, y));
            }
        }
        assert(!tetris_piece_cell((tetris_piece_t)p, 0, -1, 0));
        assert(!tetris_piece_cell((tetris_piece_t)p, 0, 0, -1));
        assert(!tetris_piece_cell((tetris_piece_t)p, 0, 4, 0));
        assert(!tetris_piece_cell((tetris_piece_t)p, 0, 0, 4));
    }
    assert(!tetris_piece_cell(TETRIS_PIECE_NONE, 0, 0, 0));
}

static void test_init_and_bag(void) {
    tetris_model_t a;
    tetris_model_t b;
    tetris_model_init(&a, 12345);
    tetris_model_init(&b, 12345);

    assert(a.state == TETRIS_PLAYING);
    assert(a.score == 0);
    assert(a.lines == 0);
    assert(a.level == 1);
    assert(a.piece >= TETRIS_PIECE_I && a.piece <= TETRIS_PIECE_L);
    assert(a.next_piece >= TETRIS_PIECE_I && a.next_piece <= TETRIS_PIECE_L);
    assert(a.piece != a.next_piece);
    assert(a.rng != 0);

    for (int y = 0; y < H; y++) {
        for (int x = 0; x < W; x++) assert(a.board[y][x] == 0);
    }

    // 每种方块每 7 个一袋:当前袋必须是 1..7 的一个排列。
    bool seen[8] = { false };
    for (int i = 0; i < 7; i++) {
        const int value = a.bag[i];
        assert(value >= 1 && value <= 7);
        assert(!seen[value]);
        seen[value] = true;
    }

    // 相同种子产生相同序列。
    assert(a.piece == b.piece);
    assert(a.next_piece == b.next_piece);
    assert(a.x == b.x);
    assert(a.y == b.y);

    // 种子 0 会被替换为非零值,xorshift 不会卡死。
    tetris_model_t zero;
    tetris_model_init(&zero, 0);
    assert(zero.rng != 0);
    assert(zero.state == TETRIS_PLAYING);
}

static void test_movement_limits(void) {
    for (int p = TETRIS_PIECE_I; p <= TETRIS_PIECE_L; p++) {
        tetris_model_t m;
        tetris_model_init(&m, 7);
        clear_board(&m);
        m.piece = (tetris_piece_t)p;
        m.rotation = 0;
        m.x = 3;
        m.y = 0;

        while (tetris_model_move(&m, -1)) { }
        assert(m.x + min_occupied_x(m.piece, 0) == 0);
        assert(!tetris_model_move(&m, -1));

        while (tetris_model_move(&m, 1)) { }
        assert(m.x + max_occupied_x(m.piece, 0) == W - 1);
        assert(!tetris_model_move(&m, 1));

        // 一次只能移动一格。
        assert(!tetris_model_move(&m, 0));
        assert(!tetris_model_move(&m, 2));
        assert(!tetris_model_move(&m, -2));
    }
}

static void test_rotation_and_kick(void) {
    tetris_model_t m;
    tetris_model_init(&m, 3);
    clear_board(&m);

    // 空地旋转四次回到初始态且不位移。
    m.piece = TETRIS_PIECE_T;
    m.rotation = 0;
    m.x = 3;
    m.y = 5;
    for (int i = 0; i < 4; i++) assert(tetris_model_rotate(&m));
    assert(m.rotation == 0);
    assert(m.x == 3);

    // 旋转后原位被挡,踢墙向左一格。
    clear_board(&m);
    m.piece = TETRIS_PIECE_T;
    m.rotation = 0;
    m.x = 5;
    m.y = 0;
    m.board[2][6] = TETRIS_PIECE_T;
    assert(tetris_model_rotate(&m));
    assert(m.rotation == 1);
    assert(m.x == 4);

    // 棋盘被填满时旋转失败,状态保持不变。
    clear_board(&m);
    m.piece = TETRIS_PIECE_T;
    m.rotation = 0;
    m.x = 3;
    m.y = 0;
    for (int y = 0; y < H; y++) {
        for (int x = 0; x < W; x++) m.board[y][x] = TETRIS_PIECE_J;
    }
    // 当前格也占满,但 rotate 只检查目标位置;目标同样占满,必然失败。
    assert(!tetris_model_rotate(&m));
    assert(m.rotation == 0);
    assert(m.x == 3);
}

static void test_gravity_and_ghost(void) {
    tetris_model_t m;
    tetris_model_init(&m, 11);
    clear_board(&m);

    const int ghost = tetris_model_ghost_y(&m);
    assert(ghost >= m.y);
    assert(tetris_model_ghost_enabled(&m));
    m.level = 2;
    assert(!tetris_model_ghost_enabled(&m));
    m.level = 1;

    tetris_model_t clone = m;
    int rest_y = clone.y;
    tetris_event_t event;
    while ((event = tetris_model_step(&clone)) == TETRIS_EVENT_MOVED) {
        rest_y = clone.y;
    }
    assert(rest_y == ghost);

    // 落点再下一格就会碰撞,那一步锁定并生成新块。
    assert(event == TETRIS_EVENT_LOCKED);
    assert(clone.state == TETRIS_PLAYING);
    assert(clone.y == 0);

    // 暂停与结束时不会下落。
    assert(tetris_model_toggle_pause(&m));
    assert(m.state == TETRIS_PAUSED);
    const int8_t paused_y = m.y;
    assert(tetris_model_step(&m) == TETRIS_EVENT_NONE);
    assert(m.y == paused_y);
    assert(!tetris_model_move(&m, -1));
    assert(!tetris_model_rotate(&m));
    assert(tetris_model_toggle_pause(&m));
    assert(m.state == TETRIS_PLAYING);

    m.state = TETRIS_GAME_OVER;
    assert(tetris_model_step(&m) == TETRIS_EVENT_NONE);
    assert(!tetris_model_toggle_pause(&m));
}

static void test_single_line_clear(void) {
    tetris_model_t m;
    tetris_model_init(&m, 21);
    clear_board(&m);

    // 竖直 I 落在第 2 列;其余列填满最底行。
    m.piece = TETRIS_PIECE_I;
    m.rotation = 1;   // 0x4444,占据 4x4 位图的第 2 列
    m.x = 0;
    m.y = 16;
    m.lines = 9;      // 未到 30 行,难度保持 1
    for (int x = 0; x < W; x++) {
        if (x != 2) m.board[H - 1][x] = TETRIS_PIECE_T;
    }

    const tetris_event_t event = tetris_model_step(&m);
    assert(event == TETRIS_EVENT_LINE_CLEAR);
    assert(m.lines == 10);
    assert(m.level == 1);
    assert(m.score == 100);   // 消行时等级为 1
    assert(m.state == TETRIS_PLAYING);
    // 底部整行被清空后,原行内容下移:只剩第 2 列一个 I 格。
    for (int x = 0; x < W; x++) {
        if (x != 2) assert(m.board[H - 1][x] == 0);
    }
    assert(m.board[H - 1][2] == TETRIS_PIECE_I);
}

static void test_double_line_clear_at_level_two(void) {
    tetris_model_t m;
    tetris_model_init(&m, 22);
    clear_board(&m);

    // O 块占据 4x4 位图的 (1,0)、(2,0)、(1,1)、(2,1),放在 8/9 列底部。
    m.piece = TETRIS_PIECE_O;
    m.rotation = 0;
    m.x = 7;
    m.y = 18;
    m.lines = 58;
    m.level = tetris_model_level_for_lines(m.lines);
    assert(m.level == 2);
    for (int y = 18; y < H; y++) {
        for (int x = 0; x < W; x++) {
            if (x != 8 && x != 9) m.board[y][x] = TETRIS_PIECE_L;
        }
    }

    const tetris_event_t event = tetris_model_step(&m);
    assert(event == TETRIS_EVENT_LINE_CLEAR);
    assert(m.score == 300U * 2U);   // 按消行前的难度 2 计分
    assert(m.lines == 60);
    assert(m.level == 3);

    // 难度提升后底部新增一行单缺口障碍,其余盘面为空。
    int filled = 0;
    for (int x = 0; x < W; x++) {
        if (m.board[H - 1][x] != 0) filled++;
    }
    assert(filled == W - 1);
    for (int y = 0; y < H - 1; y++) {
        for (int x = 0; x < W; x++) assert(m.board[y][x] == 0);
    }
}

static void test_difficulty_tier_up(void) {
    tetris_model_t m;
    tetris_model_init(&m, 27);
    clear_board(&m);

    // 第 29 行时再消一行,正好跨过 30 行门槛。
    m.piece = TETRIS_PIECE_I;
    m.rotation = 1;
    m.x = 0;
    m.y = 16;
    m.lines = 29;
    m.level = tetris_model_level_for_lines(m.lines);
    for (int x = 0; x < W; x++) {
        if (x != 2) m.board[H - 1][x] = TETRIS_PIECE_T;
    }

    assert(tetris_model_ghost_enabled(&m));
    const tetris_event_t event = tetris_model_step(&m);
    assert(event == TETRIS_EVENT_LINE_CLEAR);
    assert(m.lines == 30);
    assert(m.level == 2);
    assert(!tetris_model_ghost_enabled(&m));

    // 障碍行只有一个缺口。
    int gaps = 0;
    for (int x = 0; x < W; x++) {
        if (m.board[H - 1][x] == 0) gaps++;
    }
    assert(gaps == 1);

    // 竖 I 消底行后还剩 3 格,位于障碍行上方第 2 列。
    for (int y = 0; y < H - 4; y++) {
        for (int x = 0; x < W; x++) assert(m.board[y][x] == 0);
    }
    for (int y = H - 4; y < H - 1; y++) {
        for (int x = 0; x < W; x++) {
            if (x == 2) {
                assert(m.board[y][x] == TETRIS_PIECE_I);
            } else {
                assert(m.board[y][x] == 0);
            }
        }
    }
}

static void test_difficulty_thresholds(void) {
    assert(tetris_model_level_for_lines(0) == 1);
    assert(tetris_model_level_for_lines(29) == 1);
    assert(tetris_model_level_for_lines(30) == 2);
    assert(tetris_model_level_for_lines(59) == 2);
    assert(tetris_model_level_for_lines(60) == 3);
    assert(tetris_model_level_for_lines(65535) == 255);
}

static void test_four_line_clear(void) {
    tetris_model_t m;
    tetris_model_init(&m, 23);
    clear_board(&m);

    m.piece = TETRIS_PIECE_I;
    m.rotation = 1;
    m.x = 0;
    m.y = 16;
    for (int y = 16; y < H; y++) {
        for (int x = 0; x < W; x++) {
            if (x != 2) m.board[y][x] = TETRIS_PIECE_S;
        }
    }

    const tetris_event_t event = tetris_model_step(&m);
    assert(event == TETRIS_EVENT_LINE_CLEAR);
    assert(m.score == 800);
    assert(m.lines == 4);
    assert(m.level == 1);
    for (int y = 0; y < H; y++) {
        for (int x = 0; x < W; x++) assert(m.board[y][x] == 0);
    }
}

static void test_five_line_clear_scores_capped(void) {
    tetris_model_t m;
    tetris_model_init(&m, 26);
    clear_board(&m);

    // 异常盘面(五整行)一次清五行:按四行计分封顶,但行数照实累加。
    for (int y = 15; y < H; y++) {
        for (int x = 0; x < W; x++) m.board[y][x] = TETRIS_PIECE_J;
    }
    m.piece = TETRIS_PIECE_O;
    m.rotation = 0;
    m.x = 3;
    m.y = 13;

    const tetris_event_t event = tetris_model_step(&m);
    assert(event == TETRIS_EVENT_LINE_CLEAR);
    assert(m.lines == 5);
    assert(m.score == 800);
    assert(m.state == TETRIS_PLAYING);
}

static void test_game_over_on_spawn(void) {
    tetris_model_t m;
    tetris_model_init(&m, 24);
    clear_board(&m);

    // 摆一个正好卡住出生点的 O 块:锁定后下个块无法生成。
    m.piece = TETRIS_PIECE_O;
    m.rotation = 0;
    m.x = 3;
    m.y = 0;
    m.board[1][4] = TETRIS_PIECE_Z;
    m.board[1][5] = TETRIS_PIECE_Z;

    const tetris_event_t event = tetris_model_step(&m);
    assert(event == TETRIS_EVENT_GAME_OVER);
    assert(m.state == TETRIS_GAME_OVER);
}

static void test_drop_interval(void) {
    tetris_model_t m;
    tetris_model_init(&m, 25);

    m.level = 1;
    assert(tetris_model_drop_interval_ms(&m) == 700);
    m.level = 2;
    assert(tetris_model_drop_interval_ms(&m) == 620);
    m.level = 5;
    assert(tetris_model_drop_interval_ms(&m) == 380);
    m.level = 7;
    assert(tetris_model_drop_interval_ms(&m) == 220);
    m.level = 8;
    assert(tetris_model_drop_interval_ms(&m) == 140);
    m.level = 11;
    assert(tetris_model_drop_interval_ms(&m) == 140);
    m.level = 200;
    assert(tetris_model_drop_interval_ms(&m) == 140);
}

int main(void) {
    test_piece_geometry();
    test_init_and_bag();
    test_movement_limits();
    test_rotation_and_kick();
    test_gravity_and_ghost();
    test_single_line_clear();
    test_double_line_clear_at_level_two();
    test_difficulty_tier_up();
    test_difficulty_thresholds();
    test_four_line_clear();
    test_five_line_clear_scores_capped();
    test_game_over_on_spawn();
    test_drop_interval();
    puts("tetris model tests: PASS");
    return 0;
}

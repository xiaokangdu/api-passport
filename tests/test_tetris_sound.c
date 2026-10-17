// 音效合成 host 测试:分块渲染、时长、幅度范围、尾音淡出与非法输入。
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "tetris_sound.h"

#define RATE 16000U

static size_t render_all(tetris_sound_t sound, size_t chunk,
                         int16_t *first_out, int16_t *last_out) {
    tetris_sound_player_t player;
    assert(tetris_sound_start(&player, sound, RATE));

    int16_t buf[128];
    assert(chunk <= sizeof(buf) / sizeof(buf[0]));

    size_t total = 0;
    int16_t first = 0;
    int16_t last = 0;
    size_t calls = 0;
    size_t n;
    while ((n = tetris_sound_render(&player, buf, chunk)) > 0) {
        for (size_t i = 0; i < n; i++) {
            assert(buf[i] >= -10000);
            assert(buf[i] <= 10000);
        }
        if (total == 0) first = buf[0];
        last = buf[n - 1];
        total += n;
        calls++;
        assert(calls < 20000);   // 防止渲染永不结束
    }
    assert(!player.active);
    assert(total > 0);
    if (first_out) *first_out = first;
    if (last_out) *last_out = last;
    return total;
}

static void test_durations(void) {
    int16_t first = 0;
    int16_t last = 0;

    // 各音效时长 = 各音符毫秒数之和(16kHz 整数采样)。
    assert(render_all(TETRIS_SOUND_KEY, 128, &first, &last) == 18U * RATE / 1000U);
    assert(render_all(TETRIS_SOUND_LOCK, 128, NULL, NULL) == 100U * RATE / 1000U);
    assert(render_all(TETRIS_SOUND_LINE_CLEAR, 128, NULL, NULL) == 470U * RATE / 1000U);
    assert(render_all(TETRIS_SOUND_LEVEL_UP, 128, NULL, NULL) == 460U * RATE / 1000U);
    assert(render_all(TETRIS_SOUND_TETRIS, 128, NULL, NULL) == 860U * RATE / 1000U);
    assert(render_all(TETRIS_SOUND_MENU_CONFIRM, 128, NULL, NULL) == 140U * RATE / 1000U);
    assert(render_all(TETRIS_SOUND_GAME_OVER, 128, NULL, NULL) == 840U * RATE / 1000U);
    assert(render_all(TETRIS_SOUND_ERROR, 128, NULL, NULL) == 240U * RATE / 1000U);

    // 起音从 0 开始、尾音衰减到 0,避免爆音。
    const size_t key_frames = render_all(TETRIS_SOUND_KEY, 64, &first, &last);
    (void)key_frames;
    assert(abs(first) <= 100);
    assert(abs(last) <= 100);
}

static void test_chunk_sizes(void) {
    // 不同分块大小必须产生相同的采样数。
    assert(render_all(TETRIS_SOUND_LINE_CLEAR, 1, NULL, NULL) == 470U * RATE / 1000U);
    assert(render_all(TETRIS_SOUND_LINE_CLEAR, 37, NULL, NULL) == 470U * RATE / 1000U);
    assert(render_all(TETRIS_SOUND_LINE_CLEAR, 128, NULL, NULL) == 470U * RATE / 1000U);
}

static void test_line_clear_tail(void) {
    tetris_sound_player_t player;
    assert(tetris_sound_start(&player, TETRIS_SOUND_LINE_CLEAR, RATE));

    int16_t buf[64];
    size_t total = 0;
    size_t n;
    int max_tail = 0;
    const size_t tail_frames = 50U * RATE / 1000U;
    const size_t total_frames = 470U * RATE / 1000U;
    while ((n = tetris_sound_render(&player, buf, 64)) > 0) {
        for (size_t i = 0; i < n; i++) {
            const size_t index = total + i;
            if (index + tail_frames >= total_frames) {
                const int value = abs(buf[i]);
                if (value > max_tail) max_tail = value;
            }
        }
        total += n;
    }
    assert(total == total_frames);
    // 最后一个音符线性衰减到 0,最后 50ms 明显弱于峰值。
    assert(max_tail < 2500);
}

static void test_invalid_input(void) {
    tetris_sound_player_t player = { 0 };
    assert(!tetris_sound_start(NULL, TETRIS_SOUND_KEY, RATE));
    assert(!tetris_sound_start(&player, TETRIS_SOUND_KEY, 0));
    assert(!tetris_sound_start(&player, TETRIS_SOUND_COUNT, RATE));
    assert(!tetris_sound_start(&player, (tetris_sound_t)99, RATE));
    assert(!tetris_sound_render(NULL, NULL, 0));

    int16_t buf[8];
    assert(!tetris_sound_render(&player, buf, 8));  // 未 start,不活动

    assert(tetris_sound_start(&player, TETRIS_SOUND_KEY, RATE));
    assert(tetris_sound_render(&player, NULL, 8) == 0);
    assert(tetris_sound_render(&player, buf, 0) == 0);
    assert(player.active);
    assert(tetris_sound_render(&player, buf, 8) == 8);
}

int main(void) {
    test_durations();
    test_chunk_sizes();
    test_line_clear_tail();
    test_invalid_input();
    puts("tetris sound tests: PASS");
    return 0;
}

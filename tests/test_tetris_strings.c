// 字符串与字体覆盖 host 测试:验证 UTF-8 解码,并确认所有界面汉字都进了字体子集。
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "tetris_strings.h"

// lv_font_conv 为每个字形生成形如 "/* U+4FC4 ... */" 的注释,据此检查覆盖。
static bool font_has_glyph(const char *path, uint32_t cp) {
    char needle[16];
    snprintf(needle, sizeof(needle), "U+%04X", (unsigned)cp);

    FILE *file = fopen(path, "r");
    if (!file) {
        fprintf(stderr, "cannot open font source: %s\n", path);
        return false;
    }

    bool found = false;
    char line[512];
    while (fgets(line, sizeof(line), file)) {
        if (strstr(line, needle)) {
            found = true;
            break;
        }
    }
    fclose(file);
    return found;
}

static void test_utf8_decode(void) {
    uint32_t out[8];

    assert(tetris_utf8_decode(NULL, out, 8) == 0);
    assert(tetris_utf8_decode("", out, 8) == 0);

    assert(tetris_utf8_decode("A", out, 8) == 1);
    assert(out[0] == 'A');

    // "俄罗斯" U+4FC4 U+7F57 U+65AF
    assert(tetris_utf8_decode("俄罗斯", out, 8) == 3);
    assert(out[0] == 0x4FC4);
    assert(out[1] == 0x7F57);
    assert(out[2] == 0x65AF);

    // out 为 NULL 时只计数;out_cap 不够时只写前 cap 个,但返回值仍是总数。
    assert(tetris_utf8_decode("俄罗斯", NULL, 0) == 3);
    memset(out, 0, sizeof(out));
    assert(tetris_utf8_decode("俄罗斯", out, 2) == 3);
    assert(out[0] == 0x4FC4);
    assert(out[1] == 0x7F57);
    assert(out[2] == 0);

    // 截断的 3 字节序列被跳过。
    assert(tetris_utf8_decode("\xE4\xBF", out, 8) == 0);

    // 非法续字节:跳过坏的首字节,后面的 ASCII 正常计入。
    assert(tetris_utf8_decode("\xE4\x41", out, 8) == 1);
    assert(out[0] == 'A');

    // 非法首字节 0xFF 被跳过。
    assert(tetris_utf8_decode("\xFF" "B", out, 8) == 1);
    assert(out[0] == 'B');

    // UTF-16 代理区、过长编码、超出 U+10FFFF 的码点一律拒绝。
    assert(tetris_utf8_decode("\xED\xA0\x80", out, 8) == 0);
    assert(tetris_utf8_decode("\xC0\x80", out, 8) == 0);
    assert(tetris_utf8_decode("\xF4\x90\x80\x80", out, 8) == 0);

    // 末尾截断不影响前面已解码的字符。
    assert(tetris_utf8_decode("A\xE4", out, 8) == 1);
    assert(out[0] == 'A');
}

static void test_font_coverage(const char *path14, const char *path20) {
    uint32_t codepoints[64];

    for (size_t i = 0; i < TETRIS_UI_STRING_COUNT; i++) {
        const char *text = TETRIS_UI_STRINGS[i];
        assert(text != NULL);
        const size_t count = tetris_utf8_decode(text, codepoints, 64);
        assert(count > 0);
        assert(count <= 64);

        for (size_t j = 0; j < count; j++) {
            if (!font_has_glyph(path14, codepoints[j])) {
                fprintf(stderr, "14px font missing U+%04lX in \"%s\"\n",
                        (unsigned long)codepoints[j], text);
                assert(false);
            }
            if (!font_has_glyph(path20, codepoints[j])) {
                fprintf(stderr, "20px font missing U+%04lX in \"%s\"\n",
                        (unsigned long)codepoints[j], text);
                assert(false);
            }
        }
    }

    // 动态数值/电量文本需要的 ASCII 字符。
    for (uint32_t cp = '0'; cp <= '9'; cp++) {
        assert(font_has_glyph(path14, cp));
        assert(font_has_glyph(path20, cp));
    }
    assert(font_has_glyph(path14, '%'));
    assert(font_has_glyph(path14, '-'));
    assert(font_has_glyph(path14, ' '));

    // 反向验证:未使用的汉字不应出现在子集里,避免测试因误匹配而假通过。
    assert(!font_has_glyph(path14, 0x9F98));   // 龘,界面未使用
    assert(!font_has_glyph(path20, 0x9F98));
    assert(!font_has_glyph(path14, 0x4FC5));   // 俄 的下一个码点,界面未使用
}

int main(void) {
    test_utf8_decode();
    test_font_coverage("assets/fonts/app_font_14.c",
                       "assets/fonts/app_font_20.c");
    puts("tetris string and font coverage tests: PASS");
    return 0;
}

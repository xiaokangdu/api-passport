#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "tetris_model.h"

// 单存档槽的序列化格式:显式小端字段 + CRC32,不直接写结构体,
// 避免填充字节和平台差异。读取时强制恢复为 TETRIS_PLAYING。
#define TETRIS_SAVE_ENCODED_SIZE 233

// 把进行中的对局写入 out;GAME_OVER 或字段非法时返回 0。
size_t tetris_save_encode(const tetris_model_t *model, uint8_t *out, size_t cap);

// 校验长度、magic、版本、CRC 与字段范围。成功时 out->state 为 TETRIS_PLAYING。
bool tetris_save_decode(const uint8_t *data, size_t len, tetris_model_t *out);

uint32_t tetris_save_crc32(const uint8_t *data, size_t len);

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "tetris_sound.h"

// 音效播放:独立任务持有 ES8311/I2S,按键回调和 LVGL 任务只投递队列。
// 初始化失败时静默降级,5 秒后再次请求音效会重试初始化。
void tetris_audio_start(void);

// 非阻塞投递;音频被关闭、未就绪或队列已满时丢弃本次音效。
void tetris_audio_play(tetris_sound_t sound);

// 声音总开关。关闭会立即清空待播队列并停止当前音效。
void tetris_audio_set_enabled(bool enabled);

// 输出音量 0..100(%)。设置会立即生效并保留到下次初始化。
void tetris_audio_set_volume(uint8_t percent);

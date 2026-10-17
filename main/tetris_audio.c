#include "tetris_audio.h"

#include <stdbool.h>
#include <stdint.h>

#include "bsp_audio.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

#define AUDIO_SAMPLE_RATE   16000U
#define AUDIO_CHUNK_FRAMES  256U
#define AUDIO_VOLUME        55U
#define AUDIO_QUEUE_DEPTH   8
#define AUDIO_RETRY_US      (5LL * 1000 * 1000)

typedef enum {
    AUDIO_MSG_SOUND = 0,
    AUDIO_MSG_VOLUME,
} audio_msg_kind_t;

typedef struct {
    uint8_t kind;
    uint8_t value;
} audio_msg_t;

static const char *TAG = "tetris_audio";

static QueueHandle_t s_queue;
static bool s_ready;
static int64_t s_last_attempt_us;
static volatile bool s_enabled = true;
static volatile uint8_t s_volume = AUDIO_VOLUME;

static bool audio_ensure_ready(void) {
    if (s_ready) return true;

    const int64_t now = esp_timer_get_time();
    if (s_last_attempt_us != 0 && now - s_last_attempt_us < AUDIO_RETRY_US) {
        return false;
    }
    s_last_attempt_us = now;

    esp_err_t err = bsp_audio_init();
    if (err == ESP_OK) {
        err = bsp_audio_set_format(AUDIO_SAMPLE_RATE, 16, 1);
    }
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "音频初始化失败(%s),静默运行,稍后重试",
                 esp_err_to_name(err));
        return false;
    }

    bsp_audio_set_volume(s_volume);
    s_ready = true;
    ESP_LOGI(TAG, "音效就绪:%u Hz/16 bit/单声道,音量 %u%%",
             (unsigned)AUDIO_SAMPLE_RATE, (unsigned)s_volume);
    return true;
}

static void audio_task(void *arg) {
    (void)arg;
    // 先在后台初始化,首按键音效就不必等待 codec 启动。
    (void)audio_ensure_ready();

    int16_t buf[AUDIO_CHUNK_FRAMES];
    for (;;) {
        audio_msg_t msg;
        if (xQueueReceive(s_queue, &msg, portMAX_DELAY) != pdTRUE) continue;

        if (msg.kind == AUDIO_MSG_VOLUME) {
            if (s_ready) bsp_audio_set_volume(msg.value);
            continue;
        }
        if (!s_enabled || !audio_ensure_ready()) continue;

        tetris_sound_player_t player;
        if (!tetris_sound_start(&player, (tetris_sound_t)msg.value,
                                AUDIO_SAMPLE_RATE)) {
            continue;
        }

        size_t frames;
        while ((frames = tetris_sound_render(&player, buf, AUDIO_CHUNK_FRAMES)) > 0) {
            if (!s_enabled) break;
            if (bsp_audio_write(buf, frames * sizeof(int16_t)) != ESP_OK) {
                ESP_LOGW(TAG, "音效写入失败,下次重新初始化音频");
                s_ready = false;
                break;
            }
        }
    }
}

void tetris_audio_start(void) {
    if (s_queue) return;
    s_queue = xQueueCreate(AUDIO_QUEUE_DEPTH, sizeof(audio_msg_t));
    if (!s_queue) {
        ESP_LOGW(TAG, "音效队列创建失败,本次运行无按键音");
        return;
    }
    if (xTaskCreate(audio_task, "tetris_audio", 4096, NULL, 3, NULL) != pdPASS) {
        ESP_LOGW(TAG, "音效任务创建失败,本次运行无按键音");
        vQueueDelete(s_queue);
        s_queue = NULL;
    }
}

void tetris_audio_play(tetris_sound_t sound) {
    if (!s_enabled || !s_queue) return;
    if ((unsigned)sound >= (unsigned)TETRIS_SOUND_COUNT) return;
    const audio_msg_t msg = {
        .kind = AUDIO_MSG_SOUND,
        .value = (uint8_t)sound,
    };
    (void)xQueueSend(s_queue, &msg, 0);
}

void tetris_audio_set_enabled(bool enabled) {
    s_enabled = enabled;
    // 关闭时清掉待播音效;正在播放的音效会在下一块渲染前停止。
    if (!enabled && s_queue) {
        (void)xQueueReset(s_queue);
    }
}

void tetris_audio_set_volume(uint8_t percent) {
    if (percent > 100) percent = 100;
    s_volume = percent;
    if (!s_queue) return;
    const audio_msg_t msg = {
        .kind = AUDIO_MSG_VOLUME,
        .value = percent,
    };
    // 音量调整要立刻反馈,插到队首,不等当前音效播完。
    (void)xQueueSendToFront(s_queue, &msg, 0);
}

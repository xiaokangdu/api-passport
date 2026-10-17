// main/main.c —— 俄罗斯方块入口:初始化 BSP 显示与 LVGL 后进入开始界面。
#include "bsp_display.h"
#include "bsp_i2c.h"
#include "bsp_pins.h"
#include "esp_log.h"
#include "tetris_app.h"

static const char *TAG = "main";

void app_main(void) {
    ESP_LOGI(TAG, "俄罗斯方块启动");

    const esp_err_t i2c_err = bsp_i2c_init();
    if (i2c_err != ESP_OK) {
        ESP_LOGW(TAG, "I2C 初始化失败(%s),电量计将不可用",
                 esp_err_to_name(i2c_err));
    }

    if (bsp_display_init() != ESP_OK) {
        ESP_LOGE(TAG, "显示初始化失败,检查 SPI 接线"
                      "(MOSI=%d SCLK=%d CS=%d DC=%d BL=%d)",
                 BSP_LCD_MOSI, BSP_LCD_SCLK, BSP_LCD_CS, BSP_LCD_DC, BSP_LCD_BL);
        return;
    }
    if (!bsp_lvgl_init()) {
        ESP_LOGE(TAG, "LVGL 初始化失败,游戏无法启动");
        return;
    }
    bsp_display_backlight(100);

    tetris_app_start();
}

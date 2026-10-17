idf_component_register(
    SRCS "main.c"
         "tetris_app.c"
         "tetris_audio.c"
         "tetris_menu.c"
         "tetris_model.c"
         "tetris_input.c"
         "tetris_save.c"
         "tetris_settings.c"
         "tetris_sound.c"
         "tetris_strings.c"
    INCLUDE_DIRS "."
    REQUIRES bsp esp_hw_support esp_timer nvs_flash
)

# 由 assets/fonts/ 下的 Noto Sans SC 子集生成,直接纳入本组件编译。
target_sources(${COMPONENT_LIB} PRIVATE
    "${CMAKE_CURRENT_LIST_DIR}/../assets/fonts/app_font_14.c"
    "${CMAKE_CURRENT_LIST_DIR}/../assets/fonts/app_font_20.c"
)

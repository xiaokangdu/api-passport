<p align="right">
  <strong>简体中文</strong> · <a href="README.md">English</a>
</p>

# 资源目录（Assets）

本目录集中存放可复用的资源（字库、图片、音乐等），按资源类型分子目录管理。每个资源放在其类型对应的子目录，并记录放置路径、命名方式、集成方式与来源/许可。二进制资源（字体、图片、音频）不属于纯 markdown 文档，请勿与文档混放。涉及版权/授权的资源需注明来源与许可。

## 字库（fonts）

可复用的字库文件与生成的字库源码放在 `fonts/`。

- 命名要能反映字族、字重、字级与格式。
- 记录来源、许可、字符范围、转换命令与目标放置路径。
- 添加字库前评估 Flash 与内部 RAM 影响；ESP32-C3 无 PSRAM。
- 不提交许可不允许分发的字库。

| 文件 | 格式 | 用途与来源 |
| --- | --- | --- |
| [`fonts/NotoSansSC-Regular.otf`](fonts/NotoSansSC-Regular.otf) | OpenType，8,331,336 字节 | 俄罗斯方块中文界面使用的未修改上游源字体。2026-10-17 下载自 [notofonts/noto-cjk `Sans/SubsetOTF/SC`](https://github.com/notofonts/noto-cjk/tree/main/Sans/SubsetOTF/SC)；SHA-256 `faa6c9df652116dde789d351359f3d7e5d2285a2b2a1f04a2d7244df706d5ea9`。采用 SIL OFL 1.1 许可，见 [`fonts/LICENSE-NotoSansSC.txt`](fonts/LICENSE-NotoSansSC.txt)。 |
| [`fonts/tetris_symbols.txt`](fonts/tetris_symbols.txt) | UTF-8 文本 | 俄罗斯方块界面的字形清单：包含 `main/tetris_strings.h` 用到的全部汉字与全角感叹号；数字、百分号等 ASCII `0x20–0x7E` 另行加入。 |
| [`fonts/app_font_14.c`](fonts/app_font_14.c) | LVGL 9 C 源码，4 bpp，不压缩 | 由上述清单生成的 14 px 子集；由 `main/CMakeLists.txt` 编入固件，符号名为 `app_font_14`，用于标签、统计与提示文字。 |
| [`fonts/app_font_20.c`](fonts/app_font_20.c) | LVGL 9 C 源码，4 bpp，不压缩 | 覆盖范围相同的 20 px 子集，符号名为 `app_font_20`，用于标题、分数与弹层标题。 |

修改字形清单后，在仓库根目录重新生成两份子集：

```sh
lv_font_conv --font assets/fonts/NotoSansSC-Regular.otf --size 14 --bpp 4 \
  --format lvgl --no-compress --range 0x20-0x7E \
  --symbols "$(cat assets/fonts/tetris_symbols.txt)" \
  --lv-font-name app_font_14 --lv-include lvgl.h -o assets/fonts/app_font_14.c
lv_font_conv --font assets/fonts/NotoSansSC-Regular.otf --size 20 --bpp 4 \
  --format lvgl --no-compress --range 0x20-0x7E \
  --symbols "$(cat assets/fonts/tetris_symbols.txt)" \
  --lv-font-name app_font_20 --lv-include lvgl.h -o assets/fonts/app_font_20.c
```

若界面字符串出现子集中缺失的字符，`tests/test_tetris_strings.c` 会直接失败；请同步更新清单并重新执行上述两条命令。

## 图片（images）

可复用的源图与生成的显示资产放在 `images/`。

| 文件 | 尺寸与格式 | 用途与来源 |
| --- | --- | --- |
| [`images/home.jpg`](images/home.jpg) | 3840 × 2160，JPEG | 嵌入中英文项目 README 的产品主图，突出 AI Passport 产品形象与开放、人人可创作的理念。 |
| [`images/readme-hardware-specs.png`](images/readme-hardware-specs.png) | 2172 × 724，PNG RGBA | 保留为可选技术参考图，不再用于首页主视觉。于 2026-09-17 使用内置图像生成工具为本仓库生成；已根据文档中的硬件能力契约核对图中的六项标签与参数。 |
| [`images/logo-wordmark.png`](images/logo-wordmark.png) | 1648 × 336，PNG RGBA | 从仓库原始 `images/logo.png` 中精确裁切并去除背景的黑色字标；用于中英文项目 README 的浅色主题。 |
| [`images/logo-wordmark-dark.png`](images/logo-wordmark-dark.png) | 1648 × 336，PNG RGBA | 提取字标的白色版本；README 使用 `<picture>` 在 GitHub 深色主题下显示。 |

- 使用描述性命名，并记录尺寸、像素格式、转换步骤与目标路径。
- 优先采用适合 240 × 320 RGB565 显示的格式，并纳入 Flash 与内部 RAM 考量。
- 许可允许时保留可编辑源文件，并记录来源与许可。
- 图片中不得包含设备二维码秘密、凭证或个人数据。

## 音乐与音效（music）

可复用的音乐与音效源码放在 `music/`。

- 记录来源、许可、采样率、位深、声道、转换命令与目标路径。
- 与当前 BSP 音频路径匹配时优先采用 16 kHz、16 位单声道 PCM。
- 嵌入音频前评估 Flash 与内部 RAM 成本；长录音应流式或分块。
- 无再分发许可不提交媒体文件。

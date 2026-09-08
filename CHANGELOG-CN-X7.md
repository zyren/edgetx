# EdgeTX 2.12.4 · X7 家族中文（CN）固件改动记录

> 适用对象：PCB = X7 的全部机型（GX12 / Boxer / Pocket / Zorro / TX12MK2 等），
> 含 1MB flash（STM32F407xG）与 512KB flash（STM32F407xE 等）的机型。
> 本文记录 2025 年一次集中适配/修正的过程与最终改动，供日后回溯。

---

## 0. 背景

- 项目原本已实现 GX12 专用的中文字库/显示功能，条件全部写死 `PCBREV STREQUAL GX12 AND TRANSLATIONS STREQUAL CN`。
- 目标是让 **所有 X7 子类机型**（Pocket/Zorro/Boxer/TX12MK2…）都像 GX12 一样显示中文。
- 把所有 `PCBREV STREQUAL GX12` 条件放宽为 `PCB STREQUAL X7` 后出现两类问题：
  1. 非 GX12 机型链接报 `undefined reference to CN_*_glyphs/codepoints/widths`；
  2. 512KB 机型（Pocket/Zorro/TX12MK2）FLASH 溢出（最多 ~19.4KB）。
- 另修复了 CN 模式（10px 行高）下多处长按 8px 坐标画出来的显示错位。

---

## 1. 构建修正：中文字库对所有 X7 生效

| 文件 | 改动 |
| --- | --- |
| `radio/src/gui/common/stdlcd/CMakeLists.txt` | 字库源文件编入条件：`PCBREV STREQUAL GX12` → `PCB STREQUAL X7`（修复 `undefined reference` 的关键一处） |

> `radio/src/CMakeLists.txt` 与 `radio/src/tests/CMakeLists.txt` 里改为 `PCB STREQUAL X7` 的三处条件是本人（使用者）在本次之前已改好的，不是本文档范围。

---

## 2. 512KB 机型容量适配（精简内置字库 + 去掉频谱仪）

### 2.1 思路

- 中文功能代码本身只差 ~172B；溢出大头是内置字库数据 ≈19.2KB：
  - `cn_default_10`：720 字形 ≈16.6KB（最大头）
  - `cn_12` + `cn_16` + `cn_10` ≈2.6KB
- 1MB 机型（GX12/Boxer）保留完整内置字库；512KB 机型改编译**精简字库**（`EDGETX_CN_STDLCD_LITE`）。
- 精简原则：**只内嵌 SD 外置字库（`/FONTS/CN_BASIC.FNT`，覆盖 U+4E00–U+9FFF）覆盖不到的字符**；
  其余全部 CJK 汉字在运行时读 SD 外置字库。

### 2.2 精简字库保留的字符（新增文件，符号名与完整版一致、码点升序、索引 0 = □）

| 文件（均在 `radio/src/fonts/cn/generated/`） | 保留字形 |
| --- | --- |
| `cn_default_10_lite.h/.cpp` | `□` U+25A1、`、` U+3001、`，` U+FF0C（3 个） |
| `cn_12_lite.h/.cpp`、`cn_16_lite.h/.cpp` | 仅占位 `□`（各 1 个，原 33 个汉字由 SD 字库 12/16px 提供） |

> `cn_10.h/.cpp` 本来就只有 □，两模式共用，无需精简版。

### 2.3 编译开关接线

| 文件 | 改动 |
| --- | --- |
| `radio/src/CMakeLists.txt` | `include(gui/…)` 之前计算 `EDGETX_CN_USE_LITE_FONTS = ON`（条件：`PCB==X7 AND TRANSLATIONS==CN AND NOT NATIVE_BUILD AND NOT TARGET_FLASH_SIZE`，即 <1MB flash 的真机构建）；`firmware` 目标在 lite 时追加编译宏 `EDGETX_CN_STDLCD_LITE` |
| `radio/src/gui/common/stdlcd/CMakeLists.txt` | `cn_10.cpp` 两模式共用；按 `EDGETX_CN_USE_LITE_FONTS` 选择编入完整版或 `*_lite.cpp`；频谱仪：`if((PXX2 OR MULTIMODULE) AND NOT EDGETX_CN_USE_LITE_FONTS)` 才编入 `radio_spectrum_analyser.cpp` |
| `radio/src/gui/common/stdlcd/lcd_common.cpp` | `#if defined(EDGETX_CN_STDLCD_LITE)` 时 include `*_lite.h`，否则 include 完整版（`cn_10.h` 两者共用） |
| `radio/src/gui/common/stdlcd/radio_tools.cpp` | 三处“频谱仪”工具入口加 `#if !defined(EDGETX_CN_STDLCD_LITE)`（Power Meter / Ghost / Lua 工具保留） |

**效果**：Pocket 链接通过（省 ~19KB）；频谱仪功能只在 512K 中文机移除，1MB 机型保留。

---

## 3. CN 显示对齐修正（全部仅 `EDGETX_CN_STDLCD` 生效，英文/非 CN 不受影响）

> 根因：CN 模式行高 `FH=10`、默认文字画在行内 `y+2..y+8`（居中），
> 而原 8px 时代写死的图形/坐标在 `y..y+6`（顶对齐），因此普遍“偏高 2px”。

| 文件 | 位置 | 修改 |
| --- | --- | --- |
| `radio/src/gui/common/stdlcd/lcd_common.cpp` | `externalCnTopOffset()` | SD 外置 10px 汉字偏移 `1 → 0`，与内嵌字库同位置；消除 1px 串行到下一行 |
| 同上 | `lcdPutLegacyDefaultPattern()` | 反白(INVERS)只反白墨迹带 `y+2..y+8`，不再反白整 10px 格（选中条不压邻行） |
| 同上 | `lcdDrawChar()` CN 分支 | 默认字拦截范围 `0x20..0x7E` → `0x20..0x7F`（字符 127 的左箭头与 126 一样居中） |
| `radio/src/gui/common/stdlcd/draw_functions.cpp` | `drawCheckBox()` | CN 下 7×7 方框/高亮移到 `y+2`，正包住 `#` 对勾 |
| 同上 | `drawSlider()` | CN 下横线 `y+3 → y+5`（与滑块钮/文字中线对齐；音量/震动/长度条一次全修） |
| 同上 | `drawSource()` | 输入源 / Lua“键帽”图标 CN 下：实心块 `y→y+2`、内部小字 `y+1→y+3` |
| `radio/src/gui/common/stdlcd/calibration.cpp` | `menuCommonCalib()` | CN 下说明文字起点 `calibTextY=2`（原来是 `MENU_HEADER_HEIGHT`=10），避免压到摇杆方框（顶 y=33） |
| `radio/src/gui/128x64/model_failsafe.cpp` | 失控保护通道条 | CN 下 4px 条整体 `+2`（`barY = y+2`） |
| `radio/src/gui/common/stdlcd/model_receiver_options.cpp` | 接收机通道输出监视条 | CN 下同样 `barY = y+2` |

---

## 4. Lua 脚本适配（仅 `temp/elrs.lua`，非固件改动）

只针对黑白（mono，Pocket 等）分支：

1. 每页行数不再写死 `maxLineIndex = 6/9`，改为按屏幕高度计算
   （`maxLineIndex = floor((LCD_H - textSize - textYoffset) / textSize) - 1`），
   修掉“最后一行被屏底切掉半行”的现象。
2. 黑白分支行距 `textSize = 10`，与 CN 系统菜单 10px 行高一致
   （若日后要兼容原生 8px 行距的 EdgeTX，把它改回 `8`）。
3. 黑白标题条 `barHeight = textSize`，反白条盖满整行。
4. “No ExpressLRS”错误页改成独立紧凑 8px 排版，保证 6 行提示完整可见。

---

## 5. 已撤销的试验（勿再引用）

- `radio/src/lua/api_stdlcd.cpp` 曾临时给黑白 Lua 增加 `lcd.sizeText`（照彩屏写法）
  并让 ELRS 用 `lcd.sizeText("Qg")` 自适应行高 —— **实测无效果，已完整回滚**，
  该文件当前与原始版本一致（无 `sizeText` 残留）。

---

## 6. 注意事项

1. 所有 C++/CMake 改动需重新编译固件；`elrs.lua` 拷到遥控器 SD 卡脚本目录即可。
2. 512K 机型汉字依赖 SD 卡 `/FONTS/CN_BASIC.FNT`
   （与 GX12 发布同款；已知成品 SHA-256：`C7D01736D365736DB04AEFD1FC103DB0FC9BAAA84841C058B0CE23828F894A79`）。
   缺卡/文件缺失时 CJK 显示占位框属预期；`、` `，` `□` 由内置精简字库保证。
3. `fonts/cn/generated/*_lite.*` 是从完整表**手工抽取**的派生文件；
   将来若用 `tools/cn_fonts/generate.py` 重生成字库，需要同步重建精简版。
4. 频谱仪仅在 512K 中文（lite）固件移除；1MB 机型、模拟器保留。
5. 非本文档范围的既有改动（请勿误回滚）：
   - `radio/src/CMakeLists.txt`、`radio/src/tests/CMakeLists.txt` 中
     `PCB STREQUAL X7 AND TRANSLATIONS STREQUAL CN` 条件（本人此前已改）；
   - `radio/src/bootloader/CMakeLists.txt` 的 include 目录 `BEFORE` 关键字（本分支既有）。

---

## 7. 涉及文件速查

**改动（C++/CMake）**
```
radio/src/CMakeLists.txt
radio/src/gui/common/stdlcd/CMakeLists.txt
radio/src/gui/common/stdlcd/lcd_common.cpp
radio/src/gui/common/stdlcd/draw_functions.cpp
radio/src/gui/common/stdlcd/calibration.cpp
radio/src/gui/common/stdlcd/model_receiver_options.cpp
radio/src/gui/common/stdlcd/radio_tools.cpp
radio/src/gui/128x64/model_failsafe.cpp
```

**新增（精简字库）**
```
radio/src/fonts/cn/generated/cn_default_10_lite.h
radio/src/fonts/cn/generated/cn_default_10_lite.cpp
radio/src/fonts/cn/generated/cn_12_lite.h
radio/src/fonts/cn/generated/cn_12_lite.cpp
radio/src/fonts/cn/generated/cn_16_lite.h
radio/src/fonts/cn/generated/cn_16_lite.cpp
```

**Lua 脚本**
```
temp/elrs.lua   （改好待拷贝到遥控器使用）
```

**曾改动又已回滚（保持原样）**
```
radio/src/lua/api_stdlcd.cpp
```

**编译验证命令（在容器内）**
```
cd build_pocket && ./build-pocket.sh
```

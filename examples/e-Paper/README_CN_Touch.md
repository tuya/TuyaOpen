# 触摸墨水屏（e-Paper Touch）示例

## 概述

本示例展示如何在涂鸦开发环境中使用本地驱动库，配置、编译并烧录带触摸功能的墨水屏（例如 Waveshare 2.13inch Touch e-Paper HAT），以及如何移植驱动。

## 建议引脚映射（示例）

下列为参考接线；实际请以所用屏幕与开发板的数据手册为准：

- DIN: `P16`
- SCLK: `P14`
- CS: `P18`
- DC: `P19`
- RST: `P47`
- BUSY: `P46`
- I2C_INT: `P44`
- I2C_CLK: `P42`
- I2C_DIN: `P43`
- I2C_RST: `P40`

如硬件不同，请根据实际引脚修改接线并调整 `lib/Config/DEV_Config.c` 中的 SPI/引脚配置。

## 快速开始

1. 进入涂鸦编译环境。
2. 切换到示例目录（例如 `2.13inch_e-Paper_Touch`）：

```powershell
cd 2.13inch_e-Paper_Touch
```

3. 配置工程（示例使用 `T5AI Board`）：

```powershell
tos.py config choice
```

4. 编译工程：

```powershell
tos.py build
```

5. 编译完成后，烧录固件：

```powershell
tos.py flash
```

提示：若遇到编译或烧录问题，优先检查 `lib/Config/DEV_Config.c` 中的 SPI 与引脚配置是否与目标板一致。

## 新增或移植屏幕示例

如需新增示例或移植第三方驱动，请按下列步骤操作：

1. 在 `examples/` 下添加或替换示例程序（例如 `EPD_xxxx_test.c`）。
2. 在 `lib/e-Paper/` 下添加或替换显示驱动（`EPD_xxxx.c` / `EPD_xxxx.h`）。
3. 在 `lib/e-Driver/` 下添加或修改触摸驱动代码。
4. 确保示例中测试函数命名为 `EPD_test`，主函数会调用该函数以运行示例。

## 仓库结构

- `examples/` — 示例程序和测试代码。

- `lib/` — 驱动、字体与 GUI 辅助代码。

如需帮助，请查看各子目录的 README 或联系维护者。

## 参考链接

- Waveshare 2.13inch Touch e-Paper HAT: [https://www.waveshare.net/shop/2.13inch-Touch-e-Paper-HAT.htm](https://www.waveshare.net/shop/2.13inch-Touch-e-Paper-HAT.htm)

## 硬件建议

- 如果你购买并使用上述 Waveshare 屏幕，建议通过主板的 40-pin IO 接口连接，以获得更稳定的电源和信号线布局。

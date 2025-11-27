# 墨水屏（e-Paper）示例

## 简介

本仓库包含若干常见尺寸的墨水屏示例（例如 1.54inch / 4.26inch）。示例基于本地驱动库，演示如何在涂鸦开发环境下配置、编译、烧录以及移植屏幕驱动。

## 引脚映射（示例）

以下为样例连接（具体请参考你所用墨水屏与开发板的数据手册）：

- SCLK: `P02`
- DIN:  `P04`
- CS:   `P03`
- DC:   `P07`
- RST:  `P08`
- BUSY: `P06`
- PWR:  `P28`

如果你的硬件不同，请按实际引脚调整并修改 `lib/Config/DEV_Config.c` 中的配置。

## 快速使用步骤

1. 进入涂鸦编译环境。
2. 进入对应的墨水屏型号目录（例如 `4.26inch_e-Paper`）：

```powershell
cd 4.26inch_e-Paper
```

3. 配置项目（示例使用 `T5AI Board`）：

```powershell
tos.py config choice
```

4. 编译项目：

```powershell
tos.py build
```

5. 编译完成后，烧录到设备：

```powershell
tos.py flash
```

> Tip: 若出现编译或烧录问题，先检查 `DEV_Config.c` 中的 SPI/引脚配置与目标板一致。

## 新增或移植屏幕示例

如果需要新增屏幕示例或移植第三方驱动，请按下列步骤：

1. 在 `examples/` 下替换或添加对应的 `EPD_xxxx_test.c`（示例程序）。
2. 在 `lib/e-Paper/` 下替换或添加 `EPD_xxxx.c` 与 `EPD_xxxx.h`（屏幕驱动实现）。
3. 请保证示例中的测试函数名为 `EPD_test`，主函数中会调用此函数以运行示例。

## 目录说明

- `examples/`：示例程序入口与测试代码。
- `lib/`：驱动、字体和 GUI 工具代码。

如需帮助，可查看每个子目录下的 `README` 或联系维护者。

## 屏幕链接

- [Waveshare 4.26inch e-Paper](https://www.waveshare.net/shop/4.26inch-e-Paper.htm)
- [Waveshare 4.26inch e-Paper HAT](https://www.waveshare.net/shop/4.26inch-e-Paper-HAT.htm)
- [Waveshare 1.54inch e-Paper](https://www.waveshare.net/shop/1.54inch-e-Paper.htm)
- [Waveshare 1.54inch e-Paper Module](https://www.waveshare.net/shop/1.54inch-e-Paper-Module.htm)

# 屏幕配置

## TuyaOpen 工程配置不同的屏幕

本文档针对 TuyaOpen 开发工程中配置不同的屏幕进行说明，包括快速配置指南以及详细配置说明，方便开发者快速了解 TuyaOpen 工程的屏幕配置选项。

工程已支持屏幕清单




| SPI 协议 | gc9a01  | st7789 | ili9488 |
| -------- | ------- | ------ | ------- |
| RGB 协议 | ili9488 |        |         |

# 文件结构

TuyaOpen 中整体显示模块文件结构如下，其中，Kconfig 可以新增选择以及 menuconfig 配置方式，spi 以及 rgb 配置代码在对应的文件夹中，开发者可以根据对应的配置查看或修改代码。

```
TuyaOpen/
  ├── docs/            # 说明文档
  │   └── quick_start.md
  └── src/
      └── peripherals/ #外设
            └── display/ #显示
                  ├─ include/ #头文件
                        └── lcd_gc9a01.h
                  ├─ tft_rgb/ #rgb 配置代码
                        └── disp_rgb_ili9488.c
                  ├─ tft_spi/ #spi 配置代码
                        └── disp_spi_gc9a01.c
                  └── Kconfig #Kconfig 配置文件
  
```

# 配置说明

## 屏幕分辨率

根据屏幕参数设置屏幕分辨率

```根据屏幕参数设置屏幕分辨率
DISPLAY_LCD_WIDTH  
DISPLAY_LCD_HEIGHT 
```

颜色字节顺序反转

```
LVGL_COLOR_16_SWAP
```

"Swap Color Bytes" 通常指 颜色数据字节顺序的硬件/软件调换，这与物理线序（Pin Order）差异直接相关。
如果打开了后颜色异常（红蓝反色），则关闭反转，具体是否需要打开根据硬件连接判断

## 根据屏幕原理图以及硬件连接配置引脚

### 屏幕电源开关

屏幕是否支持引脚开关，如果原理图中有引脚开关屏幕供电，则配置该选项并且选择正确的引脚和高低电平驱动

```
ENABLE_LCD_POWER_CTRL
```

开关引脚，设置对应的开关引脚：

```
DISPLAY_LCD_POWER_PIN
```

开关引脚高低电平设置，高低电平控制开关，根据原理图配置（高电平打开或者低电平打开）

```
DISPLAY_LCD_POWER_POLARITY_LEVEL
```

### 屏幕背光开关

#### gpio控制模式

gpio 模式，选择引脚高低电平控制背光开关

```
ENABLE_LCD_BL_MODE_GPIO 
```

配置对应的背光引脚

```
DISPLAY_LCD_BL_PIN
```

配置低电平或者高电平点亮屏幕

```
DISPLAY_LCD_POWER_POLARITY_LEVEL
```

#### pwm控制模式

选择 pwm 引脚控制背光开关和亮度

```
ENABLE_LCD_BL_MODE_PWM
```

根据芯片规格书选择 pwm id

```
DISPLAY_LCD_BL_PWM_ID
```

根据芯片规格书选择 pwm 频率

```
DISPLAY_LCD_BL_PWM_FREQ
```

#### 无背光控制模式

该屏幕不支持背光开关（屏幕默认背光打开）

```
ENABLE_LCD_BL_MODE_NONE
```

### 屏幕驱动方式

SPI驱动协议采用串行通信机制，通过单根数据线（MOSI）逐位传输像素数据，搭配独立的时钟线（SCLK）实现信号同步。RGB驱动协议基于并行总线架构，通过16-24根数据线同步传输整像素色彩分量（如RGB565/RGB888），配合 HSYNC（行同步）、VSYNC（帧同步）及像素时钟（DOTCLK）实现高速时序控制。

根据屏幕驱动方式选择协议

lcd 屏幕，spi 驱动

```
ENABLE_DISPLAY_LCD_SPI
```

lcd 屏幕，rgb 驱动

```
ENABLE_DISPLAY_LCD_RGB
```

### 屏幕驱动芯片选择

屏幕驱动芯片（Display Driver IC, DDIC）是连接主控芯片与显示面板的「智能翻译官」，负责将数字信号转化为屏幕能理解的物理信号，根据硬件选择屏幕对应的驱动芯片。

选择 st7789 驱动芯片

```
ENABLE_LCD_SPI_ST7789
```

选择 gc9a01 驱动芯片

```
ENABLE_LCD_SPI_GC9A01
```

选择 ili9341 驱动芯片

```
ENABLE_LCD_SPI_ILI9341
```

### 屏幕 spi 引脚配置

根据硬件原理图和模组规格书选择对应的 SPI 端口，以 T5 模组为例[https://developer.tuya.com/cn/docs/iot/T5-E1-Module-Datasheet?id=Kdar6hf0kzmfi] P14 和 P16 选择 SPI0

```
LCD_SPI_PORT
```

spi 频率，默认最大频率 48000000 即可，如果屏幕有频率限制则改到对应频率

```
LCD_SPI_CLK
```

根据原理图选择片选引脚、数据/命令选择引脚、复位引脚

```
LCD_SPI_CS_PIN #片选引脚
LCD_SPI_DC_PIN #数据/命令选择引脚
LCD_SPI_RST_PIN #复位引脚
```

### 配置触摸选项

如果屏幕支持触摸功能，可以打开工程中的屏幕触摸选项并选择正确的配置

```
CONFIG_LVGL_ENABLE_TOUCH
```

选择 gt911 触摸芯片

```
CONFIG_ENABLE_TOUCH_GT911
```

选择 gt1151 触摸芯片

```
CONFIG_ENABLE_TOUCH_GT1151
```

选择 cst816x 触摸芯片

```
CONFIG_ENABLE_TOUCH_CST816X
```

根据原理图以及模组规格书选择对应的 i2c 端口和引脚

```
CONFIG_TOUCH_I2C_PORT #i2c 端口
CONFIG_TOUCH_I2C_SCL #配置 scl 引脚
CONFIG_TOUCH_I2C_SDA #配置 sda 引脚
```

# 快速配置示例

以圆形 spi 屏幕为示例，屏幕参数如下：

| 分辨率            | 240*240            |
| ----------------- | ------------------ |
| 驱动芯片          | gc9a01             |
| 电源控制引脚      | 无                 |
| 背光控制引脚      | P9                 |
| 背光控制方式      | GPIO模式（高电平） |
| 驱动协议          | SPI                |
| SPI引脚           | PORT 0             |
| SPI频率           | 48000000           |
| 片选引脚          | P24                |
| 数据/命令选择引脚 | P23                |
| 复位引脚          | P28                |
| 触摸              | 无                 |
| 颜色反转          | 1                  |

打开到 lvgl_demo 工程位置  

`cd TuyaOpen/platform/T5AI/examples/graphics/lvgl_demo`

运行 menuconfig

```sh
$ tos menuconfig
```

配置TuyaOpen工程

```
    configure project  --->
    Choice a board (T5AI)  --->
  *  configure tuyaopen  --->
```

配置驱动并且打开颜色反转

```
[*]     swap color bytes
    configure device driver  --->
```

设置屏幕分辨率

```
(240)   tft lcd width
(240)   tft lcd height
```

电源控制引脚，无需配置

```
[ ]     it requires controlling the power supply pin of the lcd
```

选择引脚配置模式为gpio模式

```
(X) gpio pin control the backlight
( ) pwm control the backlight
( ) controlling the backlight is not supported
```

配置 gpio 引脚和电平模式

```
(9)     tft lcd bl pin
(1)     tft lcd backlight active level 0:low 1:high
```

选择驱动芯片为gc9a01

```
( ) st7789
(X) gc9a01
( ) ili9341
```

选择协议为spi协议

```
( ) lcd rgb interface
(X) lcd spi interface
```

配置 spi 引脚

```
(0) tft lcd spi port
(48000000) tft lcd spi spi freq
(24) tft lcd cs pin
(23) tft lcd dc pin
(28) tft lcd rst pin
```

选择正确的平台进行编译和烧录，具体不懂可以参考 quick_start

```sh
    $ tos build
    $ tos flash
```

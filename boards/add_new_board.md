# 在 your_chat_bot 中添加新的开发板

新增开发板需要先在 `board/<chip_name>/<board_name>` 中创建新开发板的文件夹，完成板级**外设驱动代码**和 **config 文件** 。我们可以从其他已经适配好的开发板（如：boards/ESP32/bread-compact-wifi ）的中 copy 以下文件开始：

+ app_board_api.h
+ CMakeLists.txt
+ Kconfig
+ board_config.h
+ borad_xxx.c 

| 文件            | 功能                       |
| --------------- | :------------------------- |
| app_board_api.h | 板级模块 api               |
| board_config.h  | 板级配置文件               |
| borad_xxx.c     | 适配板级模块驱动           |
| CMakeLists.txt  | CMake 文件                 |
| Kconfig         | 配置 `menuconfig` 工具内容 |

1. `CMakeLists.txt` 和 `app_board_api.h` 一般不做修改。
2. 在 Kconfig 文件中根据开发板的实际情况进行修改，`CHIP_CHOICE` 为开发板的芯片，`BOARD_CHOICE` 为开发板的名称，`BOARD_CONFIG` 为选择需要的板级模块，`AUDIO_DRIVER_NAME` 为 audio codec 名字。修改 Konfig 文件可以在 menuconfig 工具中配置开发板功能，具体可配置功能可以参考其他 .config 文件以及查阅 `menuconfig` 文档。**此外还需要完成其他目录下 config 文件**。

   1. 在 `apps/tuya.ai/your_chat_bot/config` 新建 BOARD-XXX.config 文件，在里面添加相关的配置功能。
   2. 在 `boards/ESP32/Kconfig` 路径下的 Kconfig 文件添加自己开发板选项。

   注意：Kconfig 文件中的开发板名称需要和 board/xxx 下自己新建文件夹名称相同、大小写相同。
3. `board_config.h` 为开发板配置文件，在这个文件中进行 LCD 、 Audio 等外设进行选择，还有对 LCD 大小和 lvgl 等配置都可以放在 `board_config.h` 中。

```c
// lcd
#define DISPLAY_TYPE_UNKNOWN      0
#define DISPLAY_TYPE_OLED_SSD1306 1
#define DISPLAY_TYPE_LCD_SH8601   2

#define BOARD_DISPLAY_TYPE DISPLAY_TYPE_OLED_SSD1306

// io expander
#define IO_EXPANDER_TYPE_UNKNOWN 0
#define IO_EXPANDER_TYPE_TCA9554 1

#define BOARD_IO_EXPANDER_TYPE IO_EXPANDER_TYPE_UNKNOWN
```

4. `borad_xxx.c` 具体名称一般为开发板的名字，该文件的功能是用于开发板的硬件外设的初始化。

主要实现以下功能：

+ `app_audio_driver_init()` 音频驱动初始化
+ `board_display_init()` 屏幕硬件初始化
+ `board_display_get_panel_io_handle()` 获取屏幕 panel_io 句柄
+ `board_display_get_panel_handle()` 获取屏幕 panel 句柄

注意：`board_display_init()`, `board_display_get_panel_io_handle()` 和 `board_display_get_panel_handle()` 这三个函数主要用于 ESP32 ，T5AI 不需要。 

以 ESP32 驱动 ES8838 和 ST7789 为例，需要在 board_xxx.c 中完成下列函数。

```c
OPERATE_RET app_audio_driver_init(const char *name)
{
    /* 此处完成 codec 配置 */
    return tdd_audio_es8388_codec_register(name, codec);
}

int board_display_init(void)
{
    /* boards/ESP32/common/lcd 下配置lcd */
    return lcd_st7789_init();
}

void *board_display_get_panel_io_handle(void)
{
    return lcd_st7789_get_panel_io_handle();
}

void *board_display_get_panel_handle(void)
{
    return lcd_st7789_get_panel_handle();
}

```

完成以上功能配置可以来到 `apps/tuya.ai/your_chat_bot` 使用 `tos config_choice` 选择自己添加的开发板型号；

1. 使用 `tos menuconfig` 选择开发板的模块以及配置功能。
2. 使用 `tos savedef` 保存生成的配置内容至 `app_default.config` 中。

​	提示：可以将本次配置内容保存至  `apps/tuya.ai/your_chat_bot/config` 下新建的BOARD-XXX.config 文件中，下次可以通过 `tos config_choice` 切换开发板平台。

3. 现在可以使用 `tos build` 开始编译，看到编译成功说明框架移植成功。



接下来将会从音频驱动、LCD 驱动和 UI 三部分进行介绍，在之前为了便于添加驱动，需要先简单介绍下编译相关的设计。

在 tuyaopen 上分为 app, src, board, platform 四部分：

+ platform 中是芯片原厂的 SDK，还有 tuyaopen 的适配层 tkl
+ src 中是 tuyaopen 的源代码
+ app 是应用代码
+ board 是开发板相关的代码

这里需要注意的是，tkl 可以调用原厂的代码；src 可以调用 tkl 的代码，不能调用原厂的代码；app 可以调用 tkl，src 的代码，不能调用原厂的代码。

boards/ESP32/common 只能调用tkl、原厂的代码；boards/ESP32 下除了 common 之外的文件夹存放的是开发板的内容，这里只能调用，tkl，src，boards/common 中的代码。之所以在 boards 中新增 common 是为了满足在 tkl 接口能力满足不了的情况下，便于使用原厂的一些功能。

## 1. 音频驱动

### 1.1 音频初始化流程

音频驱动的初始化流程如下：

 - apps/tuya.ai/your_chat_bot/src/tuya_main.c:257: user_main()
 - apps/tuya.ai/your_chat_bot/src/tuya_main.c:316: app_chat_bot_init()
 - apps/tuya.ai/your_chat_bot/src/app_chat_bot.c:369 ai_audio_init()
 - apps/tuya.ai/your_chat_bot/src/ai_audio/ai_audio_main.c:261 ai_audio_input_init()
 - apps/tuya.ai/your_chat_bot/src/ai_audio/ai_audio_input.c:496 __ai_audio_input_hardware_init()
 - apps/tuya.ai/your_chat_bot/src/ai_audio/ai_audio_input.c:440 app_audio_driver_init()
 - boards/T5AI/TUYA_T5AI_BOARD/tuya_t5_ai_board.c:54 tdd_audio_t5ai_register()
 - apps/tuya.ai/your_chat_bot/src/ai_audio/ai_audio_input.c:442 tdl_audio_open()

在 `app_audio_driver_init()` 中会对音频硬件进行注册，对于 T5AI 开发板来说该函数的实现在 `boards/T5AI/TUYA_T5AI_BOARD/tuya_t5_ai_board.c`。其他开发板的音频驱动也在 `boards/xxx/xxx` 下，例如 ESP32S3 面包板是在 `boards/ESP32/bread-compact-wifi/bread_compact_wifi.c` 文件中。ESP32 的开发板来说所有音频驱动都存放在 `boards/ESP32/common/audio`，适配一款新的 ESP32 开发板时，可以先查看在 `boards/ESP32/common/audio` 下是否已经有你需要的 codec 驱动，如果有你可以直接使用，如果没有就需要你自己新增一款 codec 驱动了。

目前 ESP32 已经支持的 codec 驱动有：

 - no codec
 - es8311
 - es8388

这里以面包板为例，当 `boards/ESP32/bread-compact-wifi/bread_compact_wifi.c` 中实现 `OPERATE_RET app_audio_driver_init(const char *name);` 功能后，音频驱动就适配好了。在开始音频驱动初始化的时候将驱动名称 `name`  设置了，后续应用就可以通过 `name` 获取驱动句柄，从而操作 audio codec。

### 1.2 新增音频驱动

新增音频驱动也就新增 tdd_audio_xxx.c 和 tdd_audio_xxx.h 文件。该文件需要实现以下接口：

 - OPERATE_RET tdd_audio_xxx_register(char *name, TDD_AUDIO_XXX_T cfg);

`tdd_audio_xxx_register()` 由应用调用，用来选择使用的音频驱动。在 `tdd_audio_xxx_register()` 中会调用 `tdl_audio_driver_register()`，用于向 `tdl_audio_manage.c` （src/peripherals/audio_codecs/tdl_audio/src/tdl_audio_manage.c）注册音频驱动。

`tdl_audio_driver_register()` 中参数 `TDD_AUDIO_INTFS_T` 内容如下：

```c
typedef struct {
    OPERATE_RET (*open)(TDD_AUDIO_HANDLE_T handle, TDL_AUDIO_MIC_CB mic_cb);
    OPERATE_RET (*play)(TDD_AUDIO_HANDLE_T handle, uint8_t *data, uint32_t len);
    OPERATE_RET (*config)(TDD_AUDIO_HANDLE_T handle, TDD_AUDIO_CMD_E cmd, void *args);
    OPERATE_RET (*close)(TDD_AUDIO_HANDLE_T handle);
} TDD_AUDIO_INTFS_T;
```

 - `OPERATE_RET (*open)()` 的功能是初始化音频驱动，后续通过 MIC 采集到音频通过注册的回调函数 `TDL_AUDIO_MIC_CB mic_cb` 传给应用。这里一般默认 10ms 进行一次回调传输，mic 配置一般是采样率 16K，位深 16bit，单通道模式，所以每 10ms 一般传给应用的数据为 320 字节。
 - `OPERATE_RET (*play)()` 是进行音频播放。
 - `OPERATE_RET (*config)()` 是配置函数，目前有两个命令字 `TDD_AUDIO_CMD_SET_VOLUME`  和 `TDD_AUDIO_CMD_PLAY_STOP` 。
 - `OPERATE_RET (*close)()`是关闭函数。
   `TDD_AUDIO_CMD_SET_VOLUME` 作用是用于设置设备音量大小，`TDD_AUDIO_CMD_PLAY_STOP` 是停止当前播放。

## 2. LCD 显示驱动

目前 T5AI 使用的是 TuyaOpen 的 lvgl， ESP32 片使用的还是 ESP32 的 lvgl，所以这里导致需要对 ESP32 和 T5AI 进行一个区分说明。

### 2.1 T5AI

对于 T5AI 如果需要新增屏幕驱动可以查看下列文件：

 - apps/tuya.ai/your_chat_bot/src/display/tuya_lvgl.c
 - platform/T5AI/tuyaos/tuyaos_adapter/src/driver/tkl_display/

### 2.2 ESP32

ESP32 适配的 LCD 驱动存放位置在 `boards/ESP32/common/lcd` 中，目前支持的 LCD 驱动如下：

+ oled ssd1306
+ lcd sh8601

这里需要实现的接口为：

```
int lcd_xxx_init(void);

void *lcd_xxx_get_panel_io_handle(void);

void *lcd_xxx_get_panel_handle(void);
```

`lcd_xxx_init()` 用于实现 LCD 屏幕的初始化，`lcd_xxx_get_panel_io_handle()`  和 `lcd_xxx_get_panel_handle()` 用于获取 `panel` 和 `panel_io` 的句柄，用于 esp32 的 lvgl 初始化。

ESP32 的 lvgl 初始化在 `boards/ESP32/common/display/tuya_lvgl.c` 中。

上述三个接口实现后，LCD 驱动功能就实现了。

## 3. UI

目前由于 T5AI 和 ESP32 使用的 lvgl 不是同一个，所以 T5AI 和 ESP32 UI 放置的位置不太一样，但是 UI 中的内容是完全一样的。

+ T5AI 放在 apps/tuya.ai/your_chat_bot/src/display/ui
+ ESP32 放在 boards/ESP32/common/display/ui

目前实现的 UI 有 3 中：

+ wechat：类似于微信聊天的界面
+ chatbot： 和小智的默认 UI 一样
+ OLED：和小智的 OLED UI 一样，为小屏幕  OLED 设计的

在进行 UI 适配，需要先通过 `tos menuconfig` 选择 UI。

然后在 display/app_display.c:85 __get_ui_font() 为你的开发板选择字体。

需要注意的是，lvgl 的一些配置都放在了 board_config.h。


<p align="center"><a href="https://tuyaos.com" target="_blank"><img src="https://github.com/tuya/.github/raw/main/profile/site_logo.png" width="400"></a></p>

## 概述

支持四种语音交互模式、能够语音对话、能够感知情绪、通过摄像头感知周边环境，并且表达自己情绪的示例。开发者可以可以按照 `快速开始` 章节介绍，来配置示例，以满足自己的不同需求。

此示例是开源开放的，帮助大家理解如何使用 `Wukong AI` 硬件开发框架开发产品，希望起到抛砖引玉的作用。示例工程没有经过完整的测试流程，难免有一些问题，细节也没经过打磨，如果大家在使用的时候遇到问题，可以自行修改代码，也可以到 `TuyaOS` 开发者论坛 [联网单品开发版块](https://www.tuyaos.com/viewforum.php?f=11) 发帖咨询。如果有开发者直接基于此示例进行量产，需要自行对产品的功能进行完整的测试和验证，涂鸦不对此示例产生的不利后果负责。



## 快速开始

### 配置工程

```shell
make app_menuconfig APP_NAME=tuyaos_demo_ai_toy 
```

* 示例同时支持音频+摄像头+屏幕功能，其中默认支持语音 + `UI`，摄像头需要配置打开；也可以配置关闭 `UI`：

![](https://images.tuyacn.com/fe-static/docs/img/479cbc33-dc07-4577-bc24-4e71d65f9318.png)



* 示例支持 `5` 种硬件形态，可以按需选择开发板类型，默认使用 `T5AI_BOARD`：

![](https://images.tuyacn.com/fe-static/docs/img/a6a12c02-2c5d-48c7-8ae1-31ebad7a2872.png)

分别为：

| 形态                                                         | 功能                                                         |
| :----------------------------------------------------------- | :----------------------------------------------------------- |
| [`T5AI BOARD`](https://developer.tuya.com/cn/docs/iot-device-dev/T5-E1-IPEX-development-board?id=Ke9xehig1cabj) | `T5AI BOARD`开发底板+`3.5`寸屏幕盖板，微信聊天界面。         |
| [`T5AI BOARD EYES`](https://developer.tuya.com/cn/docs/iot-device-dev/T5-E1-IPEX-development-board?id=Ke9xehig1cabj) | `T5AI BOARD`开发底板+`SPI`双眼屏幕盖板，眼睛表情界面。       |
| [`T5AI_BOARD EVB`](https://developer.tuya.com/cn/docs/iot/T5AI-EVB-DATA-SHEET?id=Keghpxqt6wcal) | `T5AI BOARD EVB` 开发盒子，类 `xiaozhi` 聊天界面。           |
| [`T5AI_BOARD ROBOT`]()                                       | `T5AI BOARD ROBOT` 机器狗，硬件方案待发布。                  |
| [`T5AI_BOARD CELLULAR`]()                                    | `T5AI BOARD CELLULAR` 支持 `USB` 外接蜂窝模组拨号上网，无需 `Wi-Fi`配网，硬件方案待发布。 |

注意，打开摄像头即开启多模态模式。需要使用不同的`PID`。

> 默认 `PID` 使用：`e3jrgtmuqsljru1t`
>
> 多模态 `PID` 使用：`zbwbmdyemfa4ipkw`
>
> 机器狗 `PID` 使用：`y0k6ydkxphvv5g7a`
>
> 蜂窝 `PID` 使用：`a3gahyytd3g8oatg`



### 生成文件

```c
make app_config APP_NAME=tuyaos_demo_ai_toy
```

配置完成之后（选择 [`T5AI_BOARD EVB`](https://developer.tuya.com/cn/docs/iot/T5AI-EVB-DATA-SHEET?id=Keghpxqt6wcal)），必须运行上述命令生成新的 `tuya_app_config.h` 文件，否则配置不会生效。

```c
#define LV_COLOR_16_SWAP 1
#define LCD_SPI_DISPLAY 1
// CONFIG_T5AI_BOARD is not set
// CONFIG_T5AI_BOARD_CELLULAR is not set
// CONFIG_T5AI_BOARD_EYES is not set
#define T5AI_BOARD_EVB 1
// CONFIG_T5AI_BOARD_ROBOT is not set
#define ENABLE_TUYA_UI 1
```



### 编译固件

完成工程配置、生成文件之后，即可编译工程，通过`Tuya Wind IDE` 右键菜单一键编译工程，即可生成对应的固件。



## 软件说明

示例程序的产品创建、编译、烧录、调试过程请参考[`Wukong` `AI` 硬件开发框架-快速开始](https://developer.tuya.com/cn/docs/iot-device-dev/quick-start?id=Kectxdshpvsqr)。



### 头文件说明

|        文件         |                         功能                          |
| :-----------------: | :---------------------------------------------------: |
| `tuya_app_config.h` |       `menuconfig`自动生成的文件，不能直接修改        |
| `tuya_device_cfg.h` |           示例程序默认配置，可以按需修改。            |
|  `tuya_ai_debug.h`  |            示例程语音调试配置，即将废弃。             |
| `tuya_ai_battery.h` | 示例程电池管理，需要按照自己的电池型号自行更新/实现。 |
|   `tuya_ai_toy.h`   |          示例程序的各种类型定义和默认配置。           |
| `tuya_ai_display.h` |           示例程序显示状态定义和交互接口。            |



### 源文件说明

|              文件               |                             功能                             |
| :-----------------------------: | :----------------------------------------------------------: |
|      `src/tuya_app_main.c`      |                           入口文件                           |
|       `src/tuya_ai_toy.c`       |           玩具功能文件，实现了玩具的状态、交互逻辑           |
|       `src/tuya_ai_proc.c`       |           AI交互功能文件，实现了AI Agent的状态、交互逻辑           |
|       `src/audio_recorder.c`       |           语音交互状态、语音前端处理逻辑           |
|      `src/tuya_ai_debug.c`      | 示例程语音调试功能，即将废弃。 |
|      `src/reset_netcfg.c`       |        重置功能文件，3次重启之后进入配网状态功能实现         |
|   `src/media/media_src_zh.c`    |                      中文提示音资源文件                      |
|   `src/media/media_src_en.c`    |                      英文提示音资源文件                      |
| `src/audio_analysis/*.c` | 调试语音，通过串口命令 dump 语音数据 |
|      `src/display/ui/*.c`       | ui 文件目录，保存了各种表情图片转换得到的c array文件，包括emo表情、眼睛表情、字库等。 |
| `src/display/tuya_ai_display.c` |             显示功能文件，实现了屏幕驱动加载功能             |
|   `src/display/wechat_app.c`    |        微信交互界面功能文件，实现了微信对话的界面功能        |
|    `src/display/eyes_app.c`     |      双眼表情交互界面功能文件，实现了眼睛表情的界面功能      |
|   `src/display/xiaozhi_app.c`   |        小智交互界面功能文件，实现了小智对话的界面功能        |
| `src/display/robot_app.c` | 机器狗交互界面功能文件，实现了机器狗表情的界面功能 |
|            其他文件             | 参考[tuyaos_demo_quickstart/README.md](./tuyaos_demo_quickstart/README.md) |



### 接口说明

`IoT` 功能接口说明请查看涂鸦开发者 [文档中心-联网单品开发框架-能力地图](https://developer.tuya.com/cn/docs/iot-device-dev/TuyaOS-frame_iot_abi_map?id=Kc4clt7k7h62u) 板块。

`AI` 功能接口说明请查看 [文档中心-`Wukong AI` 硬件开发框架-能力地图](https://developer.tuya.com/cn/docs/iot-device-dev/wukong-abi-map?id=Keedxu1netj62) 板块。



### 功能流程

示例程序的主要流程如下图所示。上电默认是长按对话（TY_AI_TRIGGER_MODE_HOLD），通过按键触发模式切换。

![](https://images.tuyacn.com/fe-static/docs/img/df23cf1f-d925-4e46-bb63-ab5e791c84e9.png)



## 功能修改

### 基本参数调整

默认参数位于 `.include\tuya_device_cfg.h`，可以按需修改。

* #### 🚀 默认参数配置
  ```c
  // 默认音量。注意，这个默认音量仅在第一次有效，因为设备会记录当前的音量到flash，下次启动会加载。
  #ifndef TY_SPK_DEFAULT_VOL
  #define TY_SPK_DEFAULT_VOL 70
  #endif
  
  // 默认语言类型（本地提示音）: 0-chinese, 1-english
  #define TY_AI_DEFAULT_LANG 1
  

  // 低功耗模式，默认关闭，打开这个宏则支持低功耗：TUYA_CPU_SLEEP-普通Wi-Fi低功耗， TUYA_CPU_DEEP_SLEEP-深度睡眠低功耗
  // 进入低功耗之后，通过对话按键可以唤醒设备，退出低功耗状态
  // #define TY_AI_DEFAULT_LOWP_MODE TUYA_CPU_DEEP_SLEEP
  
  // AI monitor，调试工具，待完善，不要修改
  #define ENABLE_APP_AI_MONITOR 0
  
  // 使用云端提示音（提示音保持和角色一样的音色）
  #define ENABLE_CLOUD_ALERT 0
  
  // 音频分析功能，用于分析硬件、软件的拾音、回声消除等效果，默认关闭（会消耗大量内存），按需打开
  #define ENABLE_AUDIO_ANALYSIS 0
  ```
  
  * 其中音量调整在 `APP` 面板上支持，如果需要本地按键调整音量，请参考`tuya_ai_toy.c`文件中的 `key_process` 函数。
  
  * 如果想修改本地默认提示音的语言类型，参考语音提示词修改部分。
  
  
  
* #### 🚀 引脚参数配置

  每一个开发板对应着一组引脚配置，可以直接使用，也可以按照你自己的硬件修改：

  ```c
    // 按需修改
    // default pins
    #define TUYA_AI_TOY_AUDIO_TRIGGER_PIN   TUYA_GPIO_NUM_12    // 对话模式切换按钮
    #define TUYA_AI_TOY_SPK_EN_PIN          TUYA_GPIO_NUM_28    // 喇叭mute
    #define TUYA_AI_TOY_LED_PIN             TUYA_GPIO_NUM_1     // LED（单色）

    // default mode
    #define TUYA_AI_TOY_DEFAULT_CHAT_MODE   TY_AI_TRIGGER_MODE_HOLD // 默认对话模式是长按对话

    // default display
    #define LCD_DEV_NAME     "ili9488"
    #define LCD_WIDTH        320
    #define LCD_HEIGHT       480
    #define LCD_ROTATION     TKL_DISP_ROTATION_0
    #define LCD_FPS          10

    // enable ai opus encode
    #define ENABLE_APP_OPUS_ENCODER 0   // 注意，开启之后会比较消耗CPU，但会减少上传的数据量
  ```
  

  *  **对话模式切换**

  连续按键`audio_trigger_pin`，可以触发对话模式切换。目前支持4种对话模式，分别为：

  ```c
  typedef enum {
      TY_AI_TRIGGER_MODE_HOLD,        // 长按触发模式
      TY_AI_TRIGGER_MODE_ONE_SHOT,    // 单次按键，回合制对话模式
      TY_AI_TRIGGER_MODE_WAKEUP,      // 关键词唤醒模式
      TY_AI_TRIGGER_MODE_FREE,        // 关键词唤醒和自由对话模式
  } TY_AI_TRIGGER_MODE_E;
  ```
  

  * **语音参数修改**


 		可按需修改对应的开发板类型下的默认语音参数配置：

  ```c
  // 按需修改
    #define AUDIO_RECORDER_CFG_INIT(__cfg, __user_cb, __user_data, __spk_mode, __spk_io, __spk_io_level) {  \
      (__cfg)->sample_rate    = TKL_AUDIO_SAMPLE_16K;      \
      (__cfg)->sample_bits    = TKL_AUDIO_DATABITS_16;     \
      (__cfg)->channel        = TKL_AUDIO_CHANNEL_MONO;    \
      (__cfg)->mode           = __spk_mode;                \
      (__cfg)->total_ms       = AUDIO_RECORDER_TOTAL_TIME; \ // 缓存buffer空间
      (__cfg)->slice_ms       = AUDIO_RECORDER_SLICE_TIME; \ // 每次上传100ms的音频数据
      (__cfg)->vad_off_ms     = 300;                       \ // 已无效，忽略 
      (__cfg)->vad_active_ms  = 300;                       \ // 已无效，忽略 
      (__cfg)->vad_silence_ms = 800;                       \ // 已无效，忽略 
      (__cfg)->spk_io         = __spk_io;                  \
      (__cfg)->spk_io_level   = __spk_io_level;            \
      (__cfg)->output_cb      = __user_cb;                 \
      (__cfg)->user_data      = __user_data;               \
  }
  ```


### 本地提示音修改

  示例默认的语音提示词保存在 `.\src\media_src_zh.h` 和 `.\src\media_src_en.h` 中，如果需要修改或者是新增，可以参考[本地提示音修改](https://developer.tuya.com/cn/docs/developer/local-voice-prompt?id=Kehkq7n11rgas)。

  > 提示词语音文件体积较大，需要注意不要超出固件最大支持的体积，否则会编译失败。如果需要较大的内置语音，需要外挂flash，将这些语音存储在外部flash上的文件系统中。

  > 目前云端提示音设备代码已经支持，但是云端的功能还没完全发布，需要等云端发布之后才能使用。
  

### 眼睛表情修改

示例默认的眼睛表情保存在 `.\src\display\ui\eyes128\` 目录下，如果需要修改或者是新增，可以编辑好`GIF`图片之后，使用[LVGL官方转换工具](https://lvgl.io/tools/imageconverter)将图片转换成`c array`。

> 眼睛表情文件体积较大，需要注意不要超出固件最大支持的体积，否则会编译失败。如果需要较大的眼睛表情图片，需要外挂flash，将这些图片存储在外部flash上的文件系统中。



### 语音唤醒词修改

语音唤醒模式和随意对话模式可以支持语音唤醒功能，目前支持“你好涂鸦”、“小智同学”、“hey, tuya” 三种唤醒词。唤醒词不需要额外修改代码，直接使用即可。如果需要自定义唤醒词，可以参考[内置语音唤醒算法](https://developer.tuya.com/cn/docs/developer/wukong-capability-wakeup-internal?id=Keiuxitde0yee)。



### 音频质量调试

支持本地通过串口进行音频调试。默认使用串口 `0`，即烧录口作为命令交互串口。此功能是默认打开的，如果需要关闭可以修改：

```c
// 打开音频分析功能，此功能是默认关闭的，开启的时候需要较多PSRAM内存
#define ENABLE_AUDIO_ANALYSIS 1
```

* 客户端脚本

  * 客户端脚本位于 `./scripts/audio_uart_dump.py`，需要将此脚本从虚拟机中拷贝到电脑中。

  * 脚本依赖于 `python 3.8` 以上版本，并需要安装 `pyserial` 库

    ```c
    pip3 install pyserial
    ```

  * 运行脚本

    ```c
    python ./audio_uart_dump.py
    ```

    

* 使用流程

  * 连接串口（选择烧录串口）

    ```c
    >python audio_uart_dump.py
    
    可用串口列表:
    1. COM3 - USB-SERIAL CH340 (COM3)
    2. COM10 - USB-SERIAL CH340 (COM10)
    
    请选择串口号:2
    ```

  * 发送命令

    ```c
    支持命令:
    start       - 启动录音
    stop        - 停止录音
    reset       - 重置录音
    dump 0      - 转储参考通道到 dump_mic.pcm
    dump 1      - 转储麦克风通道到 dump_ref.pcm
    dump 2      - 转储AEC通道到 dump_aec.pcm
    bg 0        - 5s white
    bg 1        - 5s 1K-0dB (bg 1 1000)
    bg 2        - 4s sweep frequency
    volume 50   - 设置音量为 50%
    quit        - 退出程序
    注意: 已存在文件 dump_mic.pcm，将被覆盖
    注意: 已存在文件 dump_ref.pcm，将被覆盖
    注意: 已存在文件 dump_aec.pcm，将被覆盖
    > reset
    已发送: ao reset
    > start
    已发送: ao start
    > bg 1
    已发送: ao bg 1
    > bg 1 1000
    已发送: ao bg 1 1000
    > bg 1 3000
    已发送: ao bg 1 3000
    > stop
    已发送: ao stop
    ```

  * dump数据

    ```c
    > dump 0
    已发送: ao dump 0
    开始转储通道 0 -> dump_mic.pcm
    等待接收数据...(100ms无数据自动停止)
    接收数据长度: 352192 字节, 录音时间 11.006 s
    已停止转储通道 0, 接收长度: 352192 字节
    数据已保存到: dump_mic.pcm
    dump 1
    已发送: ao dump 1
    开始转储通道 1 -> dump_ref.pcm
    等待接收数据...(100ms无数据自动停止)
    接收数据长度: 357056 字节, 录音时间 11.158 s
    已停止转储通道 1, 接收长度: 357056 字节
    数据已保存到: dump_ref.pcm
    dump 2
    已发送: ao dump 2
    开始转储通道 2 -> dump_aec.pcm
    等待接收数据...(100ms无数据自动停止)
    接收数据长度: 358400 字节, 录音时间 11.200 s
    已停止转储通道 2, 接收长度: 358400 字节
    数据已保存到: dump_aec.pcm
    ```

* 使用 `oeanaudio` 工具分析音频

  

注意：最多保存 `30` 秒音频数据。

调试过程参考[T5语音调试指南](./docs/T5音频调试指南.md)。


### 电池电量管理

示例默认的电池电量管理位于基于`.\src\tuya_ai_battery.c`文件。目前仅支持`18650` `2000ma` 锂电池，如果是其他型号的，则自行根据电池能力曲线修改。



### 低功耗

目前支持两种低功耗模式，默认是关闭低功耗的，如果需要低功耗功能，需要打开：

```c
// 低功耗模式，默认关闭，打开这个宏则支持低功耗：TUYA_CPU_SLEEP-普通Wi-Fi低功耗， TUYA_CPU_DEEP_SLEEP-深度睡眠低功耗
// 进入低功耗之后，通过对话按键可以唤醒设备，退出低功耗状态
#define TY_AI_DEFAULT_LOWP_MODE TUYA_CPU_DEEP_SLEEP
```

* `TUYA_CPU_DEEP_SLEEP`：`50ua`左右，需要重新启动，响应速度较慢
* `TUYA_CPU_SLEEP`：`10ma` 左右，可以快速响应控制（如果无法精确控制程序的关闭相关软件功能，可以先使用深度睡眠来满足功耗要求）。
* 长保活低功耗：还在调整，待发布。



默认处于 `待命` 状态下十分钟即进入低功耗状态，开发者可以按需修改：

```c
#define TOY_DEEPSLEEP_TIMEOUT          (10 * 60 * 1000)      // 10min，可按需修改
```



低功耗的逻辑主要分为两个部分：

* 进入低功耗：在启动完成或者进入 `待命` 状态，则启动`lowpower_timer`，进入低功耗的逻辑位于`lowpower_timer`的回调函数 `ai_toy_lowpower_timer`。

  ```c
  	// lowpower timer handler
  	TIMER_ID                     lowpower_timer;
  ```

* 退出低功耗：当按键触发时，如果设备处于低功耗状态，则唤醒设备/重启设备。

  

如果需要知道低功耗原理，请参考[Wi-Fi低功耗](https://developer.tuya.com/cn/docs/iot-device-dev/TuyaOS-iot_abi_wifi_lowpower?id=Kd73i672lrhuk)。



### 双数字麦

示例在 [`T5AI_BOARD ROBOT`]() 上支持双数字麦，需要配置示例选择 [`T5AI_BOARD ROBOT`]() ，硬件原理图请参考 [T5AI 机器狗原理图](./docs/T5AI-DOG_V1.pdf)。通过双数字麦可以进行指向性的降噪，也可以进行有限的声源定位。

* 开启双数字麦
  
  ```c
    make app_config APP_NAME=tuyaos_demo_ai_toy
  ```

    配置完成之后（选择 [`T5AI_BOARD EVB`](https://developer.tuya.com/cn/docs/iot/T5AI-EVB-DATA-SHEET?id=Keghpxqt6wcal)），必须运行上述命令生成新的 `tuya_app_config.h` 文件，否则配置不会生效。

  
* 指向性降噪

  默认左声道为主麦，右声道为辅麦，主麦拾音，辅麦方向来的声音会被抑制。所以两个数字麦一般是前后布置，喇叭放置于辅麦方向，以提高喇叭的回声消除效果；同时，辅麦方向的杂声、人声也会被抑制，以提高打断的效果。

  ![](https://images.tuyacn.com/fe-static/docs/img/a20ed927-47dc-40fb-81f4-f9f69ccf266e.png)

* 左右麦
  开发中，敬请期待...

* 声源定位

  目前算法只支持两颗麦克风连线方位的声源定位，用途有限。



### 蜂窝网卡支持

示例支持通过 `USB` 外挂蜂窝网卡连接网络，从而免除 `Wi-Fi` 配网的复杂流程，提高使用体验。需要配置示例选择 [`T5AI_BOARD CELLULAR`]() ，硬件原理图请参考 [T5AI 蜂窝开发板](./docs/T5AI-Lte.pdf)。 

## others

如果开发过程遇到问题，可以到 TuyaOS 开发者论坛 [联网单品开发版块](https://www.tuyaos.com/viewforum.php?f=11) 发帖咨询。






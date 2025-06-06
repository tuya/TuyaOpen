<p align="center">
<img src="docs/images/TuyaOpen.png" width="60%" >
</p>

[English](https://github.com/tuya/TuyaOpen/blob/master/README.md) | 简体中文

## 简介
TuyaOpen 是一款跨芯片平台、操作系统的 AI+IoT 开发框架。它基于通用南向接口设计，支持 Bluetooth、Wi-Fi、Ethernet 等通信协议，提供了物联网开发的核心功能，包括配网，激活，控制，升级等；它具备强大的安全合规能力，包括设备认证、数据加密、通信加密等，满足全球各个国家和地区的数据合规需求。

基于 TuyaOpen 开发的 AI+IoT 产品，如果使用 tuya_cloud_service 组件的功能，就可以使用涂鸦APP、云服务提供的强大生态能力，并与 Power By Tuya 设备互联互通。

同时 TuyaOpen 将不断拓展，提供更多云平台接入功能，及语音、视频、人脸识别等功能。

## 开始体验

### 安装依赖
- Ubuntu and Debian

```sh
$ sudo apt-get install lcov cmake-curses-gui build-essential ninja-build wget git python3 python3-pip python3-venv libc6-i386 libsystemd-dev
```

- macOS

请运行 `tos check` 检查系统依赖，并根据提示安装依赖。

> 注：v1.1.0 版本之后，我们采用了 ninja 作为构建工具来加快编译速度，如遇到编译错误请安装 ninja。

### 克隆仓库

```sh
$ git clone https://github.com/tuya/TuyaOpen.git
```

tuyeopen 仓库中包含多个子模块，tos 工具会在编译前检查并自动下载子模块，也可以使用 `git submodule update --init` 命令手工下载。

## 设置与编译

### step1. 设置环境变量
```sh
$ cd TuyaOpen
$ export PATH=$PATH:$PWD
```
或将 TuyaOpen 路径添加到系统环境变量中。

TuyaOpen 通过 tos 命令进行编译、调试等操作，tos 命令会根据环境变量中设置的路径查找 TuyaOpen 仓库，并执行对应操作。

如果希望使用自动补全，请将`tools/completion/tos.sh`使用`source`放入终端环境中，如`.bashrc`或`.zshrc`中。

tos 命令的详细使用方法，请参考 [tos 命令](docs/zh/tos_guide/index.rst)。

### step2. 选择待编译项目

选择待编译 app 或 example，如 [apps/tuya_cloud/switch_demo](https://github.com/tuya/TuyaOpen/tree/master/apps/tuya_cloud/switch_demo) , 并切换至对应目录。

使用 `tos config_choice` 命令选择编译目标平台或目标板。

```sh
$ cd apps/tuya_cloud/switch_demo
$ tos config_choice
[TuyaOpen/apps/tuya_cloud/switch_demo/config] is empty.
Using boards default config file.
========================
Configs
  1. BK7231X.config
  2. ESP32-C3.config
  3. ESP32.config
  4. ESP32-S3.config
  5. LN882H.config
  6. T2.config
  7. T3.config
  8. T5AI.config
  9. Ubuntu.config
------------------------
Please select: 
```

`tos config_choice` 命令读取项目下 `config` 目录中的配置文件，并会生成当前工程的配置文件 `app_default.config`。

> 运行 `tos config_choice` 切换 config 后，tos 命令会自动清除当前工程下已经编译生成的编译中间文件

### step3. 编译
选择当前编译的 examples 或 apps 对应工程，运行如下命令编译：
```sh
$ cd apps/tuya_cloud/switch_demo
$ tos build
```
编译完成后目标文件位于当前编译项目 `.build/<project>/bin` 目录下，如 `apps/tuya_cloud/switch_demo/.build/bin` 目录。
编译后的目标文件包括：
- switch_demo_QIO_1.0.0.bin：包括 boot 在内的完整固件，用于烧录。
- switch_demo_UA_1.0.0.bin：未包括 boot 的应用固件，使用该文件需根据不同的 platform/chip 烧录该 bin 至对应的地址，否则可能无法正常运行。
- switch_demo_UG_1.0.0.bin：用于 OTA 升级的 bin 文件，无法直接烧录后运行。


项目名称默认为目录名称，项目版本默认为 `1.0.0`，可通过 `tos menuconfig` 配置中修改。

### step4. menuconfig 配置 
如需要修改项目的配置，选择需配置的 examples 或 apps 对应工程，在对应工程目录下运行如下命令进行菜单化配置：
```sh
$ cd apps/tuya_cloud/switch_demo
$ tos menuconfig
```

配置当前工程，配置完成后保存退出，编译工程。

> 运行 `tos menuconfig` 切换芯片或开发板后，tos 命令会自动清除当前工程下已经编译生成的编译中间文件

## 烧录
### GUI 工具烧录
tyutool gui 烧录工具已支持 T2/T3/T5AI/BK7231N/LN882H/ESP32 等多种芯片串口烧录，支持 windows/Linux/macOS 等操作系统，请根据运行操作系统选择对应的 GUI 烧录工具。
- windows：[tyutool_win](https://images.tuyacn.com/smart/embed/package/vscode/data/ide_serial/win_tyutool_gui.zip)
- Linux：[tyutool_linux.tar](https://images.tuyacn.com/smart/embed/package/vscode/data/ide_serial/tyutool_gui.tar.gz)
- macOS_x86：[tyutool_mac_x86](https://images.tuyacn.com/smart/embed/package/vscode/data/ide_serial/darwin_x86_tyutool_gui.tar.gz)
- macOS_arm64：[tyutool_mac_arm64.zip](https://images.tuyacn.com/smart/embed/package/vscode/data/ide_serial/darwin_arm64_tyutool_gui.tar.gz)

## 命令行烧录
可通过 tos flash 命令一键烧录

1. 在 Linux 环境下需要先使用如下命令设置串口权限，否则运行会报错。
```sh
$ sudo usermod -aG dialout $USER
```

设置完成后需重启系统方可生效。

2. 在需要编译完成后的项目中运行 `tos flash` 命令一键烧录，`tos flash` 会根据当前运行的环境自动下载对应的 tyutool 工具，并自动烧录。
```sh
$ cd apps/tuya_cloud/switch_demo
$ tos flash
tyutool params:
[INFO]: tyut_logger init done.
[INFO]: Run Tuya Uart Tool.
[INFO]: Use default baudrate: [921600]
[INFO]: Use default start address: [0x00]
--------------------
1. /dev/ttyS0
2. /dev/ttyACM0
3. /dev/ttyACM1
^^^^^^^^^^^^^^^^^^^^
Select serial port: 3                              ## 选择正确的串口
[INFO]: Waiting Reset ...
[INFO]: unprotect flash OK.
[INFO]: sync baudrate 921600 success
Erasing: ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━ 100% 4 bytes/s   0:00:04 / 0:00:00
[INFO]: Erase flash success
Writing: ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━ 100% 16 bytes/s   0:00:18 / 0:00:00
[INFO]: Write flash success
[INFO]: CRC check success
[INFO]: Reboot done
[INFO]: Flash write success.
```

> 注：烧录过程中需要根据芯片实际情况进入 boot 后才可以进行串口烧录。
> 烧录过程中如果串口没有响应，请检查串口是否正确选择，或串口是否被其他程序占用。

3. tos flash 烧录工具正在不断新增支持新的芯片型号，v1.8.0 之前的版本不支持自动升级工具，后续版本在启动时会检测升级并提示升级。
可通过 `tos flash --version` 查询版本情况， 
```sh
$ tyutool params: --version
tyuTool, version 1.8.3
```

v1.8.0 之前版本需要手工运行以下升级命令升级：
```shell
$ tos flash upgrade
```

更多 TuyaOpen 相关文档请参考 [TuyaOpen 开发指南](https://docs.tuyaopen.io/zh)。

## 支持 platform 列表
| 名称 | 支持状态 | 介绍 | 调试日志串口 |
| ---- | ---- | ---- | ---- |
| Ubuntu | 支持 | 可在 ubuntu 等 Linux 主机上直接运行 | |
| T2 |  支持 | 支持模组列表:  [T2-U](https://developer.tuya.com/cn/docs/iot/T2-U-module-datasheet?id=Kce1tncb80ldq) | Uart2/115200 |
| T3 |  支持 | 支持模组列表:  [T3-U](https://developer.tuya.com/cn/docs/iot/T3-U-Module-Datasheet?id=Kdd4pzscwf0il) [T3-U-IPEX](https://developer.tuya.com/cn/docs/iot/T3-U-IPEX-Module-Datasheet?id=Kdn8r7wgc24pt) [T3-2S](https://developer.tuya.com/cn/docs/iot/T3-2S-Module-Datasheet?id=Ke4h1uh9ect1s) [T3-3S](https://developer.tuya.com/cn/docs/iot/T3-3S-Module-Datasheet?id=Kdhkyow9fuplc) [T3-E2](https://developer.tuya.com/cn/docs/iot/T3-E2-Module-Datasheet?id=Kdirs4kx3uotg) 等 | Uart1/460800 |
| T5AI |  支持 | 支持模组列表: [T5-E1](https://developer.tuya.com/cn/docs/iot/T5-E1-Module-Datasheet?id=Kdar6hf0kzmfi) [T5-E1-IPEX](https://developer.tuya.com/cn/docs/iot/T5-E1-IPEX-Module-Datasheet?id=Kdskxvxe835tq) 等 | Uart1/460800 |
| ESP32/ESP32C3/ESP32S3 | 支持 | | Uart0/115200 |
| LN882H | 支持 |  | Uart1/921600 |
| BK7231N | 支持 | 支持模组列表:  [CBU](https://developer.tuya.com/cn/docs/iot/cbu-module-datasheet?id=Ka07pykl5dk4u)  [CB3S](https://developer.tuya.com/cn/docs/iot/cb3s?id=Kai94mec0s076) [CB3L](https://developer.tuya.com/cn/docs/iot/cb3l-module-datasheet?id=Kai51ngmrh3qm) [CB3SE](https://developer.tuya.com/cn/docs/iot/CB3SE-Module-Datasheet?id=Kanoiluul7nl2) [CB2S](https://developer.tuya.com/cn/docs/iot/cb2s-module-datasheet?id=Kafgfsa2aaypq) [CB2L](https://developer.tuya.com/cn/docs/iot/cb2l-module-datasheet?id=Kai2eku1m3pyl) [CB1S](https://developer.tuya.com/cn/docs/iot/cb1s-module-datasheet?id=Kaij1abmwyjq2) [CBLC5](https://developer.tuya.com/cn/docs/iot/cblc5-module-datasheet?id=Ka07iqyusq1wm) [CBLC9](https://developer.tuya.com/cn/docs/iot/cblc9-module-datasheet?id=Ka42cqnj9r0i5) [CB8P](https://developer.tuya.com/cn/docs/iot/cb8p-module-datasheet?id=Kahvig14r1yk9) 等 | Uart2/115200 |


## 示例工程 <span id="example"></span>

TuyaOpen 提供了丰富的示例工程，方便开发者快速上手，了解 TuyaOpen 的使用。
```sh
$ examples
├── ble
│   ├── ble_central
│   └── ble_peripher
├── get-started
│   └── sample_project
├── peripherals
│   ├── adc
│   ├── gpio
│   ├── i2c
│   ├── pwm
│   ├── spi
│   ├── timer
│   └── watchdog
├── protocols
│   ├── http_client
│   ├── https_client
│   ├── mqtt
│   ├── tcp_client
│   └── tcp_server
├── system
│   ├── os_event
│   ├── os_kv
│   ├── os_mutex
│   ├── os_queue
│   ├── os_semaphore
│   ├── os_sw_timer
│   └── os_thread
└── wifi
    ├── ap
    ├── low_power
    ├── scan
    └── sta
```

每个示例工程下对应有 README.md 文件，详细介绍了示例工程的配置、编译、运行等操作。

example 编译方式与 app 编译方式一致，使用 `tos config_choice` 命令选择编译目标平台或目标板，然后使用 `tos build` 编译。

## AI 应用
tuya.ai 是一个综合性的 AI 服务平台,提供以下核心能力:

- 音频处理服务
- 视频处理服务  
- 多模态 AI 服务

详细使用说明请参考 [tuya.ai 文档](apps/tuya.ai/README_zh.md)

## 云连接应用

TuyaOpen 提供了丰富的云连接应用示例，相关应用位于 apps 目录下，可点击 [云连接应用](apps/tuya_cloud/README_zh.md)。

## platform 新增与适配

TuyaOpen 支持新增与适配新的 platform，具体操作请参考 [platform 新增与适配](./docs/zh/new_platform/index.rst)。

## board 新增与适配

TuyaOpen 支持新增与适配新的 board，具体操作请参考 [board 新增与适配](./docs/zh/new_board/index.rst)。

## FAQ
1. TuyaOpen 支持的 platform 通过子仓库动态下载，更新 TuyaOpen 仓库不会主动更新子仓库，如遇到问题无法正常编译，请至 platform 文件夹下对应的目录下使用 `git pull` 命令更新，或删除 platform 文件夹下对应目录后再次下载。

2. TuyaOpen 连提供了丰富的云连接应用示例，如发现无法正常连接或无法正常激活设备，请参考 [云连接应用](apps/tuya_cloud/README_zh.md)。

## License
本项目的分发遵循 Apache License 版本 2.0。有关更多信息，请参见 LICENSE 文件。

## 贡献代码
如果您对 TuyaOpen 感兴趣，并希望参与 TuyaOpen 的开发并成为代码贡献者，请先参阅 [贡献指南](./docs/zh/contribute_guide/index.rst)。

## 免责与责任条款

用户应明确知晓，本项目可能包含由第三方开发的子模块（submodules），这些子模块可能独立于本项目进行更新。鉴于这些子模块的更新频率不受控制，本项目无法确保这些子模块始终为最新版本。因此，用户在使用本项目时，若遇到与子模块相关的问题，建议自行根据需要进行更新或于本项目提交问题（issue）。

若用户决定将本项目用于商业目的，应充分认识到其中可能涉及的功能性和安全性风险。在此情况下，用户应对产品的所有功能性和安全性问题承担全部责任，应进行全面的功能和安全测试，以确保其满足特定的商业需求。本公司不对因用户使用本项目或其子模块而造成的任何直接、间接、特殊、偶然或惩罚性损害承担责任。

## 相关链接
- Arduino 版 TuyaOpen：[https://github.com/tuya/arduino-TuyaOpen](https://github.com/tuya/arduino-TuyaOpen)
- Luanode 版 TuyaOpen：[https://github.com/tuya/luanode-TuyaOpen](https://github.com/tuya/luanode-TuyaOpen)

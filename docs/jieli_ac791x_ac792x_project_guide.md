# TuyaOpen 杰理 AC791x / AC792x 项目资料索引

> 本文汇总项目目标、代码分支、已知适配进展及杰理官方资料入口，供后续移植和外设开发索引使用。官方网页检查日期：2026-09-23。网页内容和芯片能力可能随 SDK 版本更新，开发时应以选定 SDK 分支内的文档、配置和源码为准。

## 1. 项目目标

逐步将 TuyaOpen SDK 适配到杰理 AC791x、AC792x 开发板，支持涂鸦业务能力和板级外设。当前关注范围包括 Wi-Fi、BLE、涂鸦配网、IoT、DP 及外围器件；后续按具体板卡和方案补齐。

## 2. 仓库与协作约定

| 内容 | 仓库 / 分支 | 说明 |
| --- | --- | --- |
| TuyaOpen 主工程 | [tuya/TuyaOpen `xb/jl-ac7916`](https://github.com/tuya/TuyaOpen/tree/xb/jl-ac7916) | AC791x 适配集成分支 |
| JieLi 平台适配 | [tuya/TuyaOpen-JieLi `master`](https://github.com/tuya/TuyaOpen-JieLi/tree/master) | 平台适配仓库及后续 AC792x 扩展 |
| AC791x 芯片 SDK | [fw-AC79_AIoT_SDK](https://gitee.com/Jieli-Tech/fw-AC79_AIoT_SDK) | 杰理官方 SDK；SDK 的 `doc/` 中包含硬件资料/原理图入口 |
| AC792x 芯片 SDK | [fw-AC792_SDK](https://gitee.com/Jieli-Tech/fw-AC792_SDK) | 杰理官方 SDK；SDK 的 `doc/` 中包含硬件资料/原理图入口 |

- SDK 放在 TuyaOpen 的 `platform/JIELI/chip/` 下，并通过 submodule 管理；添加或更新 SDK 时记录仓库 URL、分支/版本及对应提交。
- 所有提交通过 Pull Request 流程，不直接提交或 push 到共享目标分支。
- 当前工程面向 Windows 开发和烧录；通过 TuyaOpen 的 `tos.py` 工具统一配置、编译、烧录及串口监视流程。

## 3. 已知适配进展

### 项目提供的进展

根据项目任务资料，`xb/jl-ac7916` 分支已实现 AC791x 编译、烧录、Wi-Fi、BLE、涂鸦配网、IoT 和 DP 相关功能。

### 当前工作区可见状态

当前 TuyaOpen 工作区分支为 `xb/jl-ac7916`。本地 [JIELI 平台说明](../platform/JIELI/README_zh.md) 描述了 AC7916A/WL82 的 `jieli_uart_hello` 里程碑；AC79 SDK 子模块配置位于 `platform/JIELI/.gitmodules`，相对路径为 `chip/wl82/AC79_AIoT_SDK`，跟踪 `release/AC79NN_SDK_V1.2.0`。这份本地说明与上面项目提供的功能进展范围不同，应分别理解；后续可随平台集成情况更新该说明。

当前工作区的 JieLi 平台仓库已声明 WL82/AC79 与 WL83/AC792 SDK 的 submodule 路径。AC792 SDK 已初始化到本地，当前检出 `release/AC792N_SDK_V3`，提交为 `5abd533ffe108c35e3232d581f064b58f1983341`；平台仓库变更需要同时纳入 `chip/wl83/AC792_SDK` 的 submodule gitlink。

本工作区于 2026-09-23 在 Windows 下验证了 `apps/tuya_cloud/switch_demo`：AC79_DevKitBoard 使用完整 TuyaOpen 组件配置完成 `tos.py build`；AC792N_Develop_Board 使用 `CONFIG_JIELI_MINIMAL_HELLO=y` 完成 UART bring-up 镜像构建。AC792 bring-up 镜像启动后会输出板名及周期 UART 心跳。构建不代表已在实板烧录或实测日志。

## 4. 杰理官方文档可访问性检查

已检查两套官网文档首页及板级、开发环境、编译下载、外设、Wi-Fi 和蓝牙等下级目录。**两套文档的下级目录及正文均可通过官网访问**，可直接作为开发索引；本检查确认的是网页访问，不代表 SDK 子模块已经下载了全部附件。

### AC791x / AC79

官方文档版本路径为 `release_v1.2.0`：

| 主题 | 文档入口 |
| --- | --- |
| 文档首页与完整目录 | [JieLi_AC791 文档首页](https://doc.zh-jieli.com/AC79/zh-cn/release_v1.2.0/index.html) |
| 开发板概述与资源 | [开发板概述](https://doc.zh-jieli.com/AC79/zh-cn/release_v1.2.0/board_description/board_overview/index.html) |
| 原理图与位号图入口 | [开发板相关文档](https://doc.zh-jieli.com/AC79/zh-cn/release_v1.2.0/board_description/docs/index.html) |
| 环境安装 | [开发环境安装说明](https://doc.zh-jieli.com/AC79/zh-cn/release_v1.2.0/getting_started/environmental_install/index.html) |
| 编译与下载 | [SDK 工程编译与下载](https://doc.zh-jieli.com/AC79/zh-cn/release_v1.2.0/getting_started/project_download/index.html) |
| 外设 | [外设例程目录](https://doc.zh-jieli.com/AC79/zh-cn/release_v1.2.0/module_example/peripherals/index.html) |
| Wi-Fi | [Wi-Fi 例程目录](https://doc.zh-jieli.com/AC79/zh-cn/release_v1.2.0/module_example/wifi/index.html) |
| 蓝牙 / BLE | [蓝牙例程目录](https://doc.zh-jieli.com/AC79/zh-cn/release_v1.2.0/module_example/bluetooth/index.html) |

AC791 官方开发板概述以 JL_AC79_DevKit V1.0 / AC7916 为例，列出双核 DSP、最高 320 MHz、578 KB 片上 SRAM、Wi-Fi 802.11 b/g/n、双模蓝牙及 UART、IIC、SPI、PWM、ADC、DAC、摄像头和显示等板级资源。具体板卡 IO 映射和配置请继续查阅官网“开发板资源”“核心板 IO 功能图表”等页面及 SDK 硬件资料。

### AC792x / AC792N

官方文档当前入口位于 `wifi_video_master` 文档树：

| 主题 | 文档入口 |
| --- | --- |
| 文档首页与完整目录 | [JieLi_AC792 文档首页](https://doc.zh-jieli.com/AC792/zh-cn/wifi_video_master/index.html) |
| 开发板概述与资源 | [开发板概述](https://doc.zh-jieli.com/AC792/zh-cn/wifi_video_master/board_description/board_overview/index.html) |
| 硬件资料目录 | [开发板硬件资料相关文档](https://doc.zh-jieli.com/AC792/zh-cn/wifi_video_master/board_description/docs/index.html) |
| 环境安装 | [开发环境安装说明](https://doc.zh-jieli.com/AC792/zh-cn/wifi_video_master/getting_started/environmental_install/index.html) |
| 编译与下载 | [SDK 工程编译与下载](https://doc.zh-jieli.com/AC792/zh-cn/wifi_video_master/getting_started/project_download/index.html) |
| 外设 | [外设例程目录](https://doc.zh-jieli.com/AC792/zh-cn/wifi_video_master/module_example/peripherals/index.html) |
| Wi-Fi | [Wi-Fi 例程目录](https://doc.zh-jieli.com/AC792/zh-cn/wifi_video_master/module_example/wifi/index.html) |
| 蓝牙 / BLE | [蓝牙例程目录](https://doc.zh-jieli.com/AC792/zh-cn/wifi_video_master/module_example/bluetooth/index.html) |

AC792 官方开发板概述以 AC7926A / AC792N 开发板为例，列出最高 320 MHz 双核 DSP、256 KB 片上 SRAM、封装支持 8/16 MB DDR1、Wi-Fi 与双模蓝牙、显示/摄像头/音视频等资源。开发板资源页与芯片产品规格需区分；以实际板卡、SDK 版本和原理图为准。硬件资料页目前提供指向 `fw-AC792_SDK` 仓库 `doc/` 中开发板原理图/位号图资料的链接。

### 官网目录索引说明

两套首页均按“开发板介绍、快速入门、工程模板、模块例程、FAQ/版本”等类别组织资料。常用入口如下：

- 硬件适配：开发板资源、系统/功能框图、核心板 IO 表、扩展指南、SDK `doc/` 硬件资料。
- 构建调试：开发准备、环境安装、SDK 工程说明、编译下载、调试方式。
- 无线功能：Wi-Fi API、AP/STA/MONITOR、扫描、配网、功耗；蓝牙接口、BLE 应用、BLE 配网及常见问题。
- 外设驱动：按外设目录查找 ADC、GPIO、UART、I2C、SPI、PWM、存储、USB 等对应例程；AC792 外设索引还列出 UART_LOG、CAN 等项目。

## 5. 当前开发板命名与硬件参数索引

当前主开发板统一使用以下名称；代码、配置、构建记录和日志记录都按此名称填写：

| 统一名称 | 芯片/旧名称 | 当前用途 | 配置状态 |
| --- | --- | --- | --- |
| `AC79_DevKitBoard` | AC791 / AC7916A / WL82 | 当前 AC791 主开发与调试板 | 现有 AC791 构建适配；沿用 `AC7916A` 的旧配置符号作为兼容别名 |
| `AC792N_Develop_Board` | AC792N / WL83 | 当前 AC792 主开发目标 | 完整 `switch_demo` 镜像已完成 Windows 软件构建和链接；实板烧录、Wi-Fi/BLE 配网和 IoT/DP 验证待做 |

AC792 完整 Tuya `switch_demo` 的软件构建命令（在示例目录执行）：

```powershell
tos.py config set CONFIG_BOARD_CHOICE_AC792N_DEVELOP_BOARD=y CONFIG_JIELI_MINIMAL_HELLO=n CONFIG_JIELI_UART_LOG_PORT=0 CONFIG_JIELI_UART_LOG_BAUDRATE=1000000
tos.py build
```

2026-09-23 已确认该命令生成并链接 `switch_demo_QIO_1.0.0.bin`。本次主机未连接 AC792 开发板，因此烧录、UART 实际日志、Wi-Fi 连接、BLE 配网、Tuya 云端激活及 DP 上报/下发均没有硬件验证证据。示例中的 Tuya PID/授权信息来自本机被忽略的 secrets 头文件；该凭据文件不应提交。

`AC7916A` 是曾在 `D:\tuya_proj\jieli\ipc_ac7916a` 项目中使用的板卡/配置名，对应当前的 `AC79_DevKitBoard`。保留它用于历史索引和旧配置兼容；当前调试记录统一记为 `AC79_DevKitBoard`。不要把历史项目的 UART 配置套用到当前板卡。

### 串口、Flash 与 RAM

下表分开记录官方芯片/示例资料与实板确认值。示例输出只说明对应 SDK 示例配置，不代表手上板卡的物料配置。

| 板卡 | 日志串口（官方资料/代码参考） | Flash | RAM / 外部内存 |
| --- | --- | --- | --- |
| `AC79_DevKitBoard` | 按官方 `demo_DevKitBoard`：UART1、TX=PB3、RX 未使用、1,000,000 baud。2026-09-23 已从实板抓到完整启动日志；SerialDebug 当前占用日志串口，实际 COM/PB3 连接仍按抓取软件配置确认。旧 `ipc_ac7916a` 工程使用 UART2/PB6/115200，仅作为历史记录。 | **实测**：Flash ID `5E4017`、8 MiB；2026-09-23 通过官方 SDK `isd_download.exe` USB 下载成功。 | 芯片资料：片上 SRAM 578 KB。实板启动日志报告 `RAM_SIZE=523596` 字节、`SDRAM_SIZE=2097152` 字节（当前固件报告的 2 MiB）。官方 `demo_DevKitBoard` 示例报告 SDRAM 8 MiB；需结合板卡版本及 SDRAM 配置核实差异。 |
| `AC792N_Develop_Board` | WL83 SDK `demo_hello/board/wl83/board_demo.h`：UART0 日志 TX=PD1，默认 1,000,000 baud；接收脚为 PE11。适配镜像默认沿用 UART0/1 Mbps。SDK 示例配置，不等于已测实板连线。 | AC7926A SDK 开发板配置：Flash 8 MiB；以手上板卡丝印、原理图和 Flash ID 最终确认。 | 芯片片上 SRAM 256 KB；AC7926A SDK profile 配置 DDR1 16 MiB。封装支持 8/16 MiB DDR1，实际装配以板卡 BOM/原理图为准。 |

#### 历史项目：`ipc_ac7916a`

- 本地路径：`D:\tuya_proj\jieli\ipc_ac7916a`。该目录曾用于 AC7916A 开发，板卡对应现在统一命名的 `AC79_DevKitBoard`；保留作为历史工程参考，不作为当前调试工程。
- 工程包含 `AC79NN_SDK`、`toolchain`、`tuya_common`、`tuya_os_adapter`，README 描述为杰理 AC79NN 系列；SDK 中可见 `apps/tuya_lock` 业务代码及 AC79 开发板原理图资料。
- 历史工程 `AC79NN_SDK/apps/tuya_lock/board/wl82/board.c` 的日志口配置为 UART2、115200 baud、TX=PB6、RX=-1（未使用）；业务串口为 UART1、9600 baud、TX=PB3、RX=PB4。这是该工程的软件配置记录，不足以证明当前开发板跳线或硬件连线。当前 AC79 固件日志按官方 `demo_DevKitBoard` 改为 UART1/PB3/1 Mbps。

#### 参数确认记录模板

每次实板验证按统一名称单独记录：板卡丝印/硬件版本、芯片料号、SDK 分支与提交、Flash ID/容量、片上 RAM 与外部 RAM 启动打印、日志接口与 TX/RX 引脚、UART 号/波特率/数据位/校验/停止位、COM 号、抓取工具、启动日志片段、验证日期。确认前写“待实板验证”，不要用 SDK 默认配置替代。

## 6. 构建、烧录与日志记录

- TuyaOpen 项目统一使用 `tos.py` 入口完成配置和构建；烧录目前以 Windows 为目标环境。命令及所需工具以目标分支的 `platform/JIELI/README_zh.md` 和相应平台脚本为准。
- 逐块板记录实际开发板型号/版本、芯片丝印、固件/SDK 提交、构建命令、烧录器与烧录方式、串口设备号和参数。
- `tos.py monitor -p COMx` 会读取当前目标板 Kconfig 的 `CONFIG_JIELI_UART_LOG_BAUDRATE` 作为默认波特率；AC79 和 AC792 默认均为 1,000,000。AC79 日志配置为 UART1/TX=PB3，AC792 为 UART0/TX=PD1。AC792 USB 烧录模式根据 SDK 文档使用板上 `UPDATE` 键并重新上电，设备应枚举为 `WL83 UBOOT1.00 USB Device`。
- **AC791 实测记录（2026-09-23）**：构建 `apps/tuya_cloud/switch_demo` 成功；按官方 WL82 USB 下载模式识别到 `WL82 UBOOT1.00 USB Device`，`isd_download.exe` 报告 Chip Version B、Flash ID `5E4017`、8 MiB，固件下载并重启成功。启动日志由 SerialDebug 保存到 `apps/tuya_cloud/switch_demo/src/monitor.log`，确认 `AC79_DevKitBoard`/`wl82` 固件正常启动、KV init result 为 0；启动打印 RAM_SIZE 523596 字节、SDRAM_SIZE 2097152 字节。日志中 `wifi get mac pending...` 持续约 90 秒，未见 Wi-Fi STA connect、DHCP、Tuya cloud connect 或 DP 上报记录。此 switch_demo 配置在 Tuya BLE 配网凭据到达后才启动 Wi-Fi；该日志没有证明已完成 BLE 配网。授权部分显示 `tuyaopen_license_read` 失败、使用编译 fallback UUID/AuthKey，并警告需替换 demo 授权内容；`activate config not found:-6`，因此本轮未验证 Tuya 云激活及 DP。日志串口被 SerialDebug 占用，独立 monitor 无法同时打开。
- **AC792** 尚无实板烧录或串口抓取证据。分别补录板卡版本、COM 号、实际波特率、TX/RX/地接线、启动日志片段与验证日期。

## 7. 后续开发索引建议

1. 先固定目标板型号、硬件版本及芯片料号，并对照官方 IO 表与 SDK `doc/` 原理图确认管脚和复用。
2. 固定 SDK 仓库、分支/版本及 submodule commit；不要仅凭系列能力表假定当前 SDK 已支持某功能。
3. 在 Windows 下用 `tos.py` 复现配置、编译和烧录；单独记录 SDK 原生工程工具要求与 TuyaOpen 封装入口的差异。
4. 先获取并保存可重复的启动日志，再按 Wi-Fi、BLE/配网、IoT/DP、外设逐项验证，保留命令、配置、板卡条件及日志证据。
5. 代码变更提交到对应分支并通过 Pull Request 集成；SDK submodule 更新同时记录上游版本和变更来源。

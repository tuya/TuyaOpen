# TuyaOpen 杰理 AC791x / AC792x 项目资料索引

> 本文汇总项目目标、代码分支、已知适配进展及杰理官方资料入口，供后续移植和外设开发索引使用。官方网页检查日期：2026-09-24。网页内容和芯片能力可能随 SDK 版本更新，开发时应以选定 SDK 版本内的文档、配置和源码为准。

## 1. 项目目标

逐步将 TuyaOpen SDK 适配到杰理 AC791x、AC792x 开发板，支持涂鸦业务能力和板级外设。当前关注范围包括 Wi-Fi、BLE、涂鸦配网、IoT、DP 及外围器件；后续按具体板卡和方案补齐。

## 2. 仓库与协作约定

| 内容 | 仓库 / 分支 | 说明 |
| --- | --- | --- |
| TuyaOpen 主工程 | [tuya/TuyaOpen](https://github.com/tuya/TuyaOpen) | 当前主仓分支为 `xb/tuyaopen-jl`；2026-09-29 已通过 `git ls-remote origin refs/heads/xb/tuyaopen-jl` 核对远端 ref，提交为 `c05e4d142ac9007fac76037e31c9cf53f7743449` |
| JieLi 平台适配 | [tuya/TuyaOpen-JieLi `master`](https://github.com/tuya/TuyaOpen-JieLi/tree/master) | `platform/JIELI/` 是主仓中的嵌套 Git 工作树（非 submodule）；本地分支 `pr7-for-test` 跟踪 `origin/codex/jieli-adapter-architecture-refactor`。平台上游仓库及目标分支见平台仓库记录 |
| AC791x 芯片 SDK | [fw-AC79_AIoT_SDK](https://gitee.com/Jieli-Tech/fw-AC79_AIoT_SDK) | 本地源码标识 `release/AC79NN_SDK_V1.2.0` / `AC79NN_SDK_V1.2.13_2026-04-20`；SDK 的 `doc/` 中含硬件资料/原理图 |
| AC792x 芯片 SDK | [fw-AC792_SDK](https://gitee.com/Jieli-Tech/fw-AC792_SDK) | 本地源码标识 `release/AC792N_SDK_V3` / `AC792N_SDK_BETA_V3.1.7_2026-08-25`；来源提交见 `platform/JIELI/chip/wl83/README.md`，SDK 的 `doc/` 中含硬件资料/原理图 |

- AC79、AC792 SDK 源码直接纳入 `TuyaOpen-JieLi` 仓库的 `chip/` 目录，不使用 submodule。添加或更新 SDK 时记录上游仓库 URL、分支/版本及对应提交。
- 所有提交通过 Pull Request 流程，不直接提交或 push 到共享目标分支。
- 当前工程面向 Windows 开发和烧录；通过 TuyaOpen 的 `tos.py` 工具统一配置、编译、烧录及串口监视流程。

## 3. 已知适配进展

### 项目提供的进展

根据项目任务资料，`xb/jl-ac7916` 分支已实现 AC791x 编译、烧录、Wi-Fi、BLE、涂鸦配网、IoT 和 DP 相关功能。

### 当前工作区可见状态

当前主仓工作区分支为 `xb/tuyaopen-jl`，已核对同名远端 ref；`platform/JIELI/` 是嵌套 Git 工作树，当前分支为 `pr7-for-test`，跟踪 `origin/codex/jieli-adapter-architecture-refactor`。该目录不是主仓 submodule。AC79/AC792 SDK 源码位于平台工作树的 `chip/` 下；上游 SDK 仓库、分支和版本见本表及 [JieLi 平台说明](https://github.com/tuya/TuyaOpen-JieLi/blob/d770248e2b2b63b3561bb7791f43d255858a8fb1/README_zh.md)。

截至 2026-09-24，Windows 下 AC792N_Develop_Board 的完整 `switch_demo` 已构建并通过 `tos.py flash` 经 USB 下载，下载器识别 Flash ID `5E4017`、容量 8 MiB，写入完成并触发重启。AC792 改为 UART0/115200 后的固件尚缺本次冷启动串口日志。AC79_DevKitBoard 的历史烧录/日志记录见下方；AC79 原始日志文件当前不在工作区可定位路径中。当前执行环境没有可用实板，也没有可用于 Tuya 激活的凭据，因此本指南列出的待验收项目均未在本轮执行。

## 4. 杰理官方文档可访问性检查

已检查两套官网文档首页及板级、开发环境、编译下载、外设、Wi-Fi 和蓝牙等下级目录。**两套文档的下级目录及正文均可通过官网访问**，可直接作为开发索引；本检查确认的是网页访问，不代表 SDK 子模块已经下载了全部附件。

### AC791x / AC79

官方文档版本路径为 `release_v1.2.0`。本次检查确认首页及下级硬件、开发环境、编译下载、外设、Wi-Fi、蓝牙目录可访问；容量相关的直达资料也已单独链接在下方：

| 主题 | 文档入口 |
| --- | --- |
| 文档首页与完整目录 | [JieLi_AC791 文档首页](https://doc.zh-jieli.com/AC79/zh-cn/release_v1.2.0/index.html) |
| 开发板概述与资源 | [开发板概述](https://doc.zh-jieli.com/AC79/zh-cn/release_v1.2.0/board_description/board_overview/index.html) |
| 核心板引脚与 UART 复用 | [核心板 IO 功能图表](https://doc.zh-jieli.com/AC79/zh-cn/release_v1.2.0/board_description/io_table/index.html)；[UART 外设说明](https://doc.zh-jieli.com/AC79/zh-cn/release_v1.2.0/module_example/peripherals/uart.html) |
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
| `AC792N_Develop_Board` | AC792N / WL83 | 当前 AC792 主开发目标 | 完整 `switch_demo` 已构建并 USB 烧录；旧日志记录过联网、激活和 DP 通信。UART0/115200 最新镜像仍待补抓冷启动日志并重新验收 |

AC792 完整 Tuya `switch_demo` 的软件构建命令（在示例目录执行）：

```powershell
tos.py config set CONFIG_BOARD_CHOICE_AC792N_DEVELOP_BOARD=y CONFIG_JIELI_UART_LOG_PORT=0 CONFIG_JIELI_UART_LOG_BAUDRATE=115200
tos.py build
```

2026-09-24 已确认该命令生成并链接 `switch_demo_QIO_1.0.0.bin`，并通过 `tos.py flash` USB 烧录成功。旧 `monitor.log` 记录过 AC792 实板 Wi-Fi、Tuya 激活及 DP 收发；这些记录不能替代 UART0/115200 最新固件的冷启动日志和复测。示例中的 Tuya PID/授权信息来自本机被忽略的 secrets 头文件；该凭据文件不应提交。当前环境没有可用实板或激活凭据，以下待验收项尚未完成。

`AC7916A` 是曾在 `D:\tuya_proj\jieli\ipc_ac7916a` 项目中使用的板卡/配置名，对应当前的 `AC79_DevKitBoard`。保留它用于历史索引和旧配置兼容；当前调试记录统一记为 `AC79_DevKitBoard`。不要把历史项目的 UART 配置套用到当前板卡。

### 串口、Flash 与 RAM

下表分开记录官方板卡规格、实板 Flash 识别结果与固件内存配置。需要注意，SDK 启动日志中的 `SDRAM_SIZE`/`DDR_SIZE` 来自链接配置，并非对外部 RAM 容量的自动探测；RAM 物理容量应以板卡/芯片封装资料或完整地址范围读写测试确认。

| 板卡 | 日志串口（官方资料/代码参考） | Flash | RAM / 外部内存 |
| --- | --- | --- | --- |
| `AC79_DevKitBoard` | 日志：硬件 UART1，TX=PB3，115200 baud；TAL CLI：硬件 UART0，TX=PA5、RX=PA6，115200 baud。官方核心板 IO 表列出 PA5/PA6 与 PB3 引脚复用。旧 `ipc_ac7916a` 工程 UART2/PB6 配置仅作历史记录。 | **历史实测**：Flash ID `5E4017`、8 MiB；2026-09-23 通过官方 SDK `isd_download.exe` USB 下载成功。官方核心板资料也标注 8 MiB Flash。 | 官方标准 JL_AC79_WIFI V1.0 核心板标注 8 MiB SDRAM。AC79 SDK 原始 `demo_hello/app_config.h` 为 `__FLASH_SIZE__=4 MiB`、`__SDRAM_SIZE__=2 MiB`；Tuya 构建在 staging 副本中覆盖为 8 MiB / 8 MiB，不改 SDK 源文件。启动 `SDRAM_SIZE` 是链接配置，不是硬件探测。实板 SDRAM 芯片完整丝印和全容量读写结果待验收。 |
| `AC792N_Develop_Board` | TuyaOpen WL83 staging 使用 UART0 共用日志/CLI，TX=PD1、RX=PE11，115200 baud；上游 `board_demo.h` 默认 1 Mbps，构建按 Kconfig 覆盖。 | **历史实测**：Flash ID `5E4017`、容量 `8192K`（8 MiB），见 `apps/tuya_cloud/switch_demo/monitor.log`。 | 官方 AC792N 开发板资料说明封装支持 8/16 MiB DDR1。SDK 原始 `chip_cfg.h` 为 1 MiB Flash / 2 MiB SDRAM 默认值；staging 配置为 8 MiB Flash / 16 MiB DDR1。日志 `DDR_SIZE=16777216` 是配置值，不是容量探测。实板 DDR 封装容量与完整地址范围读写结果待验收。AC7926A Datasheet V1.5 与官网板卡概述对片上 SRAM 的规格表述不一致，须按实板完整芯片料号核定。 |

容量依据与直达来源：

- AC79：[官方功能框图](https://doc.zh-jieli.com/AC79/zh-cn/release_v1.2.0/board_description/function_diagram/index.html)说明 JL_AC79_WIFI V1.0 核心板为 8 MiB SDRAM + 8 MiB Flash；[SDRAM 配置说明](https://doc.zh-jieli.com/AC79/zh-cn/release_v1.2.0/module_example/system/sdram_cfg.html)要求按实际内存设置 `__SDRAM_SIZE__`。SDK 原始配置见[本地 app_config.h](https://github.com/tuya/TuyaOpen-JieLi/blob/d770248e2b2b63b3561bb7791f43d255858a8fb1/chip/wl82/AC79_AIoT_SDK/apps/demo/demo_hello/include/app_config.h)。`platform/JIELI/jieli_build.py` 是兼容 shim；板级内存配置与 staging 树实现位于 [`board_config.py`](https://github.com/tuya/TuyaOpen-JieLi/blob/d770248e2b2b63b3561bb7791f43d255858a8fb1/tools/jieli_build/board_config.py) 和 [`sdk_overlay.py`](https://github.com/tuya/TuyaOpen-JieLi/blob/d770248e2b2b63b3561bb7791f43d255858a8fb1/tools/jieli_build/sdk_overlay.py)。
- AC792：[官方 AC792N 开发板概述](https://doc.zh-jieli.com/AC792/zh-cn/wifi_video_master/board_description/board_overview/index.html)说明封装支持 8/16 MiB DDR1；原始 SDK 配置见[chip_cfg.h](https://github.com/tuya/TuyaOpen-JieLi/blob/d770248e2b2b63b3561bb7791f43d255858a8fb1/chip/wl83/AC792_SDK/sdk/apps/demo/demo_hello/board/wl83/chip_cfg.h)。`platform/JIELI/jieli_build.py` 是兼容 shim；staging 树实现位于 [`sdk_overlay.py`](https://github.com/tuya/TuyaOpen-JieLi/blob/d770248e2b2b63b3561bb7791f43d255858a8fb1/tools/jieli_build/sdk_overlay.py)，板级内存覆写见 [`board_config.py`](https://github.com/tuya/TuyaOpen-JieLi/blob/d770248e2b2b63b3561bb7791f43d255858a8fb1/tools/jieli_build/board_config.py)。
- Flash ID/容量是下载器或板上驱动的实测识别值；`SDRAM_SIZE`/`DDR_SIZE` 是固件链接配置值，两者不可混为一谈。

#### 历史项目：`ipc_ac7916a`

- 本地路径：`D:\tuya_proj\jieli\ipc_ac7916a`。该目录曾用于 AC7916A 开发，板卡对应现在统一命名的 `AC79_DevKitBoard`；保留作为历史工程参考，不作为当前调试工程。
- 工程包含 `AC79NN_SDK`、`toolchain`、`tuya_common`、`tuya_os_adapter`，README 描述为杰理 AC79NN 系列；SDK 中可见 `apps/tuya_lock` 业务代码及 AC79 开发板原理图资料。
- 历史工程 `AC79NN_SDK/apps/tuya_lock/board/wl82/board.c` 的日志口配置为 UART2、115200 baud、TX=PB6、RX=-1（未使用）；业务串口为 UART1、9600 baud、TX=PB3、RX=PB4。这是该工程的软件配置记录，不足以证明当前开发板跳线或硬件连线。当前 AC79 固件使用 UART1/PB3/115200 输出日志，UART0/PA5-PA6/115200 承载 TAL CLI。

#### 参数确认记录模板

每次实板验证按统一名称单独记录：板卡丝印/硬件版本、芯片完整料号/封装后缀、SDK 分支与提交、Flash ID/容量、片上 SRAM 规格、外部 RAM 芯片/封装规格、链接配置的 `SDRAM_SIZE`/`DDR_SIZE`、RAM 地址范围读写测试结果、日志接口与 TX/RX 引脚、UART 号/波特率/数据位/校验/停止位、COM 号、抓取工具、启动日志片段、验证日期。确认前写“待实板验证”，不要用 SDK 默认配置或链接日志替代物理容量确认。

### 待执行的实板验收与证据归档

截至 2026-09-29，当前工作环境没有可用的 AC791/AC792 实板，也没有 Tuya 激活凭据；本清单尚未执行，所有项目保持“待实板验证”。完成后按板卡和日期保存原始证据，例如 `docs/evidence/jieli/<board>/<YYYYMMDD>/`，并在目录中附 `README.md` 记录硬件版本、芯片完整料号、固件提交、配置、测试步骤、结果和工具版本。保留原始日志；截图仅作补充。日志中的 Wi-Fi 密码、AuthKey、token 等敏感值必须脱敏，凭据本身不归档、不提交。

| 验收项 | 可执行步骤与通过条件 | 应保存的证据 | 当前状态 |
| --- | --- | --- | --- |
| AC792 UART0/115200 启动日志 | 按 AC792 配置构建并烧录当前固件；配置 UART0、115200 baud、8N1，冷启动后完整抓取日志。确认串口可读、板卡/芯片识别正确、系统和 KV 初始化完成、无启动循环；日志需来自 UART0/115200 最新镜像。 | 完整原始串口 `.log`、抓取工具/COM 参数、板卡与芯片照片、构建提交和配置快照；记录冷启动时间及复位方式。 | 待实板验证；旧日志和烧录成功记录不能替代此项。 |
| AC791 联网、Tuya 激活与 DP | 使用 `AC79_DevKitBoard` 和有效但不归档的 PID/AuthKey；完成 BLE 配网后观察 Wi-Fi STA 关联、DHCP、云连接和激活成功，再执行至少一次 DP 查询/上报并由设备日志及手机端/云端状态交叉确认。 | 完整 UART1/115200 原始日志；BLE 配网、STA/DHCP、云连接/激活、DP 操作的时间线及脱敏截图/日志；不保存秘密凭据。 | 待实板和有效凭据；2026-09-23 历史日志未证明 BLE 配网、激活或 DP 成功。 |
| 两板外部 RAM 实际容量 | 分别拍摄 SDRAM/DDR 芯片完整丝印，按确切器件料号查对应数据手册；运行覆盖完整声明地址范围的读写/边界测试，记录起止地址、测试长度、算法、迭代次数和错误数。通过条件是器件资料与全范围测试相符；日志 `SDRAM_SIZE`/`DDR_SIZE` 只能作为固件配置记录。 | 芯片丝印照片、板卡/原理图位置、对应数据手册版本、测试固件提交和配置、完整测试输出及实际可用容量计算。 | AC791 SDRAM、AC792 DDR 容量均待确认；8 MiB/16 MiB staging 设置不构成实测证据。 |
| AC792 片上 SRAM 规格 | 先读取开发板上完整芯片料号及封装后缀，再对照该料号的正式 datasheet/官方板卡资料；解释 AC7926A Datasheet V1.5 与官网板卡概述中 SRAM 数值差异后，才记录最终规格。 | 芯片丝印照片、精确料号、匹配的数据手册版本和页码、官方板卡资料链接，以及差异判定说明。 | 待确认；当前资料分别出现 352 KB 与 256 KB，不据此预选其一。 |

## 6. 构建、烧录与日志记录

- TuyaOpen 项目统一使用 `tos.py` 入口完成配置和构建；烧录目前以 Windows 为目标环境。命令及所需工具以目标分支的 `platform/JIELI/README_zh.md` 和相应平台脚本为准。
- 逐块板记录实际开发板型号/版本、芯片丝印、固件/SDK 提交、构建命令、烧录器与烧录方式、串口设备号和参数。
- `tos.py monitor -p COMx` 会读取当前目标板 Kconfig 的 `CONFIG_JIELI_UART_LOG_BAUDRATE` 作为默认波特率；AC79 与 AC792 均为 115,200。AC79 日志为 UART1/TX=PB3，TAL CLI 为 UART0/TX=PA5、RX=PA6；AC792 使用 UART0 日志/CLI（TX=PD1、RX=PE11）。AC792 USB 烧录模式可按 SDK 文档使用板上 `UPDATE` 键并重新上电；历史记录中的 USB 烧录成功不代表 UART0/115200 启动日志验收已完成。
- **AC791 实测记录（2026-09-23）**：构建 `apps/tuya_cloud/switch_demo` 成功；按官方 WL82 USB 下载模式识别到 `WL82 UBOOT1.00 USB Device`，`isd_download.exe` 报告 Chip Version B、Flash ID `5E4017`、8 MiB，固件下载并重启成功。SerialDebug 当时抓取的启动日志确认 `AC79_DevKitBoard`/`wl82` 固件正常启动、KV init result 为 0；启动打印 RAM_SIZE 523596 字节、SDRAM_SIZE 2097152 字节。该日志中 `wifi get mac pending...` 持续约 90 秒，未见 Wi-Fi STA connect、DHCP、Tuya cloud connect 或 DP 上报记录。此 switch_demo 配置在 Tuya BLE 配网凭据到达后才启动 Wi-Fi；该日志没有证明已完成 BLE 配网。授权部分显示 `tuyaopen_license_read` 失败、使用编译 fallback UUID/AuthKey，并警告需替换 demo 授权内容；`activate config not found:-6`，因此本轮未验证 Tuya 云激活及 DP。原日志曾由 SerialDebug 保存至 `apps/tuya_cloud/switch_demo/src/monitor.log`，但该文件当前不在工作区。
- **AC792 实板记录（2026-09-24）**：`apps/tuya_cloud/switch_demo/monitor.log` 的启动日志识别 Flash ID `5E4017`、容量 `8192K`，并打印 `DDR_SIZE=16777216`；该值是当前 staging 链接配置。当天完整 `switch_demo` 重新构建、USB 烧录成功并触发重启。DDR 实物容量、烧录后 UART 运行日志及完整 Wi-Fi/BLE/云端验证记录仍需继续补录。

## 7. 后续开发索引建议

1. 先固定目标板型号、硬件版本及芯片料号，并对照官方 IO 表与 SDK `doc/` 原理图确认管脚和复用。
2. 固定 SDK 上游仓库、分支/版本及已纳入平台仓库的源码版本；不要仅凭系列能力表假定当前 SDK 已支持某功能。
3. 在 Windows 下用 `tos.py` 复现配置、编译和烧录；单独记录 SDK 原生工程工具要求与 TuyaOpen 封装入口的差异。
4. 先获取并保存可重复的启动日志，再按 Wi-Fi、BLE/配网、IoT/DP、外设逐项验证，保留命令、配置、板卡条件及日志证据。
5. 代码变更提交到对应分支并通过 Pull Request 集成；更新 SDK 源码时同时记录上游版本和变更来源。

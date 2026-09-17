# 杰理 AC7916A / WL82 TuyaOpen switch_demo 交接文档

更新时间：2026-09-17

## 1. 当前目标与结论

本工程用于在杰理 AC7916A（WL82）上运行 TuyaOpen `switch_demo`，目标是完成：

- 杰理 AC79 SDK 最终编译、打包和 `tos.py` 入口适配；
- TuyaOpen TKL 层的 system、UART、Wi-Fi、BLE、Flash 等平台适配；
- 使用杰理官方 `isd_download.exe` 完成 Windows 烧录；
- 通过 BLE 配网、Wi-Fi 入网并接入涂鸦云，验证 DP 收发。

当前状态：编译和烧录链路已跑通，UART2 日志链路已跑通，BLE/Wi-Fi 已完成代码接入并曾经出现过 BLE 配网成功；但最近一次烧录后的完整 BLE/Wi-Fi/云端/DP 链路还没有用一份全新的串口日志重新闭环确认。

## 2. 工程和外部 SDK 位置

```text
TuyaOpen 工程：D:\tuya_proj\jieli
switch_demo：  D:\tuya_proj\jieli\apps\tuya_cloud\switch_demo
Jieli 平台：   D:\tuya_proj\jieli\platform\JIELI
Jieli SDK：     D:\tuya_proj\jieli\platform\JIELI\AC79_AIoT_SDK
参考工程：      D:\tuya_proj\jieli\ipc_ac7916a
```

`AC79_AIoT_SDK` 是杰理厂商 SDK，按约定只放在 `platform/JIELI` 下作为本地构建输入，不应提交到 TuyaOpen 仓库。`ipc_ac7916a` 用于参考原有 Wi-Fi、BLE 和 UART 配置，不是 TuyaOpen 应用层的一部分。

## 3. 硬件和串口约定

| 项目 | 当前约定 |
| --- | --- |
| 芯片/平台 | 杰理 AC7916A / WL82 |
| 外挂 Flash | GD25Q32CTIG，容量固定为 4 MiB |
| SDRAM | 当前使用 2 MiB；此前切换到 8 MiB 会导致 BLE 配网异常，因此不要擅自修改 |
| 日志串口 | 杰理 `uart2` |
| PC 日志端口 | `COM11` |
| 波特率 | `115200`，8N1，无流控 |
| UART2 默认引脚 | TX：PB6；RX：PB7 |
| Tuya 逻辑 UART0 | 用于服务/CLI，映射到杰理 `uart1` |
| Tuya 逻辑 UART1 | 用于 Tuya 日志，映射到杰理 `uart2` |

烧录时关闭串口助手，烧录完成后再打开 `COM11`。`COM11` 是日志串口，不是 `isd_download.exe` 的 USB 下载传输端口。

## 4. 代码分层

当前分层约定如下：

- `tuyaopen/src` 和 `tools/porting/adapter` 提供 TuyaOpen/TAL 公共接口；
- `platform/JIELI/tuyaos/tuyaos_adapter/include` 和 `src` 放杰理 TKL 实现及必要的 license glue；
- `platform/JIELI/tuyaos/tuyaos_adapter/src` 不应新增 TAL 实现或复制 TAL 接口；
- 杰理原生 OS、UART、Wi-Fi、BLE、Flash API 由 TKL 适配层直接调用；
- `switch_demo` 应用层逻辑保持 TuyaOpen 原有实现，Wi-Fi/BLE 适配重点在 TKL 层及平台构建桥接。

最近一次整理已移除平台侧重复的 `tal_system.c`、`tal_uart.c` 和相关 TAL 头文件；平台 CMake 目前编译 TKL 文件，例如：

```text
platform/JIELI/tuyaos/tuyaos_adapter/src/tkl_system.c
platform/JIELI/tuyaos/tuyaos_adapter/src/tkl_uart.c
platform/JIELI/tuyaos/tuyaos_adapter/src/tkl_wifi.c
platform/JIELI/tuyaos/tuyaos_adapter/src/tkl_bluetooth.c
```

`platform/JIELI/jieli_build.py` 会创建临时 staging 树，注入杰理厂商 demo 的板级 UART2、Wi-Fi 校准、BLE/Wi-Fi 配置和必要库链接，不直接改写厂商 SDK 原始工程。

## 5. 编译

在工程根目录初始化 TuyaOpen 环境：

```powershell
Set-ExecutionPolicy -Scope Process Bypass
. .\export.ps1
```

构建 `switch_demo`：

```powershell
Set-Location D:\tuya_proj\jieli\apps\tuya_cloud\switch_demo
& D:\tuya_proj\jieli\.venv\Scripts\python.exe D:\tuya_proj\jieli\tos.py build
```

如果虚拟环境尚未准备，先在工程根目录执行：

```powershell
Set-Location D:\tuya_proj\jieli
& .\.venv\Scripts\python.exe .\tos.py prepare
```

最近一次成功产物：

```text
D:\tuya_proj\jieli\apps\tuya_cloud\switch_demo\dist\switch_demo_1.0.0\switch_demo_QIO_1.0.0.bin
```

杰理 SDK 的实际输出位于：

```text
D:\tuya_proj\jieli\platform\JIELI\AC79_AIoT_SDK\cpu\wl82\tools\app.bin
D:\tuya_proj\jieli\platform\JIELI\AC79_AIoT_SDK\cpu\wl82\tools\sdk.elf
```

## 6. 烧录

板卡进入 USB 下载模式后，在工程根目录执行：

```powershell
Set-Location D:\tuya_proj\jieli
& .\.venv\Scripts\python.exe .\tos.py flash -p COM11 -b 115200
```

`tos.py flash` 通过 `platform/JIELI/platform_flash_bridge.py` 调用：

```text
platform/JIELI/AC79_AIoT_SDK/cpu/wl82/tools/isd_download.exe
```

默认使用杰理 WL82 的 `isd_config.ini`、`uboot.boot` 和 `cfg_tool.bin`，并使用 `-gen2 -tonorflash -dev wl82` 等参数。输入镜像名称过长时，bridge 会临时复制为 `app.bin`，不会修改 `dist` 中的用户产物。不要使用 `-format all`，避免擦除授权、KV 和其他配置。

## 7. 串口日志

使用串口工具打开 `COM11`，参数为 `115200 / 8 / N / 1`。也可以使用：

```powershell
Set-Location D:\tuya_proj\jieli
& .\.venv\Scripts\python.exe .\tos.py monitor -p COM11 -b 115200
```

日志文件由外部串口捕获工具保存到：

```text
D:\tuya_proj\jieli\apps\tuya_cloud\switch_demo\monitor.log
D:\tuya_proj\jieli\apps\tuya_cloud\switch_demo\monitor_capture.log
```

## 8. 已完成和已观察到的结果

- `tos.py build` 已能调用杰理 pi32v2 工具链完成最终链接；
- 已生成可交给杰理下载器的 WL82 镜像；
- Windows `tos.py flash` bridge 已成功调用官方 `isd_download.exe`；
- UART2、COM11、115200 日志链路已跑通；
- 参考 `ipc_ac7916a` 完成了 UART2 日志、UART1 服务口和 Wi-Fi/BLE 板级配置迁移；
- BLE 设备曾经可以被发现并完成配网；
- 已按用户要求保持外挂 Flash 为 4 MiB，并保持当前 SDRAM 配置为 2 MiB；
- 平台 TKL 合同测试在最近一次分层调整后已通过，构建也已成功。

## 9. 当前日志中的问题和验证缺口

现有 `monitor_capture.log` 最后更新时间为 2026-09-17 17:39，早于最近一次构建/烧录产物，不能作为最近固件的有效结论。该旧日志曾出现：

```text
netdev_set_mac_addr = 00:00:00:00:00:00
BT_MODULES_IS_SUPPORT None
Driver auto reconnect ... SSID - , len - 0
CntlOidSsidProc():CNTL - No matching BSS
```

因此下一步必须先重新烧录后抓一份全新的日志，确认：

1. 板级 MAC 非全零，且 Wi-Fi/BLE 初始化完成；
2. BLE 广播可发现、BLE 配网事件能进入 Tuya netmgr；
3. Wi-Fi 扫描、连接、DHCP 成功；
4. MQTT/Tuya 云连接成功；
5. DP 下发、设备上报和 switch 按键链路都有日志；
6. Tuya 日志与杰理 vendor 日志没有因 UART2 并发输出而严重交错。

在上述日志闭环前，不应声称 switch demo 已经完成“BLE 配网 + Wi-Fi 入网 + 涂鸦云 + DP”一次性实物验证。

## 10. 后续排查优先级

1. 重新烧录当前最新构建，并清理旧的串口捕获文件名或记录新的开始时间。
2. 先看 MAC、BLE 支持能力和 BLE 广播，再看配网回调是否进入 Wi-Fi TKL。
3. 若 Wi-Fi 仍显示空 SSID，检查 Tuya netmgr 配网数据是否正确传递给杰理 Wi-Fi 配置接口，而不是继续修改 Flash 容量或 SDRAM 配置。
4. 若 BLE 能配网但后续无日志，检查 UART2 输出锁、Tuya log callback、Wi-Fi 事件线程和 MQTT 线程是否继续运行。
5. 只有拿到最新日志后，再决定是否修改 TKL；应用层 `switch_demo` 不作为首要修改点。

## 11. Git 和本地文件注意事项

- 杰理 SDK 和由其生成的中间文件属于本机外部构建输入，不要提交；
- `monitor*.log` 是实物调试产物，提交前按团队需要决定是否保留；
- 当前工作区的 `.git` 文件仍指向旧 Linux worktree 元数据，Windows Git 可能无法正常执行 `status/diff`；这不影响 `tos.py` 构建和烧录，但后续提交前应先修复 worktree 元数据或在正确的 Git 工作区提交；
- 不要迁移或提交 Codex 的 `auth.json`、history、SQLite、shell snapshot、备份和 hooks 状态文件。

## 12. Windows Codex CLI 交接

已按 `tmp.txt` 在当前 Windows 用户环境完成配置：

| 项目 | 当前状态 |
| --- | --- |
| Codex CLI | `0.154.0`，npm 全局安装 |
| Ferry | 当前 Windows 已安装 `1.4.5`；`tmp.txt` 中记录的 `1.2.1` 未在本机保留 |
| 默认模型 | `gpt-5.6-luna`，reasoning `high` |
| 执行策略 | `sandbox_mode=danger-full-access`、`approval_policy=never` |
| 项目可信 | `D:\tuya_proj\jieli` 已设为 trusted |
| RTK | `0.42.0`，已验证为 Rust Token Killer |
| 插件 | `superpowers 6.3.0`、`codex-hud 0.1.6`，均已启用 |
| Superpowers skills | 14 个，随插件安装在 Codex plugin cache |
| HUD skill | `codex-hud`，随插件安装 |
| Ferry 全局 skills | `tuya-dev-flow`、`tuya-lean`、`tuya-wiki`，Codex 投影目标已启用并同步 |
| 全局规则 | `%USERPROFILE%\.codex\AGENTS.md` 引用 `RTK.md` |
| 网关 Key | 未设置；`tmp.txt` 没有提供真实 Key，因此未写入占位值 |

插件安装后重新启动一个 Codex CLI 会话，使 skills、hooks 和 HUD 状态栏配置完整加载。当前 ChatGPT 登录状态有效；如需使用 Tuya AI Gateway，再在新 PowerShell 中手动设置 `TUYA_AI_GATEWAY_API_KEY`，不要把 Key 写入仓库或交接文档。

Codex 官方 CLI 诊断结果为配置可解析、认证有效、插件可见；诊断中的线程扫描旧文件警告和 worktree 元数据警告与本项目代码无关。

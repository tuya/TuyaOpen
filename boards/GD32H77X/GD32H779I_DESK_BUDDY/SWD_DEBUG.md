# GD32H779I Desk Buddy SWD 连接与调试

适用板型：`GD32H779I_DESK_BUDDY`，主控 `GD32H779IW`。
命令适用于 pyOCD 0.46.0 和支持 SWD 的 CMSIS-DAP 调试器，使用 Bash 或 Zsh 执行。

建议使用明确的目标型号、1 MHz 时钟、`attach` 连接方式，并显式设置
`dap_swj_use_dormant=true`。这组参数已通过 Arm DAPLink 连接与烧录回读验证，
不同调试器仍需确认兼容性。

## 硬件准备

板子正常供电，连接调试器的 SWDIO、SWCLK 和 GND，确保共地。
NRST 建议接上，以便在程序启动后无法连接时使用复位下连接。
调试器的参考电压和供电引脚按其手册连接，避免与板上电源冲突。

普通 SWD 调试不要求应用程序主动响应。应用若重新配置调试引脚、进入影响调试的
低功耗状态或启用调试访问保护，可能影响后续连接。

## 连接参数

| 参数 | 建议值 | 用途 |
| --- | --- | --- |
| 目标型号 | `gd32h779iw` | 显式选择主控，避免采用调试器报告的默认板型 |
| 协议 | SWD | CMSIS-DAP 的 SWD 通道 |
| 时钟 | `1000000` Hz | 已验证的连接与烧录频率 |
| 连接模式 | `attach` | 连接现有运行状态，不主动复位 |
| SWJ 握手 | `dap_swj_use_dormant=true` | 使用经过 dormant 状态的协议切换序列 |
| Flash 自动解锁 | `auto_unlock=false` | 禁止自动解锁导致的擦除 |
| 断开自动运行 | `resume_on_disconnect=false` | 暂停后断开仍保留现场；需要运行时显式执行 `go` |
| 芯片 Pack | `GigaDevice.GD32H77x_78x_DFP.0.6.1.pack` | GD32 芯片定义与官方 Flash 算法 |

## 环境准备与设备枚举

以下命令从 TuyaOpen 仓库根目录运行，使用 Bash 或 Zsh：

```bash
. ./export.sh

SWD_PYTHON="$OPEN_SDK_PYTHON"
SWD_PACK="$OPEN_SDK_ROOT/platform/GD32H77X/gd32h77x_os/tools/packs/GigaDevice.GD32H77x_78x_DFP.0.6.1.pack"

"$SWD_PYTHON" -m pyocd --version
"$SWD_PYTHON" -m pyocd list --probes
```

pyOCD 可选依赖的安装方法见[平台说明](../../../../platform/GD32H77X/README.md)。

从枚举结果复制实际 probe ID，替换下面的占位值：

```bash
SWD_PROBE_ID='替换为本机枚举出的调试器唯一ID'
```

调试器枚举结果中的默认目标名称可能是 NUCLEO 或 STM32；它不代表外接芯片，
连接时仍需显式传入 `-t gd32h779iw`。
USB 能识别调试器，只能证明主机到调试器的通信可用，不能证明目标 SWD 已连通。
虚拟机还需要将调试器 USB 透传到执行命令的系统。

## 连接与读取现场

先读取状态，不复位、不擦写：

```bash
"$SWD_PYTHON" -m pyocd commander \
  --no-config --script /dev/null \
  --pack "$SWD_PACK" \
  -t gd32h779iw -u "$SWD_PROBE_ID" -f 1000000 -M attach \
  -O auto_unlock=false \
  -O resume_on_disconnect=false \
  -O dap_swj_use_dormant=true \
  -c 'status'
```

`--no-config --script /dev/null` 用于排除当前目录配置和用户脚本的影响。
`/dev/null` 为 Linux 路径，Windows 运行时需使用对应的空文件路径。

卡死时保持板子供电，暂停 CPU 并读取寄存器：

```bash
"$SWD_PYTHON" -m pyocd commander \
  --no-config --script /dev/null \
  --pack "$SWD_PACK" \
  -t gd32h779iw -u "$SWD_PROBE_ID" -f 1000000 -M attach \
  -O auto_unlock=false \
  -O resume_on_disconnect=false \
  -O dap_swj_use_dormant=true \
  -c 'halt; reg; read32 0xe000ed28 24; reg psp; reg msp'
```

注意两个命令细节：

- 多个操作使用同一个 `-c` 字符串，并以分号分隔。不要直接写成
  `-c 'halt' 'reg' 'read32 ...'`，该写法会将命令参数合并并报错。
- `read32` 的长度单位是字节。读取 6 个 32 位寄存器应写 `24`，不能写 `6`。

从 `0xE000ED28` 开始读取的 24 字节依次为：

| 地址 | 寄存器 |
| --- | --- |
| `0xE000ED28` | CFSR |
| `0xE000ED2C` | HFSR |
| `0xE000ED30` | DFSR |
| `0xE000ED34` | MMFAR |
| `0xE000ED38` | BFAR |
| `0xE000ED3C` | AFSR |

读取寄存器后，结合 PSP/MSP 读取内部 SRAM 异常栈，并使用与板上固件
严格匹配的 ELF 解析异常返回 PC。读取可疑外设或外部存储器窗口可能触发新的总线错误，
应先保存 CPU、故障寄存器和内部 SRAM 栈。

## 没接 NRST 时的复位方式

普通 `attach` 不依赖 NRST，应用程序通常也不需要专门配合 SWD。
保留卡死现场时不要先复位或断电。

需要重新运行程序时，可在上述连接命令中将 `-c` 替换为：

```bash
-c 'reset sysresetreq; go; status'
```

这是软件复位并运行，会丢失当前卡死现场。它要求已经能通过 SWD 访问目标，
不能用来解决调试端口完全无应答的问题。

`under-reset` 依赖调试器控制 NRST。未接 NRST 时不要使用该方式。
若程序启动后重配 SWD 引脚或进入影响调试的低功耗状态，补接 NRST 后可尝试复位下连接。

## 烧录与交付固件

烧录继续使用平台的 QIO 包校验和 Flash bridge，流程见
[平台烧录说明](../../../../platform/GD32H77X/README.md)。
`commander` 能连接不等于 `tos.py flash` 已经采用相同的参数。
当前板级默认下载后端是 DFU；上面的 `-O` 和 `-f` 参数仅作用于直接执行的
pyOCD 命令，不会自动传给 `tos.py flash`。需要使用 SWD 烧录时，按平台说明选择
SWD 后端，并确认实际执行的 pyOCD `flash` 和回读用 `commander` 都使用
`-O dap_swj_use_dormant=true -f 1000000`。`tos.py flash` 当前没有对应的透传开关，
不要将这些参数直接追加到该命令，也不要只修改调试连接而遗漏烧录与回读连接。

GD32H779I 的 QIO 包含 CNVM 启动镜像和 ECNVM 应用镜像。
烧录应保留官方 Pack、分区范围检查和两个区域的回读校验，
不要以通用 Cortex-M 目标直接替代 GD32 Flash 算法。

直接调用 CMake 构建时，新固件可能只生成到应用的 `.build/bin/`，
而 `dist/` 仍是旧文件。烧录前确认交付包已同步，并核对包摘要、ELF 和启动日志中的
编译日期，防止烧录旧固件。

## 常见问题

| 现象 | 排查方法 |
| --- | --- |
| USB 能识别调试器，但报 `No ACK` | 确认目标供电、共地、SWD 接线和 probe ID；检查是否显式设置目标型号及 dormant 握手。USB 枚举成功不代表 SWD 已连通 |
| Keil 能连，pyOCD 不能连 | 优先对比协议、目标型号、握手、时钟与连接模式，不应只反复降低时钟或归因于接线 |
| 降频后仍无法连接 | 保留 `dap_swj_use_dormant=true`，检查初始化方式。降低频率不能替代正确的协议切换序列 |
| 调试器显示 STM32 或 NUCLEO | 该名称可能来自调试器固件的默认板型，连接外部目标仍显式使用 `-t gd32h779iw` |
| 上电后很快无法连接 | 接好 NRST 后尝试 `under-reset`；未接 NRST 时不能依赖该模式抢在应用运行前连接 |
| 暂停后退出工具，程序没有继续 | 使用了 `resume_on_disconnect=false`，重新连接后显式执行 `go` |
| `read32` 提示长度未对齐 | 长度按字节填写，且为 4 的整数倍，例如 6 个寄存器填 `24` |
| 怀疑目录配置影响连接 | 保留 `--no-config --script /dev/null`，排除自动加载的配置与脚本 |

排查连接时保留 GD32 Pack 的标准调试序列，不要将
`pack.debug_sequences.enable=false` 作为默认修复。
`No ACK` 仅表示没有收到有效 SWD 应答，单凭该信息无法区分硬件连接、握手或目标状态问题。

## 参考资料

- [GD-Link V3 Adapter User Guide Rev1.1](../../docs/GD-Link%20V3%20Adapter%20User%20Guide_Rev1.1.pdf)：
  第 2.2 节介绍接线，第 3.1 节介绍 Keil 中选择 CMSIS-DAP 与下载算法。
  pyOCD 专用参数以工具配置为准。

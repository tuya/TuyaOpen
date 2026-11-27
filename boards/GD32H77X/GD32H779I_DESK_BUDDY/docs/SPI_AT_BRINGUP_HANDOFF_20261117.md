# Desk Buddy H779–VW553 SPI AT 调试交接

更新时间：2026-11-17

## 当前目标

在新 Desk Buddy 板上打通 GD32H779 与 VW553/TY1 的 SPI AT 通讯。当前手头 PCB 的两根 SPI 飞线与新原理图标注不一致；PG4 不能作为 H779 硬件 SPI 的 MOSI，因此先用**仅 Desk Buddy 配置启用的临时 GPIO 软件 SPI**验证。此临时后端默认关闭，不自动回退；已有硬件 SPI DMA+中断路径保留，START 配置未启用软件 SPI。

## 实际接线（用户确认）

| H779 主机 | VW553/TY1 从机 | 作用 |
|---|---|---|
| PG4 | PA4 | MOSI（飞线） |
| PB14 | PA5 | MISO（飞线） |
| PC3 | PA2 | SCK |
| PC0 | PA3 | CS，低有效 |

TY1 SPI 为 SPI 从机，PA4/PA5/PA2/PA3 分别配置 MOSI/MISO/SCK/NSS；TY1 日志
UART 在 PA6/PA7（GD32VW553 UART2），H779 日志在 USART0 PF4/PF5。

CH9114 串口按 USB interface 后缀识别：

```text
/dev/serial/by-id/usb-wch.cn_USB_Quad_Serial_*-if02  H779 控制台
/dev/serial/by-id/usb-wch.cn_USB_Quad_Serial_*-if06  VW553 ROM ISP / 控制线
```

`ttyACM*` 每次插拔都可能变化，换板或换集线器后 by-id 中的 USB 序列号也会变化。
`if02` / `if06` 只表示 CH9114 的 USB 接口角色，不是板卡身份。单板时可动态选择
唯一的 `*-if06`，但仍须用 ISP 握手和芯片 ID 确认目标；多板时必须按当前 USB
序列号或物理端口选择目标设备，不能仅靠 glob 自动猜。
2026-11-26 本次验证时 `if06` 临时枚举为 `/dev/ttyACM4`，仅作为现场快照。已通过该
接口完成完整镜像烧录、芯片端 SHA-256 校验和 AT 应用启动确认。

用串口监听时将 DTR/RTS 保持为低，避免改变控制线状态影响板子。日志监控不要只报“没有日志”：脚本必须同时报告接收字节数，并及时 flush 输出；之前的监听工具把输出缓冲到停止进程后才显示，造成了“日志时有时无”的错觉。

## 当前固件与烧录状态

- H779 当前刷入 `apps/tuya_cloud/switch_demo` 的 Desk Buddy 临时软件 SPI 固件。
- 烧录通过 `tos.py flash` 完成，ECNVM 和 CNVM 均执行了回读校验，最后平台报告 `Verification complete` / `Platform flash success`，DFU Leave 后设备退出 DFU。
- TY1 未重烧，仍运行已有 SPI AT 从机固件（支持当前 256 字节 CS_ONLY v2 帧）。
- 当前不代表 AI 聊天或网络连通已验证。
- H779 Desk Buddy 配置文件：`boards/GD32H77X/GD32H779I_DESK_BUDDY/platform_config/gd32h77x.config`。其中软件 SPI 开关只在此板配置打开；Kconfig 全局默认关闭。实现与 DMA/IRQ 硬件后端分开。
- GPIO 软件 SPI 半周期为 10 µs，Mode 0，帧/AT 协议仍为原 256 字节 v2；没有改用参考工程的 32 字节协议。

## 最近一次重启抓到的设备证据

停止监听进程时，之前因工具输出缓冲而未及时显示的 H779 日志一次性输出。关键内容：

```text
[AT] SPI master GPIO Mode0 enabled half_delay_us=10 worker_priority=6
[AT] SPI protocol=2 cs_only=1
[AT] AT session ready transport=spi ...
[AT] AT transaction failed result=6 cmd= payload=0 prompt=0 sent=0 send_ok=0
[AT] RX FAIL fault=6 len=7 data=52 45 41 44 59 0d 0a
[AT] module READY observed reset_active=1
[AT] AT failed cmd=AT result=6 ... phase=0 reply=READY
[AT] module hardware reset fallback complete ok=0
[AT] module startup reset failed status=5
...
[AT] SPI_RX ms=816 seq=0 ack=0 len=7 expected=0 result=2
[AT] SPI_RX ms=904 seq=0 ack=1 len=7 expected=1 result=3
[AT] SPI_RX ms=1080 seq=1 ack=1 len=4 expected=1 result=2
[AT] SPI_RX ms=1124 seq=1 ack=1 len=4 expected=2 result=3
[AT] SPI_SOFTWARE enabled=1 frames=56
[AT] SPI_HEADER a7 5c 02 00 02 00 01 00
[AT] SPI_STATE next=2 valid=47 invalid=9 ready_hi=0 ready_lo=0 idle=0
[AT] module transport recovery complete
```

解释边界：SPI 已经有双向有效帧，故障不是“完全没有 SPI 通讯”。但是 56 帧中有 9 帧被协议/链路校验计为无效；header 看起来已同步到 `A7 5C 02 ...`。`result=6` 在 `at_client.h` 中是 `AT_RESULT_RESET`。模块启动阶段 `READY\r\n` 被报告成当前 `AT` 事务的 RX fault，随后复位兜底返回失败；日志显示异步 transport recovery 后继续初始化，但 netmgr/BLE 初始化已出现失败。因此仍未达到干净的 AT startup。

## 下一步排查建议

1. 读 `platform/GD32H77X/gd32h77x_os/components/at/at_module_reset.c/.h`、`at_session.c` 中 `READY` 处理与 `reset_active` 的状态切换。确定模块在复位后主动发 `READY` 时，为什么先生成 `AT_RESULT_RESET`/`RX FAIL`，以及它是否应被当成启动事件而不是活动命令的响应。
2. 查看 `at_spi_transport.c` 的坏帧计数分支（约 440–480 行）及 `at_spi_protocol.c`：明确 `invalid_frame_count` 对应的具体校验失败原因。当前 trace 只在有 parser/事务错误时打印，并没有逐个坏 SPI 帧的 raw bytes，所以不能断言 9 帧是 CRC、sequence、READY 时序或软 SPI 电气问题。
3. 找到 `at_spi_transport_trace_dump()` 的触发条件和 `SPI_RX result` 枚举含义，分别关联有效/无效帧、AT response line `READY` 与后续 recovery 的时间顺序。
4. 需要硬件复测时，启动**实时、无 DTR/RTS 切换、逐行 flush 且有字节计数**的监听，再由用户手动复位。先采完整日志，再改状态机；不要先靠拉长 timeout 掩盖错误。
5. 任何代码改动要保留硬件 DMA+IRQ 分支，软件 GPIO 后端仍只由 Desk Buddy 临时配置选择；START 默认和既有方案不能改变。完成最小相关 host test/build 后再讨论是否重烧。

## 相关代码/资料

- 软件 SPI GPIO 配置：`platform/GD32H77X/Kconfig`、`boards/GD32H77X/GD32H779I_DESK_BUDDY/platform_config/gd32h77x.config`
- H779 SPI master：`platform/GD32H77X/gd32h77x_os/components/at/at_spi_transport.c`
- GPIO bitbang backend：`platform/GD32H77X/gd32h77x_os/components/at/at_spi_gpio_transport.c/.h`、`at_spi_gpio.h`
- AT transaction/session：`platform/GD32H77X/gd32h77x_os/components/at/at_session.c`、`at_module_reset.c/.h`、`at_client.c`
- SPI frame parser：`platform/GD32H77X/gd32h77x_os/components/at/at_spi_protocol.c`
- 板级说明：`boards/GD32H77X/GD32H779I_DESK_BUDDY/PIN_BRINGUP.md`
- 用户提供的参考工程：`boards/GD32H77X/GD32H779I_DESK_BUDDY/doc/36_FreeRTOS_Display_Camera.zip`，其中 `Docs/W553_SoftSPI_R26.md`；参考工程用的是不同的 32 字节协议，不可直接替换当前 AT 协议。

## 静态分析结果与平台层修正（2026-11-17）

- `at_client.c` 在解析到 `READY` 时先调用 `at_client_fail(..., AT_RESULT_RESET)`，因此启动期
  `AT` 探测会失败；设备层的 `module READY observed reset_active=1` 只是随后收到的状态通知，
  不能挽救已经结束的事务。
- 启动模式原先在首次 probe 失败后直接进入硬件复位。Desk Buddy 配置了 shared reset，
  `reset_module_hardware()` 按设计返回 false，所以该兜底必然失败；模块随后进入 recovery 并
  恢复，但应用侧初始化已经先失败。
- `invalid_frame_count` 只在 `at_spi_frame_decode()` 失败时递增，不包含 sequence、duplicate、
  backpressure 或 GPIO 传输失败。当前 `valid=47 + invalid=9 = frames=56`，说明 9 帧都是完整的
  256 字节快照在 magic/version/flags/reserved/CRC/length/padding 校验中失败，而不是软件 SPI
  传输没有完成。
- 平台层已增加启动期 READY 恢复：首次 probe 被 READY 打断时，先恢复本地 transport 并重新
  probe；只有恢复或二次 probe 仍失败才进入原硬件兜底。该逻辑位于 `platform/GD32H77X`，没有
  修改应用层或板级绑定。
- SPI 诊断新增坏帧原因计数和最后一帧坏帧头，下一次实板日志可直接区分未就绪快照、头 CRC、
  载荷 CRC、版本/标志和 padding 问题。软件 SPI 仍仅由 Desk Buddy 配置启用，Kconfig 默认关闭，
  无自动回退。
- Host 测试、SPI 协议/传输测试、平台配置测试、Desk Buddy 软件 SPI 构建和 START 硬件
  DMA+IRQ 边界探针构建均通过；尚未重新烧录，尚未完成实板复测。

## 约束

- 不动分区/OTA 规划、option bytes、授权信息或通用应用层来绕过平台问题。
- 不将 UUID、auth key、Wi-Fi 密码写入文档或日志输出。
- 不声称网络/AI 已通；目前只确认 SPI 帧往返有有效数据，AT startup 仍有失败证据。

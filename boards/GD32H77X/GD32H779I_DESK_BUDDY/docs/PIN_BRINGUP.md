# GD32H779I DESK BUDDY 引脚 bring-up（2026-11-16）

输入资料：

- `../doc/H779_TY1  SCH 20261116.pdf`
- `../doc/GD32H779IWK7 GPIO分配表 20261116.xlsx`

## 已对齐

| 功能 | 新版引脚 | 当前状态 |
| --- | --- | --- |
| 日志 | USART0 PF4 TX / PF5 RX，AF4 | 板级 platform config 已配置 |
| LCD QSPI | PE12 SCK、PE14 D0、PE13 D1、PE9 D2、PE10 D3 | TKL QSPI0 默认一致 |
| LCD 控制 | PC4 CS、PF14 RST、PH6 BL、PE15 TE | CS/RST/BL 已配置；TE 尚未启用 |
| PSRAM | PE11 CS(AF11)、PB2 SCK(AF9)、PA2 IO0(AF6)、PB0 IO1(AF4)、PA3 IO2(AF6)、PA1 IO3(AF9) | OSPI0/OSPIM Port0；映射已按实板 BGA 球号和波形确认，详见 `PSRAM_PORTING.md` |
| 触摸 | I2C2 PH7 SCL / PH8 SDA，PH3 RST，PH4 INT | 已更新到板级代码 |
| 前摄 CN3 | I2C2 PH7/PH8，PG2 reset | 已配置 |
| 后摄 CN4 | I2C3 PD12/PD13，PA10 reset | 已配置 |
| 摄像头 DVP | PC6/PC7/PB13/PH12/PH14/PD3/PB8/PB9，PA6 PCLK，PA8 MCLK，PB7 VSYNC，PH15 HSYNC，PG5 PWDN | TKL DVP 默认一致 |
| 编码器 | PH9 A、PB12 B、PA0 button | 已配置 |
| IMU | I2C2 PH7/PH8，PF12 INT | 已配置 |
| TY1 | SPI1：PC1 MOSI、PC2 MISO、PC3 SCK、PC0 CS、PH10 reset | 新板使用硬件 SPI/DMA，CS_ONLY v2 协议 |
| UART3/DSP | PD0 RX、PD1 TX、PA15 SPK_FLOW | CI3422 PC1 输出流控；PA15 输入，高电平允许 H779 发送 SPK 数据 |
| 电源/充电 | PC13 PWR_CTRL、PF6 ADC、PE4 CHG_STAT | PC13 已在平台初始化早期拉高；ADC/充电状态尚未注册 |
| USB | PA11 DM、PA12 DP | 芯片默认 |
| RGB | PE7（新版 PCB；旧版 PB14） | 软件尚未注册 |

### PSRAM 引脚资料勘误（2026-11-17）

- 2026-11-18 实板逐脚验证：U7 pin3=N5/PA3(IO2)，U7 pin6=P7/PB2(SCK)；此前按原理图把 PA3 当 SCK 的映射错误。
- 当前实际映射按 BGA 球号 N5/P7 和逐脚波形验证为准。
- 软件适配以实板 BGA 球号和实测波形为准；旧原理图/网表字符串中的 PA3/PC2_C 命名不再作为最终依据。
- 本板 PSRAM 不使用 PC2_C；普通 PA1 作为 IO3 使用时要求 `PA1SWON=1`。
- 完整适配方案、风险门禁和测试计划见 `PSRAM_PORTING.md`。

## START 缺少的目标引脚

START 是 GD32H779VW（LQFP100/V 封装）。相对 Desk Buddy 的 BGA176/I 封装，
缺少全部普通 PF/PG/PH 焊盘，并且没有普通 PC2/PC3（只有 PC2_C/PC3_C）：

```text
PF4  PF5  PF6  PF12 PF14
PG2  PG5
PH3  PH4  PH6  PH7  PH8  PH9  PH10  PH12  PH14  PH15
普通 PC2、PC3
```

对应影响：

- 无法验证 Desk Buddy 的 PF4/PF5 日志口。
- 无法验证触摸 I2C2 PH7/PH8、RST PH3、INT PH4。
- 无法验证 IMU 的 PH7/PH8 + PF12。
- 无法验证编码器 A 相 PH9。
- 无法验证 TY1 的 PH10 reset，也不能验证普通 PC2/PC3 焊盘；START 只有
  PC2_C/PC3_C，需要关闭模拟开关才能复用相同数字功能，属于替代验证。
- 摄像头缺少 PH12/PH14/PH15；LCD 缺少 PF14/PH6/PE15。
- 电池采样 PF6 和 CN3 reset PG2、PWDN PG5 不可用。

## START 能验证什么

因此只能做驱动和协议替代验证：

- 可以：UART3 PD0/PD1 回环、USB、QSPI0 时序、DVP 驱动框架、编码器/GPIO 驱动。
- 只能部分：摄像头（缺 PH12/PH14/PH15，且 PC6/PC7 与 START 日志口冲突）。
- 不能替代：TY1 的 PC2/PC3 + PH10、触摸 I2C2/INT、IMU、最终射频共存。
- START 上的 TY1 应继续使用已验证的 PB13/PB14/PB15/PC0 模拟接线，不能据此声称
  新版 Desk Buddy 的 PC2/PC3 电气通路已验收。

## 实板建议顺序

2026-11-20：新板按 11/16 原理图和 IO 分配表使用硬件 SPI1：
PC1 MOSI、PC2 MISO、PC3 SCK、PC0 CS，PH10 reset。
继续使用 256 字节 CS_ONLY v2 AT 协议，TY1 须烧录匹配的 VW553 固件。
2026-11-20：新板 `switch_demo` 已完成双端烧录校验、硬件 SPI/DMA 启动握手及
AP/BLE 配网服务启动。设备授权已写入 H779 KV，并通过重启后读回核对。
用户确认本次链路已跑通，详细采集范围见 [TY1 固件说明](../firmware/vw553/README.md)。

2026-11-23 实板确认：TY 的 RTS/DTR 控制线参与 BOOT strap。串口工具如果以
pyserial 默认状态打开端口，会先把 RTS/DTR 置高，目标复位后进入 ROM DFU。
正确流程是使用 `serial.Serial(port=None, ...)` 创建对象，先将 `dtr=False`、
`rts=False`，再设置 `port` 并 `open()`；打开完成后再复位目标。
`tos.py monitor` 已按此顺序处理。不得在已经打开端口后才首次下拉控制线。

2026-11-17：新板实测 `DIEEN=1`，旧 ECNVM 驱动因此拒绝 KV 擦除。
已按用户手册 Rev1.0 表 7-1/7-2 区分独立 Data ECNVM0/1 与修复 Bank：
保留 DIEEN，独立数据区允许擦写，修复区继续保护。修复后 `switch_demo`
KV 初始化和 `rst_cnt` 写入通过，使用现有 `tal_cli auth` 写入 H779 授权，
`auth-read` 读回核对一致。尚未完成整板断电后的授权持久性和 AI 联网复验。

1. 电源轨、PC13 电源控制、PF6 电池采样、PE4 充电状态。
2. PF4/PF5 日志和 PH10 TY1 复位。
3. TY1 SPI1 PC0-PC3 与 PH10 reset、AT/联网。
4. I2C2 共享总线（触摸、前摄、IMU）和 PH4 中断。
5. LCD QSPI、背光、TE，再验证触摸坐标。
6. CN3/CN4 摄像头逐路打开，禁止两路复位同时释放导致地址冲突。
7. 编码器、UART3 DSP、RGB 灯和长稳。

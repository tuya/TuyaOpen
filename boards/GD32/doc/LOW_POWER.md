# GD32VW553 低功耗能力与限制

> 适用：`boards/GD32/*`（GD32VW553 系列）
> 结论来源：GD32VW55x User Manual Rev1.6、GD32VW553-UNIFI Datasheet Rev1.0、LCKFB_GD32VM553 实测
> 最后更新：2026-08-14

## 一句话

**这颗芯片上「低功耗」等价于「进 stop 模式」，而 stop 模式会停掉整个 VCORE 域的时钟。**
所以「省电」和「外设可用」在硬件层面就是互斥的，只有 GPIO / RTC / USART0 / I2C0 / FWDGT 五类能同时成立。这不是适配层的取舍，是电源架构决定的。

## 一、三档功耗

TuyaOpen 的 `TUYA_CPU_SLEEP` 映射到芯片的 **Deep-sleep（stop）模式**：状态保留、原地恢复。
`TUYA_CPU_DEEP_SLEEP`（复位式）对应芯片的 **Standby 模式**，**适配层尚未实现**，调用返回 `OPRT_NOT_SUPPORTED`。

| 芯片模式 | 停了什么 | 官方（模组） | 实测（整板） |
|---|---|---|---|
| Run | — | 39 mA | 39~45 mA |
| Sleep（浅睡） | 仅内核时钟 | — | 28 mA（不联网）/ 32~44 mA（联网） |
| **Deep-sleep（stop）** | **VCORE 全部时钟 + IRC16M + HXTAL + PLL** | **DTIM=1: 1.8 mA** | **6~7 mA** |
| Standby | VCORE 断电、LDO 关断、SRAM 丢失 | — | 未实现 |

官方数据手册 Table 6-3 另有 DTIM=3 → 0.68 mA、DTIM=10 → 0.34 mA。**T20 认证要求 DTIM=1，实测 6~7 mA，余量约 2 倍。**

> 实测是整板电流，含电源 LED、LDO、USB 转串口；官方是模组电流。两者在 Run 档吻合得很好（39 vs 39~45），所以 DTIM=1 的 1.8 → 6~7 mA 不能全部归因于板级开销，可能仍有数 mA 余量未挖掘。

## 二、Deep-sleep 下哪些外设还能用

手册 §4（p78）原文：

> In deep-sleep mode, **all clocks in the VCORE domain are off, and all of IRC16M, HXTAL and PLLs are disabled.**

| 外设 | 能否工作 | 机制 |
|---|---|---|
| GPIO 电平保持 / EXTI 唤醒 | ✅ | 常电域 |
| RTC | ✅ | LXTAL / IRC32K 域 |
| FWDGT | ✅ | IRC32K |
| **USART0** | ✅ | `RCU_CFG1_USART0SEL` 选 IRC16M/LXTAL + `USART_CTL0_UESM`（手册 §16.3.15 的例外条款） |
| **I2C0** | ✅（未实现） | `RCU_CFG1_I2C0SEL` 选 IRC16M + `I2C_CTL0_WUEN` |
| **PWM / 硬件定时器** | ❌ | 无低功耗时钟源 |
| **SPI / UART1 / UART2** | ❌ | 同上 |

`RCU_CFG1_TIMERSEL` 只是 ×2/×4 分频选择，**不是时钟源选择**，对低功耗无帮助。

### 串口引脚（已逐项核对数据手册 Table 2-5/2-6）

| 口 | TX | RX | 角色 |
|---|---|---|---|
| **USART0** | **PB15** AF8 | **PA8** AF2 | 唯一能在深睡中收数据的口 |
| UART1 | PA4 AF0 | PA5 AF0 | |
| UART2 | PA6 AF10 | PA7 AF8 | |

`uart.h` 里 USART0 走 `#elif CONFIG_BOARD == PLATFORM_BOARD_32VW55X_EVAL` 分支。选 PB15/PA8 有三个一致的来源：芯片数据手册 AF 表、LCKFB 板文档、GigaDevice EVAL 的 `Utilities/gd32vw553h_eval.h`。UART1 从 PB15/PA8 让到 PA4/PA5，否则两个口抢同一对焊盘 —— 谁后初始化谁赢，且无声。

> **LCKFB_GD32VM553 只有一路 USB 转串口，在 Type-C 上，接的就是 PB15/PA8。** 它同时是下载口（ROM boot loader 在 USART0 上听）、日志口和用户串口，三者时间上不重叠。日志口由 `CONFIG_GD32_LOG_UART_PORT` 选（默认 0 = USART0，免飞线）。PA0/PA1、PA4/PA5、PA6/PA7 都没有桥接。

### UART 的差别

- **USART0**：切到 IRC16M + 置 `UESM` 后，深睡期间继续把字节收进 FIFO，**不丢首字节**（2026-08-17 实测，敲 `abc` 回显完整的 `abc`）。

  波特率上限 **约 166 kbps**，按量化误差推出：`USARTDIV = uclk/(16×baud)`，分数域 4 bit，误差约 `1/(32×USARTDIV)`；要 <0.5% 就得 `USARTDIV ≥ 6`，即 `baud ≤ IRC16M/(16×6)`。**不能按「整数域装得下」推成 1 Mbps** —— SDK 日志跑 921600，那里 `USARTDIV` 只有 1.08，光量化误差就 2.9%，必然乱码。超过上限时自动保留精确时钟，行为退化成和 UART1/UART2 一样。
- **UART1 / UART2**：没有时钟源选择器，只能把 RX 脚配成 EXTI event 唤醒内核，**唤醒它的那个字节必然丢失**。协议上需要一个弃用的首字节。

## 三、为什么做不出「中间档」

理想中存在这样一档：省电，但外设时钟不停。**GD32VW553 上三条路全部堵死**，手册逐条核对如下。

手册 p78 给出三种降功耗手段：

| 手册给的手段 | VW553 实际情况 |
|---|---|
| 减慢系统时钟（HCLK/PCLK1/PCLK2） | ✅ 有。**实测 160MHz → 16MHz 只省约 12 mA**（浅睡 + 联网从约 44 降到 32 mA） |
| 关闭未使用外设的时钟（`RCU_xxxEN`） | ✅ 有，但未使用的外设本来就没开时钟，无可关 |
| 通过 `PMU_CTL0` 的 `LDOVS` 配置 LDO 输出电压 | ❌ **PMU_CTL0 位表中不存在该字段** |

`PMU_CTL0` 完整位定义（手册 p83）：`LDEN[1:0]`(19:18)、`LDNP`(11)、`LDLP`(10)、`BKPWEN`(8)、`LVDT[2:0]`(7:5)、`LVDEN`(4)、`STBRST`(3)、`WURST`(2)、`STBMOD`(1)、`LDOLP`(0)，其余全部 Reserved。`PMU_CS0_LDOVSRF` 只是一个只读 ready flag。**所有电压旋钮（`LDOLP`/`LDEN`/`LDNP`/`LDLP`）都只在 Deep-sleep 期间生效**，运行态和浅睡态无法降压。

此外 `RCU_AHB1SPEN`（偏移 0x50）**整个寄存器只有 bit15 `FMCSPEN` 一位**，没有 `APB1SPEN`/`APB2SPEN`，也没有其他 GD32 系列（如 GD32G553）具备的 `SPDPEN`（sleep **and deep-sleep** peripheral enable）。**没有任何寄存器可以让某个外设在深睡时保持时钟**（USART0/I2C0 的例外走的是各自的时钟源选择器，不是 RCU 的睡眠使能）。

> 遗留疑点：手册 p78 正文提到 `LDOVS`，但寄存器位表中没有该字段，两者矛盾。判断依据取寄存器位表。如需彻底确认可向 GigaDevice 求证。

### 对比：T2（BK7231N）为什么能做到

同为单核 + FreeRTOS tickless 的 T2 平台，其 `sctrl_hw_sleep()` 做的是：

```c
/* 关 DPLL 480M、关 XTAL2RF，但保留 26M 晶振 */
reg &= ~(BLK_EN_DPLL_480M | BLK_EN_XTAL2RF);
/* 关模拟中心偏置 —— 相当于 GD32 的 LDO 低功耗档 */
reg &= ~CENTRAL_BAIS_ENABLE_BIT;
/* 逐个外设关时钟，掩码里保留 UART1/UART2/TIMER */
REG_WRITE(ICU_PERI_CLK_PWD, peri_clk);
```

关键差异是 **`ICU_PERI_CLK_PWD` 这个逐外设时钟门控位图可以和电源域降档组合使用**，而 GD32 的 `pmu_to_deepsleepmode()` 是整个 VCORE 域一刀切。**「低功耗 + 外设可用」在 T2 上是设计内的能力，在 GD32VW553 上没有硬件通路。**

## 四、适配层的设计

### 1. 外设「限深」投票

深睡会切断传输中的 DMA、冻住 PWM 输出、停掉硬件定时器 —— **且全部静默无报错**。解法不是禁止睡眠，而是限制深度：

- `gd32_peri_busy_inc/dec()`（`tkl_sleep.c`，引用计数）持有 **wakelock id 6**
- `tickless_sleep.c` 的 `freertos_ready_to_sleep()` 把 id 6 **摘出去**（`& ~PERI_CLOCK_LOCK_MSK`），只在深睡分支单独检查
- 效果：持锁期间 idle 仍进浅睡（内核停、外设时钟全活），只是进不了 stop 模式

持锁者及时机：

| 驱动 | 持锁区间 | 原因 |
|---|---|---|
| SPI | 传输开始 → DMA ISR 拆除 / 轮询结束 | 中断模式下函数带着 DMA 一起返回 |
| ADC | 三个读入口全程 | 中间夹着 `sys_ms_sleep()` 等转换 |
| I2C | `xfer_pending=TRUE` 时跨调用持有 | 故意不发 STOP，总线被扣着 |
| PWM | `timer_enable` → `timer_disable`/`deinit` | 否则输出冻在当前电平 |
| 硬件定时器 | 同上 | 否则周期晚整个睡眠时长 |

PWM 和定时器使用按通道的**幂等**标志，因为 `duty_set()`/`polarity_set()` 会重复 `timer_enable()` 一个已在运行的定时器。

**行为约定：开启 PWM 或硬件定时器即视为退出低功耗状态**，此时电流为浅睡档（几十 mA）。这是硬件限制下的正确选择 —— 另一种可能是波形静默冻死。

### 2. tickless 睡眠时长

厂商原实现**不是 tickless**：只在 `DEEP_SLEEP_MIN_TIME_MS`(2000) 和 `DEEP_SLEEP_MAX_TIME_MS`(10000) 两个常数间二选一，完全不看 `expected_idle_time`。已改为跟随调度器截止点，并：

- `DEEP_SLEEP_FLOOR_MS = 20`：窗口更短则回落到 port 自己的 `__WFI()`（浅睡）
- `DEEP_SLEEP_EXIT_MS = 5`：闹钟提前 5ms，盖住 hxtal 重启 + pll 重锁的约 4.5ms 恢复时间。不增加唤醒次数，不影响电流
- 睡过头时 clamp 到 `idle_ms`，差值经 `port.c` 的 `vApplicationIdleHook()` 调 `xTaskCatchUpTicks()` 补回（那是空闲循环中唯一调度器未挂起的点）

### 3. 应用侧必须交还射频

`wifi_sw_init()` 会在整个协议栈生命周期内持有 `LOCK_ID_WLAN`，一条这样的登记就足以让睡眠请求被无声挡住。不使用射频的设备必须显式交还：

```c
tkl_ble_stack_deinit(0);
tkl_wifi_set_work_mode(WWM_POWERDOWN);
```

联网设备交不了 WiFi，靠 `tkl_cpu_sleep_mode_set()` 末尾的 `wifi_core_task_resume(false)` 促使 mac 放锁 —— **该调用不可删除**。

## 五、已知限制与待办

| 项 | 状态 |
|---|---|
| 深睡时系统时间快约 6.7% | **待修**。RTC 源为 IRC32K（标称 32000 Hz，实测该芯片约 34160 Hz），而深睡的 tick 补偿全靠 RTC 测量。硬件 `rtc_smooth_calibration_config()` 范围仅 ±488 ppm，不够用，需按片运行时校准 |
| ~~PWM / 硬件定时器降档未验证~~ | **已验证**（2026-08-17）。PWM 口 1 = PB13，10 kHz/20%，20 秒开 20 秒关：电流出现清晰台阶，示波器上波形全程连续 |
| ~~USART0 深睡唤醒缺前提~~ | **已补**。DMA：本适配层无 DMA 路径，天然满足；**BSY**：`tkl_uart_write` 同步路径等 `USART_FLAG_TC` 再返回，异步路径持外设时钟票由 TC 中断释放；**REA**：`usart_enable()` 后检查，不通过打 NOTICE |
| USART0 唤醒路径非官方 | 手册指定 RBNE 或 WUM 中断，当前用 RX 脚 EXTI event —— 避开了 SDK `uart_irq_hdl()` 不清 `WUF` 导致的中断风暴。**已实测成立**，保留 |
| I2C0 深睡可用 | **未实现**，可做 |
| `TUYA_CPU_DEEP_SLEEP`（Standby） | **未实现**。需要唤醒源跨复位持久化 |
| SRAM_sleep / BLE_sleep / Wi-Fi_sleep | 手册列出的另三种省电模式，TKL 层均未接入 |

## 六、验证方法

- **判断内核是否真的停止**：数 `minstret`（退休指令数），WFI 期间为零。**不要用 `mcycle`** —— 它由自由运行时钟驱动，WFI 期间照样计数。注意 `mcountinhibit`(0x320) 需先清零，`_premain_init()` 会禁用计数器。
- **判断时间精度**：**不要用 `tal_system_get_millisecond()`**，它是 `xTaskGetTickCount() * OS_MS_PER_TICK`，属循环论证。也不要只用 RTC —— IRC32K 自身就有数个百分点误差。需要第三方时基（串口日志的主机时间戳最省事）。
- **`freertos_pre_sleep_processing()` 处于关中断临界区，不能打日志**。需要统计时用静态计数器，从任务里读。
- **功耗曲线判读**：基线贴 0（深睡到位）+ 规律窄尖峰（beacon）+ 偶发宽脉冲（网络流量）。基线不贴 0 才是自身问题。**避开开机后头十几秒**，射频尚未交还，电流高一个量级。
- **网络环境的影响大于所有配置项之和**：完全空闲的网络可到 2.4 mA，一般家庭网络约 3.6~7 mA，繁忙时可达十几 mA。报数字必须说明网络环境。

## 七、参考资料

- GD32VW55x User Manual Rev1.6：<https://www.gd32mcu.com/en/download/6>（文档 498）
- GD32VW553-UNIFI Datasheet Rev1.0：<https://www.gd32mcu.com/data/documents/datasheet/GD32VW553-UNIFI%20Datasheet%20Rev1.0.pdf>
- AN150《GD32VW553 吞吐量及场景功耗测试指南》：`platform/GD32/gd32_os/docs/CN/`。注意其 §4.1 说明测试时断开跳帽 J6 直接给模组供电，是模组电流，不可与整板数据直接比较
- 例程：`examples/lowpower/cpu_sleep`、`cpu_deep_sleep`、`wifi_ps`

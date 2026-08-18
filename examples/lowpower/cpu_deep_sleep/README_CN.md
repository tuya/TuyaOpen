# cpu_deep_sleep

演示**深度睡眠**及其唤醒源。

深度睡眠的定义是**状态不保留**：芯片大部分供电被切断，醒来**从复位开始**，内存、外设配置、网络连接全部要重建。这是它和 `TUYA_CPU_SLEEP` 的分界 —— 不是「睡得更沉一点」，而是**性质不同的两件事**。

因为状态不保留，唤醒源必须在睡前**显式登记**下来（有些平台会写进 flash），能选的范围也被那一小块常电区域限制 —— 通常只有 RTC 闹钟和少数几个专用唤醒引脚，而不是任意中断。

涉及两组接口：

- `tkl_wakeup_source_set()` —— 登记哪些事件能把设备叫醒
- `tkl_cpu_sleep_mode_set(TRUE, TUYA_CPU_DEEP_SLEEP)` —— 让 CPU 进入深睡

启动时打印的复位原因（`tkl_system_get_reset_reason()`）是**判断「这次是上电还是被唤醒」的唯一依据** —— 深睡醒来必然经过复位，所以应用只能靠它来分辨。

状态保留的那一档见隔壁的 [cpu_sleep](../cpu_sleep/) 例程。本例程针对**不联网**的设备；已经连上路由的设备走的是另一条路，见 [wifi_ps](../wifi_ps/)。

## 平台支持情况

不是所有平台都提供复位式的深睡。芯片可能只有「原地恢复」那一档（那属于 `TUYA_CPU_SLEEP`），也可能有但适配层还没实现。这时接口会返回负数：

```
enter deep sleep -> -1
this platform has no deep sleep mode wired up; staying awake
```

**GD32VW553 目前就是这种情况** —— 它的 Stop 模式是原地恢复的，已经归给 `TUYA_CPU_SLEEP`；真正的复位式睡眠（Standby）适配层尚未实现。所以这个例程在这块板子上能演示唤醒源的配置，但睡眠本身不会发生，电流不会下降。

## 配置项

| 配置 | 默认 | 说明 |
|---|---|---|
| `EXAMPLE_WAKEUP_RTC` | y | 挂一个 RTC 闹钟作为唤醒源 |
| `EXAMPLE_WAKEUP_RTC_SECONDS` | 10 | 多少秒后闹钟触发 |
| `EXAMPLE_WAKEUP_GPIO` | y | 挂一个引脚作为唤醒源 |
| `EXAMPLE_WAKEUP_GPIO_PIN` | 0 | 唤醒引脚编号 |
| `EXAMPLE_WAKEUP_GPIO_EDGE` | 2 | 0 低电平 / 1 高电平 / 2 上升沿 / 3 下降沿 |

修改方式：

```bash
tos.py config set EXAMPLE_WAKEUP_RTC_SECONDS=30 EXAMPLE_WAKEUP_GPIO_PIN=0
```

例程还会在睡前交还用不到的射频（`tkl_ble_stack_deinit()` / `tkl_wifi_set_work_mode(WWM_POWERDOWN)`），见下。

## 预期输出

```
Reset reason:        0 (power on)
gpio wakeup  pin 0 edge 2 -> 0
rtc  wakeup  in 10 s -> 0
ble  stack deinit -> 0
wifi power down   -> 0
enter deep sleep -> 0
awake
awake
...
rtc alarm fired
```

- 五个 `-> 0` 表示唤醒源、射频交还、睡眠模式都配置成功；返回负数说明该平台不支持这一项
- `rtc alarm fired` 说明 RTC 唤醒回调真的被调用了
- `awake` 每 2 秒一次，说明设备还活着 —— **只是为了把「在睡」和「挂了」区分开**，不代表睡得好不好

心跳**特意不带计数**：深睡醒来会经过复位，计数每次都从头开始，摆在日志里只会让人以为出了问题。真正的验证靠电流表。

## 引脚唤醒怎么验证

引脚唤醒**没有回调** —— 它的作用是把设备叫醒，不是通知应用。所以按下按键不会打印任何东西，能看到的是设备重新启动。要确认这一项，在供电回路串一个电流表，看按键按下时电流有没有跳回运行水平。

各平台可用的唤醒引脚不同。GD32VW553 上专用的 WKUP 引脚是 **PA0、PA15、PA7、PA12**，且只响应上升沿。

## 为什么要先关射频

一个还在运行的射频协议栈会向电源管理登记「我还要用 CPU」，一条这样的登记就足以让睡眠请求被一直挡在门外 —— 而且完全无声，配置接口全部返回成功，只有电流会说话。

所以不用 WiFi 和蓝牙的设备应该明确交还它们：

```c
tkl_ble_stack_deinit(0);
tkl_wifi_set_work_mode(WWM_POWERDOWN);
```

例程用平台能力宏判断，有就关，不做成配置项。

## 关于功耗

睡眠的实际收益要用电流表测，日志证明不了任何事。**避开开机后的头十几秒再框选** —— 那段时间射频还在跑、正在被交还，电流比稳态高一个量级。

在支持复位式深睡的平台上，这一档通常能到微安量级，因为除了常电域几乎什么都断了。GD32VW553 上无法演示（见上面的平台支持情况），想看这块板子的低功耗数据请跑 [cpu_sleep](../cpu_sleep/)。

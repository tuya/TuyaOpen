# 主流嵌入式功耗管理组件实现方案调研

> 调研日期：2026-07-31
> 目的：为 TuyaOpen「设备功耗等级管理组件」提供架构参考，聚焦**实现机制与 API 设计**，非泛泛省电技巧。
> 配套：`docs/power_level_management_design.md`

---

## 0. 一句话结论

**几乎所有成熟系统都收敛到同一套骨架：机制/策略分离 + 引用计数的「约束/锁」仲裁。** 差别只在两点：①「态」是**离散枚举**还是**连续约束**；②约束是「**别比 X 更深**」（veto/floor）还是「**这东西在用，顶住**」（refcount get/put）。我们现有的 `lpmgr`（引用计数 + 取最严档）已经踩在主流范式里，方向没错。

---

## 1. Zephyr RTOS —— 教科书式的「机制/策略分离 + 残留时间策略 + 状态锁」

**状态建模**：`pm_state` 枚举，按功耗从浅到深排序；每个态在 devicetree 里带元数据 `min_residency_us`、`exit_latency_us`、上下文保留能力。

**策略层（residency policy，默认）**：内核把「距下一个调度事件的时间」告诉 policy，policy 选**能放得下的最深态**：

```
if (time_to_next_event >= state.min_residency_us + state.exit_latency)
    return state;   // 选满足条件的最深态
```

→ 这是「自动降档」的最干净实现：**不是单一全局 timeout，而是每个态自带最小驻留时间**，短空闲不进深态（避免进出开销倒亏）。

**约束/锁**：
- `pm_policy_state_lock_get()/put()` —— 组件可**禁止进入某个具体态**（如外设后台任务不能丢上下文）。这是「veto/floor」型。
- `pm_policy_latency_request()` —— 声明「我能容忍的最大唤醒延迟」，policy 据此排除退出延迟过大的深态。

**机制层**：SoC 侧 `pm_state_set()` / `pm_state_exit_post_ops()` 真正进出硬件态。策略（选哪个态）与机制（怎么进）彻底解耦。

**设备级 PM（device runtime PM）**：`pm_device_runtime_get()/put()` 引用计数，refcount 归零自动 suspend（可同步/异步 `put_async`）。关键：**一旦设备启用 runtime PM，就脱离系统睡眠周期独立管理**——系统还醒着，空闲外设也能各自睡。

## 2. ESP-IDF esp_pm —— 三类锁 + DFS + 自动 light sleep（和我们最像）

**机制**：DFS 动态调频 + automatic light sleep。

**仲裁 = 三类引用计数锁的「地板投票」**：

| 锁类型 | 作用 |
|--------|------|
| `ESP_PM_CPU_FREQ_MAX` | 顶住 CPU 最高频 |
| `ESP_PM_APB_FREQ_MAX` | 顶住 APB 频率（ESP32 = 80MHz） |
| `ESP_PM_NO_LIGHT_SLEEP` | 禁止进 light sleep |

- 锁**递归 + 引用计数**：`acquire` 加计数、`release` 减计数，归零才真正释放；可在 ISR 里调。
- 生效频率 = 所有活动锁里**最严的约束**；无锁则掉到配置的最低频。
- **自动 light sleep**：无锁且开启配置时进入；睡多久由「最近一个 FreeRTOS 任务超时 / esp_timer 事件」决定；带**预测补偿**（测实际唤醒开销反馈下一次）。

**API**：`esp_pm_configure()`（配 max/min 频 + 是否 light sleep，= 策略）、`esp_pm_lock_create/acquire/release`、`esp_pm_dump_locks()`（调试列出所有活动锁——很值得抄）。深睡另走 `esp_sleep_*` + 唤醒源配置。

## 3. RT-Thread PM —— Run/Sleep 两层 + request/release

**分层**：RUN 态（管 CPU 调频）与 SLEEP 态（管休眠深度）**用两套独立 API**，正交控制。

**仲裁**：`rt_pm_request(mode)` / `rt_pm_release(mode)` 成对使用，保护一段区间（如 DMA 传输期间禁止深睡）。空闲时组件自动挑最深可用 sleep mode，**对应用透明**。支持多级 sleep mode。

## 4. FreeRTOS tickless idle —— 「空闲判定」的底座

`configUSE_TICKLESS_IDLE=1` 后，当①仅 idle 任务可运行 ②距下一个任务就绪 ≥ `configEXPECTED_IDLE_TIME_BEFORE_SLEEP` 个 tick，内核调 `portSUPPRESS_TICKS_AND_SLEEP(idleTime)`：停 tick 中断 → `configPRE_SLEEP_PROCESSING`（平台配唤醒源）→ WFI → 醒来 `vTaskStepTick()` 补偿 tick。

→ **「设备空闲」这个信号可以直接从 RTOS 调度器免费拿到**，不必自己维护 timeout。ESP 的 auto light sleep 就是建在这上面。lpmgr 目前是自己算 timeout，可考虑对齐调度器。

## 5. Linux 内核 runtime PM —— 引用计数设备 PM 的经典范本

- `usage_count`（atomic）+ `child_count`：两者都为 0 才允许 suspend。
- `pm_runtime_get_sync()` 加计数并同步唤醒；`pm_runtime_put_autosuspend()` 减计数 + 记 last_busy，**延迟 autosuspend**（避免抖动来回切）。
- 驱动实现 `runtime_suspend/resume/idle` 三回调。
- 上层策略：`cpuidle` governor 选 CPU idle 态、`cpufreq` governor 选频率——**策略层（governor）与机制层（进态）分离**，和 Zephyr 同构。

**可借鉴点**：`autosuspend_delay`（延迟降档防抖）——比一掉空闲就立刻深睡更稳。

## 6. 网络侧「保持连接但休眠」的建模（L2 的直接同类）

这是我们 L2（DTIM 在线保活）最该参考的一层。共性模型：**Idle/Active 两态 + 一个「轮询/唤醒间隔」在「延迟 vs 功耗」间取舍 + 上游（AP/Router）缓存消息**。

| 机制 | 谁控制间隔 | 特点 |
|------|-----------|------|
| **802.11 DTIM** | AP | STA 每 DTIM 个 beacon 醒一次收缓存下行；周期越大越省、延迟越高 |
| **802.11 Listen Interval** | STA | STA 可睡过多个 beacon（比 DTIM 更长），代价是漏部分组播 |
| **802.11 TWT（WiFi 6）** | STA↔AP 协商 | 协商唤醒时刻表，最省最灵活，但需 AX AP |
| **Thread SED** | STA | poll interval 决定醒来向 parent 取包 |
| **Matter ICD** | 应用 | 见下，建模最完整 |

**Matter ICD（Intermittently Connected Device）—— 值得整段照搬的建模**：
- **两态 Idle/Active**：按 `IdleModeInterval`/`ActiveModeInterval` 切换。Active 用 **fast-poll**（最大响应），Idle 用 **slow-poll**（最省）。→ **正是我们 L1(active/低延迟) vs L2(idle/DTIM 保活) 的抽象**。
- **SIT（Short Idle Time）**：slow-poll ≤ 15s，保持对用户输入响应（门锁/窗帘这类）。
- **LIT（Long Idle Time）**：电池设备，长睡，需要 client↔device **check-in 同步**才能通信。
- 依赖**关联 Router 缓存下行**，设备轮询到点一次性取走——和 WiFi AP 在 DTIM 期间缓存下行**完全同构**。

→ **对我们缺口 3（DTIM 保活 vs MQTT keepalive）的答案**：业界（Matter LIT）不是让两者各自为政，而是把**睡眠间隔做成云端已知的协商参数（check-in）**。即 L2 下 MQTT keepalive 与 DTIM/睡眠窗口应**协同设计**，而非任其撞死 socket。

## 7. Android wakelock（思想源头，简述）

`PowerManager.WakeLock` acquire/release，任一 wakelock 持有就阻止系统睡。引用计数持锁 → 全体释放才睡。上面所有 RTOS 的「锁」本质都是它的嵌入式变体。

---

## 8. 跨系统对比表

| 系统 | 状态建模 | 仲裁模型 | 代表 API | 策略层 |
|------|---------|---------|---------|--------|
| **Zephyr sys** | 离散 `pm_state` + residency/latency 元数据 | 状态锁 veto + latency 约束 | `pm_policy_state_lock_get/put`、`pm_policy_latency_request` | residency policy / 自定义 `pm_policy_next_state` |
| **Zephyr dev** | 每设备 suspend/resume | 引用计数 get/put | `pm_device_runtime_get/put(_async)` | refcount 归零自动 suspend |
| **ESP-IDF** | 频率档 + light sleep 开关 | 三类递归计数锁「地板」 | `esp_pm_lock_acquire/release`、`esp_pm_configure` | max/min 频 + auto light sleep |
| **RT-Thread** | Run(频) + Sleep(多级) 两轴 | 成对 request/release | `rt_pm_request/release` | 空闲自动挑最深 sleep |
| **Linux** | 每设备 + cpuidle/cpufreq | `usage_count`+`child_count` 引用计数 | `pm_runtime_get_sync/put_autosuspend` | cpuidle/cpufreq governor + autosuspend delay |
| **FreeRTOS** | 有/无 tick | 调度器判空闲 | `portSUPPRESS_TICKS_AND_SLEEP` | `configEXPECTED_IDLE_TIME` 阈值 |
| **Matter ICD** | Idle/Active + poll interval | 应用请求 active | fast/slow poll interval、check-in | Idle/ActiveModeInterval |
| **TuyaOpen lpmgr（现状）** | reason→(sleep 周期, DTIM) | 引用计数取最严档 | `lpmgr_register/unregister`、`lpmgr_default_set` | 策略表 + 事件挂接 |

---

## 9. 提炼的共性设计模式

1. **机制 vs 策略必须分离**。「进哪个态」（策略）和「怎么进这个硬件态」（机制/SoC）分层。我们的 facade=策略、lpmgr/tkl=机制，天然符合。
2. **仲裁核心 = 引用计数锁**，两种风味且常并存：
   - **veto/floor 锁**（「别比 L1 更深」）→ Zephyr state_lock、ESP `NO_LIGHT_SLEEP`；
   - **refcount get/put**（「这外设在用，顶住」）→ Linux/Zephyr device、RT-Thread。
   我们应**两种都给**：产品策略地板 + 子系统持档锁。
3. **状态用离散枚举 + 每态元数据（min_residency / exit_latency / 是否保上下文）**。降档判据不该是一个拍脑袋的全局 timeout，而是「空闲时长 ≥ 该态最小驻留 + 退出延迟」。
4. **降档防抖**：Linux 的 `autosuspend_delay`、ESP 的预测补偿——掉档前留一个延迟窗口，别抖。
5. **空闲判定尽量复用 RTOS 调度器**（FreeRTOS tickless / ESP auto light sleep），而非到处埋 timeout。
6. **深睡 + 唤醒源是独立子系统**（ESP `esp_sleep_*`、Zephyr soft-off），和「在线低功耗」不共用路径——印证我们把 L3 单独封装的决定。
7. **网络保活 = Idle/Active 两态 + 轮询间隔 + 上游缓存**（Matter ICD 最完整）；睡眠间隔要让云端知晓（check-in），别和心跳各自为政。

---

## 10. 对我们「4 档 WiFi 功耗组件」的具体借鉴

1. **档位 enum 挂元数据**：给每档配 `{min_residency, exit_latency, 是否保网络, 默认 DTIM}`，降档用 Zephyr 式「空闲 ≥ min_residency+exit_latency」判据替代单一 `idle_timeout_ms`。（改进现有 API 草图的 `idle_timeout_ms`）
2. **两级锁 API 都保留**：`pm_set_level_cap()`（产品地板，Zephyr state_lock 风味）+ `pm_lock_acquire(floor)`（子系统持档，refcount 风味），底层直接落到 lpmgr `register/unregister` 与 wakelock 位图。
3. **加降档延迟窗口**（autosuspend_delay 同款），进 L2/L3 前留缓冲防抖。
4. **提供 `pm_dump()`**（抄 `esp_pm_dump_locks`）：打印当前生效档 + 所有持锁者，ULP 调试刚需——lpmgr 已有 `lpmgr_show_power_mode()`，facade 透出即可。
5. **L1↔L2 直接套 Matter ICD 的 Active/Idle + fast/slow-poll 模型**：L1=active/低延迟、L2=idle/DTIM 慢轮询，语义现成、经过大规模验证。
6. **L2 的 MQTT keepalive 联动按 ICD check-in 思路做**：把睡眠窗口作为一个云端可感知的参数协同，而不是让 app 各调各的（对应缺口 3 的推荐解法）。
7. **L3 沿用 ESP/Zephyr 的独立深睡子系统抽象**：唤醒源配置 + retention 单独一套 API，平台不支持则编译期裁掉退回 L2。
8. **可选演进**：若日后要精细管理外设（音频 codec、屏），可引入 Zephyr/Linux 式**设备级 runtime PM**（per-device get/put），与系统级档位正交——本期不做，但 enum/锁模型要给它留位。

---

## 来源

- Zephyr System PM — https://docs.zephyrproject.org/latest/services/pm/system.html
- Zephyr Device Runtime PM — https://docs.zephyrproject.org/latest/services/pm/device_runtime.html
- ESP-IDF Power Management — https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/system/power_management.html
- RT-Thread PM — https://www.rt-thread.io/document/site/programming-manual/pm/pm/ ，源码 https://github.com/RT-Thread/rt-thread/blob/master/components/drivers/pm/pm.c
- Linux Runtime PM — https://docs.kernel.org/power/runtime_pm.html
- FreeRTOS Low Power / Tickless — https://freertos.org/Documentation/02-Kernel/02-Kernel-features/07-Lower-power-support ，https://github.com/FreeRTOS/FreeRTOS-Kernel-Book/blob/main/ch11.md
- Matter ICD（Silicon Labs）— https://docs.silabs.com/matter/2.6.1/matter-overview-guides/matter-icd
- 802.11 TWT / DTIM / Listen Interval（Nordic / Cisco）— https://nrfconnectdocs.nordicsemi.com/ncs/2.6.1/nrf/protocols/wifi/station_mode/powersave.html

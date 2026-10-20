# 杰理 AC791/WL82 与 AC792/WL83 接入 TuyaOpen 功耗管理 —— 适配设计

> 状态：设计稿（第一阶段实施依据）
> 关联底座：`src/tuya_pm/`（方案库+调度器）、`src/tal_wifi_ulp/`（lpmgr）、
> `src/tal_system/src/tal_sleep.c`（CPU 休眠 + wakelock）、`src/tal_wifi/src/tal_wifi.c`
> 平台侧：`platform/JIELI/tuyaos/tuyaos_adapter/src/system/tkl_sleep.c`、
> `platform/JIELI/tuyaos/tuyaos_adapter/src/driver/tkl_wifi.c`

---

## 1. 目标与非目标

**目标**：让 AC791（WL82）与 AC792（WL83）两块板通过 TuyaOpen 通用接口被管理：

- CPU 允许休眠 / 保持唤醒；
- 业务唤醒锁（TAL wakelock）；
- Wi-Fi STA 省电开关与监听参数；
- `tuya_pm` / `tal_wifi_ulp` 的状态协调与错误反馈。

保持现有 Wi-Fi、配网、云连接、DP、OTA 功能不变；保留 SDRAM；不调 CPU 频率与电压；不做电池管理。

**第一阶段不做**（明确排除）：

- AC792 `DEEP_SLEEP_EN=2`（掉电式深睡）；
- 掉电后重启式深睡；
- 未经验证的长监听间隔（不为了更低电流擅自改长）；
- 自动关闭用户仍需要的 BLE。

---

## 2. 分层与调用链

JIELI 侧**不新增任何通用层判断**，只补齐 TKL；上层原样复用：

```
tuya_pm  (通用调度器：ACTIVE / CEC_T20 / ULP_ONLINE / DEEPSLEEP)
  └─ tuya_pm_schemes.c  enter() 直调底层
       ├─ lpmgr (ENABLE_WIFI_ULTRA_LOWPOWER)  ← 引用计数省电管理器
       │    ├─ tal_wifi_lp_enable / tal_wifi_lp_disable
       │    ├─ tal_wifi_set_lps_dtim
       │    ├─ tal_cpu_acquire/release_wakelock   (位图语义)
       │    └─ tal_cpu_set_lp_mode
       └─ tdl_power_enter_deepsleep  (第一阶段不使用)
            ↓
tal_sleep.c  /  tal_wifi.c        (通用 TAL，不改语义)
            ↓
tkl_cpu_sleep_mode_set / tkl_wifi_set_lp_mode      ← 本设计实现的两点
            ↓
platform/JIELI/.../tkl_sleep.c  /  tkl_wifi.c      ← 本设计修改的两个文件
            ↓
厂商 SDK：low_power_* (WL82 power_interface.h / WL83 power_api.h)
          wifi_psmode_* / wl_*listen* / low_power_hw_unsleep_*
```

结论：**只要把 `tkl_sleep.c` 与 `tkl_wifi.c` 两个点补实，整条 `tuya_pm → lpmgr → TAL → TKL` 链路即可在两颗芯片上工作**，通用组件不需要出现杰理芯片判断。

---

## 3. 两芯片能力矩阵

证据类型说明：
- **[H]** 厂商头文件声明（公开）；
- **[L]** 厂商静态库中已定义（`llvm-nm --defined-only`，符号类型 `T`）；
- **[S]** 厂商随包源码（`apps/common/**`，本平台会 staged 编译）；
- **[D]** 厂商自带示例/注释给出的用法证据。

### 3.1 CPU 休眠 / 保持唤醒

| 能力 | WL82 (AC791) | WL83 (AC792) |
|---|---|---|
| 保持唤醒（阻止 CPU 休眠） | `low_power_sys_unsleep_lock()` **[H][L]** | `low_power_disable()` **[H][L]** |
| 允许休眠 | `low_power_sys_unsleep_unlock()` **[H][L]** | `low_power_enable()` **[H][L]** |
| 全局省电开关（构建期） | `TCFG_LOWPOWER_LOWPOWER_SEL`（当前 `0`＝关）**[S]** | `TCFG_LOWPOWER_FUNCTION`（当前 `LOWPOWER_CLOSE`）**[S]** |
| 可用掩码 | `SYS_SLEEP_EN｜SYS_SLEEP_BY_IDLE｜RF_SLEEP_EN｜RF_FORCE_SYS_SLEEP_EN` = **30** **[H]** | 仅 `SLEEP_EN`（第一阶段）**[H]** |
| 休眠投票 / 注册式 | `low_power_get/put`、`low_power_sys_get/put`、`low_power_sys_is_idle` **[H]** | `low_power_get/put`、`low_power_sys_get/put`、`low_power_sys_is_idle` **[H][L]** |
| 深睡（掉电） | `power_set_soft_poweroff()` **[H]** | `power_set_soft_poweroff()` **[H]**、`DEEP_SLEEP_EN` **[H]** |

**关键差异**：WL82 的保持唤醒是**锁语义**（lock/unlock，天然可嵌套计数），WL83 是**全局开关语义**（enable/disable）。因此 TKL 必须在 WL83 侧自己做一次引用计数，不能把 `tkl_cpu_sleep_mode_set` 直接透传。

**已核实的厂商用法证据 [D]**：WL82 头文件注释
`power_interface.h:...`
```c
#define SYS_SLEEP_EN          BIT(1)
#define SYS_SLEEP_BY_IDLE     BIT(2)
#define RF_SLEEP_EN           BIT(3)
#define RF_FORCE_SYS_SLEEP_EN BIT(4)
```
以及 WL82 自带板级注释（`apps/demo/demo_hello/include/app_config.h`）：
```c
#define TCFG_LOWPOWER_LOWPOWER_SEL 0 //(RF_SLEEP_EN|RF_FORCE_SYS_SLEEP_EN|SYS_SLEEP_EN) //该宏在睡眠低功耗才用到，此处设置为0
```
即厂商给出的期望掩码是 `RF_SLEEP_EN|RF_FORCE_SYS_SLEEP_EN|SYS_SLEEP_EN` = 26；本设计按任务要求取 **30**（多一个 `SYS_SLEEP_BY_IDLE`）。

### 3.2 Wi-Fi STA 省电

| 能力 | WL82 (AC791) | WL83 (AC792) |
|---|---|---|
| 运行时 RF 保持唤醒 / 允许休眠 | `low_power_hw_unsleep_lock()` / `low_power_hw_unsleep_unlock()` **[H][L]** | **无对应符号**（`llvm-nm` 全库未找到 `low_power_*unsleep*`） |
| 监听策略：监听 DTIM | `wl_enabled_listen_dtim()` **[S]** | `wifi_psmode_enable_listen_dtim()` **[L]** |
| 监听策略：按默认间隔 | `wl_disenabled_listen_dtim()` **[S]** | `wifi_psmode_disable_listen_dtim()` **[L]** |
| 监听策略：自定义间隔 | `wl_set_custom_listen_interval()` + `wl_enabled_listen_custom_intervals()` **[S]** | `wifi_psmode_enable_listen_customized_intervals()` **[L]** |
| 监听策略：每个 TIM 都监听（最"醒"） | `wl_enabled_listen_each_tim()` **[S]** | `wl_enable_listen_each_tim()` **[L]** |
| 策略标志位 | `wl_listen_dtim_enabled_flag` 等 **[L]**（库中定义，`wifi_conf.c` 里 `extern`） | `wifi_psmode_listen_dtim_enable` 等 **[L]**（库内私有） |
| Wi-Fi 省电的编译期闸门 | **需要两层**：`CONFIG_LOW_POWER_ENABLE` → `WL_STA_SLEEP=2` → `PSMode=Fast_PSP`；另需 `TCFG_LOWPOWER_LOWPOWER_SEL≠0` 才能把 CPU/RF 也睡下去 | `TCFG_LOWPOWER_FUNCTION != LOWPOWER_CLOSE` → `WL_STA_SLEEP=1` → `PSMode=Legacy_PSP`（受 §4.5 的枚举缺陷影响） |

**重要更正（第一轮遗漏）**：仅设置 sleep mask **不等于**接通 STA 省电。`apps/common/net/wifi_conf.c:519-525`
依据 `CONFIG_LOW_POWER_ENABLE` / `CONFIG_WIFI_IDLE_RESUME_BASEBAND_ENABLE` 选择 `WL_STA_SLEEP`，
再在 `:561-567` 写成 `PSMode=`。demo_hello profile 两个宏都没定义，所以第一轮的产物虽然 mask=30，
STA 仍是 `CAM`（常醒）。现已改为同时定义 `CONFIG_LOW_POWER_ENABLE`（→ Fast PSP），
并在 staging 时用工具链 clang 预处理**校验实际 PSMode**（见 §4.3）。

**证据**：AC791 功耗测量工程的 `P1_FAST` 档正是 `mask 30 + CONFIG_LOW_POWER_ENABLE`，日志为
`WIFI_PS=Fast_PSP SOC_SLEEP=30 BLE=OFF`；`P1_LEGACY` 则是 `+ CONFIG_WIFI_IDLE_RESUME_BASEBAND_ENABLE`
→ `Legacy_PSP`。该工程同样未定义 `CONFIG_AUDIO_MIX_ENABLE`，故本平台开启该宏的音频缓冲区副作用不生效。

**已核实的厂商用法证据 [D]**：WL82 自带 `apps/common/example/wifi/low_power/main.c` 就是 Wi-Fi 省电开关的官方用法：

```c
low_power_hw_unsleep_lock();   //调用这句话以后开始不允许WIFI进入休眠,通常需要WIFI全力工作的工况下启用
...
low_power_hw_unsleep_unlock(); //调用这句话以后开始允许WIFI进入休眠,通常WIFI开始进入空闲工况下启用
```

**已排除的候选**：`netdev_set_power_save()` / `ext_wifi_pm_suspend()` / `ext_wifi_pm_resume()` 虽然声明在 `wifi_ext.h`，但 `llvm-nm` 显示它们**只存在于外挂 Wi-Fi 模组库**（`libaicwifi.a`、`rtl8189e/f.a`、`rtl8733bs.a`、`rtl8822cs.a`），**不在本平台使用的内置 Wi-Fi 库 `wl_wifi.a` / `wl_wifi_sta.a` 中**，且 `wifi_ext.h` 自述"该头文件用来放从 RTL8189 库调用函数"。故**不采用**。

### 3.3 监听间隔与 DTIM 的关系（为什么第一阶段不改长间隔）

厂商 SDK 的监听策略是**四选一**（`apps/common/net/wifi_conf.c` 注释）：

1. 只监听 DTIM（默认）；
2. 每个 TIM 都监听（优先级最高）；
3. 自定义监听间隔（默认值 10 个 beacon）；
4. 以上都不启用则按默认监听间隔（默认值 3 个 beacon）。

其中「DTIM 周期」是 **AP 侧**参数，「listen interval」是 **STA 在关联时告知 AP 的**参数，两者不是同一个维度；把 TuyaOpen 的 `dtim` 数值直接当 listen interval 写下去，属于未经验证的映射。因此第一阶段：

- `dtim <= 1` → 落到厂商的**监听 DTIM** 策略（语义精确对应"每个 DTIM 都醒"）；
- `dtim > 1` → **不改监听策略**，保持 SDK 默认，打印一次警告说明未映射。

---

## 4. 实现方案

### 4.1 `tkl_sleep.c`（CPU 休眠与唤醒锁）

- `tkl_cpu_sleep_mode_set(enable, mode)`
  - `mode == TUYA_CPU_SLEEP`：`enable=TRUE` → 允许休眠；`FALSE` → 保持唤醒。
  - `mode == TUYA_CPU_DEEP_SLEEP`：返回 `OPRT_NOT_SUPPORTED`（第一阶段明确不做）。
  - WL82 用 `low_power_sys_unsleep_lock/unlock`；WL83 用 `low_power_disable/enable` + 本地引用计数。
- `tkl_cpu_allow_sleep()` → 允许休眠；`tkl_cpu_force_wakeup()` → 保持唤醒（阻止下一次进入休眠）。
- `tkl_cpu_sleep_callback_register()` **保持 `OPRT_NOT_SUPPORTED`**：两颗芯片都没有把「进入/退出休眠前后」的钩子暴露到可用接口上，不伪造。
- 互斥保护 + 幂等：重复 enable/disable、多任务并发必须安全。

### 4.2 `tkl_wifi.c` 的 `tkl_wifi_set_lp_mode(enable, dtim)`

| | WL82 | WL83 |
|---|---|---|
| `enable=TRUE` | `low_power_hw_unsleep_unlock()`（允许 RF 休眠）+ `wl_enabled_listen_dtim()` | `wifi_psmode_enable_listen_dtim()` |
| `enable=FALSE` | `low_power_hw_unsleep_lock()`（禁止 RF 休眠） | `wifi_psmode_enable_listen_each_tim()`（每个 beacon 都醒，即最"醒"策略） |
| `dtim ≤ 1` | 应用（默认唤醒策略） | 应用 |
| `dtim > 1` | **返回 `OPRT_NOT_SUPPORTED`**，不改任何状态 | 同左 |

> WL83 没有运行时 RF 保持唤醒接口，Wi-Fi 省电本身由构建期 `TCFG_LOWPOWER_FUNCTION` 打开；运行时可做的、有符号证据的动作是切换监听策略，`enable=FALSE` 用厂商"每个 TIM 都监听"策略表达"保持清醒"。
>
> `dtim > 1` 返回 `OPRT_NOT_SUPPORTED`：`tuya_pm` 的 CEC_T20 请求 DTIM 1、ULP_ONLINE 请求 DTIM 10，两者在本平台执行同一策略。若都回 `OPRT_OK`，上层会以为两档深浅不同。拒绝是诚实的表示。

### 4.3 构建配置接入（只在需要时生效）

通过 **TuyaOpen 构建条件**接入，不改厂商默认示例、不全局打开：

- 仅当 `CONFIG_ENABLE_TUYA_PM=y` 时，staging 阶段才把低功耗配置注入**已 staged 的副本**：
  - WL82：`TCFG_LOWPOWER_LOWPOWER_SEL` → `30`，**并且**定义 `CONFIG_LOW_POWER_ENABLE`（→ Fast PSP）
  - WL83：`TCFG_LOWPOWER_FUNCTION` → `1`（`SLEEP_EN`，必须写字面量，理由见 §4.5；同时 → Legacy PSP）
- **写入后立即用工具链 clang 校验实际 PSMode**：从真实 `wifi_conf.c` 中切出决定 PSMode 的
  `#if/#elif/#else` 分支，拼上 staged profile 后预处理，要求只剩一个 `PSMode=` 且等于预期值。
  检查的是预处理结果而非文本——只断言"宏已写入"无法发现枚举缺陷那类"写了但不生效"。
  wl83 还需按真实链路补 `board_config.h`（`CONFIG_BOARD_DEMO` 定义）与 `power_api.h`（枚举声明）。
- `boards/JIELI/TKL_Kconfig` 增加 `ENABLE_PM`（无 prompt，default y），与 T5AI/T3 一致。
- **关闭功耗组件时不改变原有运行行为**：`configure_lowpower()` 只在 `power` 特性入选时被调用，此时 staged 的低功耗配置仍是厂商原值（wl82 mask `0`、无 `CONFIG_LOW_POWER_ENABLE`；wl83 `LOWPOWER_CLOSE`），STA 保持 CAM、硬件无法休眠；新增的 TKL 代码也没有任何调用者。
  - **注意：不能拿镜像哈希做这个验收。** 杰理镜像内嵌构建时间，同一份源码连编两次哈希就不同（本轮实测：同一 pristine 源码两次全量构建得到 `5F4813…` 与 `E8549B…`）。验收改用"staged 文件 + 特性集合"逐项核对（见 §6）。

### 4.5 WL83 的宏必须写字面量（厂商 `#if` 缺陷）

`apps/common/net/wifi_conf.c` 用 `#if TCFG_LOWPOWER_FUNCTION != LOWPOWER_CLOSE` 决定
`config_wifi_lowpower_enable` 与 `WL_STA_SLEEP`。但 `LOWPOWER_CLOSE` 与 `SLEEP_EN` 都只是
`lowpower_config_t` 的枚举常量，**在预处理器里两者都展开为 0**，所以这个比较在厂商所有板子上恒为假
（`config_wifi_lowpower_enable` 永远是 0，Wi-Fi 省电分支从未被编进去）。

已用工具链 clang 预处理实测确认：

```
enum { R_ENUM_NAME  = (LOWPOWER_CLOSE != LOWPOWER_CLOSE) };  // #if 里 => 0，假
enum { R_ENUM_SLEEP = (SLEEP_EN       != LOWPOWER_CLOSE) };  // #if 里 => 0，假
enum { R_LITERAL    = (1              != LOWPOWER_CLOSE) };  // #if 里 => 1，真
```

因此 `TCFG_LOWPOWER_FUNCTION` 必须写成 `1`，才能同时满足结构体初始化（`.config = 1`，即
`SLEEP_EN`）与预处理器的 `#if`。这一条同时影响 `audio/log_config/lib_media_config.c:57`
（`config_audio_dac_delay_off_ms` 300 vs 0），属于开启低功耗的连带效果。

### 4.4 通用层：本 PR 不改

**本 PR 只做平台适配，`src/` 一行未改。** 适配边界就是 TKL；`tuya_pm → lpmgr → tal_sleep/tal_wifi → tkl_*`
这条链本来就是为平台预留的，杰理实现 TKL 即可复用，通用组件不应为单个平台改动。

核验过程中在通用层发现以下缺陷，它们**与杰理无关**（在其他平台同样存在），应单独提交 PR：

1. `tal_sleep.c`：`tal_cpu_set_lp_mode()` 先记状态再调 TKL，TKL 返回 `OPRT_NOT_SUPPORTED` 时软件仍报"已开启"。
2. `tuya_pm_schemes.c`：`__ps_on()` 丢弃 `lpmgr_set_lps_dtim()` 的返回值；且 CEC_T20 请求 DTIM 1，
   而 lpmgr 只接受 10/20/30，该档在任何 lpmgr 平台上都未生效。
3. `tuya_pm.c`：`__apply()` 忽略 `enter()` 的返回值，切换失败仍记为已生效。
4. `lpmgr.c`：DTIM 应用失败后仍缓存目标值，导致之后不会重试。
5. BLE 生命周期：预设方案降档时 `tuya_ble_deinit()`，回到 ACTIVE 不恢复；若恢复则须还原原配置而非默认配置。

> 不建立与 `lpmgr` 平行的第二套业务锁管理器；业务锁继续用 `tuya_pm_lock_*` / TAL wakelock。

---

## 5. 明确不支持 / 未验证项

| 项 | 状态 | 原因 |
|---|---|---|
| AC792 `DEEP_SLEEP_EN=2` | 不支持（返回 `OPRT_NOT_SUPPORTED`） | 第一阶段范围排除 |
| 掉电后重启式深睡 | 不支持 | 同上 |
| `tkl_cpu_sleep_callback_register` | `OPRT_NOT_SUPPORTED` | 厂商未暴露可用的休眠前后钩子 |
| WL82 `dtim > 1` | 返回 `OPRT_NOT_SUPPORTED` | 无已验证的 dtim→listen interval 映射 |
| WL83 `dtim > 1` | 同上 | 同上 |
| WL83 运行时"关 Wi-Fi 省电" | 用"每个 TIM 都监听"策略表达 | SDK 未暴露运行时 PS 开关 |
| **`CONFIG_ENABLE_TUYA_PM`** | **第一阶段不在杰理上启用** | 见 §5.1 |
| 板上电流实测 | **未做** | 见 PR 说明 |

### 5.1 为什么第一阶段不启用 tuya_pm

本 PR 只做**平台适配**：`src/` 一行未改。TKL 的休眠原语已实现并通过编译验证，但还不足以让 `tuya_pm` 在杰理上正确工作，原因是框架与适配器的 **DTIM 契约不匹配**：

- `ENABLE_TUYA_PM` 会 `select ENABLE_WIFI_ULTRA_LOWPOWER`，即走 lpmgr；
- `lpmgr_set_lps_dtim()` 只接受 **10/20/30**，其余返回 -1 且不改状态；
- 本适配器的 `tkl_wifi_set_lp_mode()` 只应用 **DTIM ≤ 1**，更大的值返回 `OPRT_NOT_SUPPORTED`。

两集合无交集，所以：

- CEC_T20 请求 DTIM 1 → lpmgr 拒绝；
- ULP_ONLINE 请求 DTIM 10 → 适配器拒绝。

且 `__ps_on()` 丢弃返回值、`tuya_pm` 的 `__apply()` 也忽略 `enter()` 结果，**上层仍会显示切换成功**。

要启用，需先完成其一（均属通用层改动，应单独评审）：

1. 让 `tuya_pm` 按平台能力选择 Wi-Fi 后端（TKL 直连，绕过 lpmgr 的 DTIM 域）；
2. 或让 lpmgr 与 TKL 建立已验证的 DTIM 语义，并把失败向上传播。

在此之前，杰理保持厂商默认的低功耗配置，硬件行为与改动前一致。

---

## 6. 验证计划

1. 两芯片编译：`switch_demo`（AC79_DevKitBoard / AC792N_Develop_Board）。
2. **不启用** `CONFIG_ENABLE_TUYA_PM` 时：staged 的低功耗宏保持厂商原值，镜像 `PSMode=CAM`。
   （不用镜像哈希——镜像内嵌构建时间，见 §4.3。）
3. 若临时开启 `CONFIG_ENABLE_TUYA_PM` 验证 staging 路径：确认 staged 宏为 mask 30 + `CONFIG_LOW_POWER_ENABLE`，
   镜像中的 `PSMode=Fast_PSP`（wl83 为 `Legacy_PSP`）；此配置仅用于验证构建链路，**不作为交付默认**。
4. 锁/状态：重复调用、配对释放、不支持路径（`dtim > 1`、`TUYA_CPU_DEEP_SLEEP`）。
5. 回归：配网、断线重连、DP、OTA 路径不受影响。
6. 板上验证：本轮**无授权硬件**，不做；PR 中如实标注"未做板上验证"。

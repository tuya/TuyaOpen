# 设备功耗等级管理组件 —— 设计方案（v3）

> 状态：设计稿 / 待评审
> 目标读者：TuyaOpen 平台 & 应用开发者
> 配套：`docs/power_mgmt_industry_survey.md`（主流方案调研）
> 关联底座：`src/peripherals/power/`（tdl_power）、`src/tal_wifi_ulp/`（lpmgr）、`tkl_wakeup` / `tal_sleep`
> 相对 v2 的变化见 §12。

---

## 1. 背景与目标

不同产品对功耗诉求差异极大：从「常供电、不在乎功耗」到「电池供电、极少唤醒的便利贴/传感器」。TuyaOpen 已有底层原语（CPU sleep、WiFi DTIM、深睡+唤醒源）和机制层组件（`tdl_power`、`lpmgr`），但**缺一个面向产品的「功耗方案」层**。

**定位：一个「功耗方案库 + 调度器」。**
- **方案库**：内置几套涂鸦在真实产品上验证过的**预设方案**，开发者开箱即用；也允许开发者**注册自己的方案**，和预设方案一起选用。
- **调度器**：组件核心只做「在若干方案之间按策略切换」——仲裁、空闲降级、切换时调用方案的进/出实现、广播 consumer、电池兜底。**它不实现任何具体的省电动作**（那些在方案里）。

**目标**：
- 提供**可移植的「功耗方案」抽象**：一个方案 = 一段「进入/退出时做什么」的实现；
- 预设方案开箱即用，产品差异用 **consumer 注册**（外设）+ **hold-lock**（临时顶档）+ **自定义方案**（全新机制）表达；
- 用**投票/引用计数仲裁 + residency 空闲降级**决定生效方案；
- 开发者可**自排降级链**，不排则用默认链。

**非目标**：不替换 `tdl_power`/`lpmgr`；不做电池充电算法（沿用 `tdl_power`）；不在组件里包一层「动作原语」——方案实现直接调底层（lpmgr / tal_wifi / tuya_ble_deinit / tdl_power）。

---

## 2. 两个开发者，两套 API（贯穿全文的分界）

设计始终区分两类使用者，API 也据此分开：

| 角色 | 是谁 | 做什么 | 用哪些 API |
|------|------|--------|-----------|
| **方案作者** | 涂鸦内部（写预设方案）/ 需要全新机制的高级开发者 | 定义一个方案「进入/退出做什么」 | 预设方案内置；自定义方案 `tuya_pm_scheme_register()` |
| **方案使用者** | 普通产品开发者 | 选用哪些方案、怎么排降级、本产品外设怎么跟随 | `tuya_pm_chain()`（可选）、`consumer`、`hold-lock`、`battery`、`init` |

**关键**：使用者**从不定义方案、不写方案实现**。他只是「挑方案 + 排序 + 用 consumer 挂自己的外设」。方案的动作（设 DTIM、关蓝牙、深睡）是方案作者的事，藏在方案实现里。

---

## 3. 方案抽象（核心）

一个「功耗方案」抽象成一个实现了 `enter`/`exit` 的对象。**预设方案是组件内部的实例，自定义方案是开发者提供的实例，两者同构。**

```c
// 方案标识：数字。预设占低位，自定义从 BUILTIN_MAX 往后由作者自己 #define。
typedef enum {
    TUYA_PM_ACTIVE = 0,           // 全速在线
    TUYA_PM_CEC_T20,              // 联网低功耗待机；对齐 CEC Title 20 (network-standby) 能效认证
    TUYA_PM_ULP_ONLINE,           // 超低功耗在线保活（µA 级，DTIM 保活可被云端/下行唤醒）
    TUYA_PM_DEEPSLEEP,            // 离线深睡，外部唤醒
    TUYA_PM_SCHEME_BUILTIN_MAX,   // 自定义 id 从这往后
} TUYA_PM_SCHEME_E;

typedef struct {
    uint8_t      id;               // 预设=enum；自定义 >= BUILTIN_MAX
    uint32_t     min_residency_ms; // 空闲 ≥ 此值才允许降进本方案（降级门控）；0=可立即
    OPERATE_RET (*enter)(void *ctx); // 进入本方案：在这里做省电动作
    OPERATE_RET (*exit)(void *ctx);  // 离开本方案（可为 NULL）
    void        *ctx;
} TUYA_PM_SCHEME_T;
```

- `enter`/`exit` **不是钩子，是方案的实现本体**。谁定义方案谁写它，使用者永不接触。
- **方案实现直接调底层**（`lpmgr_set_lps_dtim` / `tal_wifi_*` / `tuya_ble_deinit` / `tdl_power_enter_deepsleep`），组件不提供、也不需要「动作原语」中间层。
- 方案**不带任何声明式语义标签**（无 `keep_online`/`keep_network`/`mech`）——方案在不在线、做没做深睡，是它 `enter` 实现的结果，所见即所得，不再让作者声明一遍（那类字段既冗余又可能与实现不符）。
- `min_residency_ms` 是**降级门控**（把本方案当作降级目标时的空闲门槛），归在方案上。

---

## 4. 预设方案（库存货，内置，不注册）

涂鸦在组件内部就用 §3 的抽象写好这 4 个方案，`enum` 只是拿它们的引用；使用者**不构造、不注册**，直接用 id 引用。

| 方案 | `enter` 做什么（内部实现，示意） | 语义 | 量级 |
|------|-------------------------------|------|------|
| **ACTIVE** | WiFi PS off、拉满 | 全速在线、低延迟 | 高 |
| **CEC_T20** | **关蓝牙** → WiFi PS/DTIM1 | 在线待机、对上层透明 | < 0.2W |
| **ULP_ONLINE** | **关蓝牙** → WiFi DTIM10+ 保活 | 在线保活、延迟升高 | µA 级 |
| **DEEPSLEEP** | 关蓝牙 → `tdl_power_enter_deepsleep()`（断电，板级唤醒源，≈重启） | 离线、外部唤醒 | 更低 µA |

**关蓝牙写在预设方案里**：CEC_T20/ULP_ONLINE 靠 WiFi power-save 省电，而共天线双模模组不关蓝牙 WiFi 进不了 PS——所以「关蓝牙」是这两个预设方案 `enter` 的固定第一步（`#if defined(ENABLE_BT_SERVICE)` 内部走 `tuya_ble_deinit()`；无蓝牙的构建自然跳过、也编得过）。想「进省电但保留蓝牙」的产品，注册一个不关蓝牙的**自定义方案**即可，不动预设。

**两条正交的轴**（区分方案的本质）：
- **ACTIVE↔CEC_T20↔ULP_ONLINE**：响应性 vs 功耗——越深越省，唤醒/下行延迟越大。
- **ULP_ONLINE↔DEEPSLEEP**：在线 vs 离线——**不是 mA vs µA**（都能到 µA）。ULP_ONLINE 保持关联 AP、可被云端/下行唤醒、RAM 保留；DEEPSLEEP 断网、只能外部唤醒、绝对功耗更低但丢连接、RAM 丢失（=带记忆的重启）。选哪个作日常待机档取决于「是否需保持云端可达」，而非功耗数字。

**WiFi power-save 的三个硬前提**（CEC_T20/ULP_ONLINE 才有意义）：①连上 AP、②station 模式、③蓝牙关闭。其中「关蓝牙」由预设方案 `enter` 负责；「连上 AP」由**应用用 hold-lock 门控**（没连上就顶住浅档、别降进在线保活档，见 §6.3 与 NOTE 样例的 link gate）——组件本身不查网络状态。

### 4.1 代码组织（预设方案与调度器分文件）

预设方案的实现**单独一个文件**，与调度器核心分开，但同在 `src/tuya_pm/`：

```
src/tuya_pm/
├── include/tuya_pm.h      # 公共 API：调度器 + 方案抽象 + register/chain
└── src/
    ├── tuya_pm.c          # 调度器核心：仲裁/降级/切换/consumer/lock/battery/链管理
    │                      #   —— 平台/生态无关，不 include ble/lpmgr/tal_wifi
    ├── tuya_pm_schemes.c  # 预设方案库：ACTIVE/CEC_T20/ULP_ONLINE/DEEPSLEEP 的 enter/exit
    │                      #   —— 生态特定依赖全在这：ble_mgr.h(关蓝牙)、lpmgr/tal_wifi、tdl_power
    └── tuya_pm_scheme.h   # 内部头：方案文件向调度器暴露内置方案表(tuya_pm_builtin_table 之类)
```

**价值**：调度器核心从此**不 include 任何生态特定头**（关蓝牙 / ULP / WiFi PS 全在方案文件），真正做到通用；加 / 改预设方案只动 `tuya_pm_schemes.c`，不碰调度器；方案文件里可按平台能力条件编译（`ENABLE_BT_SERVICE` / `ENABLE_WIFI_ULTRA_LOWPOWER`）。自定义方案由开发者在自己的工程里实现，与此无关。

---

## 5. 自定义方案（方案作者，注册进库）

库里没有开发者要的机制时（自定义时钟/电压、TWT、SoftAP 保活…），作者写一个方案实例、`init` 前注册：

```c
static OPERATE_RET my_enter(void *ctx) {
    lpmgr_set_lps_dtim(3);   // 直接调底层，不经 tuya_pm 包装
    my_lower_clock();        // 自己的私有动作（这才是「代码式」自定义方案的价值）
    return OPRT_OK;
}
#define MY_LOWCLOCK  TUYA_PM_SCHEME_BUILTIN_MAX   // 自己的数字 id
static const TUYA_PM_SCHEME_T my = {
    .id = MY_LOWCLOCK, .min_residency_ms = 5000, .enter = my_enter, .exit = NULL,
};

tuya_pm_scheme_register(&my);   // 单独接口，必须在 tuya_pm_init 之前调用
```

注册后它就和预设方案平等，可被降级链选用（§6.1）。

**id 与生命周期**：
- **id 由作者自己指定**（`#define` 一个 `>= TUYA_PM_SCHEME_BUILTIN_MAX` 的数字），不是 `register` 返回。`register` 时组件**做去重校验**——id 与已注册方案（含预设）冲突则返回错误、拒绝注册。
- **init 即定型**：`tuya_pm_scheme_register()` / `tuya_pm_chain()` 必须在 `tuya_pm_init()` 之前调用；init 之后方案库与降级链**只读**，不支持运行期热插拔（换取无锁的简单实现）。

---

## 6. 使用者 API

### 6.1 组降级链（可选）

降级链是一个**有序的方案 id 数组，浅→深**，顺序**同时是深浅序 + 空闲降级路径 + 仲裁可见集**（**链三合一**）——只有链中的方案能被 request / lock floor / consumer min / deepest_allowed 引用。

```c
// 高级用法：混排预设与自定义，自己决定顺序 / 跳过某档
uint8_t chain[] = { TUYA_PM_ACTIVE, MY_LOWCLOCK, TUYA_PM_ULP_ONLINE, TUYA_PM_DEEPSLEEP };
tuya_pm_chain(chain, 4);

// 便利入口：一行启用预设 4 档链
tuya_pm_chain_default();   // = { ACTIVE, CEC_T20, ULP_ONLINE, DEEPSLEEP }
```

**默认不降级**：既不调 `tuya_pm_chain` 也不调 `tuya_pm_chain_default` 时，链只有 `{ ACTIVE }`，设备**不自动降级**（停在 ACTIVE）。要降级——哪怕就用预设 4 档——必须显式组链（`tuya_pm_chain_default()` 一行即可）。这是安全默认：不配置就不会意外进省电档。

**深浅比较全部锚在「方案在链中的位置」**（index 越大越深），不靠 enum 数值——自定义方案自动纳入排序。`deepest_allowed`、hold-lock 的 floor、consumer 的 `min_powered_level`、电池 lifeboat 目标都以链中位置表达；引用不在链中的方案 = 报错。

### 6.2 初始化与产品策略

```c
typedef struct {
    uint16_t force_deepest_below_mv;   // 低电强制降到链尾（最深方案）；0=关
    BOOL_T   charging_holds_active;    // 充电中钉链首（最浅方案）
} tuya_pm_battery_policy_t;

typedef struct {
    uint8_t   deepest_allowed;          // 产品地板：链中最深允许的方案 id（默认=链尾）
    uint32_t  decay_debounce_ms;        // 降档防抖
    const tuya_pm_battery_policy_t *battery;  // NULL=不做电池策略
} tuya_pm_policy_t;

// power_dev_name：挂哪个 tdl_power（NULL=不挂电池/域/深睡；rail consumer 与深睡随之退化）
OPERATE_RET tuya_pm_init(const char *power_dev_name, const tuya_pm_policy_t *policy);
OPERATE_RET tuya_pm_set_deepest_allowed(uint8_t scheme_id); // 运行期可调地板
```

### 6.3 动态持档锁（refcount floor）

```c
typedef void *tuya_pm_lock_h;
OPERATE_RET tuya_pm_lock_create(uint8_t floor_scheme_id, tuya_pm_lock_h *out); // floor=链中位置
OPERATE_RET tuya_pm_lock_acquire(tuya_pm_lock_h lk);  // refcount++；同句柄不叠加
OPERATE_RET tuya_pm_lock_release(tuya_pm_lock_h lk);  // refcount--
OPERATE_RET tuya_pm_request(uint8_t scheme_id);        // 钉住某方案（停止衰减）
OPERATE_RET tuya_pm_activity(void);                    // 上报活动，复位衰减计时
```

> **典型用法：WiFi 关联门控（link gate）。** CEC_T20/ULP_ONLINE 要求连上 AP 才真省电，应用建一个 floor=ACTIVE 的锁：未关联时持锁（顶在 ACTIVE），`WFE_CONNECTED` 后释放（放行降级），掉线再持回。这就是「连上 AP」这个前提的落地方式——组件不查网络，应用用锁表达。

### 6.4 Consumer 模型（外设↔方案耦合）

> **consumer** = 受管的耗电外设（借 Linux/Zephyr 电源框架术语）。一张登记卡 `{撑到哪个方案 + 关我 + 开我}`；生效方案深过它撑的档 → 调 `suspend`，升回来 → `resume`。管理器不需要懂具体外设。**这是使用者给本产品外设「跟随功耗方案」的唯一手段——不是方案钩子。**

```c
typedef void *tuya_pm_consumer_h;
typedef struct {
    const char     *name;              // 仅日志/调试用
    uint8_t         min_powered_level; // 供电撑到哪个方案（含）；更深则 suspend
    uint8_t         priority;          // suspend 高优先先走；resume 反序
    OPERATE_RET   (*suspend)(void *arg); // 进更深方案前收尾（线程上下文，可阻塞/走 I2C）
    OPERATE_RET   (*resume)(void *arg);  // 回到 ≤min 方案时恢复
    void           *arg;
} tuya_pm_consumer_t;

OPERATE_RET tuya_pm_consumer_register(const tuya_pm_consumer_t *c, tuya_pm_consumer_h *out);
OPERATE_RET tuya_pm_consumer_unregister(tuya_pm_consumer_h h);

// 便利构造器：纯切电源域的简单外设（内部生成平凡 suspend=域关/resume=域开）
// 需 power_dev_name 已挂 tdl_power
OPERATE_RET tuya_pm_consumer_register_rail(const char *name, uint32_t domain_mask,
                                           uint8_t min_powered_level, uint8_t priority,
                                           tuya_pm_consumer_h *out);
```

> 简单外设一行搞定（rail 便利构造器）；带收尾的外设自己写 suspend/resume。**对外只有一个 consumer 模型。**

### 6.5 观测 / 调试

```c
uint8_t tuya_pm_current(void);   // 当前生效方案 id
OPERATE_RET tuya_pm_on_change(void (*cb)(uint8_t from, uint8_t to, void *), void *arg);
void tuya_pm_dump(void);         // 打印生效方案 + 降级链 + 所有 lock 持有者 + consumer 态
```

---

## 7. 仲裁 + 空闲降级

**生效方案计算**（深浅 = 链中位置）：

```
idle_target = 空闲衰减推出的目标方案（受各方案 min_residency_ms 门控）
lock_floor  = 所有 active hold-lock 中最浅的 floor
effective   = 最浅( idle_target, lock_floor, deepest_allowed )
```

- **产品地板 `deepest_allowed`**：永不比它更深（常供电钉 ACTIVE/CEC_T20；便利贴放开到链尾）。
- **hold-lock**：子系统临时持一个 floor（放音持 ACTIVE、OTA 持 CEC_T20），refcount，全释放才允许更深。
- **空闲衰减**：无活动且无更浅锁时，沿降级链逐跳向 `deepest_allowed` 加深，每跳受目标方案 `min_residency_ms` 门控；`tuya_pm_activity()` / 收到指令 / 按键 → 复位弹回链首。
- **降档防抖**：进更深方案前留 `decay_debounce_ms` 窗口。
- **电池（可选）**：充电中 → 内部持链首锁；电压 < `force_deepest_below_mv` → **硬性强制链尾**（最深方案，越过一切锁与 `deepest_allowed`，安全 lifeboat）。

**consumer 不进仲裁**，只跟随 `effective`。

---

## 8. 切换时序

**降档 A→B（B 更深）**：
1. 仲裁算出新 `effective=B`；防抖窗口内不动。
2. **consumer suspend**：对所有 `min_powered_level` 浅于 B 的 consumer，按 priority 高→低调 `suspend()`。
3. **方案切换**：调 `A.exit(ctx)`（如有），再调 `B.enter(ctx)`——**B 的 `enter` 里做全部机制动作**（关蓝牙 / WiFi PS / 深睡）。
4. `on_change(A, B)`。

> DEEPSLEEP 的 `enter` 内部调 `tdl_power_enter_deepsleep()`：板级通过 `tdd_power` 声明的 GPIO 唤醒源被 arm，然后 CPU 断电（唤醒=重启，`enter` 不返回）。平台无 `tkl_wakeup` 时该调用返回 `OPRT_NOT_SUPPORTED`，方案退回「尽量睡」。

**升档（唤醒/活动）**：反序——先 `B.exit` → `A.enter`（恢复系统），再按 priority 低→高 `resume()` consumer。

> consumer 间依赖用 priority 表达（依赖某电源域的外设优先级低于该域→晚 suspend、早 resume）。suspend/resume 一律线程上下文，允许阻塞/I2C。

---

## 9. 平台覆盖矩阵

| 能力 | 接口 | T5AI | ESP32 | 缺失时 |
|------|------|------|-------|--------|
| CPU sleep / WiFi PS | lpmgr / `tal_wifi_*` | ✅ | ✅ | 预设方案退回裸 tal_wifi PS（mA 级） |
| WiFi ULP 深睡后端 | `ENABLE_WIFI_ULTRA_LOWPOWER`（lpmgr） | ✅ | — | 在线档到不了 µA（只 DTIM 保活） |
| 外设电源域 | `tdl_power_domain_set` | ✅(板级) | ✅(板级) | rail consumer 退化为空操作 |
| 电池/充电 | `tdl_power_battery/charger_*` | ✅(板级) | ✅(M5PM1) | 不做电池策略 |
| **深睡唤醒源** | `tkl_wakeup` / `tdl_power_enter_deepsleep` | ✅ | ❌ | DEEPSLEEP 方案退回最深在线档 |
| 蓝牙下电 | `tuya_ble_deinit`（`ENABLE_BT_SERVICE`） | ✅ | 视平台 | 无蓝牙则跳过 |

> **µA 的两个开关**（工程约束）：①开 `ENABLE_WIFI_ULTRA_LOWPOWER`（切 lpmgr 深睡后端，否则只 DTIM 保活=mA 级）；②运行时关蓝牙（T5 上 BT controller 开机由 CP 核默认上电、顶住深睡，须 `tuya_ble_deinit`）。前者由 `ENABLE_TUYA_PM select` 自动带上，后者写在预设方案里。

---

## 10. 附录 A：NOTE 便利贴 —— 一个配置样例（验证接口，非设计正文）

> 证明通用接口能表达「电池 + e-paper + 音频 + 按键唤醒」的形态。组件代码里**不出现** SSD2683 / P43 等具体值——它们只是本产品配置。

- **降级链**：用默认 4 档（或显式 `{ACTIVE, CEC_T20, ULP_ONLINE, DEEPSLEEP}`）。
- **产品策略**：`deepest_allowed = TUYA_PM_ULP_ONLINE`（日常常驻在线）；`charging_holds_active = TRUE`；`force_deepest_below_mv = 3300`（板子 v_critical，仅低电 lifeboat 才进链尾 DEEPSLEEP）；`power_dev_name = "power"`。
- **consumer**：音频（自定义 consumer，收尾 = stop 录音→close codec→PA mute→切 AUDIO 域）；屏（自定义 consumer，收尾 = 刷完最后一帧→断 DISPLAY 域，e-paper 留图）；SD（rail 便利构造器）。均 `min_powered_level=CEC_T20`。
- **hold-lock**：说话态 / 放音期间持 ACTIVE 锁；**link gate**：未连上 AP 时持 floor=ACTIVE 锁顶住，连上释放（保证只有关联后才降进 CEC_T20/ULP_ONLINE）。
- **唤醒源**：板级 `tdd_power` 声明 `{P43(说话键), active-low}`；lifeboat DEEPSLEEP 后按键唤醒（=重启）。
- **蓝牙**：无需 app 处理——预设 CEC_T20/ULP_ONLINE 方案的 `enter` 内部已关。

---

## 11. 决策记录（本轮聊定）

1. ✅ **定位收窄为「方案库 + 调度器」**：组件核心只调度（仲裁/降级/切换/consumer/电池），**不实现机制**；机制下放到方案的 `enter`/`exit`，直接调底层，无「动作原语」中间层。
2. ✅ **方案升级为一等对象**（`id + min_residency + enter/exit`）；预设方案是内置实例，自定义方案 `register` 进库，两者同构。`enum` 保留，仅作预设方案的稳定标识。
3. ✅ **两个开发者分离**：作者定义/注册方案（写 `enter`）；使用者组链 + consumer + lock（从不写 `enter`）。
4. ✅ **降级链 = 使用者给的有序 id 数组（B）**，注册单独接口、一般在 init 前；不设则用默认链。深浅锚在链中位置。
5. ✅ **关蓝牙写进预设 CEC_T20/ULP_ONLINE 方案**（WiFi PS 硬前提，共天线）；要保留蓝牙的产品注册自定义方案，组件本身不假设有蓝牙（`ENABLE_BT_SERVICE` gate）。
6. ✅ **删除**：`keep_network`、`keep_online`、`mech` 枚举、通用 `on_enter` 钩子、`tuya_pm_act_*` 原语层——均为冗余/实现泄漏/过度包装。
7. ✅ **深睡唤醒源归 tdl_power**：DEEPSLEEP 方案 `enter` 调 `tdl_power_enter_deepsleep()`，唤醒源由板级经 `tdd_power` 声明；组件不再有 wakesrc API。
8. ✅ **µA 两开关**：`ENABLE_TUYA_PM` select `ENABLE_WIFI_ULTRA_LOWPOWER`；关蓝牙在预设方案里。
9. ✅ **预设方案与调度器分文件**（`tuya_pm_schemes.c` vs `tuya_pm.c`，同一文件夹）：生态特定依赖（ble/lpmgr/wifi）全隔离进方案文件，调度器核心零生态头依赖、彻底通用。见 §4.1。
10. ✅ **`min_residency_ms` 保留**，参与降档判据（空闲 ≥ 此值才允许降进该方案）；v2 的 `exit_latency_ms` 未用、删除。
11. ✅ **自定义方案 id 由作者自指定**（`#define` 一个 `>= BUILTIN_MAX` 的值），`register` 做**去重校验**（冲突则拒绝）。不用 `register` 返回 id。
12. ✅ **init 即定型**：`register` / `chain` 必须 init 前调用，init 后方案库与链只读、不支持运行期热插拔（换取无锁实现）。
13. ✅ **默认不降级（A：链三合一）**：降级链同时是深浅序 + 降级路径 + 仲裁可见集；不组链 → 仅 `{ ACTIVE }`、不自动降级（安全默认）。`tuya_pm_chain_default()` 一行启用预设 4 档链。只有链中方案可被 request / lock / consumer / deepest_allowed 引用，引用链外方案报错。

### 遗留待细化（进实现前再定）
- 各方案 `min_residency_ms` 的默认值标定（数值，非机制）。
- ULP_ONLINE 与 MQTT keepalive 的协同（DTIM 睡眠窗口与心跳联动，避免睡眠期心跳撞死 socket；Matter ICD check-in 思路）——**留到后面单独讨论**。

---

## 12. 与 v2 / 当前实现的关系

**相对 v2 的核心变化**：v2 是「固定 4 档 + 每档 cfg 数据 + 三个机制后端（组件内 `__mech_set` 统一实现机制）」；v3 把「档」升级为「方案对象」，机制从组件核心下放进方案实现，组件退化为纯调度器，并引入方案库 / 自定义方案注册 / 使用者组链。

**实现状态：v3 已落地（2026-08-05，NOTE 构建通过）。** `src/tuya_pm/` 现为：`tuya_pm.c`（纯调度器，chain-index 仲裁/decay/consumer/lock/battery + register/chain/chain_default）+ `tuya_pm_schemes.c`（4 预设方案，`enter` 直调 lpmgr/tal_wifi/tuya_ble_deinit/tdl_power，含关蓝牙）+ `tuya_pm_scheme.h`（内部契约）。两个消费者（`examples/system/tuya_pm`、`apps/tuya_cloud/tuya_pm_ulp_demo`）已适配 v3 并构建通过。v2 备份在仓库根 `tuya_pm_v2_backup.tar.gz`。深浅比较全部锚在链中位置；`min_residency` 由方案携带；机制全在方案 `enter` 里，调度器核心零生态头依赖。

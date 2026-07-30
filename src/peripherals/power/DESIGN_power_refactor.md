# 电源管理抽象组件重构设计（`pmic` → `power`）

> 状态：设计评审中（未动代码）
> 适用板：`TUYA_T5AI_POCKET`、`TUYA_T5AI_EINK_NFC`、`ZECTRIX_T5AI_NOTE_4`、`WAVESHARE_T5AI_TOUCH_AMOLED_1_75`（T5AI 下带电池供电的板子）

---

## 1. 背景与现状

`src/peripherals/pmic` 目前只有一个裸芯片驱动 `axp2101/`，没有仓库通用的 TDD/TDL 分层。于是三块电池板各自在板级手搓电源管理，形成了两套互不相干的实现：

| 关注点 | POCKET | EINK_NFC | NOTE_4 | WAVESHARE_AMOLED |
|---|---|---|---|---|
| 电源域开关 | AXP2101 的 LDO/DCDC 通道（ALDO3=CAM…） | 2 路 GPIO 负载开关（EINK/SD） | 3 路 GPIO 负载开关（EPD/SD/AUDIO） | 无（外置电源，暂无电源域开关） |
| 电量 | `axp2101_getBatteryPercent()` | ADC 分压 + 线性百分比 | ADC 分压（两点标定）+ 线性百分比 | ADC 分压 + 11 点查找表 |
| 充电状态 | `axp2101_isCharging()` | 1 根 GPIO（2 态：插/拔） | 2 根 GPIO（3 态：充/满/拔）+ IRQ | 1 根 GPIO（2 态：插/拔） |
| 应用怎么拿 | app UI **直接调 `axp2101_*`** | `board_charge_detect_*` | `board_charge_detect_*` | **硬件逻辑焊死在 app（`app_battery.c`）里** |

相关文件：
- 芯片驱动：`src/peripherals/pmic/axp2101/`
- POCKET：`boards/T5AI/TUYA_T5AI_POCKET/board_axp2101_api.{c,h}`
- EINK：`boards/T5AI/TUYA_T5AI_EINK_NFC/board_power_domain_api.{c,h}`、`board_charge_detect_api.{c,h}`
- NOTE4：`boards/T5AI/ZECTRIX_T5AI_NOTE_4/board_power_domain_api.{c,h}`、`board_charge_detect_api.{c,h}`
- WAVESHARE：板级只有零散宏 `boards/T5AI/WAVESHARE_T5AI_TOUCH_AMOLED_1_75/board_com_api.h`（`EXAMPLE_BAT_CHARGE_PIN=P30`、`EXAMPLE_BAT_ADC_PIN=P13`、`ADC_CHANNEL=15`、`ADC_Ratio_Voltage=2.51/0.51≈4.922`）；实际硬件逻辑在 app `apps/tuya.ai/your_chat_bot/src/battery/app_battery.{c,h}`
- 直调芯片的 app：`apps/tuya_t5_pocket/tuya_t5_pocket_ai/src/display/ui/main_screen.c:1223-1224`

## 2. 问题

1. **芯片驱动漏到应用层。** POCKET app UI 直接调 `axp2101_getBatteryPercent()` / `axp2101_isCharging()`。换芯片、换板子就得改 app。
2. **没有公共契约。** EINK 和 NOTE4 函数名相近，但枚举语义不同（2 域 vs 3 域、2 态 vs 3 态）且都是 per-board 定义；POCKET 又是另一套 API。三块板写不出可移植的上层代码。
3. **GPIO+ADC 那套大量复制粘贴。** EINK / NOTE4 / WAVESHARE 的分压换算、百分比曲线、ADC 采样平均 ~90% 相同，只有引脚/分压比/标定不同。应由板级配置驱动的共享驱动承担，而非每块板抄一遍。
4. **WAVESHARE 是最严重的反面教材（本次一并修）：**
   - 硬件细节（ADC 引脚、分压比、充电极性、电压曲线）**焊死在共享 app `app_battery.c` 里**，而非 board 层。
   - **两个真值源互相打架**：board 的 `ADC_Ratio_Voltage≈4.922` vs app 里写死的分压 `×4`（注释"2M/510K"但数字错）。
   - 用了 **T5 上有 bug 的 `tkl_adc_read_voltage()`**（读错寄存器，NOTE4 已记录），电量读数不可靠。
   - 充电检测用 **1.5s 轮询定时器**而非 GPIO 中断。
   - `app_battery_get_status()` 目前**无调用者**（电池 UI 图片已存在但未接线）→ WIP，重构风险低。

## 3. 设计原则

- **驱动只做机制（mechanism），不背策略（policy）。** 组件只暴露"这个电源模块能开/能关""电压是多少""充电状态是什么"。像"低电量→关机/降频""进休眠前关哪些电源域""待机切档"这类**策略属于 app / 上层**，不进本组件。
- **对齐仓库既有 TDD/TDL 范式。** `src/peripherals/` 下 button/display/tp/camera/led/audio/ir 全部是 TDD/TDL 两层，本组件补齐同样的分层，而非另起炉灶。
- **一个组件 = 一个设备 = 一个 `find`。** 与 led/button 一致：`power` 是**一个电源设备**，只有一个 `tdl_power_find()`。电源域(power_domain) / 电量(battery) / 充电(charger) 是这个设备的**三类能力（操作组）**，不是三个设备、不是三个 find。某板缺某类能力 → 对应操作返回 `OPRT_NOT_SUPPORTED`。
- **物理映射藏在板级注册里。** app 说"开摄像头电"，不关心背后是 PMIC 的 LDO 还是一颗 MOSFET。

## 4. TDD/TDL 分层（参照 button/led 现有写法）

- `tdl_power_driver` 接口：定义能力 ops 表（`TDL_POWER_INTFS_T`，含 power_domain/battery/charger 三组函数指针）+ `tdl_power_register(name, ctrl, ctx)`，供 tdd 自注册**一个** power 设备。
- `tdl_power_manage` 接口：app 用，`tdl_power_find(name) → handle`，之后所有操作都作用在这**一个 handle** 上。
- `tdd_power_yyy` 后端：一块板一个后端，定义板级 CFG（把 power_domain 表 + battery + charger 三类能力聚在一起）+ `tdd_power_yyy_register(name, cfg)`，内部填 ops 表调 `tdl_power_register`。
- 后端未提供的能力（对应 ops 为 NULL）返回 `OPRT_NOT_SUPPORTED`（沿用 GPIO LED breath 的做法，用于"无电池板查电量""无充电检测板查充电态"等）。

## 5. 目录结构

```
src/peripherals/power/
  Kconfig  CMakeLists.txt
  tdl_power/
    include/  tdl_power_manage.h    # 【面向应用开发者】find + 三组操作(domain/battery/charger)
              tdl_power_driver.h    # 【面向板级注册开发者】TDL_POWER_INTFS_T + tdl_power_register
              tdl_power_types.h     # 两边共用的契约类型(handle / 域角色枚举 / 充电三态 / INFO 结构)
    src/      tdl_power.c           # 设备注册表 + 句柄管理 + 三组操作分发(掩码拆分/percent 派生等)
  tdd_power/
    include/  tdd_power_soc.h      # SoC 后端(片内 GPIO+ADC)：GPIO 负载开关电源域 + GPIO 充电检测 + ADC 电量(含 tdl_power_driver.h)
              tdd_power_axp2101.h   # AXP2101 后端：一颗芯片提供三类能力(含 tdl_power_driver.h)
    src/      tdd_power_soc.c  tdd_power_axp2101.c

src/peripherals/pmic/         # 保留：axp2101 芯片驱动层（POCKET + ESP32 共用），不并入 power
  axp2101/                    # 裸芯片驱动；tdd_power_axp2101.c 在其上做薄适配
```

## 6. 接口设计

### 6.1 应用接口（`tdl_power_manage.h`）—— 一个 find，三组操作

> 契约类型（`TDL_POWER_HANDLE` / `TDL_POWER_DOMAIN_E` / `TDL_CHG_STATE_E` / `TDL_CHG_EVENT_CB` / `TDL_POWER_BATTERY_INFO_T` / `TDL_POWER_INFO_T`）全部定义在 `tdl_power_types.h`，manage 与 driver 两个头都 include 它。下面代码按逻辑归组展示，类型定义处已标注归属。

```c
/* 一个设备,一个 find,一个 handle */
typedef void *TDL_POWER_HANDLE;
TDL_POWER_HANDLE tdl_power_find(const char *name);   /* 全板就这一个，约定名 "power" */

/* —— 跨板契约：电源域"语义角色"枚举（app 只认这个，不认板级物理名）——
   通用层只含"角色"，不含 EINK_3V3/EPD_3V3/CAM_2V8 这类板级名；映射在板级注册里做。
   角色不够时在此追加一个 bit 即可（纯跨板角色，不设板级私有区）。 */
typedef enum {                              /* 位域：每个角色一个 bit，可 OR 成掩码 */
    TDL_PWR_DOMAIN_DISPLAY     = 1u << 0,   // 屏供电   (EINK_3V3 / EPD_3V3)
    TDL_PWR_DOMAIN_SD          = 1u << 1,   // SD 卡供电
    TDL_PWR_DOMAIN_AUDIO       = 1u << 2,   // 功放/音频
    TDL_PWR_DOMAIN_CAMERA      = 1u << 3,   // 摄像头主供电
    TDL_PWR_DOMAIN_CAMERA_AVDD = 1u << 4,
    TDL_PWR_DOMAIN_CAMERA_DVDD = 1u << 5,
    TDL_PWR_DOMAIN_RTC         = 1u << 6,
    TDL_PWR_DOMAIN_JOYSTICK    = 1u << 7,
    // 纯跨板角色契约；uint32_t 上限 32 个域。不设"板级私有区"：
    // 真·板级独有的域要么升成正式角色加在这里，要么留给板级代码自己驱动、不进本抽象。
} TDL_POWER_DOMAIN_E;

/* —— 能力一：电源域 power_domain（只开/关，电压在注册时 default_mv 定死）——
   set 收位掩码,可一次开/关多路(供休眠等批量场景);get 收单个域。
   tdl 拿到掩码后逐位下发到后端单域 op;掩码里某位该板没有 → 跳过降级。 */
OPERATE_RET tdl_power_domain_set(TDL_POWER_HANDLE h, uint32_t domain_mask, BOOL_T on);
OPERATE_RET tdl_power_domain_get(TDL_POWER_HANDLE h, TDL_POWER_DOMAIN_E domain, BOOL_T *on);
/* 例：休眠前一次关掉屏+卡+音频 */
/*   tdl_power_domain_set(h, TDL_PWR_DOMAIN_DISPLAY | TDL_PWR_DOMAIN_SD | TDL_PWR_DOMAIN_AUDIO, FALSE); */

/* —— 能力二：电量 battery —— */
OPERATE_RET tdl_power_battery_get_voltage(TDL_POWER_HANDLE h, uint32_t *mv);   /* 纯机制 */
OPERATE_RET tdl_power_battery_get_percent(TDL_POWER_HANDLE h, uint8_t *pct);   /* helper，可选 */

/* ↓↓↓ 以下两个 INFO 结构定义在 tdl_power_types.h ↓↓↓ */
/* 板级声明的电池特性（静态事实/推荐值,不是策略）：v_full/v_empty 兼作百分比换算端点，
   v_low/v_critical 为板级推荐阈值(0=未指定);curve 为可选电压→%查找表(NULL→线性)。
   tdl 靠这些把电压派生成百分比 —— 所以无硬件电量计的板不必注册 battery_get_percent。 */
typedef struct {
    uint16_t v_full_mv;      // 满电 (100%)
    uint16_t v_empty_mv;     // 空电 (0%)
    uint16_t v_low_mv;       // 低电告警推荐阈值
    uint16_t v_critical_mv;  // 关机保护推荐阈值
    const int32_t *curve; uint8_t curve_cnt;  // 可选 电压→% 表（如 WAVESHARE 11 点）；NULL→线性
} TDL_POWER_BATTERY_INFO_T;

/* —— POWER 设备级静态硬件参数（不是运行时 op）——
   注册时由 tdd 作为数据传给 tdl,tdl 存着,app 用 tdl_power_get_info 一次读回。
   目前含电池地标,且"可能不止 battery"——以后按需在此扩展(只加字段,不动已有)。 */
typedef struct {
    TDL_POWER_BATTERY_INFO_T battery;   // 电池电压地标
    // 预留：未来其它 POWER 设备静态参数……
} TDL_POWER_INFO_T;
/* ↑↑↑ 以上两个 INFO 结构定义在 tdl_power_types.h ↑↑↑ */

OPERATE_RET tdl_power_get_info(TDL_POWER_HANDLE h, TDL_POWER_INFO_T *info);  /* manage.h */

/* —— 能力三：充电 charger（公共枚举三态）—— */
typedef enum { TDL_CHG_DISCHARGE=0, TDL_CHG_CHARGING, TDL_CHG_FULL } TDL_CHG_STATE_E;
typedef void (*TDL_CHG_EVENT_CB)(TDL_CHG_STATE_E st, void *arg);
OPERATE_RET tdl_power_charger_get_state(TDL_POWER_HANDLE h, TDL_CHG_STATE_E *st);
OPERATE_RET tdl_power_charger_on_event(TDL_POWER_HANDLE h, TDL_CHG_EVENT_CB cb, void *arg);
```

**电源域按语义角色寻址**：跨板 app 只用 `TDL_PWR_DOMAIN_*` 角色，编译期安全、到处能编过；某板没有该角色（那条注册没写）→ 调用返回 `OPRT_NOT_SUPPORTED`，天然降级。EINK 的 `EINK_3V3` 与 NOTE4 的 `EPD_3V3` 都映射到 `TDL_PWR_DOMAIN_DISPLAY`，app 代码一字不改即通用。

**充电枚举定三态** `DISCHARGE/CHARGING/FULL`（取 NOTE4 的最全模型作为公共契约）。只有 2 态的板（EINK/POCKET/WAVESHARE）永不上报 `FULL`，app 统一按三态写、向前兼容更好。

### 6.2 驱动接口（`tdl_power_driver.h`）—— 一张能力 ops 表

tdd 填一张 `TDL_POWER_INTFS_T`（三组能力的函数指针，缺哪组填 NULL），注册**一个** power 设备：

```c
typedef struct {
    /* power_domain：ctx + 角色 → 由后端映射表解析到具体通道/引脚 */
    OPERATE_RET (*domain_set)(void *ctx, TDL_POWER_DOMAIN_E domain, BOOL_T on);
    OPERATE_RET (*domain_get)(void *ctx, TDL_POWER_DOMAIN_E domain, BOOL_T *on);
    /* battery */
    OPERATE_RET (*battery_get_voltage)(void *ctx, uint32_t *mv);   /* 根事实；有电池就应提供，NULL→NOT_SUPPORTED */
    OPERATE_RET (*battery_get_percent)(void *ctx, uint8_t *pct);   /* 仅硬件电量计时提供；NULL→tdl 从 voltage+info.battery 派生 */
    /* charger */
    OPERATE_RET (*charger_get_state)(void *ctx, TDL_CHG_STATE_E *st);          /* NULL→NOT_SUPPORTED */
    OPERATE_RET (*charger_arm_event)(void *ctx);        /* 只武装硬件中断 */
} TDL_POWER_INTFS_T;

/* 充电事件的线程模型：后端 arm_event 只设置硬件中断;中断触发时后端 ISR 调
   tdl_power_charger_irq_notify(ctx)——传后端自己的 ctx(驱动层令牌,非 app 句柄),
   TDL 据此定位是哪个设备(支持多设备),给该设备置 pending 并唤醒。TDL 持有唯一 worker
   线程,只处理置位的设备:调 charger_get_state 读状态再回调 app——状态读(含 AXP 的 I2C)
   和用户回调都在线程上下文,绝不在硬 ISR 里。SoC/AXP 共用这套。 */
void tdl_power_charger_irq_notify(void *ctx);  /* driver.h;后端 ISR 传自己的 ctx */

/* 注册一个 power 设备：intfs=运行时 ops，info=静态硬件参数（数据，非 op），ctx=后端私有 */
OPERATE_RET tdl_power_register(const char *name, const TDL_POWER_INTFS_T *intfs,
                               const TDL_POWER_INFO_T *info, void *ctx);
```

### 6.3 后端 CFG —— 每块板一个后端、一次注册

后端结构**一级按机制分类**（GPIO / ADC / AXP=I2C），命名统一 `TDD_POWER_<机制>_<能力>_T`；电源域描述项用 `role` 字段把**角色→物理**绑定（tdl 按角色索引，O(1)，无 strcmp）。

```c
/* tdd_power_soc.h ── 用 SoC 自带外设(GPIO+ADC)实现，EINK/NOTE4/WAVESHARE 共用 */

/* —— GPIO 机制 —— */
typedef struct { TDL_POWER_DOMAIN_E role; TUYA_GPIO_NUM_E pin; TUYA_GPIO_LEVEL_E active_level; BOOL_T default_on; }
    TDD_POWER_GPIO_DOMAIN_T;          /* 负载开关电源域：角色 → 引脚 */
typedef struct {
    TUYA_GPIO_NUM_E chrg_pin;  TUYA_GPIO_LEVEL_E chrg_active;   /* 充电中 */
    TUYA_GPIO_NUM_E stdby_pin; TUYA_GPIO_LEVEL_E stdby_active;  /* 充满；不用填 0xFF → 永不 FULL */
} TDD_POWER_GPIO_CHARGER_T;           /* 充电状态线检测 */

/* —— ADC 机制 —— */
typedef struct {                      /* 分压电量采集（电压地标/曲线见设备级 info.battery） */
    TUYA_ADC_NUM_E adc_num; uint8_t adc_ch; float divider_ratio;
    uint16_t cal_low, cal_span;       /* 两点标定；NOTE4 用，其它板留 0 走默认 */
    uint8_t samples;
} TDD_POWER_ADC_BATTERY_T;

/* —— 板级聚合：这块板用了哪些机制（不用的能力填 NULL/0）—— */
typedef struct {
    const TDD_POWER_GPIO_DOMAIN_T  *domains; uint8_t domain_cnt; /* NULL/0 = 无可控电源域（WAVESHARE） */
    const TDD_POWER_ADC_BATTERY_T  *battery;                     /* NULL = 无电池 */
    const TDD_POWER_GPIO_CHARGER_T *charger;                     /* NULL = 无充电检测 */
    TDL_POWER_INFO_T                info;                        /* 设备级静态参数：电池地标等 */
} TDD_POWER_SOC_CFG_T;
OPERATE_RET tdd_power_soc_register(const char *name, const TDD_POWER_SOC_CFG_T *cfg);
/* SoC 后端只实现 battery_get_voltage;percent 交给 tdl 从 voltage + info.battery 派生。 */

/* tdd_power_axp2101.h ── 外置 PMIC 的薄适配层（芯片 bring-up 由板级自己做） */
typedef struct { TDL_POWER_DOMAIN_E role; XPowersPowerChannel_t channel; }
    TDD_POWER_AXP_DOMAIN_T;           /* 角色 → AXP 通道（仅映射） */
typedef struct {
    const TDD_POWER_AXP_DOMAIN_T *domains; uint8_t domain_cnt;
    TDL_POWER_INFO_T info;             /* battery/charger 由芯片自带，只需静态地标 */
} TDD_POWER_AXP2101_CFG_T;
OPERATE_RET tdd_power_axp2101_register(const char *name, const TDD_POWER_AXP2101_CFG_T *cfg);
/* AXP 后端是"薄 ops 适配"：芯片 init / 通道电压 / 充电参数 等 bring-up 有大量 POCKET
   专属配置，由板级(board_axp2101_init)完成；后端只把 tdl 接口架在已配好的芯片上——
   domain 走通道 enable/disable，battery 走芯片电量计,charger 由 isCharging/isVbusIn 推断。
   因此不接管 init、也没有 default_mv/i2c 这类字段(避免收了不用的死参数)。 */
```

## 7. 板级注册（每块板一次注册）

`board_register_hardware()` 里，每块板声明一张 const 配置、调一次注册，注册名统一 `"power"`：

```c
/* POCKET：board_axp2101_init() 先做芯片 bring-up，再把 AXP 映射成 power 设备（角色→通道） */
static const TDD_POWER_AXP_DOMAIN_T pocket_domains[] = {
    {TDL_PWR_DOMAIN_CAMERA,      XPOWERS_ALDO3},  // CAM_2V8
    {TDL_PWR_DOMAIN_SD,          XPOWERS_ALDO4},  // SD_3V3
    {TDL_PWR_DOMAIN_CAMERA_AVDD, XPOWERS_BLDO1},
    {TDL_PWR_DOMAIN_CAMERA_DVDD, XPOWERS_BLDO2},
    {TDL_PWR_DOMAIN_JOYSTICK,    XPOWERS_DLDO2},
    {TDL_PWR_DOMAIN_RTC,         XPOWERS_LDO1 },
};
tdd_power_axp2101_register("power", &(TDD_POWER_AXP2101_CFG_T){
    .domains = pocket_domains, .domain_cnt = CNTSOF(pocket_domains),
    .info = {.battery = {.v_full_mv=4200, .v_empty_mv=3000, .v_low_mv=3400, .v_critical_mv=3300}}});  /* 阈值示例 */

/* EINK：GPIO+ADC 后端，三类能力聚在一个 cfg（角色 → 引脚） */
static const TDD_POWER_GPIO_DOMAIN_T eink_domains[] = {
    {TDL_PWR_DOMAIN_DISPLAY, GPIO_30, LEVEL_HIGH, TRUE},   // EINK_3V3
    {TDL_PWR_DOMAIN_SD,      GPIO_31, LEVEL_HIGH, TRUE},
};
static const TDD_POWER_ADC_BATTERY_T eink_batt = {.adc_num=ADC_0, .adc_ch=8, .divider_ratio=4.922f, .samples=16};
static const TDD_POWER_GPIO_CHARGER_T eink_chg = {GPIO_12, LEVEL_LOW, 0xFF, 0};  /* 单线，永不 FULL */
tdd_power_soc_register("power", &(TDD_POWER_SOC_CFG_T){
    .domains = eink_domains, .domain_cnt = 2, .battery = &eink_batt, .charger = &eink_chg,
    .info = {.battery = {.v_full_mv=4200, .v_empty_mv=3000, .v_low_mv=3300, .v_critical_mv=3100}}});  /* 阈值示例 */

/* NOTE4：3 域 + 两点标定电量 + 双线 3 态充电（EPD 同样映射到 DISPLAY 角色） */
static const TDD_POWER_GPIO_DOMAIN_T note4_domains[] = {
    {TDL_PWR_DOMAIN_DISPLAY, GPIO_23, LEVEL_HIGH, TRUE},  // EPD_3V3
    {TDL_PWR_DOMAIN_SD,      GPIO_8,  LEVEL_HIGH, TRUE},
    {TDL_PWR_DOMAIN_AUDIO,   GPIO_42, LEVEL_HIGH, TRUE},
};
static const TDD_POWER_ADC_BATTERY_T note4_batt = {
    .adc_num=ADC_0, .adc_ch=1, .divider_ratio=4.09f, .cal_low=2469, .cal_span=2429, .samples=16};
static const TDD_POWER_GPIO_CHARGER_T note4_chg = {GPIO_21, LEVEL_LOW, GPIO_22, LEVEL_HIGH}; /* CHRG+STDBY */
tdd_power_soc_register("power", &(TDD_POWER_SOC_CFG_T){
    .domains = note4_domains, .domain_cnt = 3, .battery = &note4_batt, .charger = &note4_chg,
    .info = {.battery = {.v_full_mv=4200, .v_empty_mv=3000, .v_low_mv=3300, .v_critical_mv=3100}}});  /* 阈值示例 */

/* WAVESHARE_AMOLED：无可控电源域，只有电量 + 充电 */
static const int32_t ws_curve[] = {2800,3100,3280,3440,3570,3680,3780,3880,3980,4090,4200};
static const TDD_POWER_ADC_BATTERY_T ws_batt = {.adc_num=ADC_0, .adc_ch=15, .divider_ratio=4.922f, .samples=16};
static const TDD_POWER_GPIO_CHARGER_T ws_chg = {GPIO_30, LEVEL_LOW, 0xFF, 0};
tdd_power_soc_register("power", &(TDD_POWER_SOC_CFG_T){
    .domains = NULL, .domain_cnt = 0, .battery = &ws_batt, .charger = &ws_chg,
    .info = {.battery = {.v_full_mv=4200, .v_empty_mv=2800, .v_low_mv=3300, .v_critical_mv=3100,
                         .curve=ws_curve, .curve_cnt=11}}});  /* 阈值示例;曲线归设备级 info */
```

原本每块板的 `board_power_domain_api.*` / `board_charge_detect_api.*`（共 6 个文件、大量重复）+ WAVESHARE app 里的 `app_battery.c` 硬件逻辑 → 全部收敛成**每板一张 const + 一次注册**，机制实现共享到 `tdd_power_soc.c` / `tdd_power_axp2101.c`。

## 8. app 迁移（阶段 2 收尾）

`apps/.../main_screen.c:1223-1224`：

```c
uint8_t battery_percent; tdl_power_battery_get_percent(s_pwr, &battery_percent);
TDL_CHG_STATE_E cs; tdl_power_charger_get_state(s_pwr, &cs);
current_battery_charging = (cs == TDL_CHG_CHARGING);
```

`s_pwr` 在 app 初始化时 `tdl_power_find("power")` 一次取得（一个 handle）。`axp2101_*` 不再出现在 app。

**低电/关机保护（阈值全从板级来，app 只做策略）** —— 跨板一份代码，零硬编码阈值：

```c
TDL_POWER_INFO_T info; tdl_power_get_info(s_pwr, &info);  /* 板级声明的设备信息（含电池地标） */
uint32_t mv; tdl_power_battery_get_voltage(s_pwr, &mv);
if (info.battery.v_critical_mv && mv < info.battery.v_critical_mv) { /* app 决定：存档并关机 */ }
else if (info.battery.v_low_mv && mv < info.battery.v_low_mv)      { /* app 决定：弹低电告警 */ }
```

### 8.1 WAVESHARE `app_battery.c` 掏空（本次一并改）

`apps/tuya.ai/your_chat_bot/src/battery/app_battery.c` 保留其 **app 策略**部分（`GET_BATTERY_TIME_MS` 轮询节奏、后续 DP 上报、UI 更新、`app_battery_get_status()` 对外接口不变），**删除全部硬件细节**：

- 删 ADC 初始化 / `tkl_adc_read_voltage()`（bug API）/ `sg_adc_cfg` / 分压 `×4` / `bvc_map`（曲线移到板级注册）/ 引脚宏。
- 删 `__battery_charge_pin_init` 及 1.5s `sg_charge_check_timer` 轮询。
- `app_battery_init()` → `s_pwr = tdl_power_find("power"); tdl_power_charger_on_event(s_pwr, on_charge_evt, NULL);` + 保留 5 分钟电量轮询定时器。
- `__battery_status_process()` → `tdl_power_battery_get_percent(s_pwr, &sg_battery_percentage);`
- 充电状态从事件回调更新 `sg_is_charging`，替掉轮询。

结果：app 不再碰任何 ADC/GPIO；`4.922 vs ×4` 矛盾消除；ADC bug 随共享 tdd 修复。board 的 `EXAMPLE_BAT_*` / `ADC_*` 宏收进 `board_register_hardware` 的注册参数，board_com_api.h 里可清理。

## 9. Kconfig / CMake

- Kconfig 只有一个使能开关 `config ENABLE_POWER`（板级 `select ENABLE_POWER`），不做 backend 子选项。后端 `.c` 的取舍在 CMake 里：`tdd_power_soc.c` 随 `ENABLE_POWER` 直接编；`tdd_power_axp2101.c`（阶段 2）按 AXP 芯片驱动是否可用来 gate。
- **`pmic` 组件保留、不合并、不改名**：`axp2101` 芯片驱动被 POCKET 与 ESP32 `WAVESHARE_ESP32S3_Touch_AMOLED_1.8`（直接用裸驱动，不走本抽象）共用，是独立的"芯片驱动层"。`power`（抽象层）依赖它：AXP 后端 `tdd_power_axp2101.c` 在 `CONFIG_ENABLE_PMIC` 下编译、include `../pmic/axp2101/include`。POCKET 同时 `select ENABLE_POWER` + `ENABLE_PMIC` + `ENABLE_PMIC_AXP2101`。

## 10. 落地顺序（分阶段）

1. **阶段 1**：建组件骨架（`tdl_power` 一个设备 + `TDL_POWER_INTFS_T` 三组能力）+ 两个后端；先做 **power_domain** 能力，EINK/NOTE4 电源域切过来验证；**直接删** `board_power_domain_api.*`、board `.c` 一次性改到位（不做转发层）。（WAVESHARE 无电源域，本阶段不涉及。）
2. **阶段 2**：补齐 battery + charger 两组能力；四块板全部注册（含 WAVESHARE）；迁移 POCKET app、掏空 WAVESHARE `app_battery.c`（顺带修 ADC bug + 分压矛盾）；删除重复的 `board_charge_detect`。
3. **阶段 3**：深睡 / 低功耗策略——**明确不进本组件**，以后单独议。

## 11. 决策点（已全部拍板）

1. **旧 `board_*_api` 头 —（已定）直接删**：不做转发层，`board_power_domain_api.*` / `board_charge_detect_api.*` 一律删除，board `.c` 一次性改到 `tdl_power_*`。
2. **句柄风格 —（已定）一设备一 find**：`power` 是一个设备、一个 `tdl_power_find("power")`，power_domain/battery/charger 是它的三组操作。与 led/button "一组件一 find" 一致。
3. **改名/合并 —（已定，实施中反转）不做**：原计划 `pmic`→`power` 合并 + `ENABLE_PMIC`→`ENABLE_POWER` 改名。落地时发现 `axp2101` 芯片驱动还被 ESP32 `WAVESHARE_ESP32S3_Touch_AMOLED_1.8` 直接使用，合并/改名会波及不走本抽象的板。故**保留 `pmic` 作为独立芯片驱动层**，`power` 抽象层依赖之；组件名仍是 `power`（新建），芯片驱动名仍是 `pmic`。
4. **百分比模型 —（已定）维持现状**：无电量计的板由 tdl 从 `voltage + info.battery` 派生（`curve` 有则查表、无则线性）；WAVESHARE 用 11 点曲线，EINK/NOTE4 走线性，不强行统一（曲线是否补齐留给硬件同学按实测决定）。

---

*本文件为设计评审稿，评审通过后据此进入阶段 1 编码。*

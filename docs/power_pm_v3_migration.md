# tuya_pm v2 → v3 迁移计划

> 配套设计：`docs/power_level_management_design.md`（v3）
> 目标：把现有 `src/tuya_pm/`（v2：固定 `TUYA_PM_LEVEL_E` + `levels[]` 数据驱动 + 组件内 `__mech_set` 统一机制）迁到 v3（方案库 + 纯调度器 + 预设方案分文件）。

## 原则

- **每步独立提交、每步 `tos build ZECTRIX_T5AI_NOTE_4` 通过**；两个消费者（`examples/lowpower/tuya_pm`、`apps/tuya_cloud/tuya_pm_ulp_demo`）每步都能编。
- **内部重构先行、行为等价优先、公共 API 最后切**：Step 1–2 不动公共 API（消费者零改动），Step 3 一次性切 API + 消费者跟进，中间不留「编不过」的状态。
- 改 Kconfig 符号后记得 `rm -rf .build`（见 [t5ai-tos-build-gotchas]）。

## 现状快照（迁移基线）

- `tuya_pm.c` 817 行：`__mech_init`/`__mech_set`（`#if ENABLE_WIFI_ULTRA_LOWPOWER` 双后端）、`__apply`→`__mech_set`+`__enter_deepsleep`、`__arbitrate_locked`（enum 数值比大小）、`__recompute_apply`（切档+consumer）、lock/consumer/battery/init。
- 公共类型：`TUYA_PM_LEVEL_E`{ACTIVE/CEC_T20/ULP_ONLINE/DEEPSLEEP/MAX}、`TUYA_PM_LEVEL_CFG_T`{dtim,min_residency_ms,exit_latency_ms}、`TUYA_PM_POLICY_T`{deepest_allowed,levels,level_cnt,decay_debounce_ms,battery}。
- 消费者：example `policy.levels=NULL`；ulp demo `policy.levels=s_levels`（**定制了 ULP_ONLINE dtim=10/residency=10000**）。两者 consumer/lock 参数都用 enum（迁移后兼容 `uint8_t`）。
- 关蓝牙目前在 example / ulp demo 里由 app 调 `tuya_ble_deinit`（v3 要移进预设方案）。
- 深睡前有 DEBUG 块（`PR_NOTICE`+`tal_system_sleep(20)`，标了 remove before release）。

---

## Step 1：抽出预设方案文件（纯内部重构，公共 API 不变，行为等价）

**目标**：把 `__mech_set`/`__enter_deepsleep` 的机制拆成 4 个预设方案实例的 `enter`/`exit`，放进新文件；调度器改为「切档时调 `enter`」。公共 API（`LEVEL` enum、init、request…）完全不变，消费者不改。

**动作**：
1. 新增 `src/tuya_pm/src/tuya_pm_scheme.h`（内部头）：定义 `TUYA_PM_SCHEME_T`{id, min_residency_ms, enter, exit, ctx}；声明 `const TUYA_PM_SCHEME_T *tuya_pm_builtin_table(uint8_t *cnt)` 与 `void tuya_pm_schemes_init(uint8_t deepest_allowed)`（承接 `__mech_init` 的全局初始化：`tal_cpu_set_lp_mode` / `tuya_wifi_ulp_init`+`lpmgr_register`）。
2. 新增 `src/tuya_pm/src/tuya_pm_schemes.c`：4 个 `enter` 函数，**逐字搬** `__mech_set` 各分支 + `__enter_deepsleep`——
   - `__active_enter` = ACTIVE 分支；`__standby_enter`/`__dormant_enter` = 非 ACTIVE 分支，**dtim 硬编默认（CEC_T20=1、ULP_ONLINE=10）**（取代 `s_levels[].dtim`）；`__deepsleep_enter` = `__enter_deepsleep` 体。
   - **本步先不加关蓝牙**（保持等价，关蓝牙留到 Step 3 从 app 移入），生态 include（`lpmgr`/`tal_wifi`/`tdl_power_manage`）move 到此文件。
3. `tuya_pm.c`：删 `__mech_init`/`__mech_set`/`__enter_deepsleep`；`__apply(lvl)` 改为查表调 `table[lvl].enter(ctx)`（DEEPSLEEP 的 `enter` 内部自己走 tdl_power，不再在 `__apply` 里特判）；`init` 里把 `__mech_init()` 换成 `tuya_pm_schemes_init(deepest_allowed)`；移除已下沉的 include。

**验证**：NOTE build；行为与迁移前逐档一致（dtim 1/10、CPU 睡眠、深睡路径不变）。

**风险**：`min_residency_ms` 本步仍从 `s_levels[]` 读（降级判据不动）；只搬「进档动作」。DEEPSLEEP `enter` 不返回的语义要保住。

---

## Step 2：方案库 + 降级链 + register/chain（新增 API，默认行为等价）

**目标**：调度器维护「方案库（预设 4 + 注册的）+ 降级链（默认=预设顺序）」；新增 `register`/`chain`；深浅比较从「enum 数值」改为「链中位置」（默认链下 index==enum，等价）。

**动作**：
1. `tuya_pm.c` 内部：方案库数组（预设来自 `tuya_pm_builtin_table()`，`register` 往后追加，去重校验 id）；降级链 `s_chain[]`（**默认仅 `{ACTIVE}` = 不降级**；`tuya_pm_chain_default()` 填预设 4 档）。
2. 深浅比较统一走 `__depth_of(scheme_id)` = 该 id 在 `s_chain` 里的 index（不在链里 → 视为最浅/报错）。`__arbitrate_locked`、lock floor、consumer min、`deepest_allowed` 全改用 `__depth_of` 比较。默认链下与现状等价。
3. 新增公共 API（Step 3 才收敛类型，这里先加）：`tuya_pm_scheme_register`（init 前、去重）、`tuya_pm_chain(ids,n)` / `tuya_pm_chain_default()`（init 前、可选）。**未组链 → 链仅 `{ACTIVE}`、不自动降级**；init 后置只读标志（定型）。

**验证**：NOTE build；不调 `chain`/`register` 时行为与 Step 1 完全一致。可加一个临时自测：注册一个自定义方案 + 自排链，dump 看降级路径。

**风险**：`__depth_of` 要处理「lock floor 引用了不在链里的方案」——去重与建链时校验，非法 id 报错而非静默。

---

## Step 3：公共 API 收敛 + 消费者跟进 + 关蓝牙移入预设（破坏性，一次到位）

**目标**：切掉 v2 遗留的公共类型，消费者同步改，保证编译不中断。

**动作（组件）**：
1. `tuya_pm.h`：
   - `TUYA_PM_LEVEL_E` → `TUYA_PM_SCHEME_E`（成员不变，`MAX`→`BUILTIN_MAX`）。
   - `TUYA_PM_SCHEME_T` 移入公共 header（原在内部头）。
   - lock `floor` / consumer `min_powered_level` / `deepest_allowed` / `tuya_pm_request` / `tuya_pm_current` / `on_change` 回调：类型 `TUYA_PM_LEVEL_E` → `uint8_t`（scheme id）。
   - **删** `TUYA_PM_LEVEL_CFG_T`、`policy.levels`、`policy.level_cnt`。
2. `tuya_pm.c`：跟随类型；删 `__load_default_levels` / `s_levels[]`（`min_residency` 改由方案携带，预设方案在 `tuya_pm_schemes.c` 里各自给默认值）。
3. `tuya_pm_schemes.c`：CEC_T20/ULP_ONLINE 的 `enter` **加关蓝牙**（`#if defined(ENABLE_BT_SERVICE)` → `tuya_ble_deinit()`，从 app 移来）。

**动作（消费者）**：
4. example：删 `policy.levels/level_cnt` 两行；删 §user_main 里的 `tuya_ble_deinit` 块 + `ble_mgr.h` include（改由预设方案关）；enum 引用不动（兼容 `uint8_t`）。
5. ulp demo：删 `s_levels[]` + `policy.levels/level_cnt`（ULP_ONLINE dtim=10 与预设默认一致；residency 10000→接受预设默认，或如需保留就注册一个自定义方案）；`tuya_main.c` 的 BLE deinit 视情况保留（配网期语义）或交给预设方案——迁移时确认时机后决定。

**验证**：两个消费者 NOTE build；ULP_ONLINE 实测关蓝牙由预设方案生效（app 不再自己关）。

**风险**：这是唯一破坏 API 的一步，组件 + 两消费者必须**同一提交**内改完才编得过。ulp demo 的 BLE deinit 时机（online 即关 vs 降档才关）行为可能微变，需实测确认不影响配网/上线。

---

## Step 4：清理 + 收尾

1. 删 `__mech_*` 残留注释、`__load_default_levels` 等死代码。
2. 删深睡前 DEBUG 块（`PR_NOTICE`+`tal_system_sleep(20)`，见 [todo-remove-deepsleep-log-delay]）。
3. `docs/power_level_management_design.md` §12 标注「v3 实现完成」；更新 memory。
4. 全量再 build 两个消费者 + `git` 提交。

---

## 一页纸小结

| 步 | 内容 | 公共 API | 消费者 | 风险 |
|----|------|----------|--------|------|
| 1 | 抽 `tuya_pm_schemes.c`（搬机制） | 不变 | 不改 | 低（逐字搬） |
| 2 | 方案库+链+register/chain | 只增 | 不改 | 低（默认等价） |
| 3 | 切类型+删 LEVEL_CFG+关蓝牙移入 | **破坏** | 改 | 中（同提交） |
| 4 | 清理+删 DEBUG+文档 | — | — | 低 |

**净效果**：`tuya_pm.c` 从「817 行含机制」瘦成「纯调度器」；机制进 `tuya_pm_schemes.c`；消费者少写关蓝牙、少传 `levels`。

# Linux x86_64 Board — 问题与待办

> **状态（2026-06-18）**：暂时维持当前实现，不推进 `linux_stdin_mux` 重构。  
> 本文档仅记录已知问题与后续方向，供 smart_speaker / voice_app 联调参考。

---

## P0 — STDIN 多消费者冲突

### 现象

Linux x86_64 上 **只有一个 STDIN**，但有两个模块同时读键盘：

| 模块 | 文件 | 行为 |
|------|------|------|
| 物理按键（TDL button） | `tdd_button_keyboard.c` | 10ms 扫描线程 `read(STDIN)`，匹配 `BUTTON_NAME`（默认 **`s`**） |
| Voice CLI | `voice_app_cli.c` | 独立线程 `read(STDIN)`，处理 **`r`/`t`/`e`/`d`** 等对话命令 |

非 `s` 的按键原先会被 button 扫描线程**直接丢弃**，导致 CLI 收不到 `r`/`t`/`e`，生产 E2E 无法操作。

### 根因

- 两个线程竞争同一 `STDIN_FILENO`，无统一 owner
- `voice_app_cli` 与 `tdd_button_keyboard` 各自 `tcsetattr` raw 模式，存在竞态
- `BUTTON_NAME=s` 与 CLI 无直接冲突，但 **谁先读到字节** 取决于调度；pipe 模式（非 TTY）下行为更不稳定

### 影响范围

- `apps/tuya.ai/smart_speaker` x86 手动 E2E（`x86_64_linux_ubuntu.config`）
- `test/*.sh` 通过 pipe 喂 stdin 的自测脚本
- 任何同时启用 `ENABLE_KEYBOARD_INPUT` + `voice_app_cli` 的 Linux 示例

---

## 当前临时方案（未合入，工作区本地）

为 unblock 联调，工作区存在 **spill queue** 补丁（**尚未 commit**）：

```
boards/LINUX/Linux_x86_64/tdd_button_keyboard.c   — 非 button 键写入 spill 环形队列
boards/LINUX/Linux_x86_64/tdd_button_keyboard.h   — 导出 spill API
src/voice_components/voice_app_compat/src/voice_app_cli.c — CLI 优先从 spill 取键
```

设计要点：

- Button 扫描线程仍为 STDIN 的**实际读者**
- 不匹配 `BUTTON_NAME` 的字节 → `__stdin_spill_push()`
- CLI 通过 `tdd_keyboard_stdin_spill_pop()` 消费，避免丢键

**局限（为何只是临时方案）**：

- spill 逻辑耦合在 board 驱动里，voice 层需 `extern` board API（层次倒置）
- 仍无双订阅/广播机制；新增第三个 STDIN 读者需再改 spill
- pipe/脚本测试与 TTY 手动操作行为可能不一致
- debug 日志较多（`PR_NOTICE` per key），合入前需降噪

**决策**：暂时维持此补丁于工作区，**不**在此阶段合入或扩展；待 `linux_stdin_mux` 方案评估后再统一落地。

---

## 推荐长期方案 — `linux_stdin_mux`（未实施）

在 `boards/LINUX/common/` 增加 **单一 STDIN owner** + 多订阅者分发：

```
linux_stdin_mux.c
  ├─ 唯一 read(STDIN) 线程
  ├─ 订阅者注册（button / voice_cli / 未来 shell）
  └─ 按规则分发：button 匹配键 → TDL；其余 → 各订阅者队列
```

优点：

- 消除双线程抢 STDIN
- board 与 voice 解耦，voice 不再依赖 `tdd_keyboard_*` spill API
- 便于 pipe 测试注入与 TTY 手动操作统一路径

参考：`docs/plans/master-upstream-sync-prep.md` §13（dialog / x86 联调相关讨论）。

---

## P1 — 按键语义与配置

| 键 | 用途 | 说明 |
|----|------|------|
| `s` | TDL button（`BUTTON_NAME`，Kconfig 默认） | 模拟物理按键；与 CLI 命令空间分离 |
| `r` | CLI：开始录音 | x86 无硬件 VAD，需配合 `t` |
| `t` | CLI：结束录音 / 提交 ASR | **必填**（`SPEAKER_HOST_MANUAL_VAD=y`） |
| `e` | CLI：oneshot（r→t 自动） | 需 cloud online |
| `d` | CLI：强制打断 | session 卡住时使用 |

待办：

- [ ] 文档化：`smart_speaker/README.md` 与本文档保持一致
- [ ] 评估是否将 `BUTTON_NAME` 改为非 CLI 字母（如 Space/F1），降低误触 — **低优先级**

---

## P1 — 测试与运行环境

- [ ] **TTY vs pipe**：`test/dialog_flow_test.sh` 等 pipe stdin 场景需确认 spill/mux 行为；自测优先 loopback profile（`x86_64_linux_ubuntu_loopback.config`）
- [ ] **Cloud 就绪门控**：CLI oneshot / start 需等 `cloud_cap ... online`（orch Phase 13 已部分修复）
- [ ] **Oneshot 释放**：dialog end 时 `audio_front_adapter_oneshot_stop()`（`voice_app_orch.c` 工作区改动，待与 STDIN 方案一并合入）

---

## P2 — 平台与其它

- [ ] `platform/platform_config.yaml`：LINUX pin `07cb301`（x86_64 fork 合入 TuyaOpen-ubuntu）— 与 `origin/release/v1.8.0` 仅此差异，已评估接受
- [ ] T5 真机 E2E 未验证（交叉引用，非本 board 范围）
- [ ] 上游 cherry-pick **#618** export path fix — 与 board 无直接关系

---

## 相关文件

| 路径 | 说明 |
|------|------|
| `boards/LINUX/Linux_x86_64/tdd_button_keyboard.c` | 键盘 → TDL button + spill（临时） |
| `boards/LINUX/Linux_x86_64/Kconfig` | `ENABLE_KEYBOARD_INPUT`, `BUTTON_NAME` |
| `src/voice_components/voice_app_compat/src/voice_app_cli.c` | CLI 命令线程 |
| `src/voice_components/voice_app_compat/src/voice_app_orch.c` | 对话 FSM / cloud 门控 |
| `apps/tuya.ai/smart_speaker/config/x86_64_linux_ubuntu.config` | 生产 cloud E2E |
| `apps/tuya.ai/smart_speaker/config/x86_64_linux_ubuntu_loopback.config` | CI / 自测 loopback |

---

## 变更记录

| 日期 | 说明 |
|------|------|
| 2026-06-18 | 初版：记录 STDIN 冲突、spill 临时方案、`linux_stdin_mux` 待办；决定暂不合入 |

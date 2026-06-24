[English](./README.md) | 简体中文

# smart_speaker

基于 TuyaOpen 的智能音箱应用：语音对话、本地提示音、DND、DP 控制。

## 目录结构

```text
smart_speaker/
├── README.md          # 英文主文档（见 README_zh.md）
├── test/              # 自测脚本（头部注释含详细步骤，索引见 test/README_zh.md）
├── include/ / src/    # 应用代码
└── config/            # 板级配置
```

## 快速开始

### 编译

```bash
cd apps/tuya.ai/smart_speaker
python3 ../../../tos.py build
```

产物：`dist/smart_speaker_1.0.0/smart_speaker_1.0.0.elf`


默认 `x86_64_linux_ubuntu.config` 为**生产 cloud** 联调；自测脚本需 **loopback** 配置：

```bash
python3 ../../../tos.py config choice -c x86_64_linux_ubuntu_loopback.config
python3 ../../../tos.py build
```

### 生产 cloud 联调（x86 手动）

```bash
python3 ../../../tos.py config choice -c x86_64_linux_ubuntu.config
python3 ../../../tos.py build
./dist/smart_speaker_1.0.0/smart_speaker_1.0.0.elf
```

1. 等待 log `cloud_cap ... online`（约 15–20s）。
2. **`r`** → 说话 → **`t`**（无硬件 VAD，**必须按 t**）。
3. TTS 结束后再开下一轮；推荐 **`e`** 一键 r+t。
4. 卡死时 **`d`** 复位（Phase 14 后 session 结束会自动释放 oneshot）。

### 自测

```bash
bash test/dialog_flow_test.sh
bash test/dialog_mode_test.sh
bash test/alert_playback_test.sh
bash test/file_inject_test.sh
bash test/dp_test.sh
bash test/media_playback_test.sh
```

CLI 速查见 **[test/README_zh.md](test/README_zh.md)**；各脚本详细步骤与用例见对应 `test/*.sh` 头部注释。

## 架构概览

```text
smart_speaker (产品层: DP, CLI, 提示音映射)
    │
    └── voice_app_compat (编排 / 云 / 播放 / DP / 存储)
            │
            └── tuya_voice_service (SDK 适配层)
                    └── tuya_ai_service / audio_player / audio_front
```

### 初始化链

```text
tuya_main → app_smart_speaker_init()
  → cloud_cap_comm_register_backend(production)
  → voice_service_adapter_init() → cloud_cap_comm_init() → tuya_ai_agent_init()
  → voice_app_cloud_init() / voice_app_player_init()
  → audio_front_adapter_register_backend(speaker_front_backend)
  → audio_front_adapter_init/start()
  → voice_app_start() → mgr 线程 (200ms tick)
```

上行 PCM：`ALSA / :fi` → `speaker_front_backend` → `voice_service_adapter_upload_audio()` → `tuya_ai_agent_upload_stream()`（不经 ring buffer）。

## 架构与模块边界

| 编号 | 收敛结果 | 直接收益 |
|------|----------|----------|
| A1 | `voice_service_adapter` 多订阅事件总线 | orch / player 可共享 `TTS_*` 事件 |
| A2 | `is_dialoging()` 不复用 online | 在线态不误判为对话中 |
| A3 | cloud backend / init / callback owner 分离 | 去掉二次 init 覆盖隐式状态 |
| A4 | compat 通过 `voice_app_product_port` 解耦产品层 | 不再 include `speaker_*` 私有头 |
| A5 | 产品 DP 由 `speaker_dp.c` 单路径 owner | compat 仅保留 DP 上行 MQTT 桥 |
| A6 | `ai_skill_play_ops_register()` fail-fast | 全局单例不被静默抢占 |

**原则**：单一真源 · 单向依赖 (`smart_speaker → voice_app_compat → tuya_ai_service`) · fail-fast · 手术式演进。

## 验证缺口

| 编号 | 说明 |
|------|------|
| G1 | 依赖真实 `tuya_config_local.h` credentials；占位 uuid 下 soft-skip |
| G2 | DP 上行 MQTT 仅 hermetic linkage；真实云侧需 credentials |
| G3 | host example 的 `tos.py build` 须**串行**（共享 `platform/LINUX/build`） |

## CLI 命令摘要

| 命令 | 说明 |
|------|------|
| `:mode single\|free\|multi` | 对话模式（推荐；互斥） |
| `:ct on\|off` / `:mt on\|off` | 续问/多轮（兼容旧命令） |
| `:vol N` | 音量 0–100 |
| `:alert N` | 触发提示音事件 |
| `:play_url URL` | BG 音乐 URL |
| `:play_file` / `:play_prefix` | 本地 MP3 |
| `:fi` / `:dump_*` | 上行灌测 / PCM dump |
| `:state` | 打印设备状态 |

按键 `s/r/t/e/f` 等详见 [test/README_zh.md](test/README_zh.md)（CLI 速查）及 `test/dialog_flow_test.sh` 头部注释。

## 媒体播放能力（摘要）

| 能力 | 状态 |
|------|------|
| FG 提示 / BG 音乐 / FG 抢占 BG | ✅ |
| 事件→独立提示音 / 唤醒 reply 音 | ✅ |
| playlist NEXT/PREV / `play_mp3_file` / DP205 同步 | ✅ |
| TTS 期间抑制 dingdong / 异步 FG EOS 恢复 BG | ✅ |
| 蓝牙音乐 (DP206/5/6) | 脚手架，全量暂缓 |
| Linux ALSA underrun | 偶现；已有 prepare/start 恢复 |

详见 [voice_app_compat README_zh.md](../../../src/voice_components/voice_app_compat/README_zh.md)。

## 关键源文件

| 角色 | 路径 |
|------|------|
| 应用入口 | `src/app_smart_speaker.c` |
| CLI / 按键 | `src/speaker_cmd.c` |
| 提示音 / DND | `src/speaker_hw.c` |
| DP | `src/speaker_dp.c` |
| PCM / VAD 后端 | `src/speaker_front_backend.c` |
| 上行灌测 | `src/speaker_file_inject.c` |
| 编排 | `src/voice_components/voice_app_compat/src/voice_app_orch.c` |
| 云桥 | `src/tuya_voice_service/cloud_cap_comm_adapter/` |

## 维护注意

- host example 保持串行 build
- 修改 `voice_app_interrupt_dialog()` 需高风险 impact review
- 发布前补真实 credentials 与云侧 DP 上报验证
- DP208 `reply`/`CTalk` 非 bool JSON 值会被忽略（不上报错误 DP）

## 相关文档

| 文档 | 内容 |
|------|------|
| [test/README_zh.md](test/README_zh.md) | 自测索引、CLI 速查、一键回归 |
| [voice_app_compat README_zh.md](../../../src/voice_components/voice_app_compat/README_zh.md) | 兼容层 API、dialog modes、engineering backlog |
| [tuya_voice_service README_zh.md](../../../src/tuya_voice_service/README_zh.md) | SDK 适配层 |
| [examples/voice_service README_zh.md](../../../examples/voice_service/README_zh.md) | Host 回归测试 |

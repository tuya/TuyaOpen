# NiceMCU-T5 — INMP441 + MAX98357

T5 引脚全引出裸板，外接 I2S 数字麦与 I2S 功放（无屏）。

## 接线（重要：麦和喇叭共用 I2S1 时钟）

| INMP441 | T5 | MAX98357 | T5 |
|---------|-----|----------|-----|
| VDD | 3V3 | VIN | 3V3 或 5V（共地） |
| GND | GND | GND | GND |
| SCK | **P40**（与功放 BCLK 并联） | BCLK | **P40** |
| WS | **P41**（与功放 LRC 并联） | LRC | **P41** |
| SD | **P42** | DIN | **P43** |
| L/R | GND（左声道） | SD | 3V3（常开）或 GPIO |

## 对话模式说明（重要）

本板是 **外置 I2S 麦 + 功放**，没有片内 codec 的 **回采/AEC** 通路（官方 T5 板 `tdd_audio` + `ENABLE_AUDIO_AEC` 那套）。

| 模式 | 依赖 | 本板表现 |
|------|------|----------|
| **0 按住说话** | 按键起停 | **推荐，可用** |
| 1 按键说一句 | VAD 判断说完 | 喇叭串音/底噪易导致一直 LISTEN |
| 2 唤醒对话 | KWS + AEC | **基本不可靠**（无回采，提示音会灌进麦） |
| 3 随意说 | VAD + KWS + AEC | **基本不可靠** |

驱动会在 **播报期间及结束后约 400ms** 对上行麦做软静音，减轻串音把 VAD 卡死；这不能替代硬件 AEC，唤醒词识别仍不保证。

## 驱动要点（v0.11）

- 单口双工 + 播报软静音麦（无硬件回采/AEC）
- 过滤 DMA 毛刺（`peak=32768` / `raw=0x28xxxxxx`），避免 VAD 永远不收尾
- **按键说一句**：`ai_audio_input` 在唤醒时会把粘住的 VAD SPEECH 状态清成 STOP，以便重新产生 START→STOP 边沿

测按键模式：单击进入听 → **说完后安静 1～2 秒** 等 VAD 收尾。日志应出现 `[====ai_oneshot] vad: [2]` 再 `[vad: [0或1]]`。

## 按键

用户键 **P0**，低有效。请连按切到 **mode 0** 使用。

## 编译

```bash
cd apps/tuya.ai/your_chat_bot
cp config/NICEMCU_T5.config app_default.config
tos.py build
```

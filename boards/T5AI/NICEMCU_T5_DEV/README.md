# NiceMCU-T5-DEV — T5 pin breakout

T5 引脚全引出裸板（无屏、无板载 codec）。外接 INMP441 + MAX98357 是可选外设，默认关闭。

## 可选：I2S 音频（INMP441 + MAX98357）

在 `menuconfig` / 保存的 config 里打开：

```
CONFIG_NICEMCU_T5_DEV_I2S_AUDIO=y
```

会自动 `select ENABLE_AUDIO_CODECS` 和 `ENABLE_I2S`。

接线（麦和喇叭共用 I2S1 时钟）：

| INMP441 | T5 | MAX98357 | T5 |
|---------|-----|----------|-----|
| VDD | 3V3 | VIN | 3V3 或 5V（共地） |
| GND | GND | GND | GND |
| SCK | **P40**（与功放 BCLK 并联） | BCLK | **P40** |
| WS | **P41**（与功放 LRC 并联） | LRC | **P41** |
| SD | **P42** | DIN | **P43** |
| L/R | GND（左声道） | SD | 3V3（常开）或 GPIO |

本路径没有片内 codec 的回采/AEC。对话模式建议用 **0 按住说话**；唤醒/随意说基本不可靠。

## 按键

用户键 **P0**，低有效。

## 编译

```bash
cd apps/tuya.ai/your_chat_bot
cp config/NICEMCU_T5_DEV.config app_default.config
tos.py build
```

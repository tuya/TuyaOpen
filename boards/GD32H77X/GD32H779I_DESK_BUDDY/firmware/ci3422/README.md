# Desk Buddy CI3422 音频桥固件

当前文件：`ci3422_tuya_voice_uart_1.0.0.bin`。

该固件运行在 CI3422，负责 H779 与本地麦克风/喇叭之间的 Tuya UART 音频桥：

```text
CI3422 MIC PCM -> H779 录音
H779 MP3/PCM -> CI3422 解码播放
```

未发布阶段固件版本统一为 `1.0.0`。

## 音频流控

硬件连接：

```text
CI3422 PC1 (GPIO output) -> H779 PA15 (GPIO input)
```

约定：

```text
高电平：H779 可以发送 SPK 数据
低电平：H779 暂缓发送 SPK 数据
```

CI3422 在收到 `SPK START` 后拉高 PC1，收到 `SPK STOP` 后拉低。H779 在发送每个 `SPK DATA` 包前读取 PA15；持续低电平 6 秒视为流控超时。

H779 的 PA15 不再连接/控制 CI3422 复位。CI3422 PC1 也不再复用为 DCODEC DAC STDBY。

## 配置

```text
Project: ci3422_tuya_voice_uart
Firmware version: 1.0.0
Firmware format: v2
UART: UART2 PB1/PB2, 1000000 8N1
Capture: PCM 16000 Hz / 16-bit / mono
Playback: MP3 passthrough + PCM
Volume: 0-100
ASR/DNN: disabled
```

## 文件信息

- 编译时间：`2026-11-24 15:43:12`
- 文件大小：1,036,167 字节
- SHA-256：`a9e7d0636c5d1162646bdbe752c824e789a937c7fac1e58d3e176746170c607a`

## 烧录

CI3422 SDK 工程：

```text
/home/share/samba/CI14LP_SDK_ASR_Offline_V1.0.28/projects/ci3422_tuya_voice_uart
```

使用该工程的 CI3422 DFU 流程烧录本目录中的完整固件。H779 端必须同时使用包含 SPK flow-control 支持的固件。

## 验证

- 两端代码编译通过。
- 流控硬件实机验证待完成。
- 之前的 MP3/PCM 收发、MIC 上行、音量和功放静音验证记录仍有效。

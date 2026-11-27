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

CI3422 使用与 CI1302 `develop-2.1.2` 相同的两级背压策略。UART 接收环形缓冲为 5 KiB，剩余空间低于 2 KiB 时拉低 PC1；协议任务消费到缓冲内已用空间低于 2 KiB 后重新拉高。播放器输入缓冲满时持续阻塞等待解码消费，不再超时丢数据；收到 `SPK STOP` 后等待播放器排空，避免下一段流被旧会话拒绝。接收半帧超过 200 ms 自动复位解析状态。H779 在发送每个 `SPK DATA` 包前读取 PA15；持续低电平 6 秒视为流控超时。

H779 的 PA15 不再连接/控制 CI3422 复位。CI3422 PC1 也不再复用为 DCODEC DAC STDBY。

## 配置

```text
Project: ci3422_tuya_voice_uart
Firmware version: 1.0.0
Firmware format: v2
UART: UART2 PB1/PB2, 1000000 8N1
Capture: PCM 16000 Hz / 16-bit / mono
Playback: MP3 passthrough + PCM, 1024-byte SPK packets
Volume: 0-100
ASR/DNN: disabled
```

## 文件信息

- 编译时间：`2026-11-24 20:26:51`
- 文件大小：1,032,071 字节
- SHA-256：`f3ad61fe716f962ec47a6395eb102f26ced27aa04c5b1b9caeb2f269fc44cfb6`

## 烧录

CI3422 SDK 工程：

```text
/home/share/samba/CI14LP_SDK_ASR_Offline_V1.0.28/projects/ci3422_tuya_voice_uart
```

使用该工程的 CI3422 DFU 流程烧录本目录中的完整固件。H779 端必须同时使用包含 SPK flow-control 支持的固件。

## 验证

- 两端代码编译通过，CI3422 使用 UART 接收缓冲水位流控和播放器阻塞背压。
- 流控硬件实机验证待完成。
- 之前的 MP3/PCM 收发、MIC 上行、音量和功放静音验证记录仍有效。

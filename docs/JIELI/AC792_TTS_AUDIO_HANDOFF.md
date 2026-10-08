# AC792 TTS 音频问题交接

- 更新时间：2026-09-29
- 工作区：`C:\Users\maida\.codex\worktrees\e484\TuyaOpen`
- 当前优先级：AC792N_Develop_Board 的 TTS 板载扬声器播放

## 1. 明确目标

先让 `examples/multimedia/audio_player/tts` 在 **AC792N_Develop_Board** 上从板载 SPK 清晰播放循环语音 “Hello Tuya”。必须同时满足：

1. 当前最终源码能为 AC792 / wl83 成功构建。
2. 烧录的是这次最终源码生成的固件。
3. 串口日志显示 PCM 队列完成排空、DAC FIFO 空闲后才关闭 DAC。
4. 用户实际听到板载喇叭播放语音。

这四项完成前，不能把“音频已跑通”作为结论。当前先不做 KWS，也不扩展到 AI 对话；AC791 音频验证排在 AC792 之后。后续 `output_speaker` 的链路目标是板载 MIC1/MIC2 到 J29 SPK。

## 2. 硬件与接线已核实

用户的接线方向正确：喇叭红线接 **J29 SP+**，黑线接 **J29 SP−**。AC792N Develop V1.21 原理图中 J29 是板载 LTK5313 功放的差分扬声器输出；J32 是 Earphone 接口，J30 是 AUX。SP− 是功放输出端，**不能接地**。

原理图和位号图：

- `platform/JIELI/chip/wl83/AC792_SDK/doc/硬件资料/原理图/开发板/AC792N开发板原理图位号图/AC792N Develop V1.21 原理图.pdf`
- `platform/JIELI/chip/wl83/AC792_SDK/doc/硬件资料/原理图/开发板/AC792N开发板原理图位号图/AC792N Develop V1.21 位号图.pdf`

所以，当前证据不支持“红黑线接反”或“插错 AUX/耳机口”是首要问题。TTS 例程只验证播放输出，不验证 MIC 输入。

## 3. 最新实板日志与结论

日志路径：

`D:\tuya_proj\TuyaOpen-JieLi\TuyaOpen\examples\multimedia\audio_player\tts\monitor.log`

日志最后更新时间为 2026-09-29 15:02:19。关键记录：

- 板型识别为 `AC792N_Develop_Board`。
- 配置为 `rate=16000 channel=0x0`，native SPK 输出启动成功。
- 应用反复提交 `player tts data len 9009`。
- 一次 DAC 写入记录为 `requested=8064 accepted=1598`。
- 随后约 0.94 秒报告 `ai player FG eof`，约 0.99 秒出现 `dac off`。
- 后续每隔约 2.8 秒重复同样的 TTS 开始、EOF、DAC off 流程。

这能证明播放链路已经进入板载 DAC 写入阶段，但**不能证明 9009 字节 TTS 已全部转成 PCM 并播放完成**，也不能证明 J29 上有可听输出。`DAC write accepted` 目前只记录首个 DAC 写入调用，不能据单条记录推算累计播放字节数。此日志对应的是此前固件，不是下面工作区的最终修改。

## 4. 已确认的问题根因

AGY 对前一版改动做过只读 review，认为常规队列复制与锁协调未见明显死锁，并指出以下问题：

1. `tdd_audio.c` 原先把 `OPRT_BUFFER_NOT_ENOUGH` 当作正常情况返回；`svc_ai_player.c` 又忽略 `consumer.write()` 的返回值并清空 `decode_size`，队列满时解码 PCM 会被丢弃。
2. 原生 `ao_flush()` 只等待当前 DAC writer，而后把软件队列直接清空；TKL stop 先停 DAC，导致尚未写入 FIFO 的 PCM 被丢弃。
3. flush 失败但 native stop 成功时，TKL 与 TDD 的 `started` 状态可能分叉，下一轮播放会跳过启动或报 `resource not ready`。
4. drain 超时后仍执行 destructive stop，会清空已经接受的 PCM，无法再重试。

## 5. 当前工作区已有的修改

以下代码位于隔离工作区 `C:\Users\maida\.codex\worktrees\e484\TuyaOpen`，不在用户日常主工作区 `D:\tuya_proj\TuyaOpen-JieLi\TuyaOpen`。截至 2026-09-29，外层 TuyaOpen 和内层 `platform/JIELI` 都同时有 staged、unstaged、untracked 改动。新 agent 开始前必须分别检查两个仓库的 `git status --short` 和 diff。**不要用 reset、clean、整目录 checkout 等方式清理工作区；也不要把所有改动作为本次 TTS 任务一次性提交。**

当前工作区改动横跨此前的多个任务，不全属于 TTS 静音问题：还包含 `your_chat_bot`/AI 对话、ADKEY/TDD ADC、`output_speaker`、AC791/AC792 板级配置，以及 TKL 的 KWS/VAD/队列适配。用户当前明确暂不做 KWS；这些相邻改动需要保留并独立 review，不能因为本次排查范围较窄而覆盖或回退。

本次排查最相关的外层仓库文件：

- [`tdd_audio.c`](../../src/peripherals/audio_codecs/tdd_audio/src/tdd_audio.c)：Jieli 播放遇到队列满时，以同一帧数据最多重试 300 次、每次等待 10 ms；首个满队列、恢复和超时都有日志。PLAY_STOP 同步 TDD 状态；close 即使 TDD 状态显示未启动也会重试 drain，超时则保留 AO，不调用会丢队列的 AO uninit，同时完成可安全执行的 MIC 清理。
- [`svc_ai_player.c`](../../src/audio_player/src/svc_ai_player.c)：非混音播放路径现在记录并返回 consumer 写入错误，避免静默吞掉写入失败。
- `src/audio_player/src/datasink/ai_player_datasink.c`、`datasink_mem.c`：数据 sink 的并行改动；核对其对 TTS PCM 数据流及 EOF/flush 时序的影响。
- `examples/multimedia/audio_player/tts/app_default.config`、`examples/multimedia/audio_player/tts/config/JIELI_AC792N_Develop_Board.config`：TTS 示例构建配置。
- `examples/peripherals/audio_codecs/output_speaker/`、`boards/JIELI/AC792N_Develop_Board/audio_config.h`：独立的板载 MIC/SPK 示例和板级音频配置；后续 MIC1/MIC2 到 J29 SPK 的测试从这里继续。
- `boards/JIELI/AC792N_Develop_Board/CMakeLists.txt`、`boards/JIELI/Kconfig`、`src/peripherals/audio_codecs/CMakeLists.txt`：板级与音频外设适配配置。
- 回归检查文件包括 `tests/platform/test_jieli_tdd_audio_contracts.py`、`test_jieli_tkl_audio_output.py`、`test_jieli_output_speaker.py`、`test_jieli_chat_profiles.py` 和构建/队列相关测试。先检查当前测试内容和对应代码，不要把“测试文件存在”当作实板播放成功。

本次排查最相关的内层 `platform/JIELI` 仓库文件：

- [`tkl_audio.c`](../../platform/JIELI/tuyaos/tuyaos_adapter/src/driver/tkl_audio.c)：先 drain，失败时不继续调用 destructive native stop；AO uninit 前也必须先成功 drain/stop。
- [`tkl_audio_jieli_wl83_native.c`](../../platform/JIELI/tuyaos/tuyaos_adapter/src/driver/tkl_audio_jieli_wl83_native.c)：flush 等待软件队列为空、writer 不在执行、`audio_dac_idle()` 表示空闲；超时返回 `OPRT_TIMEOUT` 并让后台继续 drain；后续 start 可以重新打开 producer gate。DAC writer 硬错误走显式停止清理路径。
- `tuyaos/tuyaos_adapter/src/driver/tkl_audio_jieli_server_native.c`、`tkl_audio_jieli_server.c` 及 `include/driver/tkl_jieli_audio_*`：音频 backend/server 注册和错误码映射。
- `jieli_build.py`、SDK 下载/烧录配置、`tkl_adc.c`、KWS/VAD/queue 等文件也有既有改动；TTS agent 应先读 diff 并保留，不要擅自回退。

**重要：上述最终一轮源码修改还没有重新构建，也没有烧录实测。** AGY 的 review 针对更早一版，指出的状态分叉、超时丢音和 consumer 错误吞掉问题随后已着手修改，但最新 diff 尚未经过 AGY 复核。新 agent 应先看最终 diff，再决定是否需要调整。
## 6. 当前验证状态与阻塞点

### 已完成

- J29 接线与 V1.21 原理图核对完成。
- 当前工作区此前曾为 AC792N_Develop_Board / wl83 构建成功。
- `tests.platform.test_jieli_tdd_audio_contracts` 和 `tests.platform.test_jieli_tkl_audio_output` 最近一次运行结果为 **13 项通过**；之后仅有一处注释文字调整。

### 尚未完成 / 阻塞

1. **最终源码构建未验证。** 当前 `dist/tts_1.0.0/tts_QIO_1.0.0.bin` 是较早构建产物（约 15:32）；音频源码在 15:34–15:36 又有修改，所以这个 bin 不能烧录，也不能代表最终代码。
2. **最终固件未烧录。** 需要重新构建后，再让 AC792 进入 WL83 USB downloader；板子运行态下只有 COM8（CH343 日志口）并不等于下载器已就绪。SerialDebug 占用 COM8 时继续通过 SerialDebug 读日志，不要抢占它。
3. **扬声器是否发声未知。** 用户尚未确认最新固件能否听到 “Hello Tuya”。软件日志无法取代实际听音验证。
4. **最终状态机需要复核。** 特别检查：flush timeout 后 native 仍运行并 drain，TKL/TDD 均可在下一次 start 恢复；close 超时不误调用 AO uninit；writer fatal error 的 stop/重试/资源回收路径；异常路径是否遗留 DAC 或线程。
5. **累计 PCM/DAC 量没有现成证据。** 现有 native 日志只报第一次 `audio_dac_write` 的 accepted 字节数；若仍静音，应加累计入队、累计写入、队列剩余、flush 等待时间的诊断，确认用户数据是否完整到达 DAC FIFO。

## 7. 新 agent 的执行顺序

1. 先检查本 handoff 涉及的 root 仓库和嵌套 `platform/JIELI` 仓库 diff；不要 reset、clean 掉既有改动，也不要把 C 工作区当成 D 主工作区。嵌套 SDK/平台仓库必须继续独立隔离。
2. 复核第 5 节列出的状态机与 AGY 上一轮意见；修正后再让 AGY 对最终 diff 复核。
3. 在根目录初始化 Windows 环境：

   ```powershell
   $env:TUYAOPEN_EXPORT_IDE = '1'
   . .\export.ps1
   Push-Location examples\multimedia\audio_player\tts
   tos.py config choice -c JIELI_AC792N_Develop_Board.config
   tos.py build
   Pop-Location
   ```

   配置文件位于 `examples/multimedia/audio_player/tts/config/`，`tos.py config choice` 使用文件名，不要再额外加 `config/` 前缀。此前 app 默认配置会将缓存切回 T5AI，必须确认最终 `using.config` 中为 JIELI / wl83 / AC792N_Develop_Board。
4. 记录最终 bin 的 SHA-256、长度和时间戳，确认它晚于所有音频源码修改；只有这个最终 bin 才能烧录。
5. 让用户将 AC792 切换到下载模式后，用本次工作区的 `tos.py flash` 烧录。不要对仍在运行态的板子误判为 downloader 已连接。
6. 保持 SerialDebug 监视 COM8，保存新的 `monitor.log`。检查正常情况下有 `draining speaker queue`、`speaker queue drained; DAC idle`，并且 DAC off 发生在排空之后；若出现 `speaker queue remained full`、`speaker drain timeout` 或 `consumer write failed`，沿返回码继续定位，不要把它们当成成功。
7. 让用户实际确认喇叭是否听到循环语音。若完整 drain 日志已出现但仍无声，再继续查板载功放使能、DAC FIFO/音量和 J29 的差分输出；保持 SP− 不接地。

当前能否继续的判定标准是：**最终 AC792 bin 构建成功、最终 bin 已烧录、串口日志证明完整播放排空、用户确认 J29 喇叭实际有声。**

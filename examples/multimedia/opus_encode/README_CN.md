# Opus 编码性能测试

示例入口位于 `src/tuya_app_main.c`。

支持测试平台预编译的 `libopus.a`，以及 TuyaOpen 的 Opus 1.3.1 源码库。通过 `ENABLE_TUYAOPEN_OPUS` 选择库；开启 `ENABLE_TUYAOPEN_OPUS` 后使用可移植的 C 定点实现，启用 `FIXED_POINT`、`DISABLE_FLOAT_API` 和编译器优化。测试不需要麦克风或音频文件系统。

## 测试参数

- PCM：16 kHz、单声道、16 位有符号整数；原始 PCM 码率为 256 kbit/s。
- 音频：`src/test_audio.h` 中的 400 ms 合成谐波扫频信号，共 6400 个采样点。它用于可重复的性能比较，不代表真实语音的编码质量或最坏耗时。
- 帧长：固定 40 ms，每帧 640 个采样点。
- 编码：`OPUS_APPLICATION_VOIP`、语音模式、16 kbit/s（16000 bit/s）CBR。
- 默认复杂度：0，可调范围 0–10。
- 每轮默认重复音频 20 次，共 200 帧、8 秒输入音频。
- 默认独立测试 20 轮，每轮重建、预热并重置编码器；总计 4000 帧、160 秒输入音频。

## 构建和运行

先在 SDK 根目录初始化环境，再进入示例目录：

```bash
. ./export.sh
cd examples/multimedia/opus_encode
tos.py config set CONFIG_OPUS_BENCH_COMPLEXITY=5 CONFIG_OPUS_BENCH_REPEAT=20
tos.py build
```

当前 `app_default.config` 选择 L511G_Y7PM。示例提供 `L511G_Y7PM` 和 `T5AI` 预设，切换平台时使用 `config choice`，避免保留另一平台的音频驱动等配置。使用对应平台的烧录/运行方式启动 `dist/` 下的产物，查看串口或控制台日志。

### L511G_Y7PM

在示例目录运行：

```bash
tos.py config choice -c L511G_Y7PM
tos.py build
```

此预设启用 TuyaOpen Opus 定点源码库。L511G 使用 Cortex-M3，预设采用可移植 C 路径，不启用 T5AI 的 ARM DSP 和 AON RTC/DWT 计时；可独立开启下文的 SMULL、NSQ 状态缓存及 NSQ 片内 RAM 执行候选。测试使用内置 PCM，不需要开启 `ENABLE_AUDIO_CODECS` 或注册麦克风、扬声器。

L511G 的 GNU 编译器在 Opus 目标上使用 `-O3 -finline`，覆盖平台全局的 `-fno-inline`，使逐采样的预测、滤波和算术辅助函数能够内联。此设置只作用于 Opus，编码参数保持一致。启动日志中的 `L511G Opus build: -O3, function inlining enabled` 可用于确认测试固件版本。性能收益需上板重测，并核对每轮包哈希。

最终固件为 `dist/opus_encode_1.0.0/opus_encode_QIO_1.0.0.binpkg`，使用 L511G 平台烧录工具下载；同目录保留 ELF 和厂商下载目录。日志经平台 unilog 输出，解码时使用本次构建产物中的日志数据库。启动后应看到 `Library source: TuyaOpen (fixed-point)`、`Fixed-point path: portable C`，测试结束后看到 `SUMMARY`。计时使用 TAL 毫秒 tick，日志中的微秒数不代表微秒精度。

首次上板可缩短测试：

```bash
tos.py config set OPUS_BENCH_REPEAT=1 OPUS_BENCH_ROUNDS=1
tos.py build
```

恢复完整基准：

```bash
tos.py config set OPUS_BENCH_REPEAT=20 OPUS_BENCH_ROUNDS=20
```

切回 T5AI 的原有 DSP + NSQ 状态缓存配置：

```bash
tos.py config choice -c T5AI
tos.py build
```

测试 T5AI 平台预编译库：

```bash
tos.py config set ENABLE_TUYAOPEN_OPUS=n OPUS_BENCH_PROFILE=n
tos.py build
```

测试 TuyaOpen 定点源码库：

```bash
tos.py config set ENABLE_TUYAOPEN_OPUS=y OPUS_BENCH_PROFILE=n
tos.py build
```

只有打开开关且源码缺失时，CMake 才会调用 Git 下载源码。已有源码时可以离线构建。平台库的定点/浮点模式由其预编译方式决定，使用 PCM16 输入不会改变库内部的计算模式。

## Opus 源码与辅助目录

以下路径均相对于 SDK 根目录，构建入口为 [thirdparty/CMakeLists.txt](../../../src/tuya_ai_service/thirdparty/CMakeLists.txt)。

| 目录 | 内容与用途 | 启用条件 |
| --- | --- | --- |
| `src/tuya_ai_service/thirdparty/opus/upstream/` | 上游 Opus `v1.3.1` 的源码和 `COPYING`，供 TuyaOpen 定点源码库编译使用 | `ENABLE_TUYAOPEN_OPUS=y` |
| `src/tuya_ai_service/thirdparty/opus/tests/` | 编码包一致性验证和目标端算术自检 | 主机测试命令见下文，目标端自检随 Cortex-M3 乘法候选启用 |
| `src/tuya_ai_service/thirdparty/opus/profile/` | SILK 耗时分析脚本和计时接口，帮助定位编码瓶颈 | `OPUS_BENCH_PROFILE=y`，依赖源码库及 T5AI 高精度计时或 L511G 毫秒计时 |
| `src/tuya_ai_service/thirdparty/opus/optimization/` | 普通 NSQ 状态缓存、L511G RAM 放置及定点乘法候选 | `ENABLE_TUYAOPEN_OPUS_NSQ_CACHE=y` 支持 T5AI ARM DSP 或 L511G；`ENABLE_TUYAOPEN_OPUS_NSQ_RAM=y` 仅支持 L511G |

`thirdparty/` 的 CMake/Kconfig 只注册库，各库在自己的目录中维护构建、配置、工具和测试。Opus 的 `upstream/` 是下载目标，没有 `.git`，也不是 Git 子模块，仅此目录被 `opus/.gitignore` 忽略；`optimization/`、`profile/` 和 `tests/` 均为本地维护代码。源码缺失或必要文件不完整时，CMake 将上游仓库浅克隆到构建临时目录，复制所需源码、头文件及 `COPYING`，随后删除临时仓库。

`profile/` 中的 `instrument_silk.py` 为四个 SILK 源文件生成带计时探针的副本，统计编码阶段、NSQ 和预测子阶段的耗时，以及码率控制迭代访问次数；`opus_profile.h` 声明阶段枚举和回调，回调实现在本示例的 `src/tuya_app_main.c` 中。分析开关默认关闭，开启后会增加计时开销。

`optimization/` 中的 `prepare_nsq.py` 为 `NSQ.c` 生成优化副本：T5AI 将随机种子、滤波标量和缓冲索引缓存为局部变量；L511G 仅缓存噪声整形缓冲索引，限制寄存器压力。都在子帧量化结束时写回状态。它只处理普通 NSQ，不修改延迟判决路径；该选项默认关闭，属于实验性优化，实际收益和输出一致性需要结合目标板卡及输入验证。

这些 Python 脚本在 CMake 配置阶段由 SDK 的 `TOS_PYTHON` 执行，生成文件分别放在对应 CMake 二进制目录的 `opus-profile/` 和 `opus-nsq-opt/` 下，再替换原文件参与编译，不直接修改下载源码。同时开启时，先插入计时探针，再对生成的 `NSQ.c` 应用状态缓存和 RAM 放置。脚本会检查预期的源码结构，匹配失败时中止配置。

更多使用方法见下文“SILK 热点分析”和“NSQ 内层状态缓存”。

### L511G 分段耗时定位

在 L511G 示例配置上启用分段计时，保持 20 轮、每轮 200 帧，累计测量 4000 帧：

```bash
tos.py config set OPUS_BENCH_PROFILE=y OPUS_BENCH_COMPLEXITY=0 OPUS_BENCH_REPEAT=20 OPUS_BENCH_ROUNDS=20 OPUS_BENCH_SWEEP=n OPUS_BENCH_DUMP_PACKETS=n
tos.py build
```

启动应看到 `SILK profiling enabled`。所有轮次结束后，`PROFILE NSQ`、`PROFILE predict.LPC`、`PROFILE predict.NLSF` 等行输出调用次数、累计耗时和平均调用耗时，随后输出平均每音频帧耗时、`encode_pct`（占总编码耗时的百分比）和 `parent_pct`（子阶段占 NSQ 或 predict 父区间的百分比）。先看父阶段的 `encode_pct` 定位大头，再看对应子阶段；父区间已经包含子区间，不能重复相加。码率控制迭代次数用于解释 NSQ 是否重复执行。

L511G 使用 1 ms tick，单次短调用可能显示为零；累计值和占比用于粗定位，不代表微秒精度。探针开销、中断和抢占包含在耗时中，profiling 的总耗时不能直接与关闭探针的基准比较。核对每轮包哈希仍为 `a679fc2e`；热点确认后，用 `tos.py config set OPUS_BENCH_PROFILE=n` 关闭探针，再测实际优化收益。

## 调整测试参数

修改复杂度：

```bash
tos.py config set CONFIG_OPUS_BENCH_COMPLEXITY=0
tos.py build
```

依次比较全部复杂度：

```bash
tos.py config set CONFIG_OPUS_BENCH_SWEEP=y
tos.py build
```

关闭扫描恢复单复杂度测试：

```bash
tos.py config set CONFIG_OPUS_BENCH_SWEEP=n CONFIG_OPUS_BENCH_COMPLEXITY=5
```

## 输出含义

启动时输出库来源、版本及定点标记。TuyaOpen 源码库必须具有 `-fixed` 标记；平台预编译库只报告标记，不因标记缺失而中止性能测试。

每种复杂度默认测试 20 轮，每轮使用独立的编码器。预热及重置不计时，仅累计 `opus_encode()` 的耗时；帧间让出 CPU 的等待、日志和初始化不计入编码耗时。连续重复同一段音频时保留编码器历史状态。

调整独立测试轮数（与每轮音频重复次数不同）：

```bash
tos.py config set OPUS_BENCH_ROUNDS=5 OPUS_BENCH_REPEAT=20
```

每轮输出单独的结果，随后 `SUMMARY` 输出所有轮次的加权平均帧耗时、各轮平均帧耗时的中位数、各轮平均值范围、全局最小/最大帧耗时和累计超时帧数。对比两个库时，优先比较中位数并观察范围；多轮测量能帮助识别波动，但不能消除系统抢占或动态调频的影响。扫描复杂度时，每个复杂度都会执行配置的轮数。

日志包含编码帧数、累计耗时、每帧平均/最小/最大耗时、超过 40 ms 的帧数、输入/输出字节数、输出码率，以及 `RTF = 编码耗时 / 音频时长`。RTF 小于 1 表示平均编码速度满足实时要求，同时应观察最大帧耗时。

T5AI 默认开启 `OPUS_BENCH_HIGH_RES`：AON RTC 测量墙钟时间（微秒单位，约 30 µs 分辨率），非 SMP 配置下 DWT 统计周期数；启动时检查计数器是否前进。SMP 任务可能迁移核心，DWT 是每个核心独立的计数器，因此 SMP 配置禁用周期统计，仅使用 RTC。每帧使用 32 位无符号差值处理 DWT 回绕，再累计到 64 位总数。周期数也包含中断和抢占，不代表当前线程独占的 CPU 周期，也不会直接报告为实际 CPU 频率。其他平台或关闭高精度开关时使用 TAL 毫秒 tick，转换为微秒显示，不提高底层分辨率。

每轮的 `Packet modes` 按输出包 TOC 统计 SILK、hybrid、CELT 包数；它们是 40 ms 输出包计数，不是 SILK 内部帧数。`encode`、最小/最大帧耗时使用微秒单位，平均值仍显示为毫秒。嵌入式任务预留 64 KiB 栈，用于 Opus 的临时数组。

## SILK 热点分析

T5AI 的 TuyaOpen 定点源码库使用以下高精度配置；L511G 使用上文“L511G 分段耗时定位”的配置：

```bash
tos.py config set ENABLE_TUYAOPEN_OPUS=y OPUS_BENCH_HIGH_RES=y OPUS_BENCH_PROFILE=y
tos.py build
```

所有轮次结束后，`PROFILE` 报告 pitch、shape、predict、gains、LBRR、NSQ 和 entropy 的累计耗时、调用次数及每次调用平均耗时。计数器排除每轮初始化和预热，仅在实测期间累计；这是单编码任务的诊断工具，不支持同时运行其他 Opus 编码器。

`rate loop iter=0..6` 的 `visits` 表示有多少 SILK 内部帧执行到了该迭代，不是最终迭代次数的直方图。`iter=0` 是参与码率控制的内部帧数。循环可能复用已有结果，因此循环访问次数不一定等于 NSQ 实际调用次数。NSQ 包含普通和延迟判决路径；entropy 包含正常编码和最后一次比特预算补救编码。

细分输出 `NSQ.rewhite/scale/normal/delayed` 分别表示重白化、状态缩放、普通子帧量化和延迟判决子帧量化。`predict.LTP_analysis/LTP_quant/LTP_filter/unvoiced_scale/LPC/NLSF/residual` 分别测量 LTP 相关分析、LTP 参数量化、LTP 滤波、非浊音缩放、LPC 分析、NLSF 量化和残差能量。探针放在函数和子帧边界，不逐采样计时。这些细分区间包含在 NSQ 或 predict 父区间内，不能把父子耗时相加；`measured` 只累计顶层阶段。未分类的时间包括滤波、拷贝、码率控制以及计时探针本身。

分析源码由 `thirdparty/opus/profile/instrument_silk.py` 在构建目录生成，原始下载源码不被修改，也不改变算法、CBR 策略或输出包。源码布局不匹配时生成失败，防止静默漏测。

详细分析会增加计时开销，不能把开启 PROFILE 的结果直接与原先未开启时的结果比较。进行平台库/源码库性能对比时先关闭：

```bash
tos.py config set OPUS_BENCH_PROFILE=n
tos.py build
```

替换测试音频时保持同样的 PCM 格式，并确保采样点总数是 640 的整数倍。

## T5AI ARM DSP 候选路径

`ENABLE_TUYAOPEN_OPUS_ARM_DSP` 默认关闭，仅在 T5AI 的 TuyaOpen 源码库上可选。开启后使用上游 `OPUS_ARM_INLINE_ASM` 和 `OPUS_ARM_INLINE_EDSP` 定点乘加与饱和运算；CMake 使用实际交叉编译器检查 ARM DSP 支持，不引入 NEON、运行时 CPU 检测或新的下载依赖。

```bash
tos.py config set ENABLE_TUYAOPEN_OPUS_ARM_DSP=y
tos.py build
```

每轮输出 `Packet hash`，覆盖包长度和每个编码字节。严格对照时使用短测试并开启包转储，分别在 DSP 开关关闭、开启时烧录运行，按复杂度、轮次和帧编号逐包比较 `PACKET` 的长度和十六进制字节：

```bash
tos.py config set OPUS_BENCH_SWEEP=y OPUS_BENCH_REPEAT=2 OPUS_BENCH_ROUNDS=2 OPUS_BENCH_DUMP_PACKETS=y
```

包转储在计时之外执行，但日志会改变缓存及调度行为。正式性能比较应关闭包转储和详细分析，并使用相同的测试参数；短对照只能验证该输入的输出一致性。

```bash
tos.py config set OPUS_BENCH_SWEEP=n OPUS_BENCH_COMPLEXITY=0 OPUS_BENCH_REPEAT=20 OPUS_BENCH_ROUNDS=20 OPUS_BENCH_DUMP_PACKETS=n OPUS_BENCH_PROFILE=n
```

### 本次 T5AI 实测

参数为复杂度 0、16 kHz 单声道 PCM16、40 ms、CBR 16000 bit/s，每轮 200 帧、独立 20 轮；关闭详细分析和包转储，使用 RTC 测量。同一块板卡上比较：

| 路径 | 平均帧耗时 | 各轮平均值中位数 | 最大帧耗时 | 超过 40 ms |
| --- | --- | --- | --- | --- |
| 便携 C | 9.758 ms | 9.754 ms | 13.843 ms | 0 |
| ARM DSP | 8.653 ms | 8.653 ms | 12.593 ms | 0 |

总编码耗时减少约 11.32%。复杂度 0～10 的短对照测试共 440 个包，长度和每个编码字节完全相同；两种路径的 20 轮基准每轮包哈希均为 `a679fc2e`。这是当前合成音频和所测配置的验证结果，不代表所有输入的性能或位精确性证明。

复杂度 0 的细分短测试中，普通量化循环约占 NSQ 父区间的 79%；NLSF 和 LPC 分别约占 predict 父区间的 29% 和 25%。这些占比来自开启探针的测试，用于定位下一步热点，不用于替代上述关闭探针的性能比较。

## L511G Cortex-M3 乘法候选

`ENABLE_TUYAOPEN_OPUS_CORTEX_M3_MUL` 默认关闭，仅在 L511G 的 TuyaOpen 源码库上可选。开启后，用 Cortex-M3 的 `SMULL` 高半乘积实现 `silk_SMULWB/SMLAWB/SMULWT/SMLAWT`，保留每次乘法的截断规则和原有 `SMULWW/SMLAWW` 定义。CBR、码率、采样率、帧长和复杂度不变。

覆盖头文件位于 `thirdparty/opus/optimization/opus_cortex_m3_macros.h`，只强制包含到 `tuyaopen_opus` 编译目标；上游源码保持不变。CMake 使用实际交叉编译器检查 ARMv7-M 指令支持，不启用 T5AI DSP、NEON 或运行时 CPU 检测。

在本示例目录启用并关闭测速探针：

```bash
tos.py config set ENABLE_TUYAOPEN_OPUS=y ENABLE_TUYAOPEN_OPUS_CORTEX_M3_MUL=y OPUS_BENCH_PROFILE=n
tos.py build
```

启动时应看到 `Fixed-point path: Cortex-M3 SMULL halfword multiplication`。优化版示例在预热及测速之前执行目标端算术自检，检查四个宏在 196 组边界输入和 4096 组确定性随机输入上的结果；成功打印 `Cortex-M3 multiplication self-test passed: 4292 input triples`，失败则停止基准。该自检只编入本示例，不增加其他应用的启动工作。

恢复基线：

```bash
tos.py config set ENABLE_TUYAOPEN_OPUS_CORTEX_M3_MUL=n OPUS_BENCH_PROFILE=n
tos.py build
```

本次生成的对照固件位于 `dist/cortex_m3_mul_ab/baseline/` 和 `dist/cortex_m3_mul_ab/optimized/`。分别编译两版后，将当次完整的 `dist/opus_encode_1.0.0/` 复制到对应目录，保留所有固件、调试文件及 `L511G_Y7PM_download/` 烧录目录，并额外保存对应的 `app_default.config`；`manifest.json` 记录包大小、SHA-256、参数及每个文件的校验值。

USB 烧录时在 EPAT 中选择对应目录下的 `L511G_Y7PM_download/config_custom_usb.ini`，保持整个烧录子目录一起使用。`pkgimg_gen` 由烧录工具从包中生成，agentboot 等工具文件使用现有 EPAT 安装。两份固件均为 complexity=0、20 轮 × 200 帧，关闭 PROFILE 和包转储。主机按 ARM 定点分支模拟 SMULL 的 3960 包对照已通过。`20261012_184644.zip` 的优化版板端日志确认算术自检通过，平均耗时 15.4985 ms；`20261012_185517.zip` 的本次基线平均耗时为 15.649 ms，两版均为 20 轮且包哈希一致。当前对照耗时减少约 0.962%，未达到拟定的 3% 采纳门槛；完整的交替 A/B/A/B 复测及板端包逐字节对照尚未完成，详情见 `BENCHMARK_RESULTS_CN.md`。

建议按基线、优化、基线、优化的顺序测试，比较各轮平均耗时的中位数、最慢轮和超时帧数。合成音频每轮预期哈希仍为 `a679fc2e`；要逐字节检查板端输出，可使用前述短测试及包转储配置。性能测试继续关闭包转储和 PROFILE。

## NSQ 内层状态缓存

`ENABLE_TUYAOPEN_OPUS_NSQ_CACHE` 默认关闭，支持 T5AI ARM DSP 路径和 L511G。T5AI 在普通 NSQ 的子帧量化函数内缓存随机种子、两个滤波标量和两个缓冲索引；L511G 仅缓存 `sLTP_shp_buf_idx`，在子帧结束时写回。两者都保持原来的定点算术和最终状态，不迁移编码器或任务栈到 SRAM，也不增加堆分配。T5AI 使用：

```bash
tos.py config set ENABLE_TUYAOPEN_OPUS_ARM_DSP=y ENABLE_TUYAOPEN_OPUS_NSQ_CACHE=y
tos.py build
```

关闭 `ENABLE_TUYAOPEN_OPUS_NSQ_CACHE` 可恢复上游 NSQ 实现。变更由 `thirdparty/opus/optimization/prepare_nsq.py` 生成到构建目录，下载源码保持不变；支持与细分分析开关同时使用。对应测试结果见 `BENCHMARK_RESULTS_CN.md`。

### L511G NSQ 缓存与片内 RAM 执行

当前工作配置启用 SMULL、NSQ 缓存和 RAM 执行；命名预设 `L511G_Y7PM` 仍保留便携 C 基线。保持原来的合成音频、16 kHz、单声道、40 ms、CBR 16000 bit/s、复杂度 0 和 20 轮 × 200 帧，不添加真实语音。

```bash
tos.py config set ENABLE_TUYAOPEN_OPUS=y ENABLE_TUYAOPEN_OPUS_CORTEX_M3_MUL=y ENABLE_TUYAOPEN_OPUS_NSQ_CACHE=y ENABLE_TUYAOPEN_OPUS_NSQ_RAM=y OPUS_BENCH_PROFILE=n OPUS_BENCH_DUMP_PACKETS=n OPUS_BENCH_SWEEP=n OPUS_BENCH_COMPLEXITY=0 OPUS_BENCH_REPEAT=20 OPUS_BENCH_ROUNDS=20
tos.py build
```

RAM 开关只把 `silk_NSQ_c` 放到 `.sect_platFMRamcode_text.opus_nsq`，由平台现有 `.load_ap_firam_msmb` 段加载/解压和唤醒恢复；不修改平台链接脚本，不搬迁任务栈或编码器数据。当前组合版该函数占 3472 字节，MSMB 使用边界为 `0x00414160`，距平台限制 `0x00418000` 还有 16032 字节。其他工程须重新核对 map 中的空间，当前余量不代表所有配置都适用。

本次对照产物位于 `dist/l511g_nsq_ab/`，三版都保留 SMULL，方便比较新增优化：

| 子目录 | NSQ 缓存 | NSQ RAM 执行 |
| --- | --- | --- |
| `baseline/` | 关闭 | 关闭 |
| `cache/` | 开启 | 关闭 |
| `cache_ram/` | 开启 | 开启 |

每版保存完整下载目录、ELF、日志数据库、构建日志及配置；`manifest.json` 保存文件 SHA-256 和配置。USB 烧录使用各版自己的 `L511G_Y7PM_download/config_custom_usb.ini`，解码日志也使用同版数据库。启动日志分别报告 `Normal NSQ` 的状态缓存方式和 `Normal NSQ placement` 的执行位置，避免混淆。

先比较 `baseline` 与 `cache`，再比较 `cache` 与 `cache_ram`；交替重复，观察平均耗时、中位数、最大值及超时帧数。预期每轮包哈希为 `a679fc2e`。这些是已编译的候选，实际提速比例、RAM 加载运行和唤醒恢复仍需板端验证。

仅关闭 RAM 执行：

```bash
tos.py config set ENABLE_TUYAOPEN_OPUS_NSQ_RAM=n
```

恢复本轮基线（保留 SMULL）：

```bash
tos.py config set ENABLE_TUYAOPEN_OPUS_NSQ_CACHE=n ENABLE_TUYAOPEN_OPUS_NSQ_RAM=n
```

主机一致性回归可在 SDK 根目录执行（需要 Python 3 和 GCC；不联网、不修改上游源码）：

```bash
python src/tuya_ai_service/thirdparty/opus/tests/verify_nsq.py
```

覆盖复杂度 0～10、合成扫频、静音、满幅交替、脉冲和确定性随机 PCM，每种组合 3960 包。分别检查便携 ARM 定点算术与 SMULL 的 C 算术模拟，各自覆盖原版、五字段缓存、仅 RAM 标记、五字段缓存加 RAM、M3 单索引缓存、M3 单索引缓存加 RAM，共十二种组合。主机上的 RAM 标记只验证生成源码，SMULL 模拟不执行 ARM 汇编，均不能替代目标板运行验证。

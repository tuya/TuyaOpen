# TKL PSRAM adaptation probe

最小 TKL PSRAM 路径验证工程，用于对比 TuyaOpen 平台适配和原厂
`bsp_aps6408l.c` 的独立裸机结果。

覆盖：

- `tkl_system_psram_init()`
- mapped PSRAM block allocator
- `tkl_system_memset()` / `tkl_system_memcpy()`
- 多块分配和释放
- SWD 可读诊断变量 `g_tkl_psram_probe`

运行：

```bash
# H77E / APS6408L
tos.py config choice -c GD32H77EI_DESK_BUDDY.config
tos.py build
tos.py flash

# H779 / APS6404L
tos.py config choice -c GD32H779I_DESK_BUDDY.config
tos.py build
tos.py flash
```

SWD 变量：

```text
g_tkl_psram_probe.magic
g_tkl_psram_probe.stage
g_tkl_psram_probe.init_rt
g_tkl_psram_probe.free_before
g_tkl_psram_probe.block_a
g_tkl_psram_probe.block_b
g_tkl_psram_probe.fill_bad
g_tkl_psram_probe.copy_bad
g_tkl_psram_probe.free_after
g_tkl_psram_probe.done
g_tkl_psram_bench.magic
g_tkl_psram_bench.stage
g_tkl_psram_bench.cpu_hz
g_tkl_psram_bench.runs
g_tkl_psram_bench.errors
g_tkl_psram_bench.entries[]
g_tkl_psram_bench.done
```

PASS 条件：

```text
stage = 100
init_rt = 0
fill_bad = 0
copy_bad = 0
done = 1
```

## 已验证结果（2026-11-26）

在 `GD32H77EI_DESK_BUDDY` 上通过 SWD 10 MHz 烧录和运行，结果 PASS：

```text
magic        = 0x544B4C50
stage        = 100
init_rt      = 0
free_before  = 0x003FFFE0
block_a      = 0x90400000
block_b      = 0x90440000
fill_bad     = 0
copy_bad     = 0
free_after   = 0x003FFFE0
done         = 1
```

原厂驱动独立验证和 TuyaOpen 适配的差异、问题根因见：

[APS6408L 原厂驱动与 TKL 适配对比](../../../boards/GD32H77X/GD32H77EI_DESK_BUDDY/docs/psram/ADAPTATION_COMPARISON.md)

驱动分层和新增 PSRAM profile 的步骤见：

[GD32H77X PSRAM 驱动分层与新增器件指南](../../../boards/GD32H77X/GD32H77EI_DESK_BUDDY/docs/psram/PSRAM_DRIVER_GUIDE.md)

## DWT 性能基准

工程使用 Cortex-M7 DWT `CYCCNT` 统计 TKL PSRAM API 的实际 CPU 周期。每项运行
5 次，同时记录最小值和平均值，覆盖 1 KiB、4 KiB、16 KiB。

测试路径：

- `tkl_system_memset()` on PSRAM
- RAM -> PSRAM `tkl_system_memcpy()`
- PSRAM -> RAM `tkl_system_memcpy()`
- PSRAM -> PSRAM `tkl_system_memcpy()`

H77E / GD32H77EIW / CPU 600 MHz，16 KiB 平均结果：

| 路径 | 优化前 | 优化后 | 提升 |
| --- | ---: | ---: | ---: |
| PSRAM memset | 3.997 MB/s | 15.769 MB/s | 3.95x |
| RAM -> PSRAM | 4.051 MB/s | 15.713 MB/s | 3.88x |
| PSRAM -> RAM | 19.333 MB/s | 52.397 MB/s | 2.71x |
| PSRAM -> PSRAM | 2.677 MB/s | 5.995 MB/s | 2.24x |

优化后的 16 KiB 详细结果：

| 路径 | 平均周期 | 耗时 | 吞吐率 |
| --- | ---: | ---: | ---: |
| PSRAM memset | 623,416 | 1.039 ms | 15.769 MB/s |
| RAM -> PSRAM | 625,621 | 1.043 ms | 15.713 MB/s |
| PSRAM -> RAM | 187,615 | 0.313 ms | 52.397 MB/s |
| PSRAM -> PSRAM | 1,639,697 | 2.733 ms | 5.995 MB/s |

快速路径条件：目标地址和长度都 4 字节对齐时，直接使用连续 32 位访问；不满足时只对
头部最多 3 字节和尾部最多 3 字节走安全读改写，中间主体仍按对齐 32 位处理。

另外，探针包含 `block_a + 1`、4093 字节 memset 和 `block_b + 3`、4091 字节 memcpy
的非对齐回归；`unaligned_bad` 必须为 0。

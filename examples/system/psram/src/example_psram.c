/**
 * @file example_psram.c
 * @brief Minimal TKL PSRAM adaptation probe.
 *
 * This example tests the TuyaOpen TKL PSRAM path:
 * init -> mapped allocator -> TKL memset/memcpy -> multi-block alloc/free.
 */

#include "tal_api.h"
#include "tkl_memory.h"
#include "tkl_output.h"

#include <stdint.h>

#define TKL_PSRAM_PROBE_MAGIC 0x544B4C50U
#define TKL_PSRAM_BENCH_MAGIC 0x544B454DU
#define BENCH_DEMCR           (*(volatile uint32_t *)0xE000EDFCU)
#define BENCH_DWT_CTRL        (*(volatile uint32_t *)0xE0001000U)
#define BENCH_DWT_CYCCNT      (*(volatile uint32_t *)0xE0001004U)
#define BENCH_DWT_LAR         (*(volatile uint32_t *)0xE0001FB0U)
#define BENCH_DEMCR_TRCENA    (1UL << 24U)
#define BENCH_DWT_CYCCNTENA   (1UL << 0U)
#define PROBE_DSB() __asm volatile("dsb 0xF" ::: "memory")
#define PROBE_ISB() __asm volatile("isb 0xF" ::: "memory")
#define BLOCK_A_SIZE          (256U * 1024U)
#define BLOCK_B_SIZE          (64U * 1024U)
#define COPY_SIZE             (16U * 1024U)
#define BENCH_RUNS            5U
#define BENCH_SIZE_COUNT      3U

extern OPERATE_RET tkl_system_psram_init(void);
extern void *tkl_system_psram_malloc(size_t size);
extern void tkl_system_psram_free(void *ptr);
extern int tkl_system_psram_get_free_heap_size(void);
extern uint32_t SystemCoreClock;

typedef struct {
    uint32_t magic;
    uint32_t stage;
    int32_t init_rt;
    int32_t free_before;
    uint32_t block_a;
    uint32_t block_b;
    uint32_t fill_bad;
    uint32_t copy_bad;
    uint32_t unaligned_bad;
    int32_t free_after;
    uint32_t done;
} tkl_psram_probe_t;

volatile tkl_psram_probe_t g_tkl_psram_probe = {
    .magic = TKL_PSRAM_PROBE_MAGIC,
};

typedef struct {
    uint32_t size;
    uint32_t memset_min;
    uint32_t memset_avg;
    uint32_t ram_to_psram_min;
    uint32_t ram_to_psram_avg;
    uint32_t psram_to_ram_min;
    uint32_t psram_to_ram_avg;
    uint32_t psram_to_psram_min;
    uint32_t psram_to_psram_avg;
} tkl_psram_bench_entry_t;

typedef struct {
    uint32_t magic;
    uint32_t stage;
    uint32_t cpu_hz;
    uint32_t runs;
    uint32_t errors;
    uint32_t done;
    tkl_psram_bench_entry_t entries[BENCH_SIZE_COUNT];
} tkl_psram_bench_t;

volatile tkl_psram_bench_t g_tkl_psram_bench = {
    .magic = TKL_PSRAM_BENCH_MAGIC,
};

static const uint32_t s_bench_sizes[BENCH_SIZE_COUNT] = {
    1U * 1024U,
    4U * 1024U,
    16U * 1024U,
};

static uint32_t benchmark_begin(void)
{
    PROBE_DSB();
    PROBE_ISB();
    return BENCH_DWT_CYCCNT;
}

static uint32_t benchmark_end(uint32_t start)
{
    PROBE_DSB();
    PROBE_ISB();
    return BENCH_DWT_CYCCNT - start;
}

static void benchmark_track(uint32_t cycles, uint32_t *minimum,
                            uint64_t *total)
{
    if (cycles < *minimum) {
        *minimum = cycles;
    }
    *total += cycles;
}

static void benchmark_init(void)
{
    BENCH_DEMCR |= BENCH_DEMCR_TRCENA;
    BENCH_DWT_LAR = 0xC5ACCE55U;
    BENCH_DWT_CTRL |= BENCH_DWT_CYCCNTENA;
    g_tkl_psram_bench.cpu_hz = SystemCoreClock;
    g_tkl_psram_bench.runs = BENCH_RUNS;
    g_tkl_psram_bench.stage = 0U;
    g_tkl_psram_bench.errors = 0U;
    g_tkl_psram_bench.done = 0U;
}

static void benchmark_run(tkl_psram_bench_entry_t *entry,
                          uint8_t *block_a, uint8_t *block_b,
                          const uint8_t *ram)
{
    uint64_t total = 0U;
    entry->memset_min = UINT32_MAX;
    for (uint32_t run = 0U; run < BENCH_RUNS; ++run) {
        g_tkl_psram_bench.stage = 1U;
        uint32_t start = benchmark_begin();
        if (tkl_system_memset(block_a, 0xA5, entry->size) == NULL) {
            ++g_tkl_psram_bench.errors;
        }
        benchmark_track(benchmark_end(start), &entry->memset_min, &total);
    }
    entry->memset_avg = (uint32_t)(total / BENCH_RUNS);

    total = 0U;
    entry->ram_to_psram_min = UINT32_MAX;
    for (uint32_t run = 0U; run < BENCH_RUNS; ++run) {
        g_tkl_psram_bench.stage = 2U;
        uint32_t start = benchmark_begin();
        if (tkl_system_memcpy(block_a, ram, entry->size) == NULL) {
            ++g_tkl_psram_bench.errors;
        }
        benchmark_track(benchmark_end(start), &entry->ram_to_psram_min,
                        &total);
    }
    entry->ram_to_psram_avg = (uint32_t)(total / BENCH_RUNS);

    total = 0U;
    entry->psram_to_ram_min = UINT32_MAX;
    for (uint32_t run = 0U; run < BENCH_RUNS; ++run) {
        g_tkl_psram_bench.stage = 3U;
        uint32_t start = benchmark_begin();
        if (tkl_system_memcpy((void *)ram, block_a, entry->size) == NULL) {
            ++g_tkl_psram_bench.errors;
        }
        benchmark_track(benchmark_end(start), &entry->psram_to_ram_min,
                        &total);
    }
    entry->psram_to_ram_avg = (uint32_t)(total / BENCH_RUNS);

    total = 0U;
    entry->psram_to_psram_min = UINT32_MAX;
    for (uint32_t run = 0U; run < BENCH_RUNS; ++run) {
        g_tkl_psram_bench.stage = 4U;
        uint32_t start = benchmark_begin();
        if (tkl_system_memcpy(block_b, block_a, entry->size) == NULL) {
            ++g_tkl_psram_bench.errors;
        }
        benchmark_track(benchmark_end(start), &entry->psram_to_psram_min,
                        &total);
    }
    entry->psram_to_psram_avg = (uint32_t)(total / BENCH_RUNS);
}

static void probe_fail(uint32_t stage)
{
    g_tkl_psram_probe.stage = stage;
    g_tkl_psram_probe.done = 1U;
    PROBE_DSB();
    PR_ERR("TKL PSRAM probe FAIL stage=%lu", (unsigned long)stage);
    while (1) {
        tal_system_sleep(1000U);
    }
}

void user_main(void)
{
    static uint8_t source[COPY_SIZE] __attribute__((aligned(32)));
    uint8_t *block_a = NULL;
    uint8_t *block_b = NULL;

    tal_log_init(TAL_LOG_LEVEL_DEBUG, 4096, (TAL_LOG_OUTPUT_CB)tkl_log_output);
    PR_NOTICE("TKL PSRAM probe start");

    g_tkl_psram_probe.stage = 1U;
    g_tkl_psram_probe.init_rt = tkl_system_psram_init();
    g_tkl_psram_probe.free_before = tkl_system_psram_get_free_heap_size();
    PR_NOTICE("TKL PSRAM init rt=%ld free=%ld",
              (long)g_tkl_psram_probe.init_rt,
              (long)g_tkl_psram_probe.free_before);
    if (g_tkl_psram_probe.init_rt != OPRT_OK ||
        g_tkl_psram_probe.free_before <= 0) {
        probe_fail(2U);
    }

    g_tkl_psram_probe.stage = 3U;
    block_a = (uint8_t *)tkl_system_psram_malloc(BLOCK_A_SIZE);
    block_b = (uint8_t *)tkl_system_psram_malloc(BLOCK_B_SIZE);
    g_tkl_psram_probe.block_a = (uint32_t)(uintptr_t)block_a;
    g_tkl_psram_probe.block_b = (uint32_t)(uintptr_t)block_b;
    PR_NOTICE("TKL PSRAM blocks a=%08lX b=%08lX",
              (unsigned long)g_tkl_psram_probe.block_a,
              (unsigned long)g_tkl_psram_probe.block_b);
    if (block_a == NULL || block_b == NULL) {
        probe_fail(4U);
    }

    g_tkl_psram_probe.stage = 5U;
    if (tkl_system_memset(block_a, 0xA5, BLOCK_A_SIZE) == NULL) {
        probe_fail(6U);
    }
    for (uint32_t index = 0U; index < BLOCK_A_SIZE; ++index) {
        if (block_a[index] != 0xA5U) {
            ++g_tkl_psram_probe.fill_bad;
            if (g_tkl_psram_probe.fill_bad >= 16U) {
                break;
            }
        }
    }
    if (g_tkl_psram_probe.fill_bad != 0U) {
        probe_fail(7U);
    }

    g_tkl_psram_probe.stage = 8U;
    for (uint32_t index = 0U; index < COPY_SIZE; ++index) {
        source[index] = (uint8_t)(index * 13U + 7U);
    }
    if (tkl_system_memcpy(block_a, source, COPY_SIZE) == NULL) {
        probe_fail(9U);
    }
    if (tkl_system_memcpy(block_b, block_a, COPY_SIZE) == NULL) {
        probe_fail(10U);
    }
    for (uint32_t index = 0U; index < COPY_SIZE; ++index) {
        if (block_a[index] != source[index] ||
            block_b[index] != source[index]) {
            ++g_tkl_psram_probe.copy_bad;
            if (g_tkl_psram_probe.copy_bad >= 16U) {
                break;
            }
        }
    }
    if (g_tkl_psram_probe.copy_bad != 0U) {
        probe_fail(11U);
    }

    g_tkl_psram_probe.stage = 12U;
    if (tkl_system_memset(block_a + 1U, 0x3C, 4093U) == NULL) {
        probe_fail(12U);
    }
    for (uint32_t index = 0U; index < 4093U; ++index) {
        if (block_a[index + 1U] != 0x3CU) {
            ++g_tkl_psram_probe.unaligned_bad;
            break;
        }
    }
    if (tkl_system_memcpy(block_b + 3U, block_a + 1U, 4091U) == NULL) {
        probe_fail(13U);
    }
    for (uint32_t index = 0U; index < 4091U; ++index) {
        if (block_b[index + 3U] != 0x3CU) {
            ++g_tkl_psram_probe.unaligned_bad;
            break;
        }
    }
    if (g_tkl_psram_probe.unaligned_bad != 0U) {
        probe_fail(14U);
    }

    g_tkl_psram_probe.stage = 15U;
    benchmark_init();
    for (uint32_t index = 0U; index < BENCH_SIZE_COUNT; ++index) {
        g_tkl_psram_bench.entries[index].size = s_bench_sizes[index];
        benchmark_run(&g_tkl_psram_bench.entries[index], block_a, block_b,
                      source);
    }
    g_tkl_psram_bench.done = 1U;
    if (g_tkl_psram_bench.errors != 0U) {
        probe_fail(15U);
    }

    g_tkl_psram_probe.stage = 16U;
    tkl_system_psram_free(block_b);
    tkl_system_psram_free(block_a);
    block_a = NULL;
    block_b = NULL;
    g_tkl_psram_probe.free_after = tkl_system_psram_get_free_heap_size();
    PR_NOTICE("TKL PSRAM probe PASS free_after=%ld",
              (long)g_tkl_psram_probe.free_after);
    if (g_tkl_psram_probe.free_after < g_tkl_psram_probe.free_before) {
        probe_fail(17U);
    }

    g_tkl_psram_probe.stage = 100U;
    g_tkl_psram_probe.done = 1U;
    PROBE_DSB();

    while (1) {
        tal_system_sleep(1000U);
    }
}

static void psram_probe_thread(void *arg)
{
    (void)arg;
    user_main();
}

void tuya_app_main(void)
{
    static THREAD_HANDLE s_probe_thread = NULL;
    THREAD_CFG_T cfg = {
        .stackDepth = 4096U,
        .priority = THREAD_PRIO_1,
        .thrdname = "psram_probe",
    };
    tal_thread_create_and_start(&s_probe_thread, NULL, NULL,
                                psram_probe_thread, NULL, &cfg);
}

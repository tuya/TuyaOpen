/**
 * @file tuya_app_main.c
 * @brief Benchmark platform or TuyaOpen Opus encoding with 40 ms PCM frames.
 * @copyright Copyright (c) 2026 Tuya Inc. All Rights Reserved.
 */
#include <stdint.h>
#include <string.h>

#include "tal_api.h"
#include "tal_log.h"
#include "tal_system.h"
#include "tal_thread.h"
#include "tkl_output.h"
#include "opus/opus.h"
#include "test_audio.h"

#define OPUS_BENCH_BITRATE       16000
#define OPUS_BENCH_PACKET_SIZE   1275
#define OPUS_BENCH_U64_TEXT_SIZE 21

/* Newlib-nano on L511G does not support %llu. Keep the full 64-bit value
 * and pass decimal text to the logger using caller-owned buffers. */
static const char *benchmark_u64_text(uint64_t value, char buffer[OPUS_BENCH_U64_TEXT_SIZE])
{
    char *end = buffer + OPUS_BENCH_U64_TEXT_SIZE - 1;
    *end      = '\0';
    do {
        *--end = (char)('0' + value % 10);
        value /= 10;
    } while (value != 0);
    return end;
}

#if defined(OPUS_BENCH_HIGH_RES) && OPUS_BENCH_HIGH_RES
/* Chip headers require sdkconfig.h, generated in the later platform build.
 * These platform API signatures are verified in aon_rtc.h and dwt.h. */
extern uint64_t bk_aon_rtc_get_us(void);
#if !defined(ENABLE_SMP) || !ENABLE_SMP
#define OPUS_BENCH_USE_DWT 1
extern void     dwt_init_cycle_counter(void);
extern uint32_t dwt_get_cycle_counter_val(void);
static int      bench_cycles_available;
#endif
#endif

static uint64_t benchmark_now_us(void)
{
#if defined(OPUS_BENCH_HIGH_RES) && OPUS_BENCH_HIGH_RES
    return bk_aon_rtc_get_us();
#else
    return (uint64_t)tal_system_get_millisecond() * 1000;
#endif
}

static uint32_t benchmark_cycles(void)
{
#if defined(OPUS_BENCH_USE_DWT)
    return bench_cycles_available ? dwt_get_cycle_counter_val() : 0;
#else
    return 0;
#endif
}

#if defined(OPUS_BENCH_PROFILE) && OPUS_BENCH_PROFILE
#include "opus_profile.h"
static int      profile_active;
static uint64_t profile_us[OPUS_PROFILE_STAGE_COUNT];
static uint32_t profile_calls[OPUS_PROFILE_STAGE_COUNT];
static uint32_t profile_iterations[7];
static uint32_t profile_iterations_overflow;

uint64_t tuyaopen_opus_profile_now(void)
{
    return benchmark_now_us();
}

void tuyaopen_opus_profile_record(OPUS_PROFILE_STAGE_T stage, uint64_t elapsed_us)
{
    if (!profile_active) {
        return;
    }
    profile_us[stage] += elapsed_us;
    profile_calls[stage]++;
}

void tuyaopen_opus_profile_iteration(unsigned int iteration)
{
    if (!profile_active) {
        return;
    }
    if (iteration < 7) {
        profile_iterations[iteration]++;
    } else {
        profile_iterations_overflow++;
    }
}

static void benchmark_profile_reset(void)
{
    memset(profile_us, 0, sizeof(profile_us));
    memset(profile_calls, 0, sizeof(profile_calls));
    memset(profile_iterations, 0, sizeof(profile_iterations));
    profile_iterations_overflow = 0;
}

static void benchmark_profile_report(uint64_t total_us, uint64_t total_frames)
{
    char                     numbers[2][OPUS_BENCH_U64_TEXT_SIZE];
    static const char *const names[]     = {"pitch",
                                            "shape",
                                            "predict",
                                            "gains",
                                            "LBRR",
                                            "NSQ",
                                            "entropy",
                                            "NSQ.rewhite",
                                            "NSQ.scale",
                                            "NSQ.normal",
                                            "NSQ.delayed",
                                            "predict.LTP_analysis",
                                            "predict.LTP_quant",
                                            "predict.LTP_filter",
                                            "predict.unvoiced_scale",
                                            "predict.LPC",
                                            "predict.NLSF",
                                            "predict.residual"};
    uint64_t                 measured_us = 0;
    for (int stage = 0; stage < OPUS_PROFILE_STAGE_COUNT; stage++) {
        /* Child intervals are included in their parent; do not count twice. */
        if (stage <= OPUS_PROFILE_ENTROPY) {
            measured_us += profile_us[stage];
        }
        PR_NOTICE("PROFILE %s: calls=%u, time=%s us, avg=%s us", names[stage], profile_calls[stage],
                  benchmark_u64_text(profile_us[stage], numbers[0]),
                  benchmark_u64_text(profile_calls[stage] ? profile_us[stage] / profile_calls[stage] : 0, numbers[1]));
        uint64_t parent_us = total_us;
        if (stage >= OPUS_PROFILE_NSQ_REWHITE && stage <= OPUS_PROFILE_NSQ_DELAYED) {
            parent_us = profile_us[OPUS_PROFILE_NSQ];
        } else if (stage >= OPUS_PROFILE_PRED_LTP_ANALYSIS) {
            parent_us = profile_us[OPUS_PROFILE_PREDICT];
        }
        uint64_t frame_us   = total_frames ? profile_us[stage] / total_frames : 0;
        uint64_t encode_pct = total_us ? profile_us[stage] * 10000 / total_us : 0;
        uint64_t parent_pct = parent_us ? profile_us[stage] * 10000 / parent_us : 0;
        PR_NOTICE("PROFILE %s: per audio frame=%s.%03u ms, encode_pct=%u.%02u, parent_pct=%u.%02u", names[stage],
                  benchmark_u64_text(frame_us / 1000, numbers[0]), (unsigned int)(frame_us % 1000),
                  (unsigned int)(encode_pct / 100), (unsigned int)(encode_pct % 100), (unsigned int)(parent_pct / 100),
                  (unsigned int)(parent_pct % 100));
    }
    uint32_t iterations = 0;
    for (int iter = 0; iter < 7; iter++) {
        iterations += profile_iterations[iter];
        PR_NOTICE("PROFILE rate loop iter=%d, visits=%u", iter, profile_iterations[iter]);
    }
    PR_NOTICE("PROFILE SILK internal frames=%u, loop visits=%u, NSQ calls=%u, overflow visits=%u",
              profile_iterations[0], iterations, profile_calls[OPUS_PROFILE_NSQ], profile_iterations_overflow);
    PR_NOTICE("PROFILE measured=%s us, total encode=%s us; remaining includes control, copies and probes",
              benchmark_u64_text(measured_us, numbers[0]), benchmark_u64_text(total_us, numbers[1]));
}
#endif

typedef struct {
    uint64_t elapsed_us;
    uint32_t frames;
    uint64_t min_us;
    uint64_t max_us;
    uint32_t over_budget;
    uint64_t cycles;
} OPUS_BENCH_RESULT_T;

static int benchmark_round(int complexity, int round, OPUS_BENCH_RESULT_T *result)
{
    int          error;
    OpusEncoder *encoder = opus_encoder_create(TEST_AUDIO_SAMPLE_RATE, 1, OPUS_APPLICATION_VOIP, &error);
    if (encoder == NULL || error != OPUS_OK) {
        PR_ERR("opus_encoder_create: %s", opus_strerror(error));
        return -1;
    }

    if ((error = opus_encoder_ctl(encoder, OPUS_SET_COMPLEXITY(complexity))) != OPUS_OK ||
        (error = opus_encoder_ctl(encoder, OPUS_SET_BITRATE(OPUS_BENCH_BITRATE))) != OPUS_OK ||
        (error = opus_encoder_ctl(encoder, OPUS_SET_VBR(0))) != OPUS_OK ||
        (error = opus_encoder_ctl(encoder, OPUS_SET_SIGNAL(OPUS_SIGNAL_VOICE))) != OPUS_OK) {
        PR_ERR("opus_encoder_ctl: %s", opus_strerror(error));
        opus_encoder_destroy(encoder);
        return -1;
    }

    unsigned char packet[OPUS_BENCH_PACKET_SIZE];
    /* Warm up code/data caches outside the measurement, then reset history. */
    error = opus_encode(encoder, test_audio_pcm, TEST_AUDIO_FRAME_SIZE, packet, sizeof(packet));
    if (error < 0 || (error = opus_encoder_ctl(encoder, OPUS_RESET_STATE)) != OPUS_OK) {
        PR_ERR("Opus warmup/reset: %s", opus_strerror(error));
        opus_encoder_destroy(encoder);
        return -1;
    }

    uint64_t elapsed_us   = 0;
    uint64_t output_bytes = 0;
    uint64_t cycles       = 0;
    uint32_t modes[3]     = {0};
    uint32_t packet_hash  = 2166136261u;
#if defined(OPUS_BENCH_PROFILE) && OPUS_BENCH_PROFILE
    profile_active = 1;
#endif
    uint64_t min_us      = (uint64_t)-1;
    uint64_t max_us      = 0;
    uint32_t frames      = 0;
    uint32_t over_budget = 0;

    for (int repeat = 0; repeat < OPUS_BENCH_REPEAT; repeat++) {
        for (size_t offset = 0; offset < TEST_AUDIO_SAMPLES; offset += TEST_AUDIO_FRAME_SIZE) {
            uint64_t start       = benchmark_now_us();
            uint32_t cycle_start = benchmark_cycles();
            int bytes = opus_encode(encoder, &test_audio_pcm[offset], TEST_AUDIO_FRAME_SIZE, packet, sizeof(packet));
            uint32_t cycle_end = benchmark_cycles();
            uint64_t duration  = benchmark_now_us() - start;
            cycles += (uint32_t)(cycle_end - cycle_start);
            if (bytes < 0) {
                PR_ERR("opus_encode frame %u: %s", frames, opus_strerror(bytes));
                opus_encoder_destroy(encoder);
                return -1;
            }
            elapsed_us += duration;
            output_bytes += (uint32_t)bytes;
            min_us = duration < min_us ? duration : min_us;
            max_us = duration > max_us ? duration : max_us;
            over_budget += duration > TEST_AUDIO_FRAME_MS * 1000;
            /* RFC 6716 TOC configuration: 0-11 SILK, 12-15 hybrid, 16-31 CELT. */
            unsigned int toc_config = packet[0] >> 3;
            modes[toc_config < 12 ? 0 : (toc_config < 16 ? 1 : 2)]++;
            /* FNV-1a covers each packet length and byte, outside timing. */
            packet_hash = (packet_hash ^ (uint32_t)bytes) * 16777619u;
            for (int byte = 0; byte < bytes; byte++) {
                packet_hash = (packet_hash ^ packet[byte]) * 16777619u;
            }
#if defined(OPUS_BENCH_DUMP_PACKETS) && OPUS_BENCH_DUMP_PACKETS
            char              hex[2 * OPUS_BENCH_PACKET_SIZE + 1];
            static const char digits[] = "0123456789abcdef";
            for (int byte = 0; byte < bytes; byte++) {
                hex[2 * byte]     = digits[packet[byte] >> 4];
                hex[2 * byte + 1] = digits[packet[byte] & 15];
            }
            hex[2 * bytes] = '\0';
            PR_NOTICE("PACKET c=%d r=%d f=%u bytes=%d hex=%s", complexity, round, frames, bytes, hex);
#endif
            frames++;
            /* Yield outside the timed region, so embedded watchdogs can run. */
            tal_system_sleep(1);
        }
    }
#if defined(OPUS_BENCH_PROFILE) && OPUS_BENCH_PROFILE
    profile_active = 0;
#endif
    opus_encoder_destroy(encoder);

    uint64_t audio_ms = (uint64_t)frames * TEST_AUDIO_FRAME_MS;
    uint64_t avg_us   = elapsed_us / frames;
    char     numbers[3][OPUS_BENCH_U64_TEXT_SIZE];
    PR_NOTICE("complexity=%d, round=%d/%d, frames=%u, audio=%s ms, encode=%s us", complexity, round, OPUS_BENCH_ROUNDS,
              frames, benchmark_u64_text(audio_ms, numbers[0]), benchmark_u64_text(elapsed_us, numbers[1]));
    PR_NOTICE("frame avg=%s.%03u ms, min=%s us, max=%s us, over 40 ms=%u",
              benchmark_u64_text(avg_us / 1000, numbers[0]), (unsigned int)(avg_us % 1000),
              benchmark_u64_text(min_us, numbers[1]), benchmark_u64_text(max_us, numbers[2]), over_budget);
    PR_NOTICE("PCM=%s bytes, Opus=%s bytes, output bitrate=%s bit/s",
              benchmark_u64_text((uint64_t)frames * TEST_AUDIO_FRAME_SIZE * sizeof(test_audio_pcm[0]), numbers[0]),
              benchmark_u64_text(output_bytes, numbers[1]),
              benchmark_u64_text(output_bytes * 8000 / audio_ms, numbers[2]));
    if (elapsed_us != 0) {
        /* RTF = encode time / audio duration; less than 1 means real time. */
        uint64_t rtf = elapsed_us / audio_ms;
        PR_NOTICE("RTF=%s.%03u (encode/audio)", benchmark_u64_text(rtf / 1000, numbers[0]), (unsigned int)(rtf % 1000));
    } else {
        PR_NOTICE("Encoding below timer resolution; increase OPUS_BENCH_REPEAT");
    }
    PR_NOTICE("Packet modes: SILK=%u, hybrid=%u, CELT=%u", modes[0], modes[1], modes[2]);
    PR_NOTICE("Packet hash=%08x", packet_hash);
#if defined(OPUS_BENCH_USE_DWT)
    PR_NOTICE("DWT cycles=%s, avg=%s cycles/frame", benchmark_u64_text(cycles, numbers[0]),
              benchmark_u64_text(cycles / frames, numbers[1]));
#endif
    result->cycles      = cycles;
    result->elapsed_us  = elapsed_us;
    result->frames      = frames;
    result->min_us      = min_us;
    result->max_us      = max_us;
    result->over_budget = over_budget;
    return 0;
}

static int benchmark_complexity(int complexity)
{
#if defined(OPUS_BENCH_PROFILE) && OPUS_BENCH_PROFILE
    benchmark_profile_reset();
#endif
    uint64_t round_us[OPUS_BENCH_ROUNDS];
    uint64_t total_us         = 0;
    uint64_t total_cycles     = 0;
    uint64_t total_frames     = 0;
    uint64_t min_us           = (uint64_t)-1;
    uint64_t max_us           = 0;
    uint32_t over_budget      = 0;
    uint32_t frames_per_round = 0;

    for (int round = 0; round < OPUS_BENCH_ROUNDS; round++) {
        OPUS_BENCH_RESULT_T result;
        if (benchmark_round(complexity, round + 1, &result) != 0) {
            return -1;
        }
        round_us[round] = result.elapsed_us;
        total_us += result.elapsed_us;
        total_cycles += result.cycles;
        total_frames += result.frames;
        frames_per_round = result.frames;
        min_us           = result.min_us < min_us ? result.min_us : min_us;
        max_us           = result.max_us > max_us ? result.max_us : max_us;
        over_budget += result.over_budget;
    }

    /* Sort round totals for median and range; every round has equal frames. */
    for (int i = 1; i < OPUS_BENCH_ROUNDS; i++) {
        uint64_t value = round_us[i];
        int      j     = i;
        while (j > 0 && round_us[j - 1] > value) {
            round_us[j] = round_us[j - 1];
            j--;
        }
        round_us[j] = value;
    }
    uint64_t mean_us = total_us / total_frames;
    uint64_t median_us;
    if (OPUS_BENCH_ROUNDS % 2 != 0) {
        median_us = round_us[OPUS_BENCH_ROUNDS / 2] / frames_per_round;
    } else {
        median_us =
            (round_us[OPUS_BENCH_ROUNDS / 2 - 1] + round_us[OPUS_BENCH_ROUNDS / 2]) / (2 * (uint64_t)frames_per_round);
    }
    uint64_t fastest_us = round_us[0] / frames_per_round;
    uint64_t slowest_us = round_us[OPUS_BENCH_ROUNDS - 1] / frames_per_round;
    uint64_t rtf        = total_us / (total_frames * TEST_AUDIO_FRAME_MS);
    char     numbers[3][OPUS_BENCH_U64_TEXT_SIZE];
    PR_NOTICE("SUMMARY complexity=%d, rounds=%d, frames=%s, encode=%s us", complexity, OPUS_BENCH_ROUNDS,
              benchmark_u64_text(total_frames, numbers[0]), benchmark_u64_text(total_us, numbers[1]));
    PR_NOTICE("frame mean=%s.%03u ms, median round avg=%s.%03u ms", benchmark_u64_text(mean_us / 1000, numbers[0]),
              (unsigned int)(mean_us % 1000), benchmark_u64_text(median_us / 1000, numbers[1]),
              (unsigned int)(median_us % 1000));
    PR_NOTICE("round avg range=%s.%03u..%s.%03u ms", benchmark_u64_text(fastest_us / 1000, numbers[0]),
              (unsigned int)(fastest_us % 1000), benchmark_u64_text(slowest_us / 1000, numbers[1]),
              (unsigned int)(slowest_us % 1000));
    PR_NOTICE("frame min=%s us, max=%s us, over 40 ms=%u, RTF=%s.%03u", benchmark_u64_text(min_us, numbers[0]),
              benchmark_u64_text(max_us, numbers[1]), over_budget, benchmark_u64_text(rtf / 1000, numbers[2]),
              (unsigned int)(rtf % 1000));
#if defined(OPUS_BENCH_USE_DWT)
    PR_NOTICE("SUMMARY cycles=%s, avg=%s cycles/frame", benchmark_u64_text(total_cycles, numbers[0]),
              benchmark_u64_text(total_cycles / total_frames, numbers[1]));
#else
    (void)total_cycles;
#endif
#if defined(OPUS_BENCH_PROFILE) && OPUS_BENCH_PROFILE
    benchmark_profile_report(total_us, total_frames);
#endif
    return 0;
}

int user_main(void)
{
    tal_time_service_init();
    tal_log_init(TAL_LOG_LEVEL_NOTICE, 1024, (TAL_LOG_OUTPUT_CB)tkl_log_output);
    const char *version = opus_get_version_string();
    PR_NOTICE("Opus benchmark: %s, mono PCM16 %d Hz, %d ms / %d samples, CBR %d bit/s", version, TEST_AUDIO_SAMPLE_RATE,
              TEST_AUDIO_FRAME_MS, TEST_AUDIO_FRAME_SIZE, OPUS_BENCH_BITRATE);
#if defined(ENABLE_TUYAOPEN_OPUS) && ENABLE_TUYAOPEN_OPUS
    PR_NOTICE("Library source: TuyaOpen (fixed-point)");
#if defined(ENABLE_TUYAOPEN_OPUS_ARM_DSP) && ENABLE_TUYAOPEN_OPUS_ARM_DSP
    PR_NOTICE("Fixed-point path: ARM DSP inline assembly");
#elif defined(ENABLE_TUYAOPEN_OPUS_CORTEX_M3_MUL) && ENABLE_TUYAOPEN_OPUS_CORTEX_M3_MUL
    PR_NOTICE("Fixed-point path: Cortex-M3 SMULL halfword multiplication");
    extern int tuyaopen_opus_m3_math_selftest(void);
    int        math_checks = tuyaopen_opus_m3_math_selftest();
    if (math_checks < 0) {
        PR_ERR("Cortex-M3 multiplication self-test failed");
        return -1;
    }
    PR_NOTICE("Cortex-M3 multiplication self-test passed: %d input triples", math_checks);
#else
    PR_NOTICE("Fixed-point path: portable C");
#endif
#if defined(BOARD_CHOICE_L511G) && BOARD_CHOICE_L511G
    PR_NOTICE("L511G Opus build: -O3, function inlining enabled");
#endif
#if defined(ENABLE_TUYAOPEN_OPUS_NSQ_CACHE) && ENABLE_TUYAOPEN_OPUS_NSQ_CACHE
#if defined(BOARD_CHOICE_L511G) && BOARD_CHOICE_L511G
    PR_NOTICE("Normal NSQ: shaping buffer index cached (Cortex-M3)");
#else
    PR_NOTICE("Normal NSQ: scalar state cached inside the sample loop");
#endif
#else
    PR_NOTICE("Normal NSQ: upstream scalar state access");
#endif
#if defined(ENABLE_TUYAOPEN_OPUS_NSQ_RAM) && ENABLE_TUYAOPEN_OPUS_NSQ_RAM
    PR_NOTICE("Normal NSQ placement: internal MSMB RAM");
#else
    PR_NOTICE("Normal NSQ placement: default code section");
#endif
    if (strstr(version, "-fixed") == NULL) {
        PR_ERR("Expected a fixed-point Opus library; check linked libraries");
        return -1;
    }
#else
    PR_NOTICE("Library source: platform prebuilt libopus.a");
    PR_NOTICE("Fixed-point build marker: %s", strstr(version, "-fixed") != NULL ? "present" : "absent");
#endif
    if (TEST_AUDIO_SAMPLES == 0 || TEST_AUDIO_SAMPLES % TEST_AUDIO_FRAME_SIZE != 0) {
        PR_ERR("Test audio must contain complete 40 ms frames");
        return -1;
    }
#if defined(OPUS_BENCH_HIGH_RES) && OPUS_BENCH_HIGH_RES
#if defined(OPUS_BENCH_USE_DWT)
    dwt_init_cycle_counter();
    uint32_t probe = dwt_get_cycle_counter_val();
#endif
    uint64_t rtc_probe = benchmark_now_us();
    tal_system_sleep(1);
#if defined(OPUS_BENCH_USE_DWT)
    bench_cycles_available = dwt_get_cycle_counter_val() != probe;
#endif
    if (benchmark_now_us() <= rtc_probe) {
        PR_ERR("AON RTC counter is not advancing");
        return -1;
    }
#if defined(OPUS_BENCH_USE_DWT)
    PR_NOTICE("Clock: AON RTC (~30 us resolution), DWT=%s", bench_cycles_available ? "active" : "unavailable");
#else
    PR_NOTICE("Clock: AON RTC (~30 us resolution), DWT disabled for SMP task migration");
#endif
#else
    PR_NOTICE("Clock: TAL millisecond ticks (values converted to us)");
#endif
#if defined(OPUS_BENCH_PROFILE) && OPUS_BENCH_PROFILE
    PR_NOTICE("SILK profiling enabled: includes probe overhead; one benchmark task only");
#if !defined(OPUS_BENCH_HIGH_RES) || !OPUS_BENCH_HIGH_RES
    PR_NOTICE("PROFILE clock resolution=1000 us; short calls may read zero; compare accumulated totals");
#endif
    PR_NOTICE("PROFILE encode_pct: share of encode time; parent_pct: share of NSQ/predict for child stages");
    PR_NOTICE("PROFILE child times are included in parents; do not sum parent and child stages");
#endif
    PR_NOTICE("Synthetic harmonic sweep; preemption and interrupts are included in timings");
#if defined(OPUS_BENCH_SWEEP) && OPUS_BENCH_SWEEP
    for (int complexity = 0; complexity <= 10; complexity++) {
        if (benchmark_complexity(complexity) != 0) {
            return -1;
        }
    }
    return 0;
#else
    return benchmark_complexity(OPUS_BENCH_COMPLEXITY);
#endif
}

#if OPERATING_SYSTEM == SYSTEM_LINUX
int main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;
    return user_main();
}
#else
static THREAD_HANDLE benchmark_thread;

static void benchmark_task(void *arg)
{
    (void)arg;
    user_main();
    tal_thread_delete(benchmark_thread);
}

void tuya_app_main(void)
{
    THREAD_CFG_T config = {0};
    /* Opus uses temporary stack arrays, including at complexity 10. */
    config.stackDepth = 64 * 1024;
    config.priority = THREAD_PRIO_1;
    config.thrdname = "opus_benchmark";
    OPERATE_RET ret = tal_thread_create_and_start(&benchmark_thread, NULL, NULL, benchmark_task, NULL, &config);
    if (ret != OPRT_OK) {
        PR_ERR("Cannot start Opus benchmark thread: %d", ret);
    }
}
#endif

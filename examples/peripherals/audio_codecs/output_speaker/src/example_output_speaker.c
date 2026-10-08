/**
 * @file example_output_speaker.c
 * @brief example_audio module is used to 
 * @version 0.1
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#include "tuya_cloud_types.h"
#include "tal_api.h"

#include "tuya_ringbuf.h"
#include "tkl_output.h"

#include "board_com_api.h"
#include "tdl_audio_manage.h"

#if defined(ENABLE_BUTTON) && (ENABLE_BUTTON == 1)
#include "tdl_button_manage.h"
#endif

/***********************************************************
************************macro define************************
***********************************************************/
// Maximum recordable duration, unit ms
#define EXAMPLE_RECORD_DURATION_MS (3 * 1000)
#define EXAMPLE_TEST_TONE_DURATION_MS (500)
#define EXAMPLE_TEST_TONE_FRAME_SAMPLES (320)
#define EXAMPLE_TEST_TONE_AMPLITUDE (10000)

/***********************************************************
***********************typedef define***********************
***********************************************************/
typedef enum {
    RECORDER_STATUS_IDLE = 0,
    RECORDER_STATUS_START,
    RECORDER_STATUS_RECORDING,
    RECORDER_STATUS_END,
    RECORDER_STATUS_PLAYING,
} RECORDER_STATUS_E;

/***********************************************************
***********************variable define**********************
***********************************************************/
static volatile RECORDER_STATUS_E sg_recorder_status = RECORDER_STATUS_IDLE;
static volatile uint8_t sg_record_stop_requested = 0;
static TDL_AUDIO_HANDLE_T sg_audio_hdl = NULL;
static TDL_AUDIO_INFO_T sg_audio_info = {0};
static TUYA_RINGBUFF_T sg_recorder_pcm_rb = NULL;
static volatile uint32_t sg_recorded_bytes = 0;
static volatile uint32_t sg_recorded_peak = 0;
static volatile uint32_t sg_recorded_abs_sum = 0;
static volatile uint32_t sg_recorded_samples = 0;

/***********************************************************
***********************function define**********************
***********************************************************/
static void __example_record_start(void)
{
    if (sg_recorder_status == RECORDER_STATUS_IDLE) {
        sg_recorded_bytes = 0;
        sg_recorded_peak = 0;
        sg_recorded_abs_sum = 0;
        sg_recorded_samples = 0;
        sg_record_stop_requested = 0;
        sg_recorder_status = RECORDER_STATUS_START;
    } else {
        PR_WARN("Please wait status IDLE");
    }
}

static void __example_record_stop(void)
{
    if (sg_recorder_status == RECORDER_STATUS_START ||
        sg_recorder_status == RECORDER_STATUS_RECORDING) {
        /* Latch release so the main loop cannot overwrite a fast START->END. */
        sg_record_stop_requested = 1;
    }
}

#if defined(ENABLE_BUTTON) && (ENABLE_BUTTON == 1)
static void __button_function_cb(char *name, TDL_BUTTON_TOUCH_EVENT_E event, void *argc)
{
    switch (event) {
    case TDL_BUTTON_PRESS_DOWN: {
        PR_NOTICE("%s: start recording", name);
        __example_record_start();
    } break;

    case TDL_BUTTON_PRESS_UP: {
        PR_NOTICE("%s: release", name);
        __example_record_stop();
    } break;

    default:
        break;
    }
}
#endif

static void __example_play_from_recorder_rb(void)
{
    if (NULL == sg_recorder_pcm_rb || NULL == sg_audio_hdl ||\
       0 == sg_audio_info.frame_size) {
        return;
    }

    uint32_t data_len = tuya_ring_buff_used_size_get(sg_recorder_pcm_rb);
    if (data_len == 0) {
        PR_NOTICE("No data in recorder ring buffer");
        return;
    }

    uint32_t out_len = 0;
    uint32_t queued_bytes = 0;
    uint32_t backpressure_retries = 0;
    uint8_t *frame_buf = tal_malloc(sg_audio_info.frame_size);
    if (NULL == frame_buf) {
        PR_ERR("tkl_system_psram_malloc failed");
        return;
    }

    do {
        memset(frame_buf, 0, sg_audio_info.frame_size);
        out_len = 0;

        data_len = tuya_ring_buff_used_size_get(sg_recorder_pcm_rb);
        if (data_len == 0) {
            PR_NOTICE("Recorder ring buffer drained");
            break;
        }

        if (data_len >sg_audio_info.frame_size) {
            tuya_ring_buff_read(sg_recorder_pcm_rb, frame_buf, sg_audio_info.frame_size);
            out_len = sg_audio_info.frame_size;
        } else {
            tuya_ring_buff_read(sg_recorder_pcm_rb, frame_buf, data_len);
            out_len = data_len;
        }

        OPERATE_RET rt;
        uint32_t retry_count = 0;
        do {
            rt = tdl_audio_play(sg_audio_hdl, frame_buf, out_len);
            if (rt == OPRT_BUFFER_NOT_ENOUGH) {
                backpressure_retries++;
                tal_system_sleep(2);
            }
        } while (rt == OPRT_BUFFER_NOT_ENOUGH && ++retry_count < 500);
        if (rt != OPRT_OK) {
            PR_ERR("playback submit failed: ret=%d queued=%u pending=%u retries=%u",
                   rt, (unsigned int)queued_bytes,
                   (unsigned int)tuya_ring_buff_used_size_get(sg_recorder_pcm_rb),
                   (unsigned int)backpressure_retries);
            break;
        }
        queued_bytes += out_len;
    } while (1);

    if (frame_buf) {
        tal_free(frame_buf);
        frame_buf = NULL;
    }

    PR_NOTICE("Playback data queued: bytes=%u backpressure_retries=%u",
              (unsigned int)queued_bytes, (unsigned int)backpressure_retries);
}

static OPERATE_RET __example_play_startup_tone(void)
{
    static const int16_t sine_1khz[16] = {
        0, 12539, 23170, 30274, 32767, 30274, 23170, 12539,
        0, -12539, -23170, -30274, -32767, -30274, -23170, -12539,
    };
    static int16_t tone_frame[EXAMPLE_TEST_TONE_FRAME_SAMPLES];
    const uint32_t frame_size = sizeof(tone_frame);
    uint32_t frame_count;
    uint32_t total_samples;
    uint32_t frame_index;
    uint32_t queued_bytes = 0;

    if (sg_audio_hdl == NULL || sg_audio_info.sample_tm_ms == 0 ||
        sg_audio_info.frame_size != frame_size) {
        PR_ERR("SPK test tone format mismatch: frame=%u expected=%u sample_tm=%u",
               (unsigned int)sg_audio_info.frame_size, (unsigned int)frame_size,
               (unsigned int)sg_audio_info.sample_tm_ms);
        return OPRT_INVALID_PARM;
    }

    frame_count = EXAMPLE_TEST_TONE_DURATION_MS / sg_audio_info.sample_tm_ms;
    if (frame_count == 0) {
        PR_ERR("SPK test tone has no frames: sample_tm=%u",
               (unsigned int)sg_audio_info.sample_tm_ms);
        return OPRT_INVALID_PARM;
    }
    total_samples = frame_count * EXAMPLE_TEST_TONE_FRAME_SAMPLES;
    PR_NOTICE("Onboard SPK test tone: 1 kHz, %u ms, PCM frame=%u",
              (unsigned int)EXAMPLE_TEST_TONE_DURATION_MS, (unsigned int)frame_size);

    for (frame_index = 0; frame_index < frame_count; ++frame_index) {
        uint32_t sample_index;
        uint32_t sample_base = frame_index * EXAMPLE_TEST_TONE_FRAME_SAMPLES;
        uint32_t retries = 0;
        OPERATE_RET rt;

        for (sample_index = 0; sample_index < EXAMPLE_TEST_TONE_FRAME_SAMPLES; ++sample_index) {
            uint32_t absolute_index = sample_base + sample_index;
            int32_t sample = (int32_t)sine_1khz[absolute_index & 0x0Fu] *
                             EXAMPLE_TEST_TONE_AMPLITUDE / 32767;

            if (absolute_index < 160u) {
                sample = sample * (int32_t)absolute_index / 160;
            } else if (absolute_index >= total_samples - 160u) {
                sample = sample * (int32_t)(total_samples - 1u - absolute_index) / 160;
            }
            tone_frame[sample_index] = (int16_t)sample;
        }

        do {
            rt = tdl_audio_play(sg_audio_hdl, (uint8_t *)tone_frame, frame_size);
            if (rt == OPRT_BUFFER_NOT_ENOUGH) {
                tal_system_sleep(2);
            }
        } while (rt == OPRT_BUFFER_NOT_ENOUGH && ++retries < 500u);

        if (rt != OPRT_OK) {
            PR_ERR("SPK test tone submit failed: ret=%d frame=%u queued=%u",
                   rt, (unsigned int)frame_index, (unsigned int)queued_bytes);
            return rt;
        }
        queued_bytes += frame_size;
    }

    PR_NOTICE("SPK test tone queued: bytes=%u", (unsigned int)queued_bytes);
    return OPRT_OK;
}

static void __example_get_audio_frame(TDL_AUDIO_FRAME_FORMAT_E type, TDL_AUDIO_STATUS_E status,\
                                      uint8_t *data, uint32_t len)
{
    if (RECORDER_STATUS_RECORDING != sg_recorder_status) {
        return;
    }

    if (sg_recorder_pcm_rb) {
        uint32_t free_size = tuya_ring_buff_free_size_get(sg_recorder_pcm_rb);
        if (free_size < len) {
            PR_WARN("recorder ring buffer overflow, free_size:%u, need_size:%u", free_size, len);
            return;
        }
        if (data != NULL && (len % sizeof(int16_t)) == 0) {
            const int16_t *samples = (const int16_t *)data;
            uint32_t sample_count = len / sizeof(int16_t);
            uint32_t index;

            for (index = 0; index < sample_count; ++index) {
                int32_t sample = samples[index];
                uint32_t magnitude = (uint32_t)(sample < 0 ? -sample : sample);
                if (magnitude > sg_recorded_peak) {
                    sg_recorded_peak = magnitude;
                }
                sg_recorded_abs_sum += magnitude;
            }
            sg_recorded_samples += sample_count;
        }
        tuya_ring_buff_write(sg_recorder_pcm_rb, data, len);
        sg_recorded_bytes += len;
    }

    return;
}

static OPERATE_RET __example_audio_open(void)
{
    OPERATE_RET rt = OPRT_OK;
    uint32_t buf_len = 0;

    TUYA_CALL_ERR_RETURN(tdl_audio_find(AUDIO_CODEC_NAME, &sg_audio_hdl));
    /* Set the requested startup volume before opening the DAC channel. */
    TUYA_CALL_ERR_RETURN(tdl_audio_volume_set(sg_audio_hdl, 80));
    TUYA_CALL_ERR_RETURN(tdl_audio_open(sg_audio_hdl, __example_get_audio_frame));
    TUYA_CALL_ERR_RETURN(tdl_audio_get_info(sg_audio_hdl, &sg_audio_info));
    if(0 == sg_audio_info.frame_size || 0 == sg_audio_info.sample_tm_ms) {
        PR_ERR("get audio info err");
        return OPRT_INVALID_PARM;
    }

    buf_len = (EXAMPLE_RECORD_DURATION_MS / sg_audio_info.sample_tm_ms) * sg_audio_info.frame_size;
    TUYA_CALL_ERR_RETURN(tuya_ring_buff_create(buf_len, \
                                               OVERFLOW_PSRAM_STOP_TYPE, &sg_recorder_pcm_rb));

    PR_NOTICE("__example_audio_open success");

    return OPRT_OK;
}


/**
 * @brief user_main
 *
 * @return int
 */
int user_main()
{
    OPERATE_RET rt = OPRT_OK;

    tal_log_init(TAL_LOG_LEVEL_DEBUG, 1024, (TAL_LOG_OUTPUT_CB)tkl_log_output);

    PR_NOTICE("Application information:");
    PR_NOTICE("Project name:        %s", PROJECT_NAME);
    PR_NOTICE("App version:         %s", PROJECT_VERSION);
    PR_NOTICE("Compile time:        %s", __DATE__);
    PR_NOTICE("TuyaOpen version:    %s", OPEN_VERSION);
    PR_NOTICE("TuyaOpen commit-id:  %s", OPEN_COMMIT);
    PR_NOTICE("Platform chip:       %s", PLATFORM_CHIP);
    PR_NOTICE("Platform board:      %s", PLATFORM_BOARD);
    PR_NOTICE("Platform commit-id:  %s", PLATFORM_COMMIT);

    /* hardware register */
    rt = board_register_hardware();
    if (rt != OPRT_OK) {
        PR_ERR("board_register_hardware failed: %d", rt);
        return rt;
    }

    rt = __example_audio_open();
    if (rt != OPRT_OK) {
        PR_ERR("audio open failed: %d", rt);
        return rt;
    }

    rt = __example_play_startup_tone();
    if (rt != OPRT_OK) {
        PR_WARN("Startup SPK test tone failed: %d; microphone loopback remains available", rt);
    }

#if defined(ENABLE_BUTTON) && (ENABLE_BUTTON == 1)
    // button create
    TDL_BUTTON_CFG_T button_cfg = {.long_start_valid_time = 3000,
                                   .long_keep_timer = 1000,
                                   .button_debounce_time = 50,
                                   .button_repeat_valid_count = 0,
                                   .button_repeat_valid_time = 500};
    TDL_BUTTON_HANDLE button_hdl = NULL;

    rt = tdl_button_create(BUTTON_NAME, &button_cfg, &button_hdl);
    if (rt != OPRT_OK) {
        PR_ERR("tdl_button_create failed: name=%s ret=%d", BUTTON_NAME, rt);
        return rt;
    }

    tdl_button_event_register(button_hdl, TDL_BUTTON_PRESS_DOWN, __button_function_cb);
    tdl_button_event_register(button_hdl, TDL_BUTTON_PRESS_UP, __button_function_cb);
    PR_NOTICE("Tuya Button ready: %s", BUTTON_NAME);
#endif

    while(1) {
        switch(sg_recorder_status) {
            case RECORDER_STATUS_START:
                PR_NOTICE("Start recording");
                sg_recorder_status = sg_record_stop_requested ? RECORDER_STATUS_END : RECORDER_STATUS_RECORDING;
                break;

            case RECORDER_STATUS_RECORDING:
                if (sg_record_stop_requested) {
                    sg_recorder_status = RECORDER_STATUS_END;
                }
                break;

            case RECORDER_STATUS_END:
                PR_NOTICE("End recording, captured %u bytes; MIC samples=%u peak=%u mean_abs=%u",
                          (unsigned int)sg_recorded_bytes, (unsigned int)sg_recorded_samples,
                          (unsigned int)sg_recorded_peak,
                          sg_recorded_samples ? (unsigned int)(sg_recorded_abs_sum / sg_recorded_samples) : 0u);
                sg_recorder_status = RECORDER_STATUS_PLAYING;
                break;

            case RECORDER_STATUS_PLAYING:
                PR_NOTICE("Start playback submission");
                __example_play_from_recorder_rb();
                PR_NOTICE("Playback submission complete");
                sg_recorder_status = RECORDER_STATUS_IDLE;
                break;
            case RECORDER_STATUS_IDLE:
                tuya_ring_buff_reset(sg_recorder_pcm_rb);
            default:
                break;
        }

        tal_system_sleep(10);
    }

}

/**
 * @brief main
 *
 * @param argc
 * @param argv
 * @return void
 */
#if OPERATING_SYSTEM == SYSTEM_LINUX
void main(int argc, char *argv[])
{
    user_main();
}
#else

/* Tuya thread handle */
static THREAD_HANDLE ty_app_thread = NULL;

/**
 * @brief  task thread
 *
 * @param[in] arg:Parameters when creating a task
 * @return none
 */
static void tuya_app_thread(void *arg)
{
    user_main();

    tal_thread_delete(ty_app_thread);
    ty_app_thread = NULL;
}

void tuya_app_main(void)
{
    THREAD_CFG_T thrd_param = {4096, 4, "tuya_app_main"};
    tal_thread_create_and_start(&ty_app_thread, NULL, NULL, tuya_app_thread, NULL, &thrd_param);
}
#endif

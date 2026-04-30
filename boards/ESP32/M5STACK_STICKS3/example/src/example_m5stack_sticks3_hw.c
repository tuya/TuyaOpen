/**
 * @file example_m5stack_sticks3_hw.c
 * @brief M5Stack StickS3 board hardware bring-up example.
 * @version 0.1
 * @date 2026-04-27
 * @copyright Copyright (c) 2026 Tuya Inc. All Rights Reserved.
 */
#include "tuya_cloud_types.h"

#include "board_com_api.h"
#include "board_config.h"
#include "tal_api.h"
#include "tdl_audio_manage.h"
#include "tdl_button_manage.h"
#include "tkl_output.h"

#include <string.h>

/* ---------------------------------------------------------------------------
 * Macros
 * --------------------------------------------------------------------------- */
#define EXAMPLE_BOOT_DELAY_MS     (5000U)
#define EXAMPLE_SAMPLE_RATE       (16000U)
#define EXAMPLE_CHIRP_DURATION_MS (250U)
#define EXAMPLE_CHIRP_GAP_MS      (1000U)
#define EXAMPLE_CHIRP_COUNT       (3U)
#define EXAMPLE_CHUNK_SAMPLES     (512U)

/* ---------------------------------------------------------------------------
 * File-scope variables
 * --------------------------------------------------------------------------- */
static THREAD_HANDLE s_app_thread    = NULL;
static const int16_t s_sine_1khz[16] = {
    0, 10716, 19800, 25868, 28000, 25868, 19800, 10716, 0, -10716, -19800, -25868, -28000, -25868, -19800, -10716,
};

/* ---------------------------------------------------------------------------
 * Function implementations
 * --------------------------------------------------------------------------- */
/**
 * @brief Get a text name for a button event.
 * @param[in] event button event.
 * @return event name string.
 */
static const char *__button_event_name(TDL_BUTTON_TOUCH_EVENT_E event)
{
    switch (event) {
    case TDL_BUTTON_PRESS_DOWN:
        return "PRESS_DOWN";
    case TDL_BUTTON_PRESS_UP:
        return "PRESS_UP";
    case TDL_BUTTON_PRESS_SINGLE_CLICK:
        return "SINGLE_CLICK";
    case TDL_BUTTON_PRESS_DOUBLE_CLICK:
        return "DOUBLE_CLICK";
    case TDL_BUTTON_PRESS_REPEAT:
        return "REPEAT";
    case TDL_BUTTON_LONG_PRESS_START:
        return "LONG_PRESS_START";
    case TDL_BUTTON_LONG_PRESS_HOLD:
        return "LONG_PRESS_HOLD";
    case TDL_BUTTON_RECOVER_PRESS_UP:
        return "RECOVER_PRESS_UP";
    default:
        return "UNKNOWN";
    }
}

/**
 * @brief Log a StickS3 button event.
 * @param[in] name button name.
 * @param[in] event button event.
 * @param[in] argc event argument.
 * @return none
 */
static void __button_cb(char *name, TDL_BUTTON_TOUCH_EVENT_E event, void *argc)
{
    PR_NOTICE("StickS3 key event: name=%s event=%s arg=%u", name, __button_event_name(event),
              (uint32_t)(uintptr_t)argc);
}

/**
 * @brief Register common button events for one button handle.
 * @param[in] handle button handle.
 * @return none
 */
static void __register_button_events(TDL_BUTTON_HANDLE handle)
{
    tdl_button_event_register(handle, TDL_BUTTON_PRESS_DOWN, __button_cb);
    tdl_button_event_register(handle, TDL_BUTTON_PRESS_UP, __button_cb);
    tdl_button_event_register(handle, TDL_BUTTON_PRESS_SINGLE_CLICK, __button_cb);
    tdl_button_event_register(handle, TDL_BUTTON_PRESS_DOUBLE_CLICK, __button_cb);
    tdl_button_event_register(handle, TDL_BUTTON_PRESS_REPEAT, __button_cb);
    tdl_button_event_register(handle, TDL_BUTTON_LONG_PRESS_START, __button_cb);
    tdl_button_event_register(handle, TDL_BUTTON_LONG_PRESS_HOLD, __button_cb);
    tdl_button_event_register(handle, TDL_BUTTON_RECOVER_PRESS_UP, __button_cb);
}

/**
 * @brief Open StickS3 user buttons and log events.
 * @return OPRT_OK on success, error code on failure.
 */
static OPERATE_RET __open_buttons(void)
{
    OPERATE_RET       rt         = OPRT_OK;
    TDL_BUTTON_HANDLE button_hdl = NULL;
    TDL_BUTTON_CFG_T  button_cfg = {
         .long_start_valid_time     = 800,
         .long_keep_timer           = 1000,
         .button_debounce_time      = 50,
         .button_repeat_valid_count = 3,
         .button_repeat_valid_time  = 500,
    };

    tdl_button_set_task_stack_size(4096);

#if defined(BUTTON_NAME)
    TUYA_CALL_ERR_RETURN(tdl_button_create(BUTTON_NAME, &button_cfg, &button_hdl));
    __register_button_events(button_hdl);
    TUYA_CALL_ERR_LOG(tdl_button_set_ready_flag(BUTTON_NAME, TRUE));
    PR_NOTICE("StickS3 button 1 ready: %s", BUTTON_NAME);
#endif

#if defined(BUTTON_NAME_2)
    button_hdl = NULL;
    TUYA_CALL_ERR_RETURN(tdl_button_create(BUTTON_NAME_2, &button_cfg, &button_hdl));
    __register_button_events(button_hdl);
    TUYA_CALL_ERR_LOG(tdl_button_set_ready_flag(BUTTON_NAME_2, TRUE));
    PR_NOTICE("StickS3 button 2 ready: %s", BUTTON_NAME_2);
#endif

    return rt;
}

/**
 * @brief Log the first microphone frame to prove the codec is receiving.
 * @param[in] type audio frame type.
 * @param[in] status audio status.
 * @param[in] data audio buffer.
 * @param[in] len audio length in bytes.
 * @return none
 */
static void __mic_cb(TDL_AUDIO_FRAME_FORMAT_E type, TDL_AUDIO_STATUS_E status, uint8_t *data, uint32_t len)
{
    static bool s_logged = false;
    (void)type;
    (void)status;
    (void)data;

    if (!s_logged && (len > 0)) {
        s_logged = true;
        PR_NOTICE("StickS3 mic stream active, first frame len=%u", len);
    }
}

/**
 * @brief Stream a tone or silence segment.
 * @param[in] audio_hdl audio handle.
 * @param[in,out] pcm temporary PCM buffer.
 * @param[in] total_samples number of samples to stream.
 * @param[in] tone true to stream 1 kHz tone, false to stream silence.
 * @return OPRT_OK on success, error code on failure.
 */
static OPERATE_RET __stream_audio_segment(TDL_AUDIO_HANDLE_T audio_hdl, int16_t *pcm, uint32_t total_samples, bool tone)
{
    OPERATE_RET rt      = OPRT_OK;
    uint32_t    written = 0;

    while (written < total_samples) {
        uint32_t chunk_samples = total_samples - written;
        if (chunk_samples > EXAMPLE_CHUNK_SAMPLES) {
            chunk_samples = EXAMPLE_CHUNK_SAMPLES;
        }

        if (tone) {
            for (uint32_t i = 0; i < chunk_samples; i++) {
                pcm[i] = s_sine_1khz[(written + i) % CNTSOF(s_sine_1khz)];
            }
        } else {
            memset(pcm, 0, chunk_samples * sizeof(int16_t));
        }

        rt = tdl_audio_play(audio_hdl, (uint8_t *)pcm, chunk_samples * sizeof(int16_t));
        if (rt != OPRT_OK) {
            PR_ERR("StickS3 audio segment play failed rt:%d", rt);
            return rt;
        }
        written += chunk_samples;
    }

    return OPRT_OK;
}

/**
 * @brief Open ES8311 audio and play 1 kHz chirps with one second gaps.
 * @return OPRT_OK on success, error code on failure.
 */
static OPERATE_RET __play_chirps(void)
{
    OPERATE_RET        rt            = OPRT_OK;
    TDL_AUDIO_HANDLE_T audio_hdl     = NULL;
    int16_t           *pcm           = NULL;
    uint32_t           chirp_samples = EXAMPLE_SAMPLE_RATE * EXAMPLE_CHIRP_DURATION_MS / 1000U;
    uint32_t           gap_samples   = EXAMPLE_SAMPLE_RATE * EXAMPLE_CHIRP_GAP_MS / 1000U;

    rt = tdl_audio_find(AUDIO_CODEC_NAME, &audio_hdl);
    if (rt != OPRT_OK) {
        PR_NOTICE("StickS3 audio test skipped: %s not registered", AUDIO_CODEC_NAME);
        return OPRT_OK;
    }

    rt = tdl_audio_open(audio_hdl, __mic_cb);
    if (rt != OPRT_OK) {
        PR_NOTICE("StickS3 audio test skipped: codec open failed rt:%d", rt);
        return OPRT_OK;
    }
    TUYA_CALL_ERR_LOG(tdl_audio_volume_set(audio_hdl, 90));

    pcm = (int16_t *)tal_malloc(EXAMPLE_CHUNK_SAMPLES * sizeof(int16_t));
    if (pcm == NULL) {
        return OPRT_MALLOC_FAILED;
    }

    PR_NOTICE("StickS3 audio test: playing %u x 1kHz chirps, %ums gap", EXAMPLE_CHIRP_COUNT, EXAMPLE_CHIRP_GAP_MS);
    for (uint32_t chirp = 0; chirp < EXAMPLE_CHIRP_COUNT; chirp++) {
        rt = __stream_audio_segment(audio_hdl, pcm, chirp_samples, true);
        if (rt != OPRT_OK) {
            break;
        }
        if (chirp + 1U < EXAMPLE_CHIRP_COUNT) {
            rt = __stream_audio_segment(audio_hdl, pcm, gap_samples, false);
            if (rt != OPRT_OK) {
                break;
            }
        }
    }

    tal_free(pcm);
    PR_NOTICE("StickS3 audio test done");

    return rt;
}

/**
 * @brief Print common application information.
 * @return none
 */
static void __print_app_info(void)
{
    PR_NOTICE("Application information:");
    PR_NOTICE("Project name:        %s", PROJECT_NAME);
    PR_NOTICE("App version:         %s", PROJECT_VERSION);
    PR_NOTICE("Compile time:        %s", __DATE__);
    PR_NOTICE("TuyaOpen version:    %s", OPEN_VERSION);
    PR_NOTICE("TuyaOpen commit-id:  %s", OPEN_COMMIT);
    PR_NOTICE("Platform chip:       %s", PLATFORM_CHIP);
    PR_NOTICE("Platform board:      %s", PLATFORM_BOARD);
    PR_NOTICE("Platform commit-id:  %s", PLATFORM_COMMIT);
}

/**
 * @brief Run the StickS3 hardware bring-up example.
 * @return none
 */
void user_main(void)
{
    OPERATE_RET rt = OPRT_OK;

    tal_system_sleep(EXAMPLE_BOOT_DELAY_MS);
    tal_log_init(TAL_LOG_LEVEL_DEBUG, 1024, (TAL_LOG_OUTPUT_CB)tkl_log_output);
    __print_app_info();

    PR_NOTICE("StickS3 board_register_hardware begin");
    rt = board_register_hardware();
    if (rt != OPRT_OK) {
        PR_ERR("StickS3 board_register_hardware failed rt:%d", rt);
    } else {
        PR_NOTICE("StickS3 board_register_hardware success");
    }

    TUYA_CALL_ERR_LOG(board_display_show_red_box());

    TUYA_CALL_ERR_LOG(__open_buttons());
    TUYA_CALL_ERR_LOG(__play_chirps());

    PR_NOTICE("StickS3 hardware example idle; press KEY1/KEY2 to log events");
    for (;;) {
        PR_NOTICE("StickS3 hardware example heartbeat, heap=%u", tal_system_get_free_heap_size());
        tal_system_sleep(10000);
    }
}

#if OPERATING_SYSTEM == SYSTEM_LINUX
/**
 * @brief Linux entry point.
 * @param[in] argc argument count.
 * @param[in] argv argument vector.
 * @return none
 */
void main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;
    user_main();
}
#else
/**
 * @brief TuyaOpen application task.
 * @param[in] arg unused task argument.
 * @return none
 */
static void __app_thread(void *arg)
{
    (void)arg;
    user_main();
    tal_thread_delete(s_app_thread);
    s_app_thread = NULL;
}

/**
 * @brief TuyaOpen ESP application entry.
 * @return none
 */
void tuya_app_main(void)
{
    THREAD_CFG_T thrd_param = {
        .stackDepth = 1024 * 6,
        .priority   = THREAD_PRIO_1,
        .thrdname   = "sticks3_hw",
    };

    tal_thread_create_and_start(&s_app_thread, NULL, NULL, __app_thread, NULL, &thrd_param);
}
#endif

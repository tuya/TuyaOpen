/**
 * @file example_vad.c
 * @version 0.1
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#include "tuya_cloud_types.h"
#include "tal_api.h"

#include "tkl_output.h"
#include "tkl_vad.h"
#include "tdl_audio_manage.h"
#include "board_com_api.h"
/***********************************************************
************************macro define************************
***********************************************************/


/***********************************************************
***********************typedef define***********************
***********************************************************/


/***********************************************************
***********************variable define**********************
***********************************************************/

/***********************************************************
***********************function define**********************
***********************************************************/
static OPERATE_RET __example_vad_init(void)
{
    OPERATE_RET rt = OPRT_OK;

    TKL_VAD_CONFIG_T vad_config;
    vad_config.sample_rate = 16000;
    vad_config.channel_num = 1;
    vad_config.speech_min_ms = 300;
    vad_config.noise_min_ms = 500;
    vad_config.scale = 1.0;
    vad_config.frame_duration_ms = 10;

    TUYA_CALL_ERR_RETURN(tkl_vad_init(&vad_config));

    TUYA_CALL_ERR_RETURN(tkl_vad_start());

    PR_NOTICE("__example_vad_init success");

    return OPRT_OK;
}

static void __example_get_audio_frame(TDL_AUDIO_FRAME_FORMAT_E type, TDL_AUDIO_STATUS_E status,\
                                      uint8_t *data, uint32_t len)
{
    tkl_vad_feed(data, len);
}

static OPERATE_RET __example_audio_open(void)
{
    OPERATE_RET rt = OPRT_OK;

    TDL_AUDIO_HANDLE_T audio_hdl = NULL;

    TUYA_CALL_ERR_RETURN(tdl_audio_find(AUDIO_CODEC_NAME, &audio_hdl));
    TUYA_CALL_ERR_RETURN(tdl_audio_open(audio_hdl, __example_get_audio_frame));

    PR_NOTICE("__example_audio_open success");

    return OPRT_OK;
}

void user_main(void)
{
    static TKL_VAD_STATUS_T last_state = TKL_VAD_STATUS_NONE;
    TKL_VAD_STATUS_T state = TKL_VAD_STATUS_NONE;

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

    /*hardware register*/
    board_register_hardware();

    __example_audio_open();

    __example_vad_init();

    while(1) {
        state = tkl_vad_get_status();

        if(last_state != state) {
            if(TKL_VAD_STATUS_SPEECH == state) {
                PR_DEBUG("VAD status: SPEECH");
            } else if(TKL_VAD_STATUS_NONE == state) {
                PR_DEBUG("VAD status: NONE");
            }
        }
        last_state = state;

        tal_system_sleep(10);
    }


    return;
}

#if OPERATING_SYSTEM == SYSTEM_LINUX

/**
 * @brief main
 *
 * @param argc
 * @param argv
 * @return void
 */
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
    THREAD_CFG_T thrd_param = {0};
    thrd_param.stackDepth = 1024 * 4;
    thrd_param.priority = THREAD_PRIO_1;
    thrd_param.thrdname = "tuya_app_main";
    tal_thread_create_and_start(&ty_app_thread, NULL, NULL, tuya_app_thread, NULL, &thrd_param);
}
#endif
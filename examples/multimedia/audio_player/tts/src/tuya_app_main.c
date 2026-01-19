/**
 * @file audio_player.c
 * @brief Audio speaker playback example for MP3 audio playback
 *
 * This file demonstrates MP3 audio playback functionality using the Tuya SDK.
 * It includes MP3 decoding, audio output configuration, and playback control.
 * The example supports multiple audio sources including embedded C arrays,
 * internal flash storage, and SD card files.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */
#include "tal_api.h"
#include "tkl_output.h"
#include "svc_ai_player.h"
#include "tdl_audio_manage.h"
#include "board_com_api.h"
#include "app_media.h"

/***********************************************************
************************macro define************************
***********************************************************/

#define MP3_FILE_ARRAY          media_src_hello_tuya_16k

/***********************************************************
***********************typedef define***********************
***********************************************************/


/***********************************************************
********************function declaration********************
***********************************************************/

/***********************************************************
***********************variable define**********************
***********************************************************/
static TDL_AUDIO_HANDLE_T sg_audio_hdl = NULL;

static AI_PLAYER_HANDLE sg_player = NULL;
static AI_PLAYLIST_HANDLE sg_playlist = NULL;

/***********************************************************
***********************function define**********************
***********************************************************/

static OPERATE_RET __example_audio_player_init(void)
{
    OPERATE_RET rt = OPRT_OK;
    
    // player init
    AI_PLAYER_CFG_T cfg = {.sample = 16000, .datebits = 16, .channel = 1};
    TUYA_CALL_ERR_RETURN(tuya_ai_player_service_init(&cfg));

    //! tts player
    TUYA_CALL_ERR_RETURN(tuya_ai_player_create(AI_PLAYER_MODE_FOREGROUND, &sg_player));

    AI_PLAYLIST_CFG_T ton_cfg = {.auto_play = true,.capacity = 2};
    TUYA_CALL_ERR_RETURN(tuya_ai_playlist_create(sg_player, &ton_cfg, &sg_playlist));

    return rt;
}

static void __example_get_audio_frame(TDL_AUDIO_FRAME_FORMAT_E type, TDL_AUDIO_STATUS_E status,\
                                      uint8_t *data, uint32_t len)
{
    return;
}

static OPERATE_RET __example_audio_open(void)
{
    OPERATE_RET rt = OPRT_OK;

    TUYA_CALL_ERR_RETURN(tdl_audio_find(AUDIO_CODEC_NAME, &sg_audio_hdl));
    TUYA_CALL_ERR_RETURN(tdl_audio_open(sg_audio_hdl, __example_get_audio_frame));

    PR_NOTICE("__example_audio_open success");

return OPRT_OK;
}

static OPERATE_RET __example_audio_play_data(AI_AUDIO_CODEC_E format, uint8_t *data, uint32_t len)
{
    OPERATE_RET rt = OPRT_OK;

    TUYA_CALL_ERR_LOG(tuya_ai_playlist_stop(sg_playlist));

    if (data && len > 0) {
        PR_NOTICE("player tts data len %d\r\n", len);
        TUYA_CALL_ERR_LOG(tuya_ai_player_start(sg_player, AI_PLAYER_SRC_MEM, NULL, format));
        TUYA_CALL_ERR_LOG(tuya_ai_player_feed(sg_player, (uint8_t *)data, len));
        TUYA_CALL_ERR_LOG(tuya_ai_player_feed(sg_player, NULL, 0));
    } 

    return rt;
}

void user_main(void)
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

    /*hardware register*/
    TUYA_CALL_ERR_LOG(board_register_hardware());

    TUYA_CALL_ERR_LOG(__example_audio_open());

    TUYA_CALL_ERR_LOG(__example_audio_player_init());

    while(1) {
        __example_audio_play_data(AI_AUDIO_CODEC_MP3, (uint8_t *)MP3_FILE_ARRAY, sizeof(MP3_FILE_ARRAY));
        tal_system_sleep(3 * 1000);
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
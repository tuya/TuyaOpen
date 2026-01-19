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

#include "netmgr.h"
#if defined(ENABLE_WIFI) && (ENABLE_WIFI == 1)
#include "netconn_wifi.h"
#endif
#if defined(ENABLE_WIRED) && (ENABLE_WIRED == 1)
#include "netconn_wired.h"
#endif
#include "tuya_register_center.h"
#include "svc_ai_player.h"

#include "tdl_audio_manage.h"
#include "board_com_api.h"

/***********************************************************
************************macro define************************
***********************************************************/
#ifdef ENABLE_WIFI
#define DEFAULT_WIFI_SSID "SSID_XXXX"
#define DEFAULT_WIFI_PSWD "PSWD_XXXX"
#endif

#define MUSIC_URL "https://XXXXXX.mp3"
#define MUSIC_FORMAT AI_AUDIO_CODEC_MP3

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
static bool is_network_connected = false;

/***********************************************************
***********************function define**********************
***********************************************************/

static OPERATE_RET __example_audio_player_init(void)
{
    OPERATE_RET rt = OPRT_OK;
    
    // player init
    AI_PLAYER_CFG_T cfg = {.sample = 16000, .datebits = 16, .channel = 1};
    TUYA_CALL_ERR_RETURN(tuya_ai_player_service_init(&cfg));

    //! music player
    TUYA_CALL_ERR_RETURN(tuya_ai_player_create(AI_PLAYER_MODE_FOREGROUND, &sg_player));

    AI_PLAYLIST_CFG_T ton_cfg = {.loop = 1, .auto_play = true,.capacity = 2};
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

OPERATE_RET __link_status_cb(void *data)
{
    netmgr_status_e status = (netmgr_status_e)data;

    if (NETMGR_LINK_UP == status || NETMGR_LINK_UP_SWITH == status) {
        is_network_connected = true;
    } else {
        is_network_connected = false;
    }

    return OPRT_OK;
}

static OPERATE_RET __example_sys_net_init(void)
{
    tal_kv_init(&(tal_kv_cfg_t){
        .seed = "vmlkasdh93dlvlcy",
        .key = "dflfuap134ddlduq",
    });
    tal_sw_timer_init();
    tal_workq_init();
    tuya_tls_init();
    tuya_register_center_init();
    tal_event_subscribe(EVENT_LINK_STATUS_CHG, "https_client", __link_status_cb, SUBSCRIBE_TYPE_NORMAL);

#if defined(ENABLE_LIBLWIP) && (ENABLE_LIBLWIP == 1)
    TUYA_LwIP_Init();
#endif

    // network init
    netmgr_type_e type = 0;
#if defined(ENABLE_WIFI) && (ENABLE_WIFI == 1)
    type |= NETCONN_WIFI;
#endif
#if defined(ENABLE_WIRED) && (ENABLE_WIRED == 1)
    type |= NETCONN_WIRED;
#endif
    netmgr_init(type);

#if defined(ENABLE_WIFI) && (ENABLE_WIFI == 1)
    // connect wifi
    netconn_wifi_info_t wifi_info = {0};
    strcpy(wifi_info.ssid, DEFAULT_WIFI_SSID);
    strcpy(wifi_info.pswd, DEFAULT_WIFI_PSWD);
    netmgr_conn_set(NETCONN_WIFI, NETCONN_CMD_SSID_PSWD, &wifi_info);
#endif   

    return OPRT_OK;
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

    TUYA_CALL_ERR_LOG(__example_sys_net_init());

    /*hardware register*/
    TUYA_CALL_ERR_LOG(board_register_hardware());

    TUYA_CALL_ERR_LOG(__example_audio_open());

    TUYA_CALL_ERR_LOG(__example_audio_player_init());

    static bool is_playing = false;
    while(1) {

        if(is_network_connected && is_playing == false) {
            TUYA_CALL_ERR_LOG(tuya_ai_playlist_add(sg_playlist, AI_PLAYER_SRC_URL,\
                                                   MUSIC_URL, MUSIC_FORMAT));
            if(rt == OPRT_OK) {
                is_playing = true;
            }
        }

        tal_system_sleep(1*1000);
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
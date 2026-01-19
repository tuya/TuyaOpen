/**
 * @file tuya_main.c
 * @brief Implements main audio functionality for IoT device
 *
 * This source file provides the implementation of the main audio functionalities
 * required for an IoT device. It includes functionality for audio processing,
 * device initialization, event handling, and network communication. The
 * implementation supports audio volume control, data point processing, and
 * interaction with the Tuya IoT platform. This file is essential for developers
 * working on IoT applications that require audio capabilities and integration
 * with the Tuya IoT ecosystem.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 *
 */

#include "tuya_cloud_types.h"

#include <assert.h>
#include "cJSON.h"
#include "tal_api.h"
#include "tuya_config.h"
#include "tuya_iot.h"
#include "tuya_iot_dp.h"
#include "netmgr.h"
#include "tkl_output.h"
#include "tal_cli.h"
#include "tuya_authorize.h"
#if defined(ENABLE_WIFI) && (ENABLE_WIFI == 1)
#include "netconn_wifi.h"
#endif
#if defined(ENABLE_WIRED) && (ENABLE_WIRED == 1)
#include "netconn_wired.h"
#endif
#if defined(ENABLE_LIBLWIP) && (ENABLE_LIBLWIP == 1)
#include "lwip_init.h"
#endif

#include "board_com_api.h"

#include "app_chat_bot.h"
#include "app_clock.h"
#include "reset_netcfg.h"
#include "app_servo.h"
#include "app_gesture.h"

/* Tuya device handle */
tuya_iot_client_t ai_client;

/* Tuya license information (uuid authkey) */
tuya_iot_license_t license;

#ifndef PROJECT_VERSION
#define PROJECT_VERSION "1.0.0"
#endif

#define DPID_VOLUME 3
#define DPID_SERVO  5

bool _s_servo_busy = FALSE;
static uint8_t _need_reset = 0;
static SERVO_ACTION_E _s_servo_action = SERVO_CENTER;

/**
 * @brief user defined log output api, in this demo, it will use uart0 as log-tx
 *
 * @param str log string
 * @return void
 */
void user_log_output_cb(const char *str)
{
    tal_uart_write(TUYA_UART_NUM_0, (const uint8_t *)str, strlen(str));
}

/**
 * @brief user defined upgrade notify callback, it will notify device a OTA request received
 *
 * @param client device info
 * @param upgrade the upgrade request info
 * @return void
 */
void user_upgrade_notify_on(tuya_iot_client_t *client, cJSON *upgrade)
{
    PR_INFO("----- Upgrade information -----");
    PR_INFO("OTA Channel: %d", cJSON_GetObjectItem(upgrade, "type")->valueint);
    PR_INFO("Version: %s", cJSON_GetObjectItem(upgrade, "version")->valuestring);
    PR_INFO("Size: %s", cJSON_GetObjectItem(upgrade, "size")->valuestring);
    PR_INFO("MD5: %s", cJSON_GetObjectItem(upgrade, "md5")->valuestring);
    PR_INFO("HMAC: %s", cJSON_GetObjectItem(upgrade, "hmac")->valuestring);
    PR_INFO("URL: %s", cJSON_GetObjectItem(upgrade, "url")->valuestring);
    PR_INFO("HTTPS URL: %s", cJSON_GetObjectItem(upgrade, "httpsUrl")->valuestring);
}


static void __servo_control_wk_cb(void *data)
{
    PR_DEBUG("Servo action: %d", _s_servo_action);
    
    // Trigger corresponding emoji expression for servo movement
#if defined(ENABLE_COMP_AI_DISPLAY) && (ENABLE_COMP_AI_DISPLAY == 1)
    switch (_s_servo_action) {
    case SERVO_UP:
        ai_ui_disp_msg(AI_UI_DISP_EMOTION, (uint8_t *)EMOJI_HAPPY, strlen(EMOJI_HAPPY));
        break;
    case SERVO_DOWN:
        ai_ui_disp_msg(AI_UI_DISP_EMOTION, (uint8_t *)EMOJI_SAD, strlen(EMOJI_SAD));
        break;
    case SERVO_LEFT:
        ai_ui_disp_msg(AI_UI_DISP_EMOTION, (uint8_t *)EMOJI_LEFT, strlen(EMOJI_LEFT));
        break;
    case SERVO_RIGHT:
        ai_ui_disp_msg(AI_UI_DISP_EMOTION, (uint8_t *)EMOJI_RIGHT, strlen(EMOJI_RIGHT));
        break;
    case SERVO_CENTER:
        ai_ui_disp_msg(AI_UI_DISP_EMOTION, (uint8_t *)EMOJI_NEUTRAL, strlen(EMOJI_NEUTRAL));
        break;
    case SERVO_NOD:
        ai_ui_disp_msg(AI_UI_DISP_EMOTION, (uint8_t *)EMOJI_WAKEUP, strlen(EMOJI_WAKEUP));
        break;
    default:
        break;
    }
#endif
    
    _s_servo_busy = TRUE;
    ai_ui_disp_msg(AI_UI_DISP_PAUSE_EMOJI_CYCLE, NULL, 0);
    app_servo_move(_s_servo_action);
    ai_ui_disp_msg(AI_UI_DISP_RESUME_EMOJI_CYCLE, NULL, 0);
    _s_servo_busy = FALSE;
}

static void __gesture_detect_cb(GESTURE_TYPE_E gesture)
{
    PR_DEBUG("Gesture detected: %d", gesture);

    // Hide weather clock and show emoji mode
#if defined(ENABLE_COMP_AI_DISPLAY) && (ENABLE_COMP_AI_DISPLAY == 1)
    ai_ui_disp_msg(AI_UI_DISP_EMOJI_UI_SHOW, NULL, 0);
    
    // Trigger corresponding emoji expression
    switch (gesture) {
    case GESTURE_RIGHT:
        ai_ui_disp_msg(AI_UI_DISP_EMOTION, (uint8_t *)EMOJI_RIGHT, strlen(EMOJI_RIGHT));
        _s_servo_action = SERVO_RIGHT;
        break;
    case GESTURE_LEFT:
        ai_ui_disp_msg(AI_UI_DISP_EMOTION, (uint8_t *)EMOJI_LEFT, strlen(EMOJI_LEFT));
        _s_servo_action = SERVO_LEFT;
        break;
    case GESTURE_UP:
        ai_ui_disp_msg(AI_UI_DISP_EMOTION, (uint8_t *)EMOJI_HAPPY, strlen(EMOJI_HAPPY));
        _s_servo_action = SERVO_UP;
        break;
    case GESTURE_DOWN:
        ai_ui_disp_msg(AI_UI_DISP_EMOTION, (uint8_t *)EMOJI_SAD, strlen(EMOJI_SAD));
        _s_servo_action = SERVO_DOWN;
        break;
    case GESTURE_CLOCKWISE:
        ai_ui_disp_msg(AI_UI_DISP_EMOTION, (uint8_t *)EMOJI_SURPRISE, strlen(EMOJI_SURPRISE));
        _s_servo_action = SERVO_CLOCKWISE; 
        break;
    case GESTURE_ANTICLOCKWISE:
        ai_ui_disp_msg(AI_UI_DISP_EMOTION, (uint8_t *)EMOJI_ANGRY, strlen(EMOJI_ANGRY));
        _s_servo_action = SERVO_ANTICLOCKWISE; 
        break;
    case GESTURE_FORWARD:
        ai_ui_disp_msg(AI_UI_DISP_EMOTION, (uint8_t *)EMOJI_WAKEUP, strlen(EMOJI_WAKEUP));
        _s_servo_action = SERVO_NOD;
        break;
    case GESTURE_BACKWARD:
        ai_ui_disp_msg(AI_UI_DISP_EMOTION, (uint8_t *)EMOJI_SLEEP, strlen(EMOJI_SLEEP));
        _s_servo_action = SERVO_CENTER;
        break;
    // Add fun expressions for special gestures
    case GESTURE_WAVE:
        ai_ui_disp_msg(AI_UI_DISP_EMOTION, (uint8_t *)EMOJI_WINK, strlen(EMOJI_WINK));
        break;
    default:
        return;
    }
    
    // Return to weather clock is now handled by emoji rotation counter
    // No need for separate timer - emoji UI will send return message when all emotions are rotated
#else
    switch (gesture) {
    case GESTURE_RIGHT:
        _s_servo_action = SERVO_RIGHT;
        break;
    case GESTURE_LEFT:
        _s_servo_action = SERVO_LEFT;
        break;
    case GESTURE_UP:
        _s_servo_action = SERVO_UP;
        break;
    case GESTURE_DOWN:
        _s_servo_action = SERVO_DOWN;
        break;
    case GESTURE_CLOCKWISE:
        _s_servo_action = SERVO_CLOCKWISE; 
        break;
    case GESTURE_ANTICLOCKWISE:
        _s_servo_action = SERVO_ANTICLOCKWISE; 
        break;
    case GESTURE_FORWARD:
        _s_servo_action = SERVO_NOD;
        break;
    case GESTURE_BACKWARD:
        _s_servo_action = SERVO_CENTER;
        break;
    default:
        return;
    }
#endif

    if (!_s_servo_busy) {
        tal_workq_schedule(WORKQ_SYSTEM, __servo_control_wk_cb, NULL);
    }
}

OPERATE_RET audio_dp_obj_proc(dp_obj_recv_t *dpobj)
{
    uint32_t index = 0;
    for (index = 0; index < dpobj->dpscnt; index++) {
        dp_obj_t *dp = dpobj->dps + index;
        PR_DEBUG("idx:%d dpid:%d type:%d ts:%u", index, dp->id, dp->type, dp->time_stamp);

        switch (dp->id) {
        case DPID_VOLUME: {
            uint8_t volume = dp->value.dp_value;
            PR_DEBUG("volume:%d", volume);
            ai_chat_set_volume(volume);
            break;
        }
        case DPID_SERVO: {
            uint8_t servo_action = dp->value.dp_value;
            PR_DEBUG("servo action:%d", servo_action);
            if (servo_action < SERVO_MAX) {
                _s_servo_action = (SERVO_ACTION_E)servo_action;
                if (!_s_servo_busy) {
                    tal_workq_schedule(WORKQ_SYSTEM, __servo_control_wk_cb, NULL);
                }
            }
            break;
        }
        default:
            break;
        }
    }

    return OPRT_OK;
}

OPERATE_RET ai_audio_volume_upload(void)
{
    tuya_iot_client_t *client = tuya_iot_client_get();
    dp_obj_t dp_obj = {0};

    uint8_t volume = ai_chat_get_volume();

    dp_obj.id = DPID_VOLUME;
    dp_obj.type = PROP_VALUE;
    dp_obj.value.dp_value = volume;

    PR_DEBUG("DP upload volume:%d", volume);

    return tuya_iot_dp_obj_report(client, client->activate.devid, &dp_obj, 1, 0);
}

/**
 * @brief user defined event handler
 *
 * @param client device info
 * @param event the event info
 * @return void
 */
void user_event_handler_on(tuya_iot_client_t *client, tuya_event_msg_t *event)
{
    PR_DEBUG("Tuya Event ID:%d(%s)", event->id, EVENT_ID2STR(event->id));
    PR_INFO("Device Free heap %d", tal_system_get_free_heap_size());

    switch (event->id) {
    case TUYA_EVENT_BIND_START:
        PR_INFO("Device Bind Start!");
        if (_need_reset == 1) {
            PR_INFO("Device Reset!");
            tal_system_reset();
        }

        #if defined(ENABLE_COMP_AI_AUDIO) && (ENABLE_COMP_AI_AUDIO == 1)
        ai_audio_player_alert(AI_AUDIO_ALERT_NETWORK_CFG);
        #endif
        
        break;
    case TUYA_EVENT_BIND_TOKEN_ON:
        break;

    /* MQTT with tuya cloud is connected, device online */
    case TUYA_EVENT_MQTT_CONNECTED:
        PR_INFO("Device MQTT Connected!");
        tal_event_publish(EVENT_MQTT_CONNECTED, NULL);
        static uint8_t first = 1;
        if (first) {
            first = 0;
            ai_audio_volume_upload();
        }
        break;

    /* MQTT with tuya cloud is disconnected, device offline */
    case TUYA_EVENT_MQTT_DISCONNECT:
        PR_INFO("Device MQTT DisConnected!");
        tal_event_publish(EVENT_MQTT_DISCONNECTED, NULL);
        break;

    /* RECV upgrade request */
    case TUYA_EVENT_UPGRADE_NOTIFY:
        user_upgrade_notify_on(client, event->value.asJSON);
        break;

    /* Sync time with tuya Cloud */
    case TUYA_EVENT_TIMESTAMP_SYNC:
        PR_INFO("Sync timestamp:%d", event->value.asInteger);
        tal_time_set_posix(event->value.asInteger, 1);
        tal_event_publish("app.time.sync", NULL);
        break;

    case TUYA_EVENT_RESET:
        PR_INFO("Device Reset:%d", event->value.asInteger);

        _need_reset = 1;
        break;

    /* RECV OBJ DP */
    case TUYA_EVENT_DP_RECEIVE_OBJ: {
        dp_obj_recv_t *dpobj = event->value.dpobj;
        PR_DEBUG("SOC Rev DP Cmd t1:%d t2:%d CNT:%u", dpobj->cmd_tp, dpobj->dtt_tp, dpobj->dpscnt);
        if (dpobj->devid != NULL) {
            PR_DEBUG("devid.%s", dpobj->devid);
        }

        audio_dp_obj_proc(dpobj);

        tuya_iot_dp_obj_report(client, dpobj->devid, dpobj->dps, dpobj->dpscnt, 0);

    } break;

    /* RECV RAW DP */
    case TUYA_EVENT_DP_RECEIVE_RAW: {
        dp_raw_recv_t *dpraw = event->value.dpraw;
        PR_DEBUG("SOC Rev DP Cmd t1:%d t2:%d", dpraw->cmd_tp, dpraw->dtt_tp);
        if (dpraw->devid != NULL) {
            PR_DEBUG("devid.%s", dpraw->devid);
        }

        uint32_t index = 0;
        dp_raw_t *dp = &dpraw->dp;
        PR_DEBUG("dpid:%d type:RAW len:%d data:", dp->id, dp->len);
        for (index = 0; index < dp->len; index++) {
            PR_DEBUG_RAW("%02x", dp->data[index]);
        }

        tuya_iot_dp_raw_report(client, dpraw->devid, &dpraw->dp, 3);

    } break;

    default:
        break;
    }
}

/**
 * @brief user defined network check callback, it will check the network every 1sec,
 *        in this demo it alwasy return ture due to it's a wired demo
 *
 * @return true
 * @return false
 */
bool user_network_check(void)
{
    netmgr_status_e status = NETMGR_LINK_DOWN;
    netmgr_conn_get(NETCONN_AUTO, NETCONN_CMD_STATUS, &status);
    return status == NETMGR_LINK_DOWN ? false : true;
}

void user_main(void)
{
    int ret = OPRT_OK;

    //! open iot development kit runtim init
#if defined(ENABLE_EXT_RAM) && (ENABLE_EXT_RAM == 1)
    cJSON_InitHooks(&(cJSON_Hooks){.malloc_fn = tal_psram_malloc, .free_fn = tal_psram_free});
#else 
    cJSON_InitHooks(&(cJSON_Hooks){.malloc_fn = tal_malloc, .free_fn = tal_free});
#endif

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

    tal_kv_init(&(tal_kv_cfg_t){
        .seed = "vmlkasdh93dlvlcy",
        .key = "dflfuap134ddlduq",
    });
    tal_sw_timer_init();
    tal_workq_init();
    tal_time_service_init();
    tal_cli_init();
    tuya_authorize_init();

    reset_netconfig_start();

    if (OPRT_OK != tuya_authorize_read(&license)) {
        license.uuid = TUYA_OPENSDK_UUID;
        license.authkey = TUYA_OPENSDK_AUTHKEY;
        PR_WARN("Replace the TUYA_OPENSDK_UUID and TUYA_OPENSDK_AUTHKEY contents, otherwise the demo cannot work.\n \
                Visit https://platform.tuya.com/purchase/index?type=6 to get the open-sdk uuid and authkey.");
    }

    /* Initialize Tuya device configuration */
    ret = tuya_iot_init(&ai_client, &(const tuya_iot_config_t){
                                        .software_ver = PROJECT_VERSION,
                                        .productkey = TUYA_PRODUCT_ID,
                                        .uuid = license.uuid,
                                        .authkey = license.authkey,
                                        // .firmware_key      = TUYA_DEVICE_FIRMWAREKEY,
                                        .event_handler = user_event_handler_on,
                                        .network_check = user_network_check,
                                    });
    assert(ret == OPRT_OK);

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
    netmgr_conn_set(NETCONN_WIFI, NETCONN_CMD_NETCFG, &(netcfg_args_t){.type = NETCFG_TUYA_BLE | NETCFG_TUYA_WIFI_AP});
#endif

    PR_DEBUG("tuya_iot_init success");

    ret = board_register_hardware();
    if (ret != OPRT_OK) {
        PR_ERR("board_register_hardware failed");
    }

    ret = app_chat_bot_init();
    if (ret != OPRT_OK) {
        PR_ERR("app_chat_bot_init failed");
    }

    ret = app_clock_init();
    if (ret != OPRT_OK) {
        PR_ERR("app_clock_init failed");
    }

    /* Start tuya iot task */
    tuya_iot_start(&ai_client);

    tkl_wifi_set_lp_mode(0, 0);

    reset_netconfig_check();

    ret = app_servo_init();
    if (ret != OPRT_OK) {
        PR_ERR("app_servo_init failed: %d", ret);
    }

    ret = app_gesture_init(__gesture_detect_cb);
    if (ret != OPRT_OK) {
        PR_ERR("app_gesture_init failed: %d", ret);
    }

    for (;;) {
        /* Loop to receive packets, and handles client keepalive */
        tuya_iot_yield(&ai_client);
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
    THREAD_CFG_T thrd_param = {0};
    thrd_param.stackDepth = 4096;
    thrd_param.priority = 4;
    thrd_param.thrdname = "tuya_app_main";
    tal_thread_create_and_start(&ty_app_thread, NULL, NULL, tuya_app_thread, NULL, &thrd_param);
}
#endif

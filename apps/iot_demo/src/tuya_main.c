/**
 * @file tuya_main.c
 * @brief tuya_main module is used to manage the Tuya device application.
 *
 * This file provides the implementation of the tuya_main module,
 * which is responsible for managing the Tuya device application.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 *
 * 2025-07-11   yangjie     Add cellular network support.
 */

#include "cJSON.h"
#include "mqtt_bind.h"
#include "tal_api.h"
#include "tal_network.h"
#include "tal_netif.h"
#include "tal_api_ops.h"
#include "tuya_config.h"
#include "tuya_iot.h"
#include "tuya_iot_dp.h"
#include "tuya_lan.h"
#include <assert.h>

#include "reset_netcfg.h"
#include "ota_app.h"

#if defined(ENABLE_QRCODE) && (ENABLE_QRCODE == 1)
#include "qrencode_print.h"
#endif

#ifndef PROJECT_VERSION
#define PROJECT_VERSION "1.0.0"
#endif

/* Tuya device handle */
tuya_iot_client_t client;

/* Tuya license information (uuid authkey) */
tuya_iot_license_t license;

/**
 * @brief user defined log output api, in this demo, it will use uart0 as log-tx
 *
 * @param str log string
 * @return void
 */
void user_log_output_cb(const char *str)
{
    TAL_API_OPS_T *ops = tal_api_get_ops();
    if (NULL != ops && ops->log_output) {
        ops->log_output(str);
    }
}

/**
 * @brief user defined upgrade notify callback, it will notify device a OTA
 * request received
 *
 * @param client device info
 * @param upgrade the upgrade request info
 * @return void
 */
void user_upgrade_notify_on(tuya_iot_client_t *client, cJSON *upgrade)
{
    PR_INFO("----- Upgrade information -----");
    if (!upgrade) {
        PR_WARN("upgrade JSON is NULL");
        return;
    }

    cJSON *type_item    = cJSON_GetObjectItem(upgrade, "type");
    cJSON *version_item = cJSON_GetObjectItem(upgrade, "version");
    cJSON *size_item    = cJSON_GetObjectItem(upgrade, "size");
    cJSON *md5_item     = cJSON_GetObjectItem(upgrade, "md5");
    cJSON *hmac_item    = cJSON_GetObjectItem(upgrade, "hmac");
    cJSON *url_item     = cJSON_GetObjectItem(upgrade, "url");
    cJSON *https_item   = cJSON_GetObjectItem(upgrade, "httpsUrl");

    PR_INFO("OTA Channel: %d", cJSON_IsNumber(type_item) ? type_item->valueint : -1);
    PR_INFO("Version: %s", cJSON_IsString(version_item) ? version_item->valuestring : "N/A");
    PR_INFO("Size: %s", cJSON_IsString(size_item) ? size_item->valuestring : "N/A");
    PR_INFO("MD5: %s", cJSON_IsString(md5_item) ? md5_item->valuestring : "N/A");
    PR_INFO("HMAC: %s", cJSON_IsString(hmac_item) ? hmac_item->valuestring : "N/A");
    PR_INFO("URL: %s", cJSON_IsString(url_item) ? url_item->valuestring : "N/A");
    PR_INFO("HTTPS URL: %s", cJSON_IsString(https_item) ? https_item->valuestring : "N/A");
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
    switch (event->id) {
    case TUYA_EVENT_BIND_START:
        PR_INFO("Device Bind Start!");
        break;

    /* Print the QRCode for Tuya APP bind */
    case TUYA_EVENT_DIRECT_MQTT_CONNECTED: {
#if defined(ENABLE_QRCODE) && (ENABLE_QRCODE == 1)
        char buffer[255];
        sprintf(buffer, "https://smartapp.tuya.com/s/p?p=%s&uuid=%s&v=2.0", TUYA_PRODUCT_ID, license.uuid);
        qrcode_string_output(buffer, user_log_output_cb, 0);
#endif
    } break;
    /* 已激活设备连上云端 MQTT 后启动局域网服务（原由 netmgr 定时器在激活后带起） */
    case TUYA_EVENT_MQTT_CONNECTED:
#if defined(ENABLE_LOCAL_LINK) && (ENABLE_LOCAL_LINK == 1)
        tuya_lan_enable();
#endif
        break;
    /* RECV upgrade request */
    case TUYA_EVENT_UPGRADE_NOTIFY:
        user_upgrade_notify_on(client, event->value.asJSON);
        break;
    case TUYA_EVENT_RESET: {
        tuya_reset_type_t reset_type = (tuya_reset_type_t)event->value.asInteger;
        PR_INFO("Device Reset:%d", reset_type);

        // TUYA_RESET_TYPE_FACTORY, TUYA_RESET_TYPE_REMOTE_FACTORY, TUYA_RESET_TYPE_DATA_FACTORY
        // Need remove the device application data from the kv store
    } break;
    case TUYA_EVENT_RESET_COMPLETE: {
        PR_INFO("Device Reset Complete!");

        // Restart the device
        tal_system_reset();
    } break;
    /* RECV OBJ DP */
    case TUYA_EVENT_DP_RECEIVE_OBJ: {
        dp_obj_recv_t *dpobj = event->value.dpobj;
        PR_DEBUG("SOC Rev DP Cmd t1:%d t2:%d CNT:%u", dpobj->cmd_tp, dpobj->dtt_tp, dpobj->dpscnt);
        if (dpobj->devid != NULL) {
            PR_DEBUG("devid.%s", dpobj->devid);
        }

        uint32_t index = 0;
        for (index = 0; index < dpobj->dpscnt; index++) {
            dp_obj_t *dp = dpobj->dps + index;
            PR_DEBUG("idx:%d dpid:%d type:%d ts:%u", index, dp->id, dp->type, dp->time_stamp);
            switch (dp->type) {
            case PROP_BOOL: {
                PR_DEBUG("bool value:%d", dp->value.dp_bool);
                break;
            }
            case PROP_VALUE: {
                PR_DEBUG("int value:%d", dp->value.dp_value);
                break;
            }
            case PROP_STR: {
                PR_DEBUG("str value:%s", dp->value.dp_str);
                break;
            }
            case PROP_ENUM: {
                PR_DEBUG("enum value:%u", dp->value.dp_enum);
                break;
            }
            case PROP_BITMAP: {
                PR_DEBUG("bits value:0x%X", dp->value.dp_bitmap);
                break;
            }
            default: {
                PR_ERR("idx:%d dpid:%d type:%d ts:%u is invalid", index, dp->id, dp->type, dp->time_stamp);
                break;
            }
            } // end of switch
        }

        tuya_iot_dp_obj_report(client, dpobj->devid, dpobj->dps, dpobj->dpscnt, 0);

    } break;

    /* RECV RAW DP */
    case TUYA_EVENT_DP_RECEIVE_RAW: {
        dp_raw_recv_t *dpraw = event->value.dpraw;
        PR_DEBUG("SOC Rev DP Cmd t1:%d t2:%d", dpraw->cmd_tp, dpraw->dtt_tp);
        if (dpraw->devid != NULL) {
            PR_DEBUG("devid.%s", dpraw->devid);
        }

        uint32_t  index = 0;
        dp_raw_t *dp    = &dpraw->dp;
        PR_DEBUG("dpid:%d type:RAW len:%d data:", dp->id, dp->len);
        for (index = 0; index < dp->len; index++) {
            PR_DEBUG_RAW("%02x", dp->data[index]);
        }

        tuya_iot_dp_raw_report(client, dpraw->devid, &dpraw->dp, 3);

    } break;

        /* TBD.. add other event if necessary */

    default:
        break;
    }
}

/* 链路状态：由 port 层（port_netif）状态回调维护，core 经 user_network_check 拉取 */
static volatile bool s_link_up = false;

/**
 * @brief port 链路状态回调：port_netif 注册后立即上报当前状态，
 * 之后周期轮询、状态变化时再次回调
 *
 * @param stat 当前链路状态
 * @return void
 */
static void __user_link_status_cb(TAL_NETIF_LINK_STAT_E stat)
{
    s_link_up = (TAL_NETIF_LINK_UP == stat);

    /* 同步活动 IP：link up ≠ IP 就绪（port_netif 仅查 IFF_UP，不代表已获地址）。
     * 出站 socket 的源地址绑定与 LAN 广播包里的本机 IP 均取自此值；取不到置 0 表示不绑定 */
    TUYA_IP_ADDR_T addr = 0;
    if (s_link_up) {
        NW_IP_S ip = {0};
        if (OPRT_OK == tal_netif_get_ip(&ip) && ip.ip[0] != '\0') {
            addr = tal_net_str2addr(ip.ip);
        }
    }
    tal_network_set_active_ip(addr);

    PR_NOTICE("link status changed to %d, link %s", stat, s_link_up ? "UP" : "DOWN");
}

/**
 * @brief user defined network check callback, it will check the network every
 * 1sec by reading the link status maintained by the port layer
 *
 * @return true
 * @return false
 */
bool user_network_check(void)
{
    return s_link_up;
}

void user_main(void)
{
    int rt = OPRT_OK;

    //! open iot development kit runtim init
    cJSON_InitHooks(&(cJSON_Hooks){.malloc_fn = tal_malloc, .free_fn = tal_free});
    /* Before the log: every log line timestamps itself through the time
     * service, whose mutex this creates. */
    tal_time_service_init();
    tal_log_init(TAL_LOG_LEVEL_DEBUG, 1024, user_log_output_cb);

    PR_NOTICE("Application information:");
    PR_NOTICE("App version:         %s", PROJECT_VERSION);
    PR_NOTICE("Compile time:        %s", __DATE__);

    tal_kv_init(&(tal_kv_cfg_t){
        .seed = "vmlkasdh93dlvlcy",
        .key  = "dflfuap134ddlduq",
    });
    tal_sw_timer_init();
    tal_workq_init();

    /* OTA 回调注入（应用层写文件实现，见 ota_app.c）：须在云连接建立前
     * 注册，云端升级指令经 core 的 tuya_ota 流程分发到注册的回调；
     * 不注册的应用即不支持 OTA（tal_ota_* 返回 OPRT_NOT_SUPPORTED） */
    ota_app_register();

    reset_netconfig_start();

    /* 直接使用编译期凭据（tuya_config.h 或 -D 注入） */
    license.uuid    = TUYA_OPENSDK_UUID;
    license.authkey = TUYA_OPENSDK_AUTHKEY;
    // PR_DEBUG("uuid %s, authkey %s", license.uuid, license.authkey);
    /* Initialize Tuya device configuration */
    rt = tuya_iot_init(&client, &(const tuya_iot_config_t){
                                    .software_ver  = PROJECT_VERSION,
                                    .productkey    = TUYA_PRODUCT_ID,
                                    .uuid          = license.uuid,
                                    .authkey       = license.authkey,
                                    .event_handler = user_event_handler_on,
                                    .network_check = user_network_check,
                                });
    assert(rt == OPRT_OK);

    /* 链路状态接入：port（port_netif）立即上报当前状态并轮询变化，
     * 替代原 netmgr_init → netconn_wired_open 的状态回调注册 */
    tal_netif_set_link_cb(__user_link_status_cb);

    /* token 获取 port：mqtt_bind 直连绑定，MQTT 连上后派发
     * TUYA_EVENT_DIRECT_MQTT_CONNECTED 打印二维码，
     * 替代原 netconn_wired_open 里的 token port 注册 */
    tuya_iot_token_get_port_register(&client, mqtt_bind_token_get);

    PR_DEBUG("tuya_iot_init success");
    /* Start tuya iot task */
    tuya_iot_start(&client);

    reset_netconfig_check();

    for (;;) {
        /* Loop to receive packets, and handles client keepalive */
        tuya_iot_yield(&client);
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
    thrd_param.stackDepth   = 1024 * 4;
    thrd_param.priority     = THREAD_PRIO_1;
    thrd_param.thrdname     = "tuya_app_main";
    tal_thread_create_and_start(&ty_app_thread, NULL, NULL, tuya_app_thread, NULL, &thrd_param);
}
#endif

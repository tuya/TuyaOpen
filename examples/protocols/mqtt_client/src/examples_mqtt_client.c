/**
 * @file examples_mqtt_client.c
 * @brief Demonstrates mqtt client usage in Tuya SDK applications.
 *
 * This file provides an example of how to use the mqtt client interface provided by the Tuya SDK to connect to mqtt
 * broker. It includes initializing the SDK, setting up network connections (both WiFi and wired, depending on the
 * configuration), initializing the mqtt client and connect to the mqtt broker, and subscribe/unsubscribe topics,
 * publish/receive message. The example also demonstrates how to publ network link status changes and perform cleanups.
 *
 * Key operations demonstrated in this file:
 * - Initialization of the Tuya SDK and network manager.
 * - initializing mqtt client and connect to mqtt broker.
 * - Handling connect ack and subscribe topics
 * - handling subscribe ack and publish messages
 * - handling publish ack and disconnect the mqtt broker
 * - Cleanup and resource management.
 *
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 *
 */

#include "tuya_cloud_types.h"
#include "mqtt_client_interface.h"
#include "tuya_config_defaults.h"
#include "core_mqtt_config.h"
#include "core_mqtt.h"
#include "tuya_transporter.h"
#include "backoff_algorithm.h"
#include "tal_api.h"
#include "tkl_output.h"
#include "netmgr.h"
#if defined(ENABLE_WIFI) && (ENABLE_WIFI == 1)
#include "netconn_wifi.h"
#endif
#if defined(ENABLE_WIRED) && (ENABLE_WIRED == 1)
#include "netconn_wired.h"
#endif

/***********************************************************
*********************** macro define ***********************
***********************************************************/
#ifdef ENABLE_WIFI
#define DEFAULT_WIFI_SSID "your-ssid-****"
#define DEFAULT_WIFI_PSWD "your-pswd-****"
#endif
/***********************************************************
********************** typedef define **********************
***********************************************************/
typedef struct {
    mqtt_client_config_t config;
    MQTTContext_t mqclient;
    tuya_transporter_t network;
    uint8_t mqttbuffer[CORE_MQTT_BUFFER_SIZE];
} mqtt_client_context_t;

/***********************************************************
********************** variable define *********************
***********************************************************/
static netmgr_status_e netmgr_status = NETMGR_LINK_DOWN;

/***********************************************************
********************** function define *********************
***********************************************************/
static void mqtt_client_connected_cb(void *client, void *userdata)
{
    PR_INFO("mqtt client connected! try to subscribe tuya/tos-test");
    uint16_t msgid = mqtt_client_subscribe(client, "tuya/tos-test", MQTT_QOS_0);
    if (msgid <= 0) {
        PR_ERR("Subscribe failed!");
    }
    PR_DEBUG("Subscribe topic tuya/tos-test ID:%d", msgid);
}

static void mqtt_client_disconnected_cb(void *client, void *userdata)
{
    PR_INFO("mqtt client disconnected!");

    // PR_DEBUG("MQTT Client Deinit");
    // mqtt_client_deinit(client);
}

static void mqtt_client_message_cb(void *client, uint16_t msgid, const mqtt_client_message_t *msg, void *userdata)
{
    PR_DEBUG("recv message TopicName:%s, payload len:%d", msg->topic, msg->length);
}

static void mqtt_client_subscribed_cb(void *client, uint16_t msgid, void *userdata)
{
    PR_DEBUG("Subscribe successed ID:%d", msgid);
    uint16_t new_msgid = mqtt_client_publish(client, "tuya/tos-test", "hello, tuya-open-sdk-for-device",
                                             strlen("hello, tuya-open-sdk-for-device") + 1, MQTT_QOS_1);
    if (new_msgid <= 0) {
        PR_ERR("Publish failed!");
    }
    PR_DEBUG("Publish msg ID:%d", new_msgid);
}

static void mqtt_client_puback_cb(void *client, uint16_t msgid, void *userdata)
{
    PR_DEBUG("PUBACK successed ID:%d", msgid);
    PR_DEBUG("UnSubscribe topic tuya/tos-test");
    mqtt_client_unsubscribe(client, "tuya/tos-test", MQTT_QOS_0);

    // PR_DEBUG("MQTT Client Disconnect");
    // mqtt_client_disconnect(client);
}

static void mqtt_client_example(void)
{
    PR_DEBUG("start mqtt client to broker.emqx.io");

    /* MQTT Client init */
    mqtt_client_context_t mqtt_client = {0};
    mqtt_client_status_t mqtt_status;
    const mqtt_client_config_t mqtt_config = {.cacert = NULL,
                                              .cacert_len = 0,
                                              .host = "broker.emqx.io",
                                              .port = 1883,
                                              .keepalive = MQTT_KEEPALIVE_INTERVALIN,
                                              .timeout_ms = MATOP_TIMEOUT_MS_DEFAULT,
                                              .clientid = "tuya-open-sdk-for-device-01",
                                              .username = "emqx",
                                              .password = "public",
                                              .on_connected = mqtt_client_connected_cb,
                                              .on_disconnected = mqtt_client_disconnected_cb,
                                              .on_message = mqtt_client_message_cb,
                                              .on_subscribed = mqtt_client_subscribed_cb,
                                              .on_published = mqtt_client_puback_cb,
                                              .userdata = NULL};
    mqtt_status = mqtt_client_init(&mqtt_client, &mqtt_config);
    if (mqtt_status != MQTT_STATUS_SUCCESS) {
        PR_ERR("MQTT init failed: Status = %d.", mqtt_status);
        return OPRT_COM_ERROR;
    }

    mqtt_status = mqtt_client_connect(&mqtt_client);
    if (MQTT_STATUS_NOT_AUTHORIZED == mqtt_status) {
        PR_ERR("MQTT connect fail:%d", mqtt_status);
        return OPRT_AUTHENTICATION_FAIL;
    }

    mqtt_client_yield(&mqtt_client);
}

/**
 * @brief  __link_status_cb
 *
 * @param[in] param:Task parameters
 * @return none
 */
OPERATE_RET __link_status_cb(void *data)
{
    PR_DEBUG("link status changed: %d", (netmgr_status_e)data);
    if (netmgr_status == (netmgr_status_e)data && NETMGR_LINK_UP == (netmgr_status_e)data)
        return OPRT_OK;

    netmgr_status = (netmgr_status_e)data;

    return OPRT_OK;
}

/**
 * @brief user_main
 *
 * @return void
 */
void user_main(void)
{
    OPERATE_RET rt = OPRT_OK;

    /* basic init */
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
    tal_event_subscribe(EVENT_LINK_STATUS_CHG, "mqtt_client", __link_status_cb, SUBSCRIBE_TYPE_NORMAL);

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
    netconn_wifi_info_t wifi_info = {0};
    // connect wifi
    strcpy(wifi_info.ssid, DEFAULT_WIFI_SSID);
    strcpy(wifi_info.pswd, DEFAULT_WIFI_PSWD);
    netmgr_conn_set(NETCONN_WIFI, NETCONN_CMD_SSID_PSWD, &wifi_info);
#endif

    while (1) {
        if (netmgr_status == NETMGR_LINK_UP) {
            mqtt_client_example();
            break;
        } else {
            tal_system_sleep(50);
        }
    }

    return;
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
    while (1) {
        tal_system_sleep(500);
    }
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

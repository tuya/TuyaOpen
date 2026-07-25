/**
 * @file mqtt_bind.c
 * @brief Implementation of MQTT binding process for Tuya devices.
 *
 * This file contains the implementation of the MQTT binding process, which
 * includes initializing the MQTT service, connecting to the Tuya MQTT broker,
 * and handling the various states of the MQTT binding lifecycle. It utilizes
 * the Tuya IoT SDK to facilitate secure and reliable communication between the
 * device and the Tuya cloud platform.
 *
 * The binding process is crucial for devices to establish a secure MQTT
 * connection with the Tuya cloud, enabling them to send and receive messages
 * for device control, status updates, and other IoT functionalities.
 *
 * @copyright Copyright (c) 2021-2024 Tuya Inc. All Rights Reserved.
 *
 */

#include "tuya_config_defaults.h"
#include "tuya_error_code.h"

#include "tuya_iot.h"
#include "tuya_endpoint.h"
#include "mqtt_service.h"
#include "cJSON.h"
#include "tal_system.h"

typedef enum {
    STATE_MQTT_BIND_INIT,
    STATE_MQTT_BIND_START,
    STATE_MQTT_BIND_CONNECT,
    STATE_MQTT_BIND_COMPLETE,
    STATE_MQTT_BIND_TIMEOUT,
    STATE_MQTT_BIND_FAILED,
    STATE_MQTT_BIND_EXIT,
    STATE_MQTT_BIND_CONNECTED_WAIT,
    STATE_MQTT_BIND_TOKEN_WAIT,
} mqbind_state_t;

typedef struct {
    tuya_iot_config_t *config;
    tuya_mqtt_context_t mqctx;
    mqbind_state_t state;
    THREAD_HANDLE thread;
} mqtt_bind_t;

static mqtt_bind_t *s_mqbind = NULL;

void mqtt_bind_free(void)
{
    if (s_mqbind) {
        tal_free(s_mqbind);
        s_mqbind = NULL;
    }
}

static OPERATE_RET __mqbind_reset_event_cb(void *data)
{
    mqtt_bind_t *mqbind = (mqtt_bind_t *)s_mqbind;
    if (mqbind) {
        PR_DEBUG("__mqbind_reset_event_cb");
        mqbind->state = STATE_MQTT_BIND_COMPLETE;
    }

    return OPRT_OK;
}

static OPERATE_RET __mqbind_link_activete_cb(void *data)
{
    mqtt_bind_t *mqbind = (mqtt_bind_t *)s_mqbind;

    if (mqbind) {
        PR_DEBUG("__mqbind_link_activete_cb");
        mqbind->state = STATE_MQTT_BIND_COMPLETE;
    }

    return OPRT_OK;
}

static void mqtt_bind_activate_token_on(tuya_protocol_event_t *ev)
{
    cJSON *data = (cJSON *)(ev->data);
    mqtt_bind_t *mqbind = (mqtt_bind_t *)(ev->user_data);

    if (NULL == cJSON_GetObjectItem(data, "token")) {
        PR_ERR("not found token");
        return;
    }

    if (NULL == cJSON_GetObjectItem(data, "region")) {
        PR_ERR("not found region");
        return;
    }

    /* get token from cJSON object */
    char *token = cJSON_GetObjectItem(data, "token")->valuestring;
    char *region = cJSON_GetObjectItem(data, "region")->valuestring;
    char *regist_key = "pro"; // online env default

    if (cJSON_GetObjectItem(data, "env")) {
        regist_key = cJSON_GetObjectItem(data, "env")->valuestring;
    }

    if (strlen(token) > MAX_LENGTH_TOKEN) {
        PR_ERR("token length error");
        return;
    }

    if (strlen(region) > MAX_LENGTH_REGION) {
        PR_ERR("region length error");
        return;
    }

    if (strlen(regist_key) > MAX_LENGTH_REGIST) {
        PR_ERR("regist_key length error");
        return;
    }
    tuya_binding_info_t binding;

    memset(&binding, 0, sizeof(tuya_binding_info_t));
    strncpy(binding.token, token, sizeof(binding.token) - 1);
    binding.token[sizeof(binding.token) - 1] = '\0';
    strncpy(binding.region, region, sizeof(binding.region) - 1);
    binding.region[sizeof(binding.region) - 1] = '\0';
    strncpy(binding.regist_key, regist_key, sizeof(binding.regist_key) - 1);
    binding.regist_key[sizeof(binding.regist_key) - 1] = '\0';

    tal_event_unsubscribe(EVENT_LINK_ACTIVATE, "mqbind", __mqbind_link_activete_cb);
    tal_event_publish(EVENT_LINK_ACTIVATE, &binding);

    mqbind->state = STATE_MQTT_BIND_COMPLETE;
}

/**
 * @brief This function is a thread function that retrieves the MQTT bind token.
 *
 * @param args Pointer to the arguments passed to the thread.
 */
void mqtt_bind_token_get_thread(void *args)
{
    int rt = OPRT_OK;

    mqtt_bind_t *mqbind = (mqtt_bind_t *)args;

    mqbind->state = STATE_MQTT_BIND_INIT;

    TUYA_CALL_ERR_LOG(
        tal_event_subscribe(EVENT_LINK_ACTIVATE, "mqbind", __mqbind_link_activete_cb, SUBSCRIBE_TYPE_ONETIME));

    while (mqbind->state != STATE_MQTT_BIND_EXIT) {

        switch (mqbind->state) {

        case STATE_MQTT_BIND_INIT:
            if (mqbind->config->network_check()) {
                mqbind->state = STATE_MQTT_BIND_START;
            } else {
                tal_system_sleep(200);
            }
            break;

        case STATE_MQTT_BIND_START: {
            /* Update region demain */
            if (OPRT_OK != tuya_endpoint_update_auto_region()) {
                tal_system_sleep(1000);
                continue;
            }
            /* mqtt init */
            const tuya_endpoint_t *endpoint = tuya_endpoint_get();
            rt = tuya_mqtt_init(&mqbind->mqctx, &(const tuya_mqtt_config_t){
                                                    .cacert = endpoint->cert,
                                                    .cacert_len = endpoint->cert_len,
                                                    .host = endpoint->mqtt.host,
                                                    .port = endpoint->mqtt.port,
                                                    .uuid = mqbind->config->uuid,
                                                    .authkey = mqbind->config->authkey,
                                                    .timeout = MQTT_BIND_TIMEOUT_MS_DEFAULT,
                                                });
            if (OPRT_OK != rt) {
                PR_ERR("tuya mqtt init error:%d", rt);
                tuya_mqtt_destory(&mqbind->mqctx);
                tal_system_sleep(1000);
                continue;
            }
            /* register token callback */
            tuya_mqtt_protocol_register(&mqbind->mqctx, PRO_MQ_ACTIVE_TOKEN_ON, mqtt_bind_activate_token_on, mqbind);
            mqbind->state = STATE_MQTT_BIND_CONNECT;
            break;
        }

        case STATE_MQTT_BIND_CONNECT:
            rt = tuya_mqtt_start(&mqbind->mqctx);
            if (OPRT_OK != rt) {
                PR_ERR("tuya mqtt connect fail:%d, retry..", rt);
                tal_system_sleep(1000);
                break;
            }
            mqbind->state = STATE_MQTT_BIND_CONNECTED_WAIT;
            break;

        case STATE_MQTT_BIND_CONNECTED_WAIT:
            if (tuya_mqtt_connected(&mqbind->mqctx)) {
                /* Send connected event*/
                tuya_iot_client_t *client = tuya_iot_client_get();
                client->event.id = TUYA_EVENT_DIRECT_MQTT_CONNECTED;
                client->event.type = TUYA_DATE_TYPE_UNDEFINED;
                tuya_iot_dispatch_event(client);

                PR_INFO("MQTT direct connected!");
                PR_INFO("Wait Tuya APP scan the Device QR code...");
                mqbind->state = STATE_MQTT_BIND_TOKEN_WAIT;
            }
            break;

        case STATE_MQTT_BIND_TOKEN_WAIT:
            tuya_mqtt_loop(&mqbind->mqctx);
            break;

        case STATE_MQTT_BIND_COMPLETE:
            PR_DEBUG("STATE_MQTT_BIND_COMPLETE");
            tuya_mqtt_stop(&mqbind->mqctx);
            tuya_mqtt_destory(&mqbind->mqctx);
            mqbind->state = STATE_MQTT_BIND_EXIT;
            break;

        default:
            PR_ERR("state error:%d", mqbind->state);
            break;
        }
    }

    if (mqbind->thread != NULL) {
        tal_thread_delete(mqbind->thread);
        mqbind->thread = NULL;
    }

    mqtt_bind_free();
}

/**
 * @brief Retrieves the MQTT bind token for the Tuya IoT configuration.
 *
 * This function is used to get the MQTT bind token from the Tuya IoT
 * configuration.
 *
 * @param config Pointer to the Tuya IoT configuration structure.
 * @return The MQTT bind token.
 */
int mqtt_bind_token_get(tuya_iot_config_t *config)
{
    int rt = OPRT_OK;

    //! simple wait for reset
    while (s_mqbind != NULL) {
        tal_system_sleep(200);
    }

    TUYA_CALL_ERR_RETURN(tal_event_subscribe(EVENT_RESET, "mqbind", __mqbind_reset_event_cb, SUBSCRIBE_TYPE_NORMAL));

    s_mqbind = tal_malloc(sizeof(mqtt_bind_t));
    if (NULL == s_mqbind) {
        return OPRT_MALLOC_FAILED;
    }
    memset(s_mqbind, 0, sizeof(mqtt_bind_t));

    s_mqbind->config = config;

    THREAD_CFG_T thread_cfg = {.priority = THREAD_PRIO_3, .stackDepth = 4096, .thrdname = "mqtt_bind"};
    rt = tal_thread_create_and_start(&s_mqbind->thread, NULL, NULL, mqtt_bind_token_get_thread, s_mqbind, &thread_cfg);
    if (OPRT_OK != rt) {
        PR_ERR("tuya cli create thread failed %d", rt);
        tal_free(s_mqbind);
        s_mqbind = NULL;
    }

    return rt;
}

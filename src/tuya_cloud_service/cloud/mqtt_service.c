/**
 * @file mqtt_service.c
 * @brief This file contains the MQTT service implementation for Tuya devices,
 * including functions for MQTT connection management, message subscription and
 * publishing, and protocol message parsing.
 *
 *        It provides mechanisms for secure MQTT communication, handling device
 * activation, and processing incoming protocol messages. The service
 * facilitates the interaction between Tuya devices and the Tuya cloud platform,
 * enabling device control and status updates.
 *
 *        Key functionalities include:
 *        - MQTT client connection and disconnection handling
 *        - Subscription to MQTT topics and message callback registration
 *        - Publishing messages to MQTT topics
 *        - Parsing and processing of Tuya protocol messages
 *        - Secure signature generation for MQTT access
 *
 *        This implementation supports both activated and unactivated devices,
 * with specific handling for each case to ensure secure and efficient
 * communication with the Tuya cloud.
 *
 * @copyright Copyright (c) 2021-2024 Tuya Inc. All Rights Reserved.
 *
 */

#include "tuya_config_defaults.h"

#include "tuya_error_code.h"
#include "mqtt_client_interface.h"
#include "cJSON.h"
#include "mqtt_service.h"
#include "tal_security.h"
#include "crc32i.h"
#include "tal_api.h"
#include "tuya_protocol.h"

static void on_subscribe_message_default(uint16_t msgid, const mqtt_client_message_t *msg, void *userdata);

typedef struct {
    uint32_t sequence;
    uint32_t source;
    size_t datalen;
    uint8_t data[0];
} pv22_packet_object_t;

static int tuya_mqtt_signature_tool(const tuya_meta_info_t *input, tuya_mqtt_access_t *signout)
{
    if (NULL == input || signout == NULL) {
        PR_ERR("param error");
        return OPRT_INVALID_PARM;
    }

    // clear
    int i;
    uint8_t digest[16] = {0};
    memset(signout, 0, sizeof(tuya_mqtt_access_t));

    if (input->devid && input->seckey && input->localkey) {
        // ACTIVED
        memcpy(signout->cipherkey, input->localkey, 16);
        sprintf(signout->clientid, "%s", input->devid);
        sprintf(signout->username, "%s", input->devid);
        tal_md5_ret((const uint8_t *)input->seckey, strlen(input->seckey), digest);
        for (i = 0; i < 8; ++i) {
            sprintf(&signout->password[i * 2], "%02x", (unsigned char)digest[i + 4]);
        }

        // IO topic
        sprintf(signout->topic_in, "smart/device/in/%s", input->devid);
        sprintf(signout->topic_out, "smart/device/out/%s", input->devid);

    } else if (input->uuid && input->authkey) {
        // UNACTIVED
        memcpy(signout->cipherkey, input->authkey, 16);
        sprintf(signout->clientid, "acon_%s", input->uuid);
        sprintf(signout->username, "acon_%s|pv=%s", input->uuid, TUYA_PV23);
        tal_md5_ret((const uint8_t *)input->authkey, strlen(input->authkey), digest);
        for (i = 0; i < 8; ++i) {
            sprintf(&signout->password[i * 2], "%02x", (unsigned char)digest[i + 4]);
        }

        // IO topic
        sprintf(signout->topic_in, "d/ai/%s", input->uuid);
        sprintf(signout->topic_out, "%s",
                ""); // not support publish data on direct mode

    } else {
        PR_ERR("input error");
        return OPRT_INVALID_PARM;
    }
    return OPRT_OK;
}

/**
 * @brief Registers a callback function for handling MQTT subscribe messages.
 *
 * This function allows you to register a callback function that will be called
 * when an MQTT subscribe message is received.
 *
 * @param context The MQTT context.
 * @param topic The topic to subscribe to.
 * @param cb The callback function to be called when a subscribe message is
 * received.
 * @param userdata User-defined data that will be passed to the callback
 * function.
 *
 * @return Returns 0 on success, or a negative error code on failure.
 */
int tuya_mqtt_subscribe_message_callback_register(tuya_mqtt_context_t *context, const char *topic,
                                                  mqtt_subscribe_message_cb_t cb, void *userdata)
{
    if (!context || !topic) {
        return OPRT_INVALID_PARM;
    }

    uint16_t msgid = mqtt_client_subscribe(context->mqtt_client, topic, MQTT_QOS_1);
    if (msgid <= 0) {
        return OPRT_COM_ERROR;
    }

    /* Repetition filter */
    mqtt_subscribe_handle_t *target = context->subscribe_list;
    while (target) {
        if (!memcmp(target->topic, topic, target->topic_length) && target->cb == cb) {
            PR_WARN("Repetition:%s", topic);
            return OPRT_OK;
        }
        target = target->next;
    }

    /* Intser new handle */
    mqtt_subscribe_handle_t *newtarget = tal_calloc(1, sizeof(mqtt_subscribe_handle_t));
    if (!newtarget) {
        PR_ERR("malloc error");
        return OPRT_MALLOC_FAILED;
    }

    newtarget->topic_length = strlen(topic);
    newtarget->topic = tal_calloc(1, newtarget->topic_length + 1); // strdup
    strcpy(newtarget->topic, topic);

    if (cb) {
        newtarget->cb = cb;
    } else {
        newtarget->cb = on_subscribe_message_default;
    }
    newtarget->userdata = userdata;
    /* LOCK */
    newtarget->next = context->subscribe_list;
    context->subscribe_list = newtarget;
    /* UNLOCK */
    return OPRT_OK;
}

/**
 * Unregisters the callback function for handling MQTT subscribe messages.
 *
 * This function unregisters the callback function that was previously
 * registered for handling MQTT subscribe messages. The callback function will
 * no longer be called when a subscribe message is received for the specified
 * topic.
 *
 * @param context The MQTT context.
 * @param topic The topic for which the callback function should be
 * unregistered.
 * @return Returns 0 on success, or a negative error code on failure.
 */
int tuya_mqtt_subscribe_message_callback_unregister(tuya_mqtt_context_t *context, const char *topic)
{
    if (!context || !topic) {
        return OPRT_INVALID_PARM;
    }

    size_t topic_length = strlen(topic);

    /* LOCK */
    /* Remove object form list */
    mqtt_subscribe_handle_t **target = &context->subscribe_list;
    while (*target) {
        mqtt_subscribe_handle_t *entry = *target;
        if (entry->topic_length == topic_length && !memcmp(topic, entry->topic, topic_length)) {
            *target = entry->next;
            tal_free(entry->topic);
            tal_free(entry);
        } else {
            target = &entry->next;
        }
    }
    /* UNLOCK */

    uint16_t msgid = mqtt_client_unsubscribe(context->mqtt_client, topic, MQTT_QOS_1);
    if (msgid <= 0) {
        return OPRT_COM_ERROR;
    }

    return OPRT_OK;
}

static void mqtt_subscribe_message_distribute(tuya_mqtt_context_t *context, uint16_t msgid,
                                              const mqtt_client_message_t *msg)
{
    const char *topic = msg->topic;
    size_t topic_length = strlen(msg->topic);

    /* LOCK */
    mqtt_subscribe_handle_t *target = context->subscribe_list;
    for (; target; target = target->next) {
        if (target->topic_length == topic_length && !memcmp(topic, target->topic, target->topic_length)) {
            target->cb(msgid, msg, target->userdata);
        }
    }
    /* UNLOCK */
}

/* -------------------------------------------------------------------------- */
/*                       Tuya internal subscribe message                      */
/* -------------------------------------------------------------------------- */
static int tuya_protocol_message_parse_process(tuya_mqtt_context_t *context, const uint8_t *payload, size_t payload_len)
{
    int ret = OPRT_OK;

    char *jsonstr = NULL;
    ret = tuya_parse_protocol_data(DP_CMD_MQ, (uint8_t *)payload, payload_len, context->signature.cipherkey,
                                   (char **)&jsonstr);
    if (OPRT_OK != ret) {
        PR_ERR("Cmd Parse Fail:%d", ret);
        return OPRT_COM_ERROR;
    }

    PR_DEBUG("Data JSON:%s", jsonstr);

    /* json parse */
    cJSON *root = NULL;
    cJSON *json = NULL;
    root = cJSON_Parse((const char *)jsonstr);
    tal_free(jsonstr);
    if (NULL == root) {
        PR_ERR("JSON parse error");
        return OPRT_CJSON_PARSE_ERR;
    }

    /* JSON key verfiy */
    if ((NULL == cJSON_GetObjectItem(root, "protocol")) || (NULL == cJSON_GetObjectItem(root, "t")) ||
        (NULL == cJSON_GetObjectItem(root, "data"))) {
        PR_ERR("param is no correct");
        cJSON_Delete(root);
        return OPRT_CJSON_GET_ERR;
    }

    /* protocol ID */
    int protocol_id = cJSON_GetObjectItem(root, "protocol")->valueint;
    json = cJSON_GetObjectItem(root, "data");
    if (NULL == json) {
        PR_ERR("get json err");
        cJSON_Delete(root);
        return OPRT_CJSON_GET_ERR;
    }

    /* dispatch */
    tuya_protocol_event_t event;
    event.event_id = protocol_id;
    event.root_json = root;
    event.data = cJSON_GetObjectItem(root, "data");

    /* LOCK */
    tuya_protocol_handle_t *target = context->protocol_list;
    for (; target; target = target->next) {
        if (target->id == protocol_id) {
            event.user_data = target->user_data, target->cb(&event);
        }
    }
    /* UNLOCK */

    cJSON_Delete(root);
    return OPRT_OK;
}

static void on_subscribe_message_default(uint16_t msgid, const mqtt_client_message_t *msg, void *userdata)
{
    tuya_mqtt_context_t *context = (tuya_mqtt_context_t *)userdata;
    int ret = tuya_protocol_message_parse_process(context, msg->payload, msg->length);
    if (ret != OPRT_OK) {
        PR_ERR("protocol message parse error:%d", ret);
    }
}

/* -------------------------------------------------------------------------- */
/*                         MQTT Client event callback                         */
/* -------------------------------------------------------------------------- */
static void mqtt_client_connected_cb(void *client, void *userdata)
{
    client = client;
    tuya_mqtt_context_t *context = (tuya_mqtt_context_t *)userdata;
    PR_INFO("mqtt client connected!");

    tuya_mqtt_subscribe_message_callback_register(context, context->signature.topic_in, on_subscribe_message_default,
                                                  userdata);
    PR_DEBUG("SUBSCRIBE sent for topic %s to broker.", context->signature.topic_in);
    context->is_connected = true;
    if (context->on_connected) {
        context->on_connected(context, context->user_data);
    }
}

static void mqtt_client_disconnected_cb(void *client, void *userdata)
{
    client = client;
    tuya_mqtt_context_t *context = (tuya_mqtt_context_t *)userdata;
    PR_INFO("mqtt client disconnected!");
    context->is_connected = false;
    if (context->on_disconnect) {
        context->on_disconnect(context, context->user_data);
    }
}

static void mqtt_client_message_cb(void *client, uint16_t msgid, const mqtt_client_message_t *msg, void *userdata)
{
    client = client;
    tuya_mqtt_context_t *context = (tuya_mqtt_context_t *)userdata;

    /* topic filter */
    PR_DEBUG("recv message TopicName:%s, payload len:%d", msg->topic, msg->length);
    mqtt_subscribe_message_distribute(context, msgid, msg);
}

static void mqtt_client_subscribed_cb(void *client, uint16_t msgid, void *userdata)
{
    client = client;
    userdata = userdata;
    PR_DEBUG("Subscribe successed ID:%d", msgid);
}

static void mqtt_client_puback_cb(void *client, uint16_t msgid, void *userdata)
{
    client = client;
    tuya_mqtt_context_t *context = (tuya_mqtt_context_t *)userdata;
    PR_DEBUG("PUBACK ID:%d", msgid);

    /* LOCK */
    /* publish async process */
    mqtt_publish_handle_t **next_handle = &context->publish_list;
    for (; *next_handle; next_handle = &(*next_handle)->next) {
        mqtt_publish_handle_t *entry = *next_handle;
        if (msgid == entry->msgid) {
            entry->cb(OPRT_OK, entry->user_data);
            *next_handle = entry->next;
            tal_free(entry->payload);
            tal_free(entry);
            break;
        }
    }
    /* UNLOCK */
}

/**
 * @brief Initializes the Tuya MQTT service.
 *
 * This function initializes the Tuya MQTT service with the provided
 * configuration.
 *
 * @param context Pointer to the MQTT context structure.
 * @param config Pointer to the MQTT configuration structure.
 * @return Returns 0 on success, or a negative error code on failure.
 */
int tuya_mqtt_init(tuya_mqtt_context_t *context, const tuya_mqtt_config_t *config)
{
    int rt = OPRT_OK;
    mqtt_client_status_t mqtt_status;

    /* Clean to zero */
    memset(context, 0, sizeof(tuya_mqtt_context_t));

    /* configuration */
    context->user_data = config->user_data;
    context->on_unbind = config->on_unbind;
    context->on_connected = config->on_connected;
    context->on_disconnect = config->on_disconnect;

    /* Device token signature */
    rt = tuya_mqtt_signature_tool(
        &(const tuya_meta_info_t){
            .uuid = config->uuid,
            .authkey = config->authkey,
            .devid = config->devid,
            .seckey = config->seckey,
            .localkey = config->localkey,
        },
        &context->signature);
    if (OPRT_OK != rt) {
        PR_ERR("mqtt token sign error:%d", rt);
        return rt;
    }

    /* MQTT Client object new */
    context->mqtt_client = mqtt_client_new();
    if (context->mqtt_client == NULL) {
        PR_ERR("mqtt client new fault.");
        return OPRT_MALLOC_FAILED;
    }

    /* MQTT Client init */
    const mqtt_client_config_t mqtt_config = {.cacert = config->cacert,
                                              .cacert_len = config->cacert_len,
                                              .host = config->host,
                                              .port = config->port,
                                              .keepalive = MQTT_KEEPALIVE_INTERVALIN,
                                              .timeout_ms = config->timeout,
                                              .clientid = context->signature.clientid,
                                              .username = context->signature.username,
                                              .password = context->signature.password,
                                              .on_connected = mqtt_client_connected_cb,
                                              .on_disconnected = mqtt_client_disconnected_cb,
                                              .on_message = mqtt_client_message_cb,
                                              .on_subscribed = mqtt_client_subscribed_cb,
                                              .on_published = mqtt_client_puback_cb,
                                              .userdata = context};
    mqtt_status = mqtt_client_init(context->mqtt_client, &mqtt_config);
    if (mqtt_status != MQTT_STATUS_SUCCESS) {
        PR_ERR("MQTT init failed: Status = %d.", mqtt_status);
        return OPRT_COM_ERROR;
    }

    BackoffAlgorithm_InitializeParams(&context->backoff_algorithm, MQTT_CONNECT_RETRY_MIN_DELAY_MS,
                                      MQTT_CONNECT_RETRY_MAX_DELAY_MS, MQTT_CONNECT_RETRY_MAX_ATTEMPTS);

    // rand
    context->sequence_out = rand() & 0xffff;
    context->sequence_in = -1;

    /* Wait start task */
    context->is_inited = true;
    context->manual_disconnect = true;
    return OPRT_OK;
}

/**
 * @brief Starts the MQTT service.
 *
 * This function initializes and starts the MQTT service using the provided
 * context.
 *
 * @param context The MQTT context containing the necessary configuration
 * parameters.
 * @return Returns 0 on success, or a negative error code on failure.
 */
int tuya_mqtt_start(tuya_mqtt_context_t *context)
{
    if (context == NULL || context->is_inited == false) {
        return OPRT_INVALID_PARM;
    }

    PR_INFO("clientid:%s", context->signature.clientid);
    PR_INFO("username:%s", context->signature.username);
    PR_DEBUG("password:%s", context->signature.password);
    PR_INFO("topic_in:%s", context->signature.topic_in);
    PR_INFO("topic_out:%s", context->signature.topic_out);
    PR_INFO("tuya_mqtt_start...");
    context->manual_disconnect = false;

    mqtt_client_status_t mqtt_status;

    mqtt_status = mqtt_client_connect(context->mqtt_client);
    if (MQTT_STATUS_NOT_AUTHORIZED == mqtt_status) {
        PR_ERR("MQTT connect fail:%d", mqtt_status);
        if (context->on_unbind) {
            context->on_unbind(context, context->user_data);
        }
        return OPRT_AUTHENTICATION_FAIL;
    }
    //! TODO: rand
    if (MQTT_STATUS_SUCCESS != mqtt_status) {
        PR_ERR("MQTT connect fail:%d", mqtt_status);
        /* Generate a random number and get back-off value (in milliseconds) for
         * the next connection retry. */
        uint16_t nextRetryBackOff = 0U;
        if (BackoffAlgorithm_GetNextBackoff(&context->backoff_algorithm, rand(), &nextRetryBackOff) ==
            BackoffAlgorithmSuccess) {
            PR_WARN("Connection to the MQTT server failed. Retrying "
                    "connection after %hu ms backoff.",
                    (unsigned short)nextRetryBackOff);
            tal_system_sleep(nextRetryBackOff + 10000);
        }
        return OPRT_COM_ERROR;
    }
    return OPRT_OK;
}

/**
 * @brief Stops the MQTT service.
 *
 * This function stops the MQTT service associated with the given context.
 *
 * @param context Pointer to the MQTT context.
 * @return Returns 0 on success, or a negative error code on failure.
 */
int tuya_mqtt_stop(tuya_mqtt_context_t *context)
{
    if (context == NULL || context->is_inited == false) {
        return OPRT_INVALID_PARM;
    }

    int ret = tuya_mqtt_subscribe_message_callback_unregister(context, context->signature.topic_in);
    PR_DEBUG("MQTT unsubscribe result:%d", ret);

    mqtt_client_status_t mqtt_status;
    mqtt_status = mqtt_client_disconnect(context->mqtt_client);
    PR_DEBUG("MQTT disconnect result:%d", mqtt_status);

    context->manual_disconnect = true;
    return OPRT_OK;
}

/**
 * @brief Registers a MQTT protocol with the given context.
 *
 * This function registers a MQTT protocol with the specified context. The
 * protocol is identified by the protocol ID. When a message with the registered
 * protocol ID is received, the provided callback function will be called.
 *
 * @param[in] context The MQTT context to register the protocol with.
 * @param[in] protocol_id The ID of the protocol to register.
 * @param[in] cb The callback function to be called when a message with the
 * registered protocol ID is received.
 * @param[in] user_data User data to be passed to the callback function.
 *
 * @return 0 on success, negative error code on failure.
 */
int tuya_mqtt_protocol_register(tuya_mqtt_context_t *context, uint16_t protocol_id, tuya_protocol_callback_t cb,
                                void *user_data)
{
    if (context == NULL || context->is_inited == false || cb == NULL) {
        return OPRT_INVALID_PARM;
    }

    /* LOCK */
    /* Repetition filter */
    tuya_protocol_handle_t *target = context->protocol_list;
    while (target) {
        if (target->id == protocol_id && target->cb == cb) {
            return OPRT_COM_ERROR;
        }
        target = target->next;
    }

    tuya_protocol_handle_t *new_handle = tal_calloc(1, sizeof(tuya_protocol_handle_t));
    if (!new_handle) {
        return OPRT_MALLOC_FAILED;
    }
    new_handle->id = protocol_id;
    new_handle->cb = cb;
    new_handle->user_data = user_data;
    new_handle->next = context->protocol_list;
    context->protocol_list = new_handle;
    /* UNLOCK */

    return OPRT_OK;
}

/**
 * Unregisters a protocol from the Tuya MQTT service.
 *
 * This function unregisters a protocol identified by the given protocol ID from
 * the Tuya MQTT service. The protocol ID and callback function must match the
 * ones used during registration.
 *
 * @param context The Tuya MQTT context.
 * @param protocol_id The ID of the protocol to unregister.
 * @param cb The callback function associated with the protocol.
 * @return Returns 0 on success, or a negative error code on failure.
 */
int tuya_mqtt_protocol_unregister(tuya_mqtt_context_t *context, uint16_t protocol_id, tuya_protocol_callback_t cb)
{
    if (context == NULL || context->is_inited == false || cb == NULL) {
        return OPRT_INVALID_PARM;
    }

    /* LOCK */
    /* Remove object form list */
    tuya_protocol_handle_t **target = &context->protocol_list;
    while (*target) {
        tuya_protocol_handle_t *entry = *target;
        if (entry->id == protocol_id && entry->cb == cb) {
            *target = entry->next;
            tal_free(entry);
        } else {
            target = &entry->next;
        }
    }
    /* UNLOCK */

    return OPRT_OK;
}

/**
 * Unregisters all MQTT protocols from the given MQTT context.
 *
 * @param context The MQTT context from which to unregister the protocols.
 * @return Returns an integer value indicating the success or failure of the
 * operation. A return value of 0 indicates success, while a non-zero value
 * indicates failure.
 */
int tuya_mqtt_protocol_unregister_all(tuya_mqtt_context_t *context)
{
    if (context == NULL || context->is_inited == false) {
        return OPRT_INVALID_PARM;
    }

    PR_DEBUG("Unregister all MQTT Protocol");
    /* LOCK */
    /* Remove object form list */
    tuya_protocol_handle_t *entry = NULL;
    tuya_protocol_handle_t *target = context->protocol_list;
    while (target) {
        entry = target;
        target = entry->next;
        tal_free(entry);
    }
    /* UNLOCK */

    context->protocol_list = NULL;

    return OPRT_OK;
}

/**
 * Publishes a message to an MQTT topic using the Tuya MQTT client.
 *
 * @param context The MQTT context.
 * @param topic The topic to publish the message to.
 * @param payload The payload of the message.
 * @param payload_length The length of the payload.
 * @param cb The callback function to be called when the publish operation is
 * complete.
 * @param user_data User data to be passed to the callback function.
 * @param timeout_ms The timeout for the publish operation in milliseconds.
 * @param async Whether to perform the publish operation asynchronously or not.
 *
 * @return 0 on success, or a negative error code on failure.
 */
int tuya_mqtt_client_publish_common(tuya_mqtt_context_t *context, const char *topic, const uint8_t *payload,
                                    size_t payload_length, mqtt_publish_notify_cb_t cb, void *user_data, int timeout_ms,
                                    bool async)
{
    if (context == NULL || topic == NULL || payload == NULL || (cb == NULL && async == true)) {
        return OPRT_INVALID_PARM;
    }

    if (cb == NULL) {
        uint16_t msgid = mqtt_client_publish(context->mqtt_client, topic, payload, payload_length, MQTT_QOS_0);
        if (msgid <= 0) {
            return OPRT_COM_ERROR;
        }
        return OPRT_OK;
    }

    mqtt_publish_handle_t *handle = tal_malloc(sizeof(mqtt_publish_handle_t));
    TUYA_CHECK_NULL_RETURN(handle, OPRT_MALLOC_FAILED);
    handle->next = NULL;
    handle->msgid = 0;
    handle->topic = (char *)topic;
    handle->timeout = tal_time_get_posix() + timeout_ms;
    handle->cb = cb;
    handle->user_data = user_data;
    handle->payload_length = payload_length;
    handle->payload = tal_malloc(payload_length);
    if (handle->payload == NULL) {
        return OPRT_MALLOC_FAILED;
    }
    memcpy(handle->payload, payload, payload_length);

    if (async == false) {
        handle->msgid = mqtt_client_publish(context->mqtt_client, handle->topic, handle->payload,
                                            handle->payload_length, MQTT_QOS_1);
    }

    if (context->publish_list == NULL) {
        context->publish_list = handle;
        return OPRT_OK;
    }

    mqtt_publish_handle_t *last = context->publish_list;
    while (last->next != NULL) {
        last = last->next;
    }
    last->next = handle;

    return OPRT_OK;
}

/**
 * Publishes MQTT protocol data with a common topic.
 *
 * @param context The MQTT context.
 * @param topic The topic to publish the data to.
 * @param protocol_id The protocol ID.
 * @param data The data to be published.
 * @param length The length of the data.
 * @param cb The callback function to be called after the publish operation is
 * complete.
 * @param user_data User data to be passed to the callback function.
 * @param timeout_ms The timeout value for the publish operation in
 * milliseconds.
 * @param async Specifies whether the publish operation should be performed
 * asynchronously.
 * @return Returns 0 on success, or a negative error code on failure.
 */
int tuya_mqtt_protocol_data_publish_with_topic_common(tuya_mqtt_context_t *context, const char *topic,
                                                      uint16_t protocol_id, const uint8_t *data, uint16_t length,
                                                      mqtt_publish_notify_cb_t cb, void *user_data, int timeout_ms,
                                                      bool async)
{
    if (context == NULL || context->is_inited == false) {
        return OPRT_INVALID_PARM;
    }

    if (context->is_connected == false) {
        return OPRT_COM_ERROR;
    }

    int ret = OPRT_OK;

    char *buffer = NULL;
    uint32_t buffer_len = 0;

    ret = tuya_pack_protocol_data(DP_CMD_MQ, (const char *)data, protocol_id, (uint8_t *)context->signature.cipherkey,
                                  &buffer, &buffer_len);
    if (ret != OPRT_OK) {
        PR_ERR("tuya_pack_protocol_data error:%d", ret);
        return ret;
    }

    /* mqtt client publish */
    ret = tuya_mqtt_client_publish_common(context, (const char *)topic, (const uint8_t *)buffer, buffer_len, cb,
                                          user_data, timeout_ms, async);
    tal_free(buffer);
    return ret;
}

/**
 * Publishes common MQTT protocol data.
 *
 * This function is used to publish common MQTT protocol data to the specified
 * MQTT context.
 *
 * @param context The MQTT context to publish the data to.
 * @param protocol_id The protocol ID associated with the data.
 * @param data The data to be published.
 * @param length The length of the data.
 * @param cb The callback function to be called when the publish operation is
 * complete.
 * @param user_data User data to be passed to the callback function.
 * @param timeout_ms The timeout value for the publish operation in
 * milliseconds.
 * @param async Specifies whether the publish operation should be performed
 * asynchronously.
 *
 * @return 0 on success, or a negative error code on failure.
 */
int tuya_mqtt_protocol_data_publish_common(tuya_mqtt_context_t *context, uint16_t protocol_id, const uint8_t *data,
                                           uint16_t length, mqtt_publish_notify_cb_t cb, void *user_data,
                                           int timeout_ms, bool async)
{
    return tuya_mqtt_protocol_data_publish_with_topic_common(context, context->signature.topic_out, protocol_id, data,
                                                             length, cb, user_data, timeout_ms, async);
}

/**
 * @brief Publishes protocol data with a specified topic using the MQTT service.
 *
 * This function publishes protocol data with a specified topic using the MQTT
 * service.
 *
 * @param context The MQTT context.
 * @param topic The topic to publish the data to.
 * @param protocol_id The protocol ID.
 * @param data The data to be published.
 * @param length The length of the data.
 * @return The result of the operation.
 */
int tuya_mqtt_protocol_data_publish_with_topic(tuya_mqtt_context_t *context, const char *topic, uint16_t protocol_id,
                                               const uint8_t *data, uint16_t length)
{
    return tuya_mqtt_protocol_data_publish_with_topic_common(context, topic, protocol_id, data, length, NULL, NULL, 0,
                                                             false);
}

/**
 * @brief Publishes protocol data using the MQTT service.
 *
 * This function publishes protocol data using the MQTT service. It takes the
 * MQTT context, protocol ID, data, and length as parameters and calls the
 * `tuya_mqtt_protocol_data_publish_with_topic` function with the appropriate
 * topic.
 *
 * @param context The MQTT context.
 * @param protocol_id The protocol ID.
 * @param data The data to be published.
 * @param length The length of the data.
 * @return The result of the publish operation.
 */
int tuya_mqtt_protocol_data_publish(tuya_mqtt_context_t *context, uint16_t protocol_id, const uint8_t *data,
                                    uint16_t length)
{
    return tuya_mqtt_protocol_data_publish_with_topic(context, context->signature.topic_out, protocol_id, data, length);
}

/**
 * @brief Executes the MQTT event loop for the Tuya MQTT service.
 *
 * This function is responsible for processing incoming MQTT messages and
 * handling MQTT events for the Tuya MQTT service. It should be called
 * periodically to ensure proper functioning of the MQTT service.
 *
 * @param context Pointer to the Tuya MQTT context structure.
 * @return Returns 0 on success, or a negative error code on failure.
 */
int tuya_mqtt_loop(tuya_mqtt_context_t *context)
{
    if (context == NULL) {
        return OPRT_COM_ERROR;
    }

    int rt = OPRT_OK;
    mqtt_client_status_t mqtt_status;

    if (context->is_inited == false || context->manual_disconnect == true) {
        return rt;
    }

    /* reconnect */
    if (context->is_connected == false) {
        mqtt_status = mqtt_client_connect(context->mqtt_client);
        if (mqtt_status == MQTT_STATUS_NOT_AUTHORIZED) {
            if (context->on_unbind) {
                context->on_unbind(context, context->user_data);
            }
            return rt;

        } else if (mqtt_status != MQTT_STATUS_SUCCESS) {
            uint16_t nextRetryBackOff = 0U;
            if (BackoffAlgorithm_GetNextBackoff(&context->backoff_algorithm, rand(), &nextRetryBackOff) ==
                BackoffAlgorithmSuccess) {
                PR_WARN("Connection to the MQTT server failed. Retrying "
                        "connection after %hu ms backoff.",
                        (unsigned short)nextRetryBackOff);
                tal_system_sleep(nextRetryBackOff);
                return rt;
            }
        }
        return rt;
    }

    /* LOCK */
    /* publish async process */
    mqtt_publish_handle_t **next_handle = &context->publish_list;
    for (; *next_handle; next_handle = &(*next_handle)->next) {
        mqtt_publish_handle_t *entry = *next_handle;

        if (entry->timeout <= tal_time_get_posix()) {
            entry->cb(OPRT_TIMEOUT, entry->user_data);
            *next_handle = entry->next;
            tal_free(entry->payload);
            tal_free(entry);
            continue;
        }

        if (entry->msgid <= 0) {
            entry->msgid =
                mqtt_client_publish(context->mqtt_client, entry->topic, entry->payload, entry->payload_length, 1);
        }
    }
    /* UNLOCK */

    /* yield */
    mqtt_client_yield(context->mqtt_client);

    return rt;
}

/**
 * @brief Destroys the MQTT context.
 *
 * This function is used to destroy the MQTT context and release any allocated
 * resources.
 *
 * @param context Pointer to the MQTT context.
 * @return Returns 0 on success, or a negative error code on failure.
 */
int tuya_mqtt_destory(tuya_mqtt_context_t *context)
{
    if (context == NULL || context->is_inited != true) {
        return OPRT_COM_ERROR;
    }

    tuya_mqtt_protocol_unregister_all(context);
    if (context->mqtt_client) {
        mqtt_client_status_t mqtt_status = mqtt_client_deinit(context->mqtt_client);
        mqtt_client_free(context->mqtt_client);
        context->mqtt_client = NULL;
        if (mqtt_status != MQTT_STATUS_SUCCESS) {
            return OPRT_COM_ERROR;
        }
    }

    return OPRT_OK;
}

/**
 * @brief Checks if the MQTT connection is established.
 *
 * This function checks if the MQTT connection is established for the given MQTT
 * context.
 *
 * @param context The MQTT context.
 * @return `true` if the MQTT connection is established, `false` otherwise.
 */
bool tuya_mqtt_connected(tuya_mqtt_context_t *context)
{
    if (context == NULL) {
        return false;
    }
    return context->is_connected;
}

/**
 * @brief Reports the progress of an upgrade operation over MQTT.
 *
 * This function is used to report the progress of an upgrade operation over
 * MQTT.
 *
 * @param context Pointer to the MQTT context.
 * @param channel The channel number of the upgrade operation.
 * @param percent The progress percentage of the upgrade operation.
 *
 * @return Returns 0 on success, or a negative error code on failure.
 */
int tuya_mqtt_upgrade_progress_report(tuya_mqtt_context_t *context, int channel, int percent)
{
    if (percent > 100) {
        PR_ERR("input invalid:%d", percent);
        return OPRT_INVALID_PARM;
    }

    uint8_t *data_buf = tal_malloc(128);
    if (NULL == data_buf) {
        return OPRT_MALLOC_FAILED;
    }

    int buffer_size = sprintf((char *)data_buf, "{\"progress\":\"%d\",\"firmwareType\":%d}", percent, channel);
    uint16_t msgid = tuya_mqtt_protocol_data_publish(context, PRO_UPGE_PUSH, data_buf, (uint16_t)buffer_size);
    tal_free(data_buf);
    if (msgid <= 0) {
        return OPRT_COM_ERROR;
    }
    return OPRT_OK;
}

/**
 * @file mqtt_service.h
 * @brief Header file for the MQTT service in the Tuya IoT SDK.
 *
 * This file declares constants, structures, and functions for the MQTT service
 * used within the Tuya IoT SDK. It includes definitions for maximum lengths of
 * various MQTT parameters such as client ID, username, password, and topic.
 * Additionally, it defines protocol numbers for different types of MQTT
 * messages, such as device-to-cloud data push, cloud-to-device commands, device
 * unbinding, device reset, and timer update information.
 *
 * The constants and definitions provided in this file are essential for the
 * correct operation of the MQTT service, ensuring that the communication
 * between IoT devices and the Tuya cloud platform is secure, reliable, and
 * adheres to the protocol specifications.
 *
 * @copyright Copyright (c) 2021-2024 Tuya Inc. All Rights Reserved.
 *
 */

#ifndef TUYA_MQTT_SERVICE_H_
#define TUYA_MQTT_SERVICE_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include "cJSON.h"
#include "mqtt_client_interface.h"
#include "backoff_algorithm.h"

// data max len
#define TUYA_MQTT_CLIENTID_MAXLEN   (32U)
#define TUYA_MQTT_USERNAME_MAXLEN   (32U)
#define TUYA_MQTT_PASSWORD_MAXLEN   (32U)
#define TUYA_MQTT_CIPHER_KEY_MAXLEN (32U)
#define TUYA_MQTT_DEVICE_ID_MAXLEN  (32U)
#define TUYA_MQTT_UUID_MAXLEN       (32U)
#define TUYA_MQTT_TOPIC_MAXLEN      (64U)
#define TUYA_MQTT_TOPIC_MAXLEN      (64U)

// Tuya mqtt protocol
#define PRO_DATA_PUSH            4  /* device -> cloud push dp data */
#define PRO_CMD                  5  /* cloud -> device send dp data */
#define PRO_DEV_UNBIND           8  /* cloud -> device */
#define PRO_GW_RESET             11 /* cloud -> device reset device */
#define PRO_TIMER_UG_INF         13 /* cloud -> device update timer */
#define PRO_UPGD_REQ             15 /* cloud -> device update device/gateway */
#define PRO_UPGE_PUSH            16 /* device -> cloud update upgrade percent */
#define PRO_DEV_DA_REQ           18  /* device -> cloud send data request */
#define PRO_DEV_DA_RESP          19  /* cloud -> device send data response */
#define PRO_IOT_DA_REQ           22 /* cloud -> device send data request */
#define PRO_IOT_DA_RESP          23 /* device -> cloud send data response */
#define PRO_DEV_LINE_STAT_UPDATE 25 /* device -> sub device online status update */
#define PRO_CMD_ACK              26 /* device -> cloud device send ackId to cloud */
#define PRO_MQ_EXT_CFG_INF                                                                                             \
    27                                  /* cloud -> device runtime configuration update                                \
                                         */
#define PRO_MQ_QUERY_DP             31  /* cloud -> device query dp status */
#define PRO_GW_SIGMESH_TOPO_UPDATE  33  /* cloud -> device sigmesh topology update */
#define PRO_GW_LINKAGE_UPDATE       49  /* cloud -> device scene update push */
#define PRO_UG_SUMMER_TABLE         41  // upgrade summer timer table
#define PRO_GW_UPLOAD_LOG           45  /* device -> cloud, upload log */
#define PRO_MQ_ACTIVE_TOKEN_ON      46  /* cloud -> device direct device activation token issuance */
#define PRO_GW_LINKAGE_UPDATE       49  /* cloud -> device scene update push */
#define PRO_MQ_THINGCONFIG          51  /* device password-free networking */
#define PRO_MQ_LOG_CONFIG           55  /* log configuration */
#define PRO_MQ_DPCACHE_NOTIFY       103 /* dp cache notify */
#define PRO_MQ_EN_GW_ADD_DEV_REQ    200 // gateway enable add sub device request
#define PRO_MQ_EN_GW_ADD_DEV_RESP   201 // gateway enable add sub device response
#define PRO_DEV_LC_GROUP_OPER       202 /* cloud -> device */
#define PRO_DEV_LC_GROUP_OPER_RESP  203 /* device -> cloud */
#define PRO_DEV_LC_SENCE_OPER       204 /* cloud -> device */
#define PRO_DEV_LC_SENCE_OPER_RESP  205 /* device -> cloud */
#define PRO_DEV_LC_SENCE_EXEC       206 /* cloud -> device */
#define PRO_CLOUD_STORAGE_ORDER_REQ 300 /* cloud storage order */
#define PRO_3RD_PARTY_STREAMING_REQ 301 /* echo show/chromecast request */
#define PRO_RTC_REQ                 302 /* cloud -> device */
#define PRO_AI_DETECT_DATA_SYNC_REQ                                                                                    \
    304 /* local AI data update, currently used for face detection sample data                                         \
           update (add/delete/change) */
#define PRO_FACE_DETECT_DATA_SYNC                                                                                      \
    306                                 /* face recognition data synchronization notification, used by access          \
                                           control devices */
#define PRO_CLOUD_STORAGE_EVENT_REQ 307 /* trigger cloud storage linkage */
#define PRO_DOORBELL_STATUS_REQ     308 /* doorbell request handled by user, answer or reject */
#define PRO_MQ_CLOUD_STREAM_GATEWAY 312
#define PRO_GW_COM_SENCE_EXE        403 /* cloud -> device move cloud scene to local execution */
#define PRO_DEV_ALARM_DOWN          701 /* cloud -> device */
#define PRO_DEV_ALARM_UP            702 /* device -> cloud */

typedef struct {
    const char *uuid;
    const char *authkey;
    const char *devid;
    const char *seckey;
    const char *localkey;
} tuya_meta_info_t;

typedef struct {
    const uint8_t *cacert;
    size_t cacert_len;
    const char *host;
    uint16_t port;
    uint32_t timeout;
    const char *uuid;
    const char *authkey;
    const char *devid;
    const char *seckey;
    const char *localkey;
    void *user_data;
    void (*on_connected)(void *context, void *user_data);
    void (*on_disconnect)(void *context, void *user_data);
    void (*on_unbind)(void *context, void *user_data);
} tuya_mqtt_config_t;

typedef struct {
    char clientid[TUYA_MQTT_CLIENTID_MAXLEN + 1];
    char username[TUYA_MQTT_USERNAME_MAXLEN + 1];
    char password[TUYA_MQTT_PASSWORD_MAXLEN + 1];
    char cipherkey[TUYA_MQTT_CIPHER_KEY_MAXLEN + 1];
    char topic_in[TUYA_MQTT_TOPIC_MAXLEN + 1];
    char topic_out[TUYA_MQTT_TOPIC_MAXLEN + 1];
} tuya_mqtt_access_t;

typedef struct {
    uint16_t event_id;
    cJSON *root_json;
    cJSON *data;
    void *user_data;
} tuya_protocol_event_t;

typedef tuya_protocol_event_t tuya_mqtt_event_t; // compat TODO:remove

typedef void (*tuya_protocol_callback_t)(tuya_protocol_event_t *event);

typedef struct tuya_protocol_handle {
    struct tuya_protocol_handle *next;
    uint16_t id;
    tuya_protocol_callback_t cb;
    void *user_data;
} tuya_protocol_handle_t;

typedef void (*mqtt_subscribe_message_cb_t)(uint16_t msgid, const mqtt_client_message_t *msg, void *userdata);

typedef struct mqtt_subscribe_handle {
    struct mqtt_subscribe_handle *next;
    char *topic;
    size_t topic_length;
    mqtt_subscribe_message_cb_t cb;
    void *userdata;
} mqtt_subscribe_handle_t;

typedef void (*mqtt_publish_notify_cb_t)(int result, void *user_data);

typedef struct mqtt_publish_handle {
    struct mqtt_publish_handle *next;
    uint16_t msgid;
    int timeout;
    char *topic;
    uint8_t *payload;
    size_t payload_length;
    mqtt_publish_notify_cb_t cb;
    void *user_data;
} mqtt_publish_handle_t;

typedef struct {
    void *mqtt_client;
    tuya_mqtt_access_t signature;
    tuya_protocol_handle_t *protocol_list;
    mqtt_subscribe_handle_t *subscribe_list;
    mqtt_publish_handle_t *publish_list;
    BackoffAlgorithmContext_t backoff_algorithm;
    uint32_t sequence_in;
    uint32_t sequence_out;
    bool manual_disconnect;
    bool is_inited;
    bool is_connected;
    void *user_data;
    void (*on_connected)(void *context, void *user_data);
    void (*on_disconnect)(void *context, void *user_data);
    void (*on_unbind)(void *context, void *user_data);
} tuya_mqtt_context_t;

/**
 * @brief Initializes the MQTT service.
 *
 * This function initializes the MQTT service with the provided context and
 * configuration.
 *
 * @param context Pointer to the MQTT context structure.
 * @param config Pointer to the MQTT configuration structure.
 * @return Returns 0 on success, or a negative error code on failure.
 */
int tuya_mqtt_init(tuya_mqtt_context_t *context, const tuya_mqtt_config_t *config);

/**
 * @brief Starts the MQTT service.
 *
 * This function starts the MQTT service using the provided MQTT context.
 *
 * @param context The MQTT context to be used for starting the service.
 * @return Returns 0 on success, or a negative error code on failure.
 */
int tuya_mqtt_start(tuya_mqtt_context_t *context);

/**
 * @brief Stops the MQTT service.
 *
 * This function stops the MQTT service associated with the given context.
 *
 * @param context Pointer to the MQTT context.
 * @return Returns 0 on success, or a negative error code on failure.
 */
int tuya_mqtt_stop(tuya_mqtt_context_t *context);

/**
 * @brief Executes the MQTT event loop for the Tuya MQTT service.
 *
 * This function is responsible for processing incoming MQTT messages and
 * handling any pending MQTT operations. It should be called periodically to
 * ensure proper functioning of the MQTT service.
 *
 * @param context A pointer to the MQTT context structure.
 * @return An integer value indicating the result of the operation.
 *         - 0: Success.
 *         - Negative values: Error codes indicating failure.
 */
int tuya_mqtt_loop(tuya_mqtt_context_t *context);

/**
 * @brief Destroys the MQTT context and releases any resources associated with
 * it.
 *
 * @param context Pointer to the MQTT context.
 * @return Returns 0 on success, or a negative error code on failure.
 */
int tuya_mqtt_destory(tuya_mqtt_context_t *context);

/**
 * @brief Checks if the MQTT connection is established.
 *
 * This function checks whether the MQTT connection is established or not.
 *
 * @param context Pointer to the MQTT context.
 * @return `true` if the MQTT connection is established, `false` otherwise.
 */
bool tuya_mqtt_connected(tuya_mqtt_context_t *context);

/**
 * @brief Registers a MQTT protocol with the given context.
 *
 * This function registers a MQTT protocol with the specified context. The
 * protocol is identified by the protocol ID. When a message with the registered
 * protocol ID is received, the provided callback function will be called.
 *
 * @param context The MQTT context to register the protocol with.
 * @param protocol_id The ID of the protocol to register.
 * @param cb The callback function to be called when a message with the
 * registered protocol ID is received.
 * @param user_data User data to be passed to the callback function.
 *
 * @return 0 on success, or a negative error code on failure.
 */
int tuya_mqtt_protocol_register(tuya_mqtt_context_t *context, uint16_t protocol_id, tuya_protocol_callback_t cb,
                                void *user_data);

/**
 * @brief Unregisters a MQTT protocol with the specified protocol ID and
 * callback function.
 *
 * This function unregisters a MQTT protocol from the given MQTT context. The
 * protocol ID and callback function are used to identify the protocol to be
 * unregistered. Once unregistered, the protocol will no longer receive MQTT
 * messages.
 *
 * @param context The MQTT context from which to unregister the protocol.
 * @param protocol_id The ID of the protocol to unregister.
 * @param cb The callback function associated with the protocol.
 * @return int Returns 0 on success, or a negative error code on failure.
 */
int tuya_mqtt_protocol_unregister(tuya_mqtt_context_t *context, uint16_t protocol_id, tuya_protocol_callback_t cb);

/**
 * @brief Publishes protocol data using MQTT.
 *
 * This function is used to publish protocol data using MQTT. It takes a MQTT
 * context, protocol ID, data, and length as parameters.
 *
 * @param context The MQTT context.
 * @param protocol_id The protocol ID.
 * @param data The data to be published.
 * @param length The length of the data.
 *
 * @return Returns an integer value indicating the success or failure of the
 * operation.
 */

int tuya_mqtt_protocol_data_publish(tuya_mqtt_context_t *context, uint16_t protocol_id, const uint8_t *data,
                                    uint16_t length);

/**
 * Publishes protocol data with a specified topic using the MQTT service.
 *
 * @param context The MQTT context.
 * @param topic The topic to publish the data to.
 * @param protocol_id The protocol ID.
 * @param data The data to be published.
 * @param length The length of the data.
 * @return Returns 0 on success, or a negative error code on failure.
 */
int tuya_mqtt_protocol_data_publish_with_topic(tuya_mqtt_context_t *context, const char *topic, uint16_t protocol_id,
                                               const uint8_t *data, uint16_t length);

/**
 * @brief Publishes common MQTT protocol data.
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
 * @return Returns 0 on success, or a negative error code on failure.
 */
int tuya_mqtt_protocol_data_publish_common(tuya_mqtt_context_t *context, uint16_t protocol_id, const uint8_t *data,
                                           uint16_t length, mqtt_publish_notify_cb_t cb, void *user_data,
                                           int timeout_ms, bool async);

/**
 * Publishes MQTT protocol data with a common topic.
 *
 * This function is used to publish MQTT protocol data with a specified topic.
 *
 * @param context The MQTT context.
 * @param topic The topic to publish the data to.
 * @param protocol_id The protocol ID.
 * @param data The data to be published.
 * @param length The length of the data.
 * @param cb The callback function to be called when the publish operation is
 * complete.
 * @param user_data User data to be passed to the callback function.
 * @param timeout_ms The timeout value in milliseconds.
 * @param async Specifies whether the publish operation should be performed
 * asynchronously.
 *
 * @return Returns 0 on success, or a negative error code on failure.
 */
int tuya_mqtt_protocol_data_publish_with_topic_common(tuya_mqtt_context_t *context, const char *topic,
                                                      uint16_t protocol_id, const uint8_t *data, uint16_t length,
                                                      mqtt_publish_notify_cb_t cb, void *user_data, int timeout_ms,
                                                      bool async);

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
 * @return 0 on success, or a negative error code on failure.
 */
int tuya_mqtt_client_publish_common(tuya_mqtt_context_t *context, const char *topic, const uint8_t *payload,
                                    size_t payload_length, mqtt_publish_notify_cb_t cb, void *user_data, int timeout_ms,
                                    bool async);

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
                                                  mqtt_subscribe_message_cb_t cb, void *userdata);

/**
 * @brief Unregisters the callback function for handling MQTT subscribe
 * messages.
 *
 * This function unregisters the callback function that was previously
 * registered for handling MQTT subscribe messages. Once unregistered, the
 * callback function will no longer be called when a subscribe message is
 * received.
 *
 * @param context The MQTT context.
 * @param topic The topic for which the callback function should be
 * unregistered.
 *
 * @return Returns 0 on success, or a negative error code on failure.
 */
int tuya_mqtt_subscribe_message_callback_unregister(tuya_mqtt_context_t *context, const char *topic);

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
int tuya_mqtt_upgrade_progress_report(tuya_mqtt_context_t *context, int channel, int percent);

#ifdef __cplusplus
}
#endif
#endif

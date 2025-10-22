/**
 * @file tuya_iot.h
 * @brief Header file for Tuya IoT SDK.
 *
 * This header file provides the declarations for the Tuya IoT SDK
 * functionalities. It includes integration with various Tuya services such as
 * MQTT, atop, and matop services, JSON processing with cJSON, software timers,
 * protocol handling, data point schema definitions, OTA updates, and Tuya's
 * abstract layer API. It defines the SDK version information and various fields
 * length constants.
 *
 * @copyright Copyright (c. 2021-2024 Tuya Inc. All Rights Reserved.
 *
 */

#ifndef _TUYA_IOT_H_
#define _TUYA_IOT_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "tuya_error_code.h"
#include "tuya_endpoint.h"
#include "tuya_log.h"
#include "mqtt_service.h"
#include "atop_service.h"
#include "matop_service.h"
#include "cJSON.h"
#include "tal_sw_timer.h"
#include "tuya_protocol.h"
#include "dp_schema.h"
#include "tuya_ota.h"
#include "tal_api.h"

/**
 * @brief SDK Version info
 *
 */
#define BS_VERSION TUYA_BSV
#define PV_VERSION TUYA_PV23

/**
 * @brief Fields length
 *
 */
#define MAX_LENGTH_PRODUCT_ID 16
#define MAX_LENGTH_UUID       25
#define MAX_LENGTH_AUTHKEY    32
#define MAX_LENGTH_DEVICE_ID  25
#define MAX_LENGTH_SECKEY     16
#define MAX_LENGTH_LOCALKEY   16
#define MAX_LENGTH_SCHEMA_ID  16
#define MAX_LENGTH_TIMEZONE   6
#define MAX_LENGTH_SW_VER     10 // max string length of VERSION
#define MAX_LENGTH_TOKEN      8  // max string length of TOKEN

/* tuya link sdk event type */
typedef enum {
    TUYA_EVENT_RESET,
    TUYA_EVENT_BIND_START,
    TUYA_EVENT_BIND_TOKEN_ON,
    TUYA_EVENT_ACTIVATE_SUCCESSED,
    TUYA_EVENT_MQTT_CONNECTED,
    TUYA_EVENT_MQTT_DISCONNECT,
    TUYA_EVENT_DP_RECEIVE,
    TUYA_EVENT_DP_RECEIVE_CJSON,
    TUYA_EVENT_DP_RECEIVE_OBJ,
    TUYA_EVENT_DP_RECEIVE_RAW,
    TUYA_EVENT_UPGRADE_NOTIFY,
    TUYA_EVENT_RESET_COMPLETE,
    TUYA_EVENT_TIMESTAMP_SYNC,
    TUYA_EVENT_DPCACHE_NOTIFY,
    TUYA_EVENT_BINDED_NOTIFY,
    TUYA_EVENT_DIRECT_MQTT_CONNECTED,
} tuya_event_id_t;

#define EVENT_ID2STR(S)                                                                                                                                \
    ((S) == TUYA_EVENT_RESET                                                                                                                           \
         ? "TUYA_EVENT_RESET"                                                                                                                          \
         : ((S) == TUYA_EVENT_BIND_START                                                                                                               \
                ? "TUYA_EVENT_BIND_START"                                                                                                              \
                : ((S) == TUYA_EVENT_BIND_TOKEN_ON                                                                                                     \
                       ? "TUYA_EVENT_BIND_TOKEN_ON"                                                                                                    \
                       : ((S) == TUYA_EVENT_ACTIVATE_SUCCESSED                                                                                         \
                              ? "TUYA_EVENT_ACTIVATE_SUCCESSED"                                                                                        \
                              : ((S) == TUYA_EVENT_MQTT_CONNECTED                                                                                      \
                                     ? "TUYA_EVENT_MQTT_CONNECTED"                                                                                     \
                                     : ((S) == TUYA_EVENT_MQTT_DISCONNECT                                                                              \
                                            ? "TUYA_EVENT_MQTT_DISCONNECT"                                                                             \
                                            : ((S) == TUYA_EVENT_DP_RECEIVE                                                                            \
                                                   ? "TUYA_EVENT_DP_RECEIVE"                                                                           \
                                                   : ((S) == TUYA_EVENT_DP_RECEIVE_CJSON                                                               \
                                                          ? "TUYA_EVENT_DP_RECEIVE_CJSON"                                                              \
                                                          : ((S) == TUYA_EVENT_UPGRADE_NOTIFY                                                          \
                                                                 ? "TUYA_EVENT_UPGRADE_NOTIFY"                                                         \
                                                                 : ((S) == TUYA_EVENT_RESET_COMPLETE                                                   \
                                                                        ? "TUYA_EVENT_RESET_COMPLETE"                                                  \
                                                                        : ((S) == TUYA_EVENT_TIMESTAMP_SYNC                                            \
                                                                               ? "TUYA_EVENT_TIMESTAMP_SYNC"                                           \
                                                                               : ((S) == TUYA_EVENT_DPCACHE_NOTIFY                                     \
                                                                                      ? "TUYA_EVENT_DPCACHE_NOTIFY"                                    \
                                                                                      : ((S) == TUYA_EVENT_BINDED_NOTIFY                               \
                                                                                             ? "TUYA_EVENT_BINDED_"                                    \
                                                                                               "NOTIFY"                                                \
                                                                                             : ((S) == TUYA_EVENT_DP_RECEIVE_OBJ                       \
                                                                                                    ? "TUYA_EVENT_DP_"                                 \
                                                                                                      "RECEIVE_OBJ"                                    \
                                                                                                    : ((S) == TUYA_EVENT_DP_RECEIVE_RAW                \
                                                                                                           ? "TUYA_"                                   \
                                                                                                             "EVENT_"                                  \
                                                                                                             "DP_"                                     \
                                                                                                             "RECEIVE"                                 \
                                                                                                             "_RAW"                                    \
                                                                                                           : ((S) == TUYA_EVENT_DIRECT_MQTT_CONNECTED  \
                                                                                                                  ? "TUYA_EVENT_DIRECT_MQTT_CONNECTED" \
                                                                                                                  : "Unknown"))))))))))))))))
// clang-format on

typedef enum {
    TUYA_STATUS_UNACTIVE = 0,
    TUYA_STATUS_NETCFG_IDLE = 1,
    TUYA_STATUS_UNCONNECT_ROUTER = 2,
    TUYA_STATUS_WIFI_CONNECTED = 3,
    TUYA_STATUS_MQTT_CONNECTED = 4,
} tuya_client_status_t;

typedef enum {
    TUYA_RESET_TYPE_FACTORY,
    TUYA_RESET_TYPE_REMOTE_UNACTIVE,
    TUYA_RESET_TYPE_LOCAL_UNACTIVE,
    TUYA_RESET_TYPE_REMOTE_FACTORY,
    TUYA_RESET_TYPE_DATA_FACTORY,
} tuya_reset_type_t;

typedef enum {
    TUYA_DATE_TYPE_UNDEFINED,
    TUYA_DATE_TYPE_BOOLEAN,
    TUYA_DATE_TYPE_INTEGER,
    TUYA_DATE_TYPE_STRING,
    TUYA_DATE_TYPE_RAW,
    TUYA_DATE_TYPE_JSON
} tuya_data_type_t;

typedef void (*tuya_dp_notify_cb_t)(int result, void *user_data);

typedef union {
    char *asString;
    bool asBoolean;
    int32_t asInteger;
    cJSON *asJSON;
    struct {
        uint8_t *buffer;
        uint32_t length;
    } asBuffer;
    dp_obj_recv_t *dpobj;
    dp_raw_recv_t *dpraw;
} tuya_data_value_t;

typedef struct {
    tuya_event_id_t id;
    tuya_data_type_t type;
    tuya_data_value_t value;
} tuya_event_msg_t;

typedef struct tuya_iot_client_handle tuya_iot_client_t;

typedef void (*event_handle_cb_t)(tuya_iot_client_t *client, tuya_event_msg_t *event);

typedef bool (*network_check_cb_t)(void);

typedef struct {
    char *uuid;
    char *authkey;
} tuya_iot_license_t;

typedef struct {
    const char *productkey;
    const char *uuid;
    const char *pincode;
    const char *authkey;
    const char *software_ver;
    const char *modules;
    const char *skill_param;
    const char *storage_namespace;
    const char *firmware_key;
    event_handle_cb_t event_handler;
    network_check_cb_t network_check;
    tuya_ota_event_cb_t ota_handler;
} tuya_iot_config_t;

typedef struct {
    char devid[MAX_LENGTH_DEVICE_ID + 1];
    char seckey[MAX_LENGTH_SECKEY + 1];
    char localkey[MAX_LENGTH_LOCALKEY + 1];
    char schemaId[MAX_LENGTH_SCHEMA_ID + 1];
    char timezone[MAX_LENGTH_TIMEZONE + 1];
    bool resetFactory;
    int capability;
} tuya_activated_data_t;

typedef struct {
    char token[MAX_LENGTH_TOKEN + 1];
    char region[MAX_LENGTH_REGION + 1];
    char regist_key[MAX_LENGTH_REGIST + 1];
} tuya_binding_info_t;

typedef int (*tuya_token_get_cb_t)(tuya_iot_config_t *config);

#define MAX_TOKEN_GET_NUM 2

typedef struct {
    int result;
    uint8_t count;
    SEM_HANDLE sem;
    tuya_token_get_cb_t cb[MAX_TOKEN_GET_NUM];
} tuya_token_get_t;

struct tuya_iot_client_handle {
    tuya_iot_config_t config;
    tuya_activated_data_t activate;
    tuya_mqtt_context_t mqctx;
    matop_context_t matop;
    tuya_event_msg_t event;
    tuya_token_get_t token_get;
    tuya_binding_info_t *binding;
    TIMER_ID check_upgrade_timer;
    uint8_t status;
    uint8_t state;
    uint8_t nextstate;
    bool is_activated;
    /** device manage */
    dp_schema_t *schema;
};

/**
 * @brief Read cloud activation license from chip
 *
 * @param[out] license -  cloud activation license
 * @return int - OPRT_OK successful or error code.
 */
int tuya_iot_license_read(tuya_iot_license_t *license);

/**
 * @brief Initialize the Tuya client implementation
 *
 * @param client - The context to initialize.
 * @param config - defines the properties of the Tuya connection.
 * @return int - OPRT_OK successful or error code.
 */
int tuya_iot_init(tuya_iot_client_t *client, const tuya_iot_config_t *config);

/**
 * @brief Start Tuya client cloud service.
 *
 * @param client - The Tuya client context.
 * @return int - OPRT_OK successful or error code.
 */
int tuya_iot_start(tuya_iot_client_t *client);

/**
 * @brief Stop Tuya client cloud service.
 *
 * @param client - The Tuya client context.
 * @return int - OPRT_OK successful or error code.
 */
int tuya_iot_stop(tuya_iot_client_t *client);

/**
 * @brief Reset the Tuya client.
 *
 * @param client - The Tuya client context.
 * @return int - OPRT_OK successful or error code.
 */
int tuya_iot_reset(tuya_iot_client_t *client);

/**
 * @brief Reconncet the Tuya mqtt client.
 *
 * @param client - The Tuya client context.
 * @return int - OPRT_OK successful or error code.
 */
int tuya_iot_reconnect(tuya_iot_client_t *client);

/**
 * @brief Destroy the Tuya client and release resources.
 *
 * @param client - The Tuya client context.
 * @return int - OPRT_OK successful or error code.
 */
int tuya_iot_destroy(tuya_iot_client_t *client);

/**
 * @brief Loop called to yield the current thread to the underlying Tuya client.
 *
 * @param client - The Tuya client context.
 * @return int - OPRT_OK successful or error code.
 */
int tuya_iot_yield(tuya_iot_client_t *client);

/**
 * @brief Report Tuya data point(DP) services to the cloud.
 *
 * @param client - The Tuya client context.
 * @param dps - DP JSON format e.g: "{"101":true}"
 * @return int - OPRT_OK successful or error code.
 */
int tuya_iot_dp_report_json(tuya_iot_client_t *client, const char *dps);

/**
 * @brief Report Tuya data point(DP) services to the cloud,with time.
 *
 * @param client - The Tuya client context.
 * @param dps - DP JSON format e.g: "{"101":true}"
 * @param time - time e.g: "{"101":1612324744}"
 * @return int - OPRT_OK successful or error code.
 */
int tuya_iot_dp_report_json_with_time(tuya_iot_client_t *client, const char *dps, const char *time);

/**
 * @brief Async report Tuya data point(DP) services to the cloud, with notify.
 *
 * @param client - The Tuya client context.
 * @param dps - DP JSON format e.g: "{"101":true}".
 * @param time - time e.g: "{"101":1612324744}" or NULL.
 * @param cb - report result callback, result: OPRT_OK or OPRT_TIMEOUT.
 * @param user_data - user context data.
 * @param timeout_ms - timeout setting uint ms.
 * @return int - OPRT_OK successful or error code.
 */
int tuya_iot_dp_report_json_async(tuya_iot_client_t *client, const char *dps, const char *time, tuya_dp_notify_cb_t cb,
                                  void *user_data, int timeout_ms);

/**
 * @brief Report Tuya data point(DP) services to the cloud, with notify.
 *
 * @param client - The Tuya client context.
 * @param dps - DP JSON format e.g: "{"101":true}".
 * @param time - time e.g: "{"101":1612324744}" or NULL.
 * @param cb - report result callback, result: OPRT_OK or OPRT_TIMEOUT.
 * @param user_data - user context data.
 * @param timeout_ms - timeout setting uint ms.
 * @return int - OPRT_OK successful or error code.
 */
int tuya_iot_dp_report_json_with_notify(tuya_iot_client_t *client, const char *dps, const char *time,
                                        tuya_dp_notify_cb_t cb, void *user_data, int timeout_ms);

/**
 * @brief Is Tuya client has been activated?
 *
 * @param client - The Tuya client context.
 * @return true activated
 * @return false inactivated.
 */
bool tuya_iot_activated(tuya_iot_client_t *client);

/**
 * @brief Remove Tuya client activated data
 *
 * @param client - The Tuya client context.
 * @return int - OPRT_OK successful or error code.
 */
int tuya_iot_activated_data_remove(tuya_iot_client_t *client);

/**
 * @brief Set up a customized get token interface.
 *
 * @param client - The Tuya client context.
 * @param token_get_func - token get func
 * @return int - OPRT_OK successful or error code.
 */
int tuya_iot_token_get_port_register(tuya_iot_client_t *client, tuya_token_get_cb_t token_get_func);

/**
 * @brief Synchronously update the client software version information.
 *
 * @param client - The Tuya client context.
 * @return int - OPRT_OK successful or error code.
 */
int tuya_iot_version_update_sync(tuya_iot_client_t *client);

/**
 * @brief Synchronously update the client extension modules version information.
 *
 * @param client - The Tuya client context.
 * @param version - New extension modules verison.
 * @return int - OPRT_OK successful or error code.
 */
int tuya_iot_extension_modules_version_update(tuya_iot_client_t *client, const char *version);

/**
 * @brief Get the client device ID.
 *
 * @param client - The Tuya client context.
 * @return const char* - client device ID string.
 */
const char *tuya_iot_devid_get(tuya_iot_client_t *client);

/**
 * @brief Get the client localkey.
 *
 * @param client - The Tuya client context.
 * @return const char* - client localkey string.
 */
const char *tuya_iot_localkey_get(tuya_iot_client_t *client);

/**
 * @brief Get the client seckey.
 *
 * @param client - The Tuya client context.
 * @return const char* - client seckey string.
 */
const char *tuya_iot_seckey_get(tuya_iot_client_t *client);

/**
 * @brief Get the client timezone.
 *
 * @param client - The Tuya client context.
 * @return const char* - client timezone string.
 */
const char *tuya_iot_timezone_get(tuya_iot_client_t *client);

/**
 * @brief Retrieves the Tuya IoT client instance.
 *
 * This function returns a pointer to the Tuya IoT client instance.
 *
 * @return A pointer to the Tuya IoT client instance.
 */
tuya_iot_client_t *tuya_iot_client_get(void);

/**
 * @brief Checks if the Tuya IoT service is connected.
 *
 * This function returns a boolean value indicating whether the Tuya IoT service
 * is currently connected.
 *
 * @return true if the Tuya IoT service is connected, false otherwise.
 */
bool tuya_iot_is_connected(void);

/**
 * @brief Dispatches an event for the Tuya IoT client.
 *
 * This function is responsible for dispatching an event for the Tuya IoT
 * client.
 *
 * @param client A pointer to the Tuya IoT client.
 * @return An integer value indicating the status of the event dispatching.
 *         Returns 0 on success, and a negative value on failure.
 */
int tuya_iot_dispatch_event(tuya_iot_client_t *client);

#ifdef __cplusplus
}
#endif
#endif

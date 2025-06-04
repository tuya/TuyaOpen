/**
 * @file atop_service.c
 * @brief Implementation of ATOP service functions.
 *
 * This file contains the implementation of functions for device activation,
 * data reporting, and other services using the ATOP protocol. It includes
 * functions for sending activation requests to the Tuya cloud, handling the
 * responses, and managing the communication process between devices and the
 * Tuya cloud platform.
 *
 * The ATOP service functions are designed to facilitate the secure and
 * efficient exchange of data between IoT devices and the Tuya cloud, supporting
 * various operations such as device activation, status reporting, and firmware
 * updates.
 *
 * @copyright Copyright (c) 2021-2024 Tuya Inc. All Rights Reserved.
 *
 */

#include <inttypes.h>
#include "tuya_cloud_types.h"
#include "atop_base.h"
#include "tal_log.h"
#include "tuya_error_code.h"
#include "atop_service.h"
#include "tal_time_service.h"
#include "tal_memory.h"
#include "tuya_iot.h"

#define ATOP_DEFAULT_POST_BUFFER_LEN (128)

#define CAD_VER       "1.0.3"
#define CD_VER        "1.0.0"
#define ATTRIBUTE_OTA (11)

/**
 * @brief Sends an activate request to the ATOP service.
 *
 * This function sends an activate request to the ATOP service using the
 * provided request parameters.
 *
 * @param request The activate request parameters.
 * @param response The response structure to store the result.
 * @return Returns OPRT_OK if the request is successful, otherwise returns an
 * error code.
 */
int atop_service_activate_request(const tuya_activite_request_t *request, atop_base_response_t *response)
{
    if (NULL == request || NULL == response) {
        return OPRT_INVALID_PARM;
    }

    int rt = OPRT_OK;

    /* post data */
#define ACTIVATE_POST_BUFFER_LEN (255)
    size_t prealloc_size = ACTIVATE_POST_BUFFER_LEN;

    if (request->devid) {
        prealloc_size += strlen(request->devid) + 10;
    }

    if (request->modules) {
        prealloc_size += strlen(request->modules) + 10;
    }

    if (request->feature) {
        prealloc_size += strlen(request->feature) + 10;
    }

    if (request->skill_param) {
        prealloc_size += strlen(request->skill_param) + 10;
    }

    if (request->firmware_key) {
        prealloc_size += strlen(request->firmware_key) + 10;
    }

    char *buffer = tal_malloc(prealloc_size);
    if (NULL == buffer) {
        PR_ERR("post buffer malloc fail");
        return OPRT_MALLOC_FAILED;
    }
    uint32_t timestamp = tal_time_get_posix();

    /* activate JSON format */
    size_t offset = 0;

    /* Requires params */
    offset = sprintf(buffer,
                     "{\"token\":\"%s\",\"softVer\":\"%s\",\"productKey\":\"%"
                     "s\",\"protocolVer\":\"%s\",\"baselineVer\":\"%s\"",
                     request->token, request->sw_ver, request->product_key, request->pv, request->bv);

    /* option params */
    offset += sprintf(buffer + offset, ",\"options\": \"%s", "{\\\"otaChannel\\\":0, ");
    if (request->firmware_key && request->firmware_key[0]) {
        offset += sprintf(buffer + offset, "\\\"isFK\\\":true");
    } else {
        offset += sprintf(buffer + offset, "\\\"isFK\\\":false");
    }
    offset += sprintf(buffer + offset, "}\"");

    /* firmware_key */
    if (request->firmware_key && request->firmware_key[0]) {
        offset += sprintf(buffer + offset, ",\"productKeyStr\":\"%s\"", request->firmware_key);
    }

    /* Activated atop */
    if (request->devid && strlen(request->devid) > 0) {
        offset += sprintf(buffer + offset, ",\"devId\":\"%s\"", request->devid);
    }

    /* modules */
    if (request->modules && strlen(request->modules) > 0) {
        offset += sprintf(buffer + offset, ",\"modules\":\"%s\"", request->modules);
    }

    /* feature */
    if (request->feature && strlen(request->feature) > 0) {
        offset += sprintf(buffer + offset, ",\"feature\":\"%s\"", request->feature);
    }

    /* skill_param */
    if (request->skill_param && strlen(request->skill_param) > 0) {
        offset += sprintf(buffer + offset, ",\"skillParam\":\"%s\"", request->skill_param);
    }

    /* default support device OTA */
    offset += sprintf(buffer + offset, ",\"devAttribute\":%u", 1 << ATTRIBUTE_OTA);

    offset +=
        sprintf(buffer + offset, ",\"cadVer\":\"%s\",\"cdVer\":\"%s\",\"t\":%" PRIu32 "}", CAD_VER, CD_VER, timestamp);

    PR_DEBUG("POST JSON:%s", buffer);

    /* atop_base_request object construct */
    atop_base_request_t atop_request = {.uuid = request->uuid,
                                        .key = request->authkey,
                                        .path = "/d.json",
                                        .timestamp = timestamp,
                                        .api = "thing.device.opensdk.active",
                                        .version = "1.0",
                                        .data = buffer,
                                        .datalen = offset,
                                        .user_data = request->user_data};

    /* ATOP service request send */
    rt = atop_base_request(&atop_request, response);
    tal_free(buffer);
    if (OPRT_OK != rt) {
        PR_ERR("atop_base_request error:%d", rt);
        return rt;
    }
    return rt;
}

/**
 * @brief Resets the client with the specified ID and key.
 *
 * This function sends a request to reset the client with the provided ID and
 * key.
 *
 * @param id The ID of the client.
 * @param key The key of the client.
 * @return Returns OPRT_OK if the reset request is successful, otherwise returns
 * an error code.
 */
int atop_service_client_reset(const char *id, const char *key)
{
    if (NULL == id || NULL == key) {
        return OPRT_INVALID_PARM;
    }

    int rt = OPRT_OK;

    /* post data */
#define RESET_POST_BUFFER_LEN 32
    size_t buffer_len = 0;
    char *buffer = tal_malloc(RESET_POST_BUFFER_LEN);
    if (NULL == buffer) {
        PR_ERR("post buffer malloc fail");
        return OPRT_MALLOC_FAILED;
    }

    uint32_t timestamp = tal_time_get_posix();
    buffer_len = snprintf(buffer, RESET_POST_BUFFER_LEN, "{\"t\":%" PRIu32 "}", timestamp);
    PR_DEBUG("POST JSON:%s", buffer);

    /* atop_base_request object construct */
    atop_base_request_t atop_request = {.devid = id,
                                        .key = key,
                                        .path = "/d.json",
                                        .timestamp = timestamp,
                                        .api = "tuya.device.reset",
                                        .version = "4.0",
                                        .data = buffer,
                                        .datalen = buffer_len,
                                        .user_data = NULL};

    atop_base_response_t response = {0};

    /* ATOP service request send */
    rt = atop_base_request(&atop_request, &response);
    tal_free(buffer);

    bool success = response.success;
    atop_base_response_free(&response);

    if (OPRT_OK != rt) {
        PR_ERR("atop_base_request error:%d", rt);
        return rt;
    }

    if (success == false) {
        return OPRT_COM_ERROR;
    }

    return rt;
}

/**
 * @brief Retrieves the dynamic configuration for a specific service.
 *
 * This function retrieves the dynamic configuration for a specific service
 * identified by the given ID and key. The configuration is returned in the
 * provided response structure.
 *
 * @param id The ID of the service.
 * @param key The key of the service.
 * @param type The type of the dynamic configuration.
 * @param response Pointer to the response structure where the configuration
 * will be stored.
 * @return Returns 0 on success, or a negative error code on failure.
 */
int atop_service_dynamic_cfg_get_v20(const char *id, const char *key, HTTP_DYNAMIC_CFG_TYPE type,
                                     atop_base_response_t *response)
{
    if (NULL == id || NULL == key || NULL == response) {
        return OPRT_INVALID_PARM;
    }

    int rt = OPRT_OK;

    /* post data */
    size_t buffer_len = 0;
    char *buffer = tal_malloc(ATOP_DEFAULT_POST_BUFFER_LEN);
    if (NULL == buffer) {
        PR_ERR("post buffer malloc fail");
        return OPRT_MALLOC_FAILED;
    }

    /* timestamp */
    uint32_t timestamp = tal_time_get_posix();

    switch (type) {
    case HTTP_DYNAMIC_CFG_TZ:
        snprintf(buffer, ATOP_DEFAULT_POST_BUFFER_LEN, "{\"type\":\"[\\\"timezone\\\"]\",\"t\":%" PRIu32 "}",
                 timestamp);
        break;
    case HTTP_DYNAMIC_CFG_RATERULE:
        snprintf(buffer, ATOP_DEFAULT_POST_BUFFER_LEN, "{\"type\":\"[\\\"rateRule\\\"]\",\"t\":%" PRIu32 "}",
                 timestamp);
        break;
    case HTTP_DYNAMIC_CFG_ALL:
    default:
        snprintf(buffer, ATOP_DEFAULT_POST_BUFFER_LEN,
                 "{\"type\":\"[\\\"timezone\\\",\\\"rateRule\\\"]\",\"t\":%" PRIu32 "}", timestamp);
        break;
    }

    buffer_len = strlen(buffer) + 1;
    PR_DEBUG("POST JSON:%s", buffer);

    /* atop_base_request object construct */
    atop_base_request_t atop_request = {.devid = id,
                                        .key = key,
                                        .path = "/d.json",
                                        .timestamp = timestamp,
                                        .api = "tuya.device.dynamic.config.get",
                                        .version = "2.0",
                                        .data = buffer,
                                        .datalen = buffer_len,
                                        .user_data = NULL};

    /* ATOP service request send */
    rt = atop_base_request(&atop_request, response);
    tal_free(buffer);
    if (OPRT_OK != rt) {
        PR_ERR("atop_base_request error:%d", rt);
        return rt;
    }
    return rt;
}

/**
 * @brief Retrieves upgrade information for a device.
 *
 * This function sends a request to the Tuya Cloud API to retrieve upgrade
 * information for a device.
 *
 * @param id The device ID.
 * @param key The device key.
 * @param channel The channel number.
 * @param response Pointer to the atop_base_response_t structure to store the
 * response data.
 * @return Returns OPRT_OK if the request is successful, otherwise returns an
 * error code.
 */
int atop_service_upgrade_info_get_v44(const char *id, const char *key, int channel, atop_base_response_t *response)
{
    if (NULL == id || NULL == key || NULL == response) {
        return OPRT_INVALID_PARM;
    }

    int rt = OPRT_OK;

    /* post data */
    size_t buffer_len = 0;
    char *buffer = tal_malloc(ATOP_DEFAULT_POST_BUFFER_LEN);
    if (NULL == buffer) {
        PR_ERR("post buffer malloc fail");
        return OPRT_MALLOC_FAILED;
    }

    uint32_t timestamp = tal_time_get_posix();
    buffer_len = snprintf(buffer, ATOP_DEFAULT_POST_BUFFER_LEN, "{\"type\":%d,\"t\":%" PRIu32 "}", channel, timestamp);
    PR_DEBUG("POST JSON:%s", buffer);

    /* atop_base_request object construct */
    atop_base_request_t atop_request = {
        .devid = id,
        .key = key,
        .path = "/d.json",
        .timestamp = timestamp,
        .api = "tuya.device.upgrade.get",
        .version = "4.4",
        .data = buffer,
        .datalen = buffer_len,
        .user_data = NULL,
    };

    /* ATOP service request send */
    rt = atop_base_request(&atop_request, response);
    tal_free(buffer);
    if (OPRT_OK != rt) {
        PR_ERR("atop_base_request error:%d", rt);
        return rt;
    }
    return rt;
}

/**
 * Retrieves auto upgrade information for a service.
 *
 * This function retrieves auto upgrade information for a service identified by
 * the provided ID and key. The retrieved information is stored in the provided
 * response structure.
 *
 * @param id The ID of the service.
 * @param key The key of the service.
 * @param response Pointer to the atop_base_response_t structure where the
 * retrieved information will be stored.
 * @return An integer value indicating the success or failure of the operation.
 */
int atop_service_auto_upgrade_info_get_v44(const char *id, const char *key, atop_base_response_t *response)
{
    if (NULL == id || NULL == key || NULL == response) {
        return OPRT_INVALID_PARM;
    }

    int rt = OPRT_OK;

    /* post data */
    size_t buffer_len = 0;
    char *buffer = tal_malloc(ATOP_DEFAULT_POST_BUFFER_LEN);
    if (NULL == buffer) {
        PR_ERR("post buffer malloc fail");
        return OPRT_MALLOC_FAILED;
    }

    uint32_t timestamp = tal_time_get_posix();
    buffer_len = snprintf(buffer, ATOP_DEFAULT_POST_BUFFER_LEN, "{\"subId\":null,\"t\":%" PRIu32 "}", timestamp);
    PR_DEBUG("POST JSON:%s", buffer);

    /* atop_base_request object construct */
    atop_base_request_t atop_request = {
        .devid = id,
        .key = key,
        .path = "/d.json",
        .timestamp = timestamp,
        .api = "tuya.device.upgrade.silent.get",
        .version = "4.4",
        .data = buffer,
        .datalen = buffer_len,
        .user_data = NULL,
    };

    /* ATOP service request send */
    rt = atop_base_request(&atop_request, response);
    tal_free(buffer);
    if (OPRT_OK != rt) {
        PR_ERR("atop_base_request error:%d", rt);
        return rt;
    }
    return rt;
}

/**
 * @brief Updates the upgrade status for a device.
 *
 * This function is used to update the upgrade status of a device in the Tuya
 * Cloud service.
 *
 * @param id The device ID.
 * @param key The device key.
 * @param channel The channel number.
 * @param status The upgrade status to be updated.
 *
 * @return The result of the upgrade status update operation.
 */
int atop_service_upgrade_status_update_v41(const char *id, const char *key, int channel, int status)
{
    if (NULL == id || NULL == key) {
        return OPRT_INVALID_PARM;
    }

    int rt = OPRT_OK;

    /* post data */
    size_t buffer_len = 0;
    char *buffer = tal_malloc(ATOP_DEFAULT_POST_BUFFER_LEN);
    if (NULL == buffer) {
        PR_ERR("post buffer malloc fail");
        return OPRT_MALLOC_FAILED;
    }

    uint32_t timestamp = tal_time_get_posix();
    buffer_len = snprintf(buffer, ATOP_DEFAULT_POST_BUFFER_LEN, "{\"type\":%d,\"upgradeStatus\":%d,\"t\":%" PRIu32 "}",
                          channel, status, timestamp);
    PR_DEBUG("POST JSON:%s", buffer);

    /* atop_base_request object construct */
    atop_base_request_t atop_request = {
        .devid = id,
        .key = key,
        .path = "/d.json",
        .timestamp = timestamp,
        .api = "tuya.device.upgrade.status.update",
        .version = "4.1",
        .data = buffer,
        .datalen = buffer_len,
        .user_data = NULL,
    };

    atop_base_response_t response = {0};

    /* ATOP service request send */
    rt = atop_base_request(&atop_request, &response);
    tal_free(buffer);

    bool success = response.success;
    atop_base_response_free(&response);

    if (OPRT_OK != rt) {
        PR_ERR("atop_base_request error:%d", rt);
        return rt;
    }

    if (success == false) {
        return OPRT_COM_ERROR;
    }

    return rt;
}

/**
 * @brief Updates the version of a service.
 *
 * This function updates the version of a service by sending a POST request to
 * the Tuya Cloud API.
 *
 * @param id The device ID.
 * @param key The device key.
 * @param versions The new version to be updated.
 * @return Returns OPRT_INVALID_PARM if any of the input parameters are NULL.
 *         Returns OPRT_MALLOC_FAILED if memory allocation fails.
 *         Returns the result code of the ATOP service request.
 */
int atop_service_version_update_v41(const char *id, const char *key, const char *versions)
{
    if (NULL == id || NULL == key || NULL == versions) {
        return OPRT_INVALID_PARM;
    }

    int rt = OPRT_OK;

    /* post data */
#define UPDATE_VERSION_BUFFER_LEN 196
    size_t buffer_len = 0;
    char *buffer = tal_malloc(UPDATE_VERSION_BUFFER_LEN);
    if (NULL == buffer) {
        PR_ERR("post buffer malloc fail");
        return OPRT_MALLOC_FAILED;
    }

    uint32_t timestamp = tal_time_get_posix();
    buffer_len =
        snprintf(buffer, UPDATE_VERSION_BUFFER_LEN, "{\"versions\":\"%s\",\"t\":%" PRIu32 "}", versions, timestamp);
    PR_DEBUG("POST JSON:%s", buffer);

    /* atop_base_request object construct */
    atop_base_request_t atop_request = {
        .devid = id,
        .key = key,
        .path = "/d.json",
        .timestamp = timestamp,
        .api = "tuya.device.versions.update",
        .version = "4.1",
        .data = buffer,
        .datalen = buffer_len,
        .user_data = NULL,
    };

    atop_base_response_t response = {0};

    /* ATOP service request send */
    rt = atop_base_request(&atop_request, &response);
    tal_free(buffer);

    bool success = response.success;
    atop_base_response_free(&response);

    if (OPRT_OK != rt) {
        PR_ERR("atop_base_request error:%d", rt);
        return rt;
    }

    if (success == false) {
        return OPRT_COM_ERROR;
    }

    return rt;
}

/**
 * @brief Sends a request to put a reset log to the ATOP service.
 *
 * This function sends a request to the ATOP service to put a reset log.
 *
 * @param id The device ID.
 * @param key The device key.
 * @param rst_buffer The reset buffer containing the log data.
 * @return Returns the operation result code:
 *         - OPRT_INVALID_PARM if either `id` or `key` is NULL.
 *         - OPRT_MALLOC_FAILED if memory allocation for the post buffer fails.
 *         - OPRT_COM_ERROR if the request to the ATOP service fails.
 *         - Any other error code returned by `atop_base_request`.
 *         - OPRT_OK if the request is successful.
 */
int atop_service_put_rst_log_v10(const char *id, const char *key, const char *rst_buffer)
{
    if (NULL == id || NULL == key) {
        return OPRT_INVALID_PARM;
    }

    int rt = OPRT_OK;

    /* post data */
    size_t buffer_len = 0;
    char *buffer = tal_malloc(ATOP_DEFAULT_POST_BUFFER_LEN);
    if (NULL == buffer) {
        PR_ERR("post buffer malloc fail");
        return OPRT_MALLOC_FAILED;
    }
    uint32_t timestamp = tal_time_get_posix();
    buffer_len = snprintf(buffer, ATOP_DEFAULT_POST_BUFFER_LEN, "{%s,\"t\":%" PRIu32 "}", rst_buffer, timestamp);
    PR_DEBUG("POST JSON:%s", buffer);

    /* atop_base_request object construct */
    atop_base_request_t atop_request = {
        .devid = id,
        .key = key,
        .path = "/d.json",
        .timestamp = timestamp,
        .api = "atop.online.debug.log",
        .version = NULL,
        .data = buffer,
        .datalen = buffer_len,
        .user_data = NULL,
    };

    atop_base_response_t response = {0};

    /* ATOP service request send */
    rt = atop_base_request(&atop_request, &response);
    tal_free(buffer);

    bool success = response.success;
    atop_base_response_free(&response);

    if (OPRT_OK != rt) {
        PR_ERR("atop_base_request error:%d", rt);
        return rt;
    }

    if (success == false) {
        return OPRT_COM_ERROR;
    }

    return rt;
}

/**
 * @brief Uploads outdoors property for the ATOP service.
 *
 * This function is used to upload the outdoors property for the ATOP service.
 *
 * @param id The ID of the property.
 * @param key The key of the property.
 * @param countryCode The country code of the property.
 * @param phone The phone number associated with the property.
 * @return Returns 0 on success, or a negative error code on failure.
 */
int atop_service_outdoors_property_upload(const char *id, const char *key, const char *countryCode, const char *phone)
{
    if (NULL == id || NULL == key || NULL == countryCode || NULL == phone) {
        return OPRT_INVALID_PARM;
    }

    int rt = OPRT_OK;
    uint32_t timestamp = tal_time_get_posix();

    /* post data */
#define UPDATE_PROPERTY_BUFFER_LEN 255
    size_t buffer_len = 0;
    char *buffer = tal_malloc(UPDATE_PROPERTY_BUFFER_LEN);
    if (NULL == buffer) {
        PR_ERR("post buffer malloc fail");
        return OPRT_MALLOC_FAILED;
    }

    // {"countryCode":"86","phone":"15656065877"}
    buffer_len = snprintf(buffer, UPDATE_PROPERTY_BUFFER_LEN,
                          "{\"devId\":\"%s\",\"property\":{\"code\":\"phoneInfo\",\"value\":{"
                          "\"countryCode\":\"%s\",\"phone\":\"%s\"}},\"t\":%" PRIu32 "}",
                          id, countryCode, phone, timestamp);
    PR_DEBUG("POST JSON:%s", buffer);

    /* atop_base_request object construct */
    atop_base_request_t atop_request = {
        .devid = id,
        .key = key,
        .path = "/d.json",
        .timestamp = timestamp,
        .api = "tuya.device.trip.outdoors.device.property",
        .version = "1.0",
        .data = buffer,
        .datalen = buffer_len,
        .user_data = NULL,
    };

    atop_base_response_t response = {0};

    /* ATOP service request send */
    rt = atop_base_request(&atop_request, &response);
    tal_free(buffer);

    bool success = response.success;
    atop_base_response_free(&response);

    if (OPRT_OK != rt) {
        PR_ERR("atop_base_request error:%d", rt);
        return rt;
    }

    if (success == false) {
        return OPRT_COM_ERROR;
    }

    return rt;
}

/**
 * @brief Uploads the ICCID (Integrated Circuit Card Identifier) to the Tuya
 * cloud service.
 *
 * This function is used to upload the ICCID of a device to the Tuya cloud
 * service.
 *
 * @param id The ID of the device.
 * @param key The key of the device.
 * @param iccid The ICCID of the device.
 *
 * @return Returns 0 on success, or a negative error code on failure.
 */
int atop_service_iccid_upload(const char *id, const char *key, const char *iccid)
{
    if (NULL == id || NULL == key || NULL == iccid) {
        return OPRT_INVALID_PARM;
    }

    int rt = OPRT_OK;
    uint32_t timestamp = tal_time_get_posix();

    /* post data */
#define UPDATE_PROPERTY_BUFFER_LEN 255
    size_t buffer_len = 0;
    char *buffer = tal_malloc(UPDATE_PROPERTY_BUFFER_LEN);
    if (NULL == buffer) {
        PR_ERR("post buffer malloc fail");
        return OPRT_MALLOC_FAILED;
    }

    // {"countryCode":"86","phone":"15656065877"}
    buffer_len = snprintf(buffer, UPDATE_PROPERTY_BUFFER_LEN, "{\"metas\":{\"catIccId\":\"%s\"},\"t\":%" PRIu32 "}",
                          iccid, timestamp);
    PR_DEBUG("POST JSON:%s", buffer);

    /* atop_base_request object construct */
    atop_base_request_t atop_request = {
        .devid = id,
        .key = key,
        .path = "/d.json",
        .timestamp = timestamp,
        .api = "tuya.device.meta.save",
        .version = "1.0",
        .data = buffer,
        .datalen = buffer_len,
        .user_data = NULL,
    };

    atop_base_response_t response = {0};

    /* ATOP service request send */
    rt = atop_base_request(&atop_request, &response);
    tal_free(buffer);

    bool success = response.success;
    atop_base_response_free(&response);

    if (OPRT_OK != rt) {
        PR_ERR("atop_base_request error:%d", rt);
        return rt;
    }

    if (success == false) {
        return OPRT_COM_ERROR;
    }

    return rt;
}

/**
 * @brief Performs a sync check for the ATOP service.
 *
 * This function checks the sync status of the ATOP service using the provided
 * ID and key.
 *
 * @param id The ID of the ATOP service.
 * @param key The key of the ATOP service.
 * @param p_status Pointer to a variable that will store the sync status.
 *
 * @return Returns 0 on success, or a negative error code on failure.
 */
int atop_service_sync_check(const char *id, const char *key, DEV_SYNC_STATUS_E *p_status)
{
    if (NULL == id || NULL == key || NULL == p_status) {
        return OPRT_INVALID_PARM;
    }

    int rt = OPRT_OK;
    uint32_t timestamp = tal_time_get_posix();

    /* post data */
    size_t buffer_len = 0;
    char *buffer = tal_malloc(ATOP_DEFAULT_POST_BUFFER_LEN);
    if (NULL == buffer) {
        PR_ERR("post buffer malloc fail");
        return OPRT_MALLOC_FAILED;
    }

    buffer_len = snprintf(buffer, ATOP_DEFAULT_POST_BUFFER_LEN, "{\"t\":%" PRIu32 "}", timestamp);
    PR_DEBUG("POST JSON:%s", buffer);

    /* atop_base_request object construct */
    atop_base_request_t atop_request = {
        .devid = id,
        .key = key,
        .path = "/d.json",
        .timestamp = timestamp,
        .api = "tuya.device.info.sync",
        .version = "1.0",
        .data = buffer,
        .datalen = buffer_len,
        .user_data = NULL,
    };

    atop_base_response_t response = {0};

    /* ATOP service request send */
    rt = atop_base_request(&atop_request, &response);
    tal_free(buffer);
    if (OPRT_OK != rt) {
        PR_ERR("atop_base_request error:%d", rt);
        return rt;
    }

    cJSON *js_ret = cJSON_GetObjectItem(response.result, "status");
    if (js_ret != NULL && js_ret->type == cJSON_String) {
        if (0 == strcmp("reset_factory", js_ret->valuestring)) {
            *p_status = DEV_STATUS_RESET_FACTORY;
            PR_NOTICE("RESET_FACTORY.");
        } else if (0 == strcmp("reset", js_ret->valuestring)) {
            *p_status = DEV_STATUS_RESET;
            PR_NOTICE("RESET.");
        } else if (0 == strcmp("enable", js_ret->valuestring)) {
            *p_status = DEV_STATUS_ENABLE;
            PR_NOTICE("ENABLE.");
        } else {
            PR_NOTICE("INVALID CODE.");
            rt = OPRT_COM_ERROR;
        }
    } else {
        rt = OPRT_COM_ERROR;
    }

    atop_base_response_free(&response);
    return rt;
}

/**
 * @brief Retrieves the cached data points (DPs) for a specific device.
 *
 * This function retrieves the cached data points (DPs) for a specific device
 * identified by the provided `id` and `key`. The requested DPs are specified
 * by the `req_dps` parameter. The retrieved DPs are stored in the `response`
 * structure.
 *
 * @param id The identifier of the device.
 * @param key The key of the device.
 * @param req_dps The requested data points (DPs).
 * @param response The structure to store the retrieved DPs.
 * @return An integer value indicating the success or failure of the operation.
 *         A return value of 0 indicates success, while a non-zero value
 *         indicates failure.
 */
int atop_service_cache_dp_get(const char *id, const char *key, const char *req_dps, atop_base_response_t *response)
{
    if (NULL == id || key == NULL || response == NULL) {
        return OPRT_INVALID_PARM;
    }

    int rt = OPRT_OK;
    uint32_t timestamp = tal_time_get_posix();
    /* post data */

    size_t buffer_len = 0;
    char *buffer = tal_malloc(ATOP_DEFAULT_POST_BUFFER_LEN);
    if (NULL == buffer) {
        PR_ERR("post buffer malloc fail");
        return OPRT_MALLOC_FAILED;
    }

    buffer_len = snprintf(buffer, ATOP_DEFAULT_POST_BUFFER_LEN, "{\"dps\":[%s],\"t\":%" PRIu32 "}", req_dps, timestamp);
    PR_DEBUG("POST JSON:%s", buffer);

    /* atop_base_request object construct */
    atop_base_request_t atop_request = {
        .devid = id,
        .key = key,
        .path = "/d.json",
        .timestamp = timestamp,
        .api = "tuya.device.dev.dp.get",
        .version = "2.0",
        .data = buffer,
        .datalen = buffer_len,
        .user_data = NULL,
    };

    /* ATOP service request send */
    rt = atop_base_request(&atop_request, response);
    tal_free(buffer);
    if (OPRT_OK != rt) {
        PR_ERR("atop_base_request error:%d", rt);
        return rt;
    }
    return rt;
}

/**
 * @brief Enables the communication node for the specified ID and key.
 *
 * This function enables the communication node for the specified ID and key.
 *
 * @param id The ID of the communication node.
 * @param key The key of the communication node.
 * @return 0 if the communication node is successfully enabled, otherwise an
 * error code.
 */
int atop_service_comm_node_enable(const char *id, const char *key)
{
    if (NULL == id || NULL == key) {
        return OPRT_INVALID_PARM;
    }

    int rt = OPRT_OK;

    /* post data */
    size_t buffer_len = 0;
    char *buffer = tal_malloc(ATOP_DEFAULT_POST_BUFFER_LEN);
    if (NULL == buffer) {
        PR_ERR("post buffer malloc fail");
        return OPRT_MALLOC_FAILED;
    }

    uint32_t timestamp = tal_time_get_posix();
    buffer_len = snprintf(buffer, ATOP_DEFAULT_POST_BUFFER_LEN, "{\"t\":%" PRIu32 "}", timestamp);
    PR_DEBUG("POST JSON:%s", buffer);

    /* atop_base_request object construct */
    atop_base_request_t atop_request = {.devid = id,
                                        .key = key,
                                        .path = "/d.json",
                                        .timestamp = timestamp,
                                        .api = "tuya.device.comm.node.enable",
                                        .version = "1.0",
                                        .data = buffer,
                                        .datalen = buffer_len,
                                        .user_data = NULL};

    atop_base_response_t response = {0};

    /* ATOP service request send */
    rt = atop_base_request(&atop_request, &response);
    tal_free(buffer);

    bool success = response.success;
    atop_base_response_free(&response);

    if (OPRT_OK != rt) {
        PR_ERR("atop_base_request error:%d", rt);
        return rt;
    }

    if (success == false) {
        return OPRT_COM_ERROR;
    }

    return rt;
}

/**
 * @brief Disable communication node for ATOP service.
 *
 * This function disables the communication node for ATOP service.
 *
 * @param id The ID of the communication node.
 * @param key The key of the communication node.
 * @return Returns 0 on success, or a negative error code on failure.
 */
int atop_service_comm_node_disable(const char *id, const char *key)
{
    if (NULL == id || NULL == key) {
        return OPRT_INVALID_PARM;
    }

    int rt = OPRT_OK;

    /* post data */
    size_t buffer_len = 0;
    char *buffer = tal_malloc(ATOP_DEFAULT_POST_BUFFER_LEN);
    if (NULL == buffer) {
        PR_ERR("post buffer malloc fail");
        return OPRT_MALLOC_FAILED;
    }

    uint32_t timestamp = tal_time_get_posix();
    buffer_len = snprintf(buffer, ATOP_DEFAULT_POST_BUFFER_LEN, "{\"t\":%" PRIu32 "}", timestamp);
    PR_DEBUG("POST JSON:%s", buffer);

    /* atop_base_request object construct */
    atop_base_request_t atop_request = {.devid = id,
                                        .key = key,
                                        .path = "/d.json",
                                        .timestamp = timestamp,
                                        .api = "tuya.device.comm.node.disable",
                                        .version = "1.0",
                                        .data = buffer,
                                        .datalen = buffer_len,
                                        .user_data = NULL};

    atop_base_response_t response = {0};

    /* ATOP service request send */
    rt = atop_base_request(&atop_request, &response);
    tal_free(buffer);

    bool success = response.success;
    atop_base_response_free(&response);

    if (OPRT_OK != rt) {
        PR_ERR("atop_base_request error:%d", rt);
        return rt;
    }

    if (success == false) {
        return OPRT_COM_ERROR;
    }

    return rt;
}

/**
 * @brief Sends a simple POST request to the Tuya cloud service.
 *
 * This function sends a simple POST request to the Tuya cloud service using the
 * specified API, version, body, and user data.
 *
 * @param api The API endpoint to send the request to.
 * @param version The version of the API.
 * @param body The body of the request.
 * @param user_data Additional user data to include in the request.
 * @param result A pointer to a cJSON object to store the response result.
 *
 * @return Returns 0 on success, or a negative error code on failure.
 */
int atop_service_comm_post_simple(const char *api, const char *version, const char *body, const char *user_data,
                                  cJSON **result)
{
    if (NULL == api || version == NULL) {
        return OPRT_INVALID_PARM;
    }

    int rt = OPRT_OK;
    tuya_iot_client_t *iot_client = tuya_iot_client_get();
    uint32_t timestamp = tal_time_get_posix();
    /* post data */

    char *buffer = NULL;
    size_t buffer_len = 0;
    if (body) {
        buffer = tal_malloc(strlen(body) + 1);
        if (NULL == buffer) {
            PR_ERR("post buffer malloc fail");
            return OPRT_MALLOC_FAILED;
        }

        buffer_len = snprintf(buffer, ATOP_DEFAULT_POST_BUFFER_LEN, "%s", body);
    } else {
        buffer = tal_malloc(ATOP_DEFAULT_POST_BUFFER_LEN);
        if (NULL == buffer) {
            PR_ERR("post buffer malloc fail");
            return OPRT_MALLOC_FAILED;
        }

        buffer_len = snprintf(buffer, ATOP_DEFAULT_POST_BUFFER_LEN, "{\"t\":%" PRIu32 "}", timestamp);
    }
    PR_DEBUG("POST JSON:%s", buffer);

    /* atop_base_request object construct */
    atop_base_request_t atop_request = {
        .devid = iot_client->activate.devid,
        .key = iot_client->activate.seckey,
        .path = "/d.json",
        .timestamp = timestamp,
        .api = api,
        .version = version,
        .data = buffer,
        .datalen = buffer_len,
        .user_data = user_data,
    };

    /* ATOP service request send */
    atop_base_response_t response = {0};
    rt = atop_base_request(&atop_request, &response);
    if (buffer) {
        tal_free(buffer);
    }

    bool success = response.success;
    if (result) {
        if (OPRT_OK == rt && success) {
            *result = cJSON_Duplicate(response.result, TRUE);
        }
    }

    atop_base_response_free(&response);
    return rt;
}
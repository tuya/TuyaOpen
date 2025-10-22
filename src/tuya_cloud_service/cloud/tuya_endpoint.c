/**
 * @file tuya_endpoint.c
 * @brief Implementation of endpoint management for Tuya IoT devices.
 *
 * This source file contains the implementation of functions related to managing
 * the endpoints of Tuya IoT devices. It includes functionalities such as
 * retrieving cloud endpoints based on the device's region and environment,
 * storing and managing region and registration key information, and writing
 * these details into persistent storage for future use.
 *
 * The endpoint management is crucial for ensuring that IoT devices can
 * communicate with the correct Tuya cloud services, which may vary based on the
 * device's geographical location and the operational environment (e.g.,
 * development, staging, production). This file leverages Tuya's Abstract Layer
 * API (TAL API) for operations like key-value storage to maintain endpoint
 * information.
 *
 * @copyright Copyright (c) 2021-2024 Tuya Inc. All Rights Reserved.
 *
 */

#include "tuya_endpoint.h"
#include "tal_api.h"

#include "tal_kv.h"

extern int iotdns_cloud_endpoint_get(const char *region, const char *env, tuya_endpoint_t *endpoint);

typedef struct {
    char region[MAX_LENGTH_REGION + 1];
    char regist_key[MAX_LENGTH_REGIST + 1];
    tuya_endpoint_t endpoint;
} endpoint_management_t;

static endpoint_management_t endpoint_mgr;

static int tuya_region_regist_key_write(const char *region, const char *regist_key)
{
    if (NULL == region || NULL == regist_key) {
        PR_ERR("Invalid param");
        return OPRT_INVALID_PARM;
    }

    /* Write kv storage */
    int ret = 0;
    ret = tal_kv_set("region", (const uint8_t *)region, strlen(region));
    if (ret != OPRT_OK) {
        PR_ERR("tal_kv_set region, error:0x%02x", ret);
        return OPRT_KVS_WR_FAIL;
    }

    ret = tal_kv_set("regist_key", (const uint8_t *)regist_key, strlen(regist_key));
    if (ret != OPRT_OK) {
        PR_ERR("tal_kv_set regist_key, error:0x%02x", ret);
        return OPRT_KVS_WR_FAIL;
    }

    return OPRT_OK;
}

static int tuya_region_regist_key_read(char *region, char *regist_key)
{
    if (NULL == region || NULL == regist_key) {
        PR_ERR("Invalid param");
        return OPRT_INVALID_PARM;
    }

    /* Read the region&env from kv storage */
    int ret = 0;
    size_t len = 0;
    uint8_t *value = NULL;
    ret = tal_kv_get("region", &value, &len);
    if (ret != OPRT_OK) {
        PR_ERR("tal_kv_get region fail:0x%02x", ret);
        return OPRT_KVS_RD_FAIL;
    }
    memcpy(region, value, MAX_LENGTH_REGION);
    region[MAX_LENGTH_REGION] = 0;
    tal_kv_free(value);

    ret = tal_kv_get("regist_key", &value, &len);
    if (ret != OPRT_OK) {
        PR_ERR("tal_kv_get regist_key fail:0x%02x", ret);
        return OPRT_KVS_RD_FAIL;
    }
    memcpy(regist_key, value, MAX_LENGTH_REGIST);
    regist_key[MAX_LENGTH_REGIST] = 0;
    tal_kv_free(value);

    return OPRT_OK;
}

/**
 * @brief Retrieves the certificate for the specified endpoint.
 *
 * This function retrieves the certificate for the specified endpoint from the
 * key-value store.
 *
 * @param[in] endpoint Pointer to the tuya_endpoint_t structure representing the
 * endpoint.
 * @return Operation result. Returns OPRT_OK on success, or an error code on
 * failure.
 */
int tuya_endpoint_cert_get(tuya_endpoint_t *endpoint)
{
    if (NULL == endpoint) {
        PR_ERR("Invalid param");
        return OPRT_INVALID_PARM;
    }

    int ret = tal_kv_get("endpoint.cert", &endpoint->cert, &endpoint->cert_len);
    if (ret != OPRT_OK) {
        PR_ERR("tal_kv_get endpoint.cert fail:0x%02x", ret);
    }

    return ret;
}

/**
 * @brief Sets the certificate for the Tuya endpoint.
 *
 * This function sets the certificate for the Tuya endpoint by storing it in the
 * key-value store.
 *
 * @param endpoint Pointer to the Tuya endpoint structure.
 * @return Returns OPRT_OK on success, or an error code on failure.
 */
int tuya_endpoint_cert_set(tuya_endpoint_t *endpoint)
{
    if (NULL == endpoint) {
        PR_ERR("Invalid param");
        return OPRT_INVALID_PARM;
    }

    int ret = tal_kv_set("endpoint.cert", endpoint->cert, endpoint->cert_len);
    if (ret != OPRT_OK) {
        PR_ERR("tal_kv_set endpoint.cert fail:0x%02x", ret);
    }

    return ret;
}

/**
 * @brief Retrieves the domain information for the endpoint.
 *
 * This function retrieves the domain information for the endpoint by reading
 * the key-value pairs from the key-value database.
 *
 * @param[in] endpoint Pointer to the tuya_endpoint_t structure to store the
 * retrieved domain information.
 * @return Returns OPRT_OK on success, or an error code on failure.
 */
int tuya_endpoint_domain_get(tuya_endpoint_t *endpoint)
{
    if (NULL == endpoint) {
        PR_ERR("Invalid param");
        return OPRT_INVALID_PARM;
    }
    int ret = 0;

    kv_db_t kvdb[] = {
        {"atop.host", KV_STRING, endpoint->atop.host, sizeof(endpoint->atop.host)},
        {"atop.port", KV_USHORT, &endpoint->atop.port, sizeof(endpoint->atop.port)},
        {"atop.path", KV_STRING, endpoint->atop.path, sizeof(endpoint->atop.path)},
        {"mqtt.host", KV_STRING, endpoint->mqtt.host, sizeof(endpoint->mqtt.host)},
        {"mqtt.port", KV_USHORT, &endpoint->mqtt.port, sizeof(endpoint->mqtt.port)},
    };

    ret = tal_kv_serialize_get("endpoint.domain", kvdb, sizeof(kvdb) / sizeof(kvdb[0]));
    if (ret != OPRT_OK) {
        PR_ERR("tal_kv_serialize_set error:%d", ret);
    }

    return ret;
}

/**
 * @brief Sets the domain for the Tuya endpoint.
 *
 * This function sets the domain for the Tuya endpoint by updating the key-value
 * database (kvdb) with the provided endpoint information.
 *
 * @param endpoint Pointer to the tuya_endpoint_t structure containing the
 * endpoint information.
 * @return Returns OPRT_OK if the domain is set successfully, otherwise returns
 * an error code.
 */
int tuya_endpoint_domain_set(tuya_endpoint_t *endpoint)
{
    if (NULL == endpoint) {
        PR_ERR("Invalid param");
        return OPRT_INVALID_PARM;
    }

    int ret = 0;

    kv_db_t kvdb[] = {
        {"atop.host", KV_STRING, endpoint->atop.host, sizeof(endpoint->atop.host)},
        {"atop.port", KV_USHORT, &endpoint->atop.port, sizeof(endpoint->atop.port)},
        {"atop.path", KV_STRING, endpoint->atop.path, sizeof(endpoint->atop.path)},
        {"mqtt.host", KV_STRING, endpoint->mqtt.host, sizeof(endpoint->mqtt.host)},
        {"mqtt.port", KV_USHORT, &endpoint->mqtt.port, sizeof(endpoint->mqtt.port)},
    };

    ret = tal_kv_serialize_set("endpoint.domain", kvdb, sizeof(kvdb) / sizeof(kvdb[0]));
    if (ret != OPRT_OK) {
        PR_ERR("tal_kv_serialize_set error:%d", ret);
    }

    return ret;
}

/**
 * @brief Sets the region and registration key for the Tuya endpoint.
 *
 * This function sets the region and registration key for the Tuya endpoint by
 * writing them to the appropriate variables.
 *
 * @param region The region to set for the Tuya endpoint.
 * @param regist_key The registration key to set for the Tuya endpoint.
 * @return Returns OPRT_OK if the region and registration key are set
 * successfully, or OPRT_KVS_WR_FAIL if there was an error writing the region
 * and registration key.
 */
int tuya_endpoint_region_regist_set(const char *region, const char *regist_key)
{
    if (tuya_region_regist_key_write(region, regist_key) != OPRT_OK) {
        PR_ERR("region_regist_key_write error");
        return OPRT_KVS_WR_FAIL;
    }

    strcpy(endpoint_mgr.region, region);
    strcpy(endpoint_mgr.regist_key, regist_key);
    return OPRT_OK;
}

/**
 * @brief Removes the endpoint configuration from the device.
 *
 * This function removes the endpoint configuration from the device by deleting
 * the corresponding key-value pairs from the key-value store.
 *
 * @return OPRT_OK if the endpoint configuration is successfully removed,
 * otherwise an error code.
 */
int tuya_endpoint_remove(void)
{
    tal_kv_del("region");
    tal_kv_del("regist_key");
    tal_kv_del("endpoint.cert");
    tal_kv_del("endpoint.domain");

    return OPRT_OK;
}

/**
 * @brief Initializes the Tuya endpoint.
 *
 * This function reads the storage region and registration key, and sets the
 * default online environment if necessary.
 *
 * @return The result of the initialization process.
 */
int tuya_endpoint_init(void)
{
    int ret;

    /* Read storage region & registration key */
    ret = tuya_region_regist_key_read(endpoint_mgr.region, endpoint_mgr.regist_key);
    PR_INFO("endpoint_mgr.region:%s", endpoint_mgr.region);
    PR_INFO("endpoint_mgr.regist_key:%s", endpoint_mgr.regist_key);

    /* Default online environment */
    if (endpoint_mgr.regist_key[0] == 0) {
        strcpy(endpoint_mgr.regist_key, "pro");
    }

    return ret;
}

/**
 * @brief Updates the Tuya endpoint.
 *
 * This function updates the Tuya endpoint by retrieving the iot-dns domain data
 * using the specified region and registration key. If the iotdns has already
 * been called, the allocated certificate memory will be released before
 * updating the endpoint.
 *
 * @return The result of the endpoint update operation.
 *         Returns 0 on success, or an error code on failure.
 */
int tuya_endpoint_update(void)
{
    int ret;

    /* If iotdns has already been called,
     * the allocated certificate memory needs to be released. */
    if (endpoint_mgr.endpoint.cert != NULL) {
        PR_TRACE("Free endpoint already exist cert.");
        tal_free(endpoint_mgr.endpoint.cert);
    }
    /* Try to get the iot-dns domain data */
    ret = iotdns_cloud_endpoint_get(endpoint_mgr.region, endpoint_mgr.regist_key, &endpoint_mgr.endpoint);

    return ret;
}

/**
 * @brief Updates the auto region for the Tuya endpoint.
 *
 * This function updates the auto region for the Tuya endpoint by retrieving the
 * iot-dns domain data. If the iotdns has already been called, the allocated
 * certificate memory is released.
 *
 * @return The result of the operation. Returns 0 on success, or an error code
 * on failure.
 */
int tuya_endpoint_update_auto_region(void)
{
    int ret;

    /* If iotdns has already been called,
     * the allocated certificate memory needs to be released. */
    if (endpoint_mgr.endpoint.cert != NULL) {
        PR_TRACE("Free endpoint already exist cert.");
        tal_free(endpoint_mgr.endpoint.cert);
    }
    /* Try to get the iot-dns domain data */
    ret = iotdns_cloud_endpoint_get(NULL, endpoint_mgr.regist_key, &endpoint_mgr.endpoint);
    return ret;
}

/**
 * @brief Retrieves the Tuya endpoint.
 *
 * This function returns a pointer to the Tuya endpoint structure.
 *
 * @return A constant pointer to the Tuya endpoint structure.
 */
const tuya_endpoint_t *tuya_endpoint_get(void)
{
    return (const tuya_endpoint_t *)&endpoint_mgr.endpoint;
}

/**
 * @file tuya_authorize.c
 * @brief Implementation of Tuya device authorization and license management.
 *
 * This file implements the core functionality for managing device authorization
 * and licensing in Tuya IoT cloud services. It provides secure storage and
 * retrieval of device credentials including UUID and authentication keys using
 * both Key-Value (KV) storage and One-Time Programmable (OTP) memory. The
 * implementation includes CLI commands for interactive credential management
 * during development and testing.
 *
 * Key features implemented:
 * - Secure credential storage using KV and OTP memory systems
 * - Device UUID and authentication key validation and management
 * - Fallback mechanism from KV storage to OTP for credential retrieval
 * - CLI interface for interactive authorization management
 * - Credential reset functionality for device reprovisioning
 * - Error handling and logging for authorization operations
 *
 * The authorization system ensures that devices can securely authenticate with
 * Tuya's cloud infrastructure by maintaining proper credential management and
 * providing reliable access to device identity information. The implementation
 * supports both development scenarios (using KV storage) and production
 * deployment (using OTP memory) for maximum flexibility.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 *
 */

/*============================ INCLUDES ======================================*/
#include "tal_system.h"
#include "tuya_cloud_types.h"
#include "tuya_iot.h"
#include "tal_log.h"
#include "tal_cli.h"
#include "tal_kv.h"
#include "tuyaopen_license.h"
#if defined(ENABLE_WIFI) && (ENABLE_WIFI == 1)
#include "tal_wifi.h"
#endif
#include "cJSON.h"

/*============================ MACROS ========================================*/
#define KVKEY_TYOPEN_UUID    "UUID_TUYAOPEN"
#define KVKEY_TYOPEN_AUTHKEY "AUTHKEY_TUYAOPEN"
#define UUID_LENGTH          20
#define UUID_LENGTH_16       16
#define AUTHKEY_LENGTH       32

/*============================ MACROFIED FUNCTIONS ===========================*/
/*============================ TYPES =========================================*/
/*============================ PROTOTYPES ====================================*/
static void cli_authorize(int argc, char *argv[]);
static void cli_authorize_read(int argc, char *argv[]);
static void cli_authorize_reset(int argc, char *argv[]);
static void cli_read_mac(int argc, char *argv[]);

/*============================ LOCAL VARIABLES ===============================*/
static char UUID_BUF[UUID_LENGTH + 1] = {0};
static char AUTHKEY_BUF[AUTHKEY_LENGTH + 1] = {0};

static const cli_cmd_t s_cli_cmd[] = {
    {
        .name = "auth",
        .help = "auth $uuid $authkey",
        .func = cli_authorize,
    },
    {
        .name = "auth-read",
        .help = "Read authorization information",
        .func = cli_authorize_read,
    },
    {
        .name = "auth-reset",
        .help = "Reset authorization information",
        .func = cli_authorize_reset,
    },{
        .name = "read_mac",
        .help = "Read device MAC address",
        .func = cli_read_mac,
    }
};

/*============================ IMPLEMENTATION ================================*/
/**
 * @brief Save authorization information to KV
 *
 * @param[in] uuid: need length 20
 * @param[in] authkey: need length 32
 *
 * @return OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 */
OPERATE_RET tuya_authorize_write(const char *uuid, const char *authkey)
{
    if ((OPRT_OK == tal_kv_set(KVKEY_TYOPEN_UUID, (const uint8_t *)uuid, UUID_LENGTH)) &&
        (OPRT_OK == tal_kv_set(KVKEY_TYOPEN_AUTHKEY, (const uint8_t *)authkey, AUTHKEY_LENGTH))) {
        PR_INFO("Authorization write succeeds.");
        return OPRT_OK;
    } else {
        PR_ERR("Authorization write failure.");
        return OPRT_KVS_WR_FAIL;
    }
}

/**
 * @brief Write authorization information with specified storage
 *
 * @param[in] license: uuid and authkey
 * @param[in] mac: MAC address string (12 hex chars), used only when storage == 1 (OTP).
 *                 If NULL, will be obtained via tal_wifi_get_mac when wifi is enabled.
 *                 Ignored for KV storage.
 * @param[in] storage: 0 for KV, 1 for OTP
 *
 * @return OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 */
OPERATE_RET tuya_authorize_write_with_storage(tuya_iot_license_t *license, char *mac, int storage)
{
    OPERATE_RET rt = OPRT_OK;

    if (license == NULL) {
        return OPRT_INVALID_PARM;
    }

    if (storage == 0) {
        // KV storage
        return tuya_authorize_write(license->uuid, license->authkey);
    } else if (storage == 1) {
        // Chip efuse/flash/otp storage
        char mac_buf[13] = {0};
        char *mac_to_use = mac;

#if defined(ENABLE_WIFI) && (ENABLE_WIFI == 1)
        if (mac_to_use == NULL) {
            NW_MAC_S mac_struct = {0};
            rt = tal_wifi_get_mac(WF_STATION, &mac_struct);
            if (rt == OPRT_OK) {
                snprintf(mac_buf, sizeof(mac_buf), "%02X%02X%02X%02X%02X%02X",
                         mac_struct.mac[0], mac_struct.mac[1], mac_struct.mac[2],
                         mac_struct.mac[3], mac_struct.mac[4], mac_struct.mac[5]);
                mac_to_use = mac_buf;
                PR_DEBUG("tal get wifi mac:%s", mac_to_use);
            }
        }
#endif

        cJSON *root = cJSON_CreateObject();
        if (root == NULL) {
            return OPRT_COM_ERROR;
        }
        cJSON_AddStringToObject(root, "auzkey", license->authkey);
        cJSON_AddStringToObject(root, "uuid", license->uuid);
        cJSON_AddBoolToObject(root, "prod_test", false);
        cJSON_AddStringToObject(root, "ap_ssid", "SmartLife");
        if (mac_to_use != NULL) {
            cJSON_AddStringToObject(root, "mac", mac_to_use);
        }

        char *json_str = cJSON_PrintUnformatted(root);
        if (json_str == NULL) {
            cJSON_Delete(root);
            return OPRT_COM_ERROR;
        }

        PR_DEBUG("json_str:%s", json_str);

        rt = tuyaopen_license_write(json_str, strlen(json_str));

        cJSON_free(json_str);
        cJSON_Delete(root);

        return rt;
    }

    PR_ERR("Invalid storage type: %d", storage);
    return OPRT_INVALID_PARM;
}

/**
 * @brief Read authorization information from KV and OTP
 *
 * @param[out] license: uuid and authkey
 *
 * @return OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 */
OPERATE_RET tuya_authorize_read(tuya_iot_license_t *license)
{
    OPERATE_RET rt = OPRT_OK;

    char *uuid = NULL;
    char *authkey = NULL;
    size_t readlen = 0;

    if ((OPRT_OK == tal_kv_get(KVKEY_TYOPEN_UUID, (uint8_t **)&uuid, &readlen)) &&
        (OPRT_OK == tal_kv_get(KVKEY_TYOPEN_AUTHKEY, (uint8_t **)&authkey, &readlen))) {
        // KV read
        memcpy(UUID_BUF, uuid, UUID_LENGTH);
        UUID_BUF[UUID_LENGTH] = '\0';
        memcpy(AUTHKEY_BUF, authkey, AUTHKEY_LENGTH);
        AUTHKEY_BUF[AUTHKEY_LENGTH] = '\0';
        license->uuid = UUID_BUF;
        license->authkey = AUTHKEY_BUF;
        tal_kv_free((uint8_t *)uuid);
        tal_kv_free((uint8_t *)authkey);
        PR_INFO("Authorization read succeeds.");
        return OPRT_OK;
    } else {
        // KV read failed, try other read
        char *data = NULL;
        uint32_t data_len = 0;
        rt = tuyaopen_license_read(&data, &data_len);
        if (OPRT_OK != rt) {
            PR_ERR("tuyaopen_license_read read failure.");
            return OPRT_COM_ERROR;
        }

        // Parse license JSON, e.g.
        // {"auzkey":"keyxxxxxxxxxxxxxxxxxxxxxxxxxxxxx","uuid":"uuidxxxxxxxxxxxxxxxx",
        //  "prod_test":false,"ap_ssid":"SmartLife","mac":"001122334455"}
        cJSON *root = cJSON_ParseWithLength((const char *)data, data_len);
        if (root == NULL) {
            PR_ERR("Authorization license JSON parse failure.");
            tal_free(data);
            return OPRT_COM_ERROR;
        }

        cJSON *j_uuid    = cJSON_GetObjectItemCaseSensitive(root, "uuid");
        cJSON *j_authkey = cJSON_GetObjectItemCaseSensitive(root, "auzkey");
        if (!cJSON_IsString(j_uuid) || !cJSON_IsString(j_authkey)) {
            PR_ERR("Authorization license JSON missing uuid/auzkey.");
            cJSON_Delete(root);
            tal_free(data);
            return OPRT_COM_ERROR;
        }

        char *uuid_str    = j_uuid->valuestring;
        char *authkey_str = j_authkey->valuestring;
        size_t uuid_len    = strlen(uuid_str);
        size_t authkey_len = strlen(authkey_str);

        memset(UUID_BUF, 0, sizeof(UUID_BUF));
        memcpy(UUID_BUF, uuid_str, (uuid_len < UUID_LENGTH) ? uuid_len : UUID_LENGTH);
        memset(AUTHKEY_BUF, 0, sizeof(AUTHKEY_BUF));
        memcpy(AUTHKEY_BUF, authkey_str, (authkey_len < AUTHKEY_LENGTH) ? authkey_len : AUTHKEY_LENGTH);

        cJSON_Delete(root);
        tal_free(data);

        // Write back to KV so subsequent reads can be served from KV directly.
        // Failure here does not invalidate the license read result.
        if (OPRT_OK != tuya_authorize_write(UUID_BUF, AUTHKEY_BUF)) {
            PR_WARN("Authorization license->KV writeback failed, will retry on next read.");
        }

        license->uuid    = UUID_BUF;
        license->authkey = AUTHKEY_BUF;

        PR_INFO("Authorization license read succeeds.");
        return OPRT_OK;
    }
}

/**
 * @brief Read authorization information with specified storage
 *
 * @param[out] license: uuid and authkey
 * @param[in] storage: 0 for KV, 1 for OTP
 *
 * @return OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 */
OPERATE_RET tuya_authorize_read_with_storage(tuya_iot_license_t *license, int storage)
{
    OPERATE_RET rt = OPRT_OK;

    if (storage == 0) {
        // KV read
        char *uuid = NULL;
        char *authkey = NULL;
        size_t readlen = 0;

        if ((OPRT_OK == tal_kv_get(KVKEY_TYOPEN_UUID, (uint8_t **)&uuid, &readlen)) &&
            (OPRT_OK == tal_kv_get(KVKEY_TYOPEN_AUTHKEY, (uint8_t **)&authkey, &readlen))) {
            memset(UUID_BUF, 0, sizeof(UUID_BUF));
            memcpy(UUID_BUF, uuid, (strlen(uuid) < UUID_LENGTH) ? strlen(uuid) : UUID_LENGTH);
            memset(AUTHKEY_BUF, 0, sizeof(AUTHKEY_BUF));
            memcpy(AUTHKEY_BUF, authkey, AUTHKEY_LENGTH);
            license->uuid = UUID_BUF;
            license->authkey = AUTHKEY_BUF;
            tal_kv_free((uint8_t *)uuid);
            tal_kv_free((uint8_t *)authkey);
            PR_INFO("Authorization KV read succeeds.");
            return OPRT_OK;
        } else {
            PR_ERR("Authorization KV read failure.");
            return OPRT_COM_ERROR;
        }
    } else if (storage == 1) {
        // Chip efuse/flash/otp read

        char *data = NULL;
        uint32_t data_len = 0;
        rt = tuyaopen_license_read(&data, &data_len);
        if (OPRT_OK != rt) {
            PR_ERR("Authorization OTP read failure.");
            return OPRT_COM_ERROR;
        }

        // Parse license JSON, e.g.
        // {"auzkey":"keyxxxxxxxxxxxxxxxxxxxxxxxxxxxxx","uuid":"uuidxxxxxxxxxxxxxxxx",
        //  "prod_test":false,"ap_ssid":"SmartLife","mac":"001122334455"}
        cJSON *root = cJSON_ParseWithLength((const char *)data, data_len);
        if (root == NULL) {
            PR_ERR("Authorization OTP JSON parse failure.");
            tal_free(data);
            return OPRT_COM_ERROR;
        }

        cJSON *j_uuid    = cJSON_GetObjectItemCaseSensitive(root, "uuid");
        cJSON *j_authkey = cJSON_GetObjectItemCaseSensitive(root, "auzkey");
        if (!cJSON_IsString(j_uuid) || !cJSON_IsString(j_authkey)) {
            PR_ERR("Authorization OTP JSON missing uuid/auzkey.");
            cJSON_Delete(root);
            tal_free(data);
            return OPRT_COM_ERROR;
        }

        char *uuid_str    = j_uuid->valuestring;
        char *authkey_str = j_authkey->valuestring;
        size_t uuid_len    = strlen(uuid_str);
        size_t authkey_len = strlen(authkey_str);

        memset(UUID_BUF, 0, sizeof(UUID_BUF));
        memcpy(UUID_BUF, uuid_str, (uuid_len < UUID_LENGTH) ? uuid_len : UUID_LENGTH);
        memset(AUTHKEY_BUF, 0, sizeof(AUTHKEY_BUF));
        memcpy(AUTHKEY_BUF, authkey_str, (authkey_len < AUTHKEY_LENGTH) ? authkey_len : AUTHKEY_LENGTH);

        license->uuid    = UUID_BUF;
        license->authkey = AUTHKEY_BUF;

        cJSON_Delete(root);
        tal_free(data);

        PR_INFO("tuyaopen_license_read read succeeds.");
        return OPRT_OK;
    } else {
        PR_ERR("Invalid storage type: %d", storage);
        return OPRT_INVALID_PARM;
    }

    return rt;
}

/**
 * @brief Reset authorization information
 *
 * @return OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 */
OPERATE_RET tuya_authorize_reset()
{
    if ((OPRT_OK == tal_kv_del(KVKEY_TYOPEN_UUID)) && (OPRT_OK == tal_kv_del(KVKEY_TYOPEN_AUTHKEY))) {
        PR_INFO("Authorization reset succeeds.");
        return OPRT_OK;
    } else {
        PR_ERR("Authorization reset failure.");
        return OPRT_KVS_WR_FAIL;
    }
}

/**
 * @brief Initializes the Tuya authorize module.
 *
 * @return OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 */
OPERATE_RET tuya_authorize_init(void)
{
    OPERATE_RET ret = OPRT_OK;

    ret = tal_cli_cmd_register((cli_cmd_t *)&s_cli_cmd, sizeof(s_cli_cmd) / sizeof(s_cli_cmd[0]));

    return ret;
}

static void cli_authorize(int argc, char *argv[])
{
    if (argc < 3) {
        tal_cli_echo("Use like: auth uuidxxxxxxxxxxxxxxxx keyxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
        return;
    }

    int storage = 0; // 0:kv, 1:otp

    char *uuid = argv[1];
    char *authkey = argv[2];
    int uuid_len = strlen(uuid);
    int authkey_len = strlen(authkey);
    PR_DEBUG("uuid:%s(%d)", uuid, uuid_len);
    PR_DEBUG("authkey:%s(%d)", authkey, authkey_len);

    if ((uuid_len != UUID_LENGTH && uuid_len != UUID_LENGTH_16) || (authkey_len != AUTHKEY_LENGTH)) {
        tal_cli_echo("uuid length must be 20/16, authkey length must be 32");
        return;
    }

    char *mac = NULL;

    if (argc >= 4) {
        char *storage_str = argv[3];
        if (strcmp(storage_str, "0") == 0) {
            storage = 0;
        } else if (strcmp(storage_str, "1") == 0) {
            storage = 1;
        } else {
            tal_cli_echo("storage must be 0 or 1");
            return;
        }
        PR_DEBUG("storage:%d", storage);
    }

    if (argc >= 5) {
        mac = argv[4];
        if (strlen(mac) != 12) {
            tal_cli_echo("mac length must be 12");
            return;
        }
        PR_DEBUG("mac:%s", mac);
    }

    tuya_iot_license_t license = {0};
    license.uuid = uuid;
    license.authkey = authkey;

    OPERATE_RET rt = tuya_authorize_write_with_storage(&license, mac, storage);
    if (OPRT_OK == rt) {
        if (storage == 0) {
            tal_cli_echo("Authorization write succeeds.\r\nPlease reset the system to ensure the new credentials are used.");
        } else {
            tal_cli_echo("Authorization write to OTP Succeeds.");
        }
    } else {
        if (storage == 0) {
            tal_cli_echo("Authorization write failure.");
        } else {
            tal_cli_echo("Authorization write to OTP failure.");
        }
    }
}

static void cli_authorize_read(int argc, char *argv[])
{
    OPERATE_RET ret = OPRT_OK;
    tuya_iot_license_t license;

    int storage = 0; // 0:kv, 1:otp

    if (argc >= 2) {
        char *storage_str = argv[1];
        if (strcmp(storage_str, "0") == 0) {
            storage = 0;
        } else if (strcmp(storage_str, "1") == 0) {
            storage = 1;
        } else {
            tal_cli_echo("storage must be 0 or 1");
            return;
        }
        PR_DEBUG("storage:%d", storage);
    }

    ret = tuya_authorize_read_with_storage(&license, storage);
    if (OPRT_OK != ret) {
        tal_cli_echo("Authorization read failure.");
        return;
    }

    tal_cli_echo(UUID_BUF);
    tal_cli_echo(AUTHKEY_BUF);
}

static void cli_authorize_reset(int argc, char *argv[])
{
    if (OPRT_OK == tuya_authorize_reset()) {
        tal_cli_echo("Authorization reset succeeds.");
    } else {
        tal_cli_echo("Authorization reset failure.");
    }
}

static void cli_read_mac(int argc, char *argv[])
{
    OPERATE_RET rt = OPRT_OK;

    char mac_buf[32] = {0};
#if defined(ENABLE_WIFI) && (ENABLE_WIFI == 1)
    NW_MAC_S mac_s;
    memset(&mac_s, 0, sizeof(NW_MAC_S));
    rt = tal_wifi_get_mac(WF_STATION, &mac_s);
    if (OPRT_OK != rt) {
        tal_cli_echo("Failed to get MAC address.");
        return;
    }
    sprintf(mac_buf, "mac: %02x:%02x:%02x:%02x:%02x:%02x", mac_s.mac[0], mac_s.mac[1], mac_s.mac[2], mac_s.mac[3], mac_s.mac[4], mac_s.mac[5]);

    tal_cli_echo(mac_buf);
#else
    tal_cli_echo("Failed to get MAC address.");
#endif
}

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
#include "tal_wifi.h"
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
    },
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
 * @brief Read authorization information from KV and OTP
 *
 * @param[out] license: uuid and authkey
 *
 * @return OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 */
OPERATE_RET tuya_authorize_read(tuya_iot_license_t *license)
{
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
        if (OPRT_OK == tuya_iot_license_read(license)) {
            // otp read
            PR_INFO("Authorization otp read succeeds.");
            return OPRT_OK;
        }
        PR_ERR("Authorization read failure.");
        return OPRT_COM_ERROR;
    }
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
    OPERATE_RET rt = OPRT_OK;

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

    char *p_mac = NULL;
    char mac_buf[13] = {0};

    if (argc >= 5) {
        p_mac = argv[4];
        if (strlen(p_mac) != 12) {
            tal_cli_echo("mac length must be 12");
            return;
        }
        snprintf(mac_buf, sizeof(mac_buf), "%s", p_mac);
        PR_DEBUG("mac:%s", mac_buf);
    } else {
        NW_MAC_S mac_struct = {0};
        rt = tal_wifi_get_mac(WF_STATION, &mac_struct);
        if (rt != OPRT_OK) {
            tal_cli_echo("Authorization write flailure: tal_wifi_get_mac failed");
            return;
        }
        snprintf(mac_buf, sizeof(mac_buf), "%02X%02X%02X%02X%02X%02X",
                 mac_struct.mac[0], mac_struct.mac[1], mac_struct.mac[2],
                 mac_struct.mac[3], mac_struct.mac[4], mac_struct.mac[5]);
        PR_DEBUG("tal get wifi mac:%s", mac_buf);
    }

    if (storage == 0) {
        if (OPRT_OK == tuya_authorize_write((const char *)uuid, (const char *)authkey)) {
            tal_cli_echo("Authorization write succeeds.\r\nPlease reset the system to ensure the new credentials are used.");
        } else {
            tal_cli_echo("Authorization write failure.");
        }
    } else {
        // Only support T5 platform for OTP storage
#if (defined(PLATFORM_T5) && (PLATFORM_T5 == 1))
        // For t5 cmd: 
        // auth uuidxxxxxxxxxxxxxxxx keyxxxxxxxxxxxxxxxxxxxxxxxxxxxxx 1 001122334455
        // auth $uuid $authkey $storage $mac
        // $storage: 0:kv, 1:otp
        // $mac: 001122334455

        // T5 write uuid and authkey to opt
        // {"auzkey":"keyxxxxxxxxxxxxxxxxxxxxxxxxxxxxx","uuid":"uuidxxxxxxxxxxxxxxxx","prod_test":false,"ap_ssid":"SmartLife","mac":"001122334455"}

        // if is default mac, send warning
        if (strcmp(mac_buf, "C8478C000018") == 0) { // "C8478C000018" is the default mac in T5 platform
            tal_cli_echo("Warning: mac is default mac, please check your device mac.");
        }

        cJSON *root = cJSON_CreateObject();
        if (root == NULL) {
            tal_cli_echo("Authorization write flailure: cJSON_CreateObject failed");
            return;
        }
        cJSON_AddStringToObject(root, "auzkey", authkey);
        cJSON_AddStringToObject(root, "uuid", uuid);
        cJSON_AddBoolToObject(root, "prod_test", false);
        cJSON_AddStringToObject(root, "ap_ssid", "SmartLife");
        cJSON_AddStringToObject(root, "mac", mac_buf);

        char *json_str = cJSON_PrintUnformatted(root);
        if (json_str == NULL) {
            tal_cli_echo("Authorization write flailure: cJSON_PrintUnformatted failed");
            cJSON_Delete(root);
            return;
        }

        PR_DEBUG("json_str:%s", json_str);

        // write to otp
        extern int tal_otp_flash_write(uint8_t *data, uint16_t datalen);
        rt = tal_otp_flash_write((uint8_t *)json_str, strlen(json_str));
        if (rt != OPRT_OK) {
            tal_cli_echo("Authorization write to OTP failure.");
        }

        extern int tal_otp_flash_read(uint8_t **data, uint16_t *datalen);
        uint8_t *read_data = NULL;
        uint16_t read_datalen = 0;
        rt = tal_otp_flash_read(&read_data, &read_datalen);
        if (rt != OPRT_OK || read_data == NULL) {
            tal_cli_echo("Authorization read from OTP failure.");
        } else {
            PR_DEBUG("read_data:%s", read_data);
            tal_free(read_data);
            read_data = NULL;
        }

        cJSON_free(json_str);
        cJSON_Delete(root);
#else
        tal_cli_echo("OTP storage is only supported on T5 platform.");
        return;
#endif
    }
}

static void cli_authorize_read(int argc, char *argv[])
{
    OPERATE_RET ret = OPRT_OK;
    tuya_iot_license_t license;

    ret = tuya_authorize_read(&license);
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

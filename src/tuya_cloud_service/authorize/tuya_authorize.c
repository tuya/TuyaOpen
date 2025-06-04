/**
 * @file tuya_authorize.c
 * @date 2025-04-11
 * @brief
 *
 * @copyright Copyright (c) 2021-2024 Tuya Inc. All Rights Reserved.
 *
 */

/*============================ INCLUDES ======================================*/
#include "tuya_cloud_types.h"
#include "tuya_iot.h"
#include "tal_log.h"
#include "tal_cli.h"
#include "tal_kv.h"

/*============================ MACROS ========================================*/
#define KVKEY_TYOPEN_UUID    "UUID_TUYAOPEN"
#define KVKEY_TYOPEN_AUTHKEY "AUTHKEY_TUYAOPEN"
#define UUID_LENGTH          20
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
    if (argc < 3) {
        tal_cli_echo("Use like: auth uuidxxxxxxxxxxxxxxxx keyxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
        return;
    }

    char *uuid = argv[1];
    char *authkey = argv[2];
    int uuid_len = strlen(uuid);
    int authkey_len = strlen(authkey);
    PR_DEBUG("uuid:%s(%d)", uuid, uuid_len);
    PR_DEBUG("authkey:%s(%d)", authkey, authkey_len);

    if ((UUID_LENGTH != uuid_len) || (AUTHKEY_LENGTH != authkey_len)) {
        PR_ERR("uuid len not equal 20 or authkey len not equal 32.");
        tal_cli_echo("uuid len not equal 20 or authkey len not equal 32.");
        return;
    }

    if (OPRT_OK == tuya_authorize_write((const char *)uuid, (const char *)authkey)) {
        tal_cli_echo("Authorization write succeeds.");
    } else {
        tal_cli_echo("Authorization write failure.");
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

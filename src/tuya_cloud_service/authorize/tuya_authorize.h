/**
 * @file tuya_authorize.h
 * @brief Tuya device authorization and license management interface.
 *
 * This file defines the interface for managing device authorization and licensing
 * in Tuya IoT cloud services. It provides functions for reading, writing, and
 * resetting device authorization information including UUID and authentication
 * keys. The authorization system supports both Key-Value (KV) storage and
 * One-Time Programmable (OTP) memory for secure credential management.
 *
 * Key functionalities:
 * - Device authorization initialization and setup
 * - Secure storage and retrieval of device UUID and authentication keys
 * - Support for both volatile (KV) and non-volatile (OTP) credential storage
 * - Authorization reset capabilities for device reprovisioning
 * - Validation of credential format and length requirements
 *
 * The authorization module ensures secure device-to-cloud authentication by
 * managing unique device identifiers and cryptographic keys required for
 * establishing trusted connections with Tuya's cloud infrastructure.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 *
 */

#ifndef __TUYA_AUTHORIZE_H__
#define __TUYA_AUTHORIZE_H__

#include "tuya_cloud_types.h"
#include "tuya_iot.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initializes the Tuya authorize module.
 *
 * @return OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 */
OPERATE_RET tuya_authorize_init(void);

/**
 * @brief Save authorization information to KV
 *
 * @param[in] uuid: need length 20
 * @param[in] authkey: need length 32
 *
 * @return OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 */
OPERATE_RET tuya_authorize_write(const char *uuid, const char *authkey);

/**
 * @brief Read authorization information from KV and OTP
 *
 * @param[out] license: uuid and authkey
 *
 * @return OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 */
OPERATE_RET tuya_authorize_read(tuya_iot_license_t *license);

/**
 * @brief Reset authorization information
 *
 * @return OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 */
OPERATE_RET tuya_authorize_reset(void);

#ifdef __cplusplus
}
#endif
#endif // !__TUYA_AUTHORIZE_H__

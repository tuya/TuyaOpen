/**
 * @file tuya_register_center.h
 * @brief Header file for Tuya's device registration center functionalities.
 *
 * This header file defines the structures, enums, and function prototypes for
 * managing the device registration process with Tuya's cloud services. It
 * includes support for different registration sources, handling of registration
 * data, and management of certificate authority (CA) certificates for secure
 * communication with Tuya's IoT platform. The registration center is crucial
 * for the initial setup and secure operation of IoT devices.
 *
 * @copyright Copyright (c) 2021-2024 Tuya Inc. All Rights Reserved.
 *
 */

#ifndef __TUYA_REGISTER_CENTER_H__
#define __TUYA_REGISTER_CENTER_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "tuya_cloud_types.h"
#include "cJSON.h"

/**
 * Register Center Source
 */
typedef enum { RCS_APP, RCS_MF, RCS_CODE, RCS_MAX } RCS_E;

/**
 * Definition of Register Center
 */
typedef struct {
    RCS_E source;
    BOOL_T pub;
    char *code;
    char *url0;
    char *urlx;
    uint8_t *ca_cert;
    uint32_t ca_cert_len;
} register_center_t;

/**
 * @brief Init and load register center
 *
 * @return OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 */
OPERATE_RET tuya_register_center_init(void);

/**
 * @brief Save register center
 *
 * @return OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 */
OPERATE_RET tuya_register_center_save(RCS_E source, cJSON *rcs);

/**
 * @brief Get register center
 *
 * @return OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 */
OPERATE_RET tuya_register_center_get(register_center_t *rcs);

/**
 * @brief Update ca cert of register center
 *
 * @return OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 */
OPERATE_RET tuya_register_center_update(uint8_t *ca_cert, uint32_t ca_cert_len);

/**
 * @brief Reset register center
 *
 * @return OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 */
OPERATE_RET tuya_register_center_reset(void);

/**
 * @brief get register center url
 *
 * @return url address
 */
char *tuya_register_center_get_url(void);

#ifdef __cplusplus
}
#endif

#endif //__TUYA_REGISTER_CENTER_H__

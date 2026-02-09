/**
 * @file board_power_domain_api.h
 * @author Tuya Inc.
 * @brief Header file for power domain control API for TUYA_T5AI_EINK_NFC board.
 *
 * This module provides APIs to control the power domains for EINK 3V3 and SD card 3V3.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#ifndef __BOARD_POWER_DOMAIN_API_H__
#define __BOARD_POWER_DOMAIN_API_H__

#include "tuya_cloud_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/***********************************************************
************************macro define************************
***********************************************************/

/**
 * @brief GPIO pin used to control EINK 3V3 domain power
 */
#define BOARD_POWER_EINK_3V3_PIN TUYA_GPIO_NUM_30

/**
 * @brief GPIO pin used to control SD card 3V3 domain power
 */
#define BOARD_POWER_SD_3V3_PIN TUYA_GPIO_NUM_31

/**
 * @brief GPIO level to enable power domain (high = enabled)
 */
#define BOARD_POWER_DOMAIN_ENABLE_LV TUYA_GPIO_LEVEL_HIGH

/**
 * @brief GPIO level to disable power domain (low = disabled)
 */
#define BOARD_POWER_DOMAIN_DISABLE_LV TUYA_GPIO_LEVEL_LOW

/***********************************************************
***********************typedef define***********************
***********************************************************/

/**
 * @brief Power domain enumeration
 */
typedef enum {
    BOARD_POWER_DOMAIN_EINK_3V3 = 0, /**< EINK 3V3 power domain */
    BOARD_POWER_DOMAIN_SD_3V3   = 1  /**< SD card 3V3 power domain */
} BOARD_POWER_DOMAIN_E;

/***********************************************************
********************function declaration********************
***********************************************************/

/**
 * @brief Initialize the power domain GPIO pins and enable both domains by default
 *
 * This function initializes the GPIO pins used to control the power domains
 * and sets both EINK 3V3 and SD card 3V3 to enabled (high) by default.
 *
 * @return Returns OPRT_OK on success, or an appropriate error code on failure.
 */
OPERATE_RET board_power_domain_init(void);

/**
 * @brief Enable the EINK 3V3 power domain
 *
 * @return Returns OPRT_OK on success, or an appropriate error code on failure.
 */
OPERATE_RET board_power_domain_eink_3v3_enable(void);

/**
 * @brief Disable the EINK 3V3 power domain
 *
 * @return Returns OPRT_OK on success, or an appropriate error code on failure.
 */
OPERATE_RET board_power_domain_eink_3v3_disable(void);

/**
 * @brief Enable the SD card 3V3 power domain
 *
 * @return Returns OPRT_OK on success, or an appropriate error code on failure.
 */
OPERATE_RET board_power_domain_sd_3v3_enable(void);

/**
 * @brief Disable the SD card 3V3 power domain
 *
 * @return Returns OPRT_OK on success, or an appropriate error code on failure.
 */
OPERATE_RET board_power_domain_sd_3v3_disable(void);

/**
 * @brief Set the power domain state
 *
 * @param[in] domain The power domain to control
 * @param[in] enable true to enable, false to disable
 * @return Returns OPRT_OK on success, or an appropriate error code on failure.
 */
OPERATE_RET board_power_domain_set(BOARD_POWER_DOMAIN_E domain, BOOL_T enable);

/**
 * @brief Get the power domain state
 *
 * @param[in] domain The power domain to query
 * @param[out] enable Pointer to store the state (true = enabled, false = disabled)
 * @return Returns OPRT_OK on success, or an appropriate error code on failure.
 */
OPERATE_RET board_power_domain_get(BOARD_POWER_DOMAIN_E domain, BOOL_T *enable);

/**
 * @brief Deinitialize the power domain GPIO pins
 *
 * @return Returns OPRT_OK on success, or an appropriate error code on failure.
 */
OPERATE_RET board_power_domain_deinit(void);

#ifdef __cplusplus
}
#endif

#endif /* __BOARD_POWER_DOMAIN_API_H__ */

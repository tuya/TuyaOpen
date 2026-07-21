/**
 * @file board_power_domain_api.h
 * @author Tuya Inc.
 * @brief Header file for power domain control API for ZECTRIX_NOTE4_TY board.
 *
 * This board has three switchable 3V3 power domains, each driven by a load-switch
 * MOSFET that is enabled when the control GPIO is driven HIGH:
 *   - E-paper display (3V3_EPD)  : EPD_PWR_EN  -> P23
 *   - SD card          (3V3_SD)  : SD_PWR_EN   -> P8  (V1.1; was P4 on V1.0)
 *   - Audio / speaker  (AVDD_3V3): PA_PWR_EN   -> P42
 *
 * @copyright Copyright (c) 2021-2026 Tuya Inc. All Rights Reserved.
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
 * @brief GPIO pin used to control the E-paper 3V3 domain power (EPD_PWR_EN)
 */
#define BOARD_POWER_EPD_3V3_PIN TUYA_GPIO_NUM_23

/**
 * @brief GPIO pin used to control the SD card 3V3 domain power (SD_PWR_EN)
 * @note V1.1 schematic moved this from P4 to P8 (P4 is now EPD_SDA).
 */
#define BOARD_POWER_SD_3V3_PIN TUYA_GPIO_NUM_8

/**
 * @brief GPIO pin used to control the audio/speaker AVDD_3V3 domain power (PA_PWR_EN)
 */
#define BOARD_POWER_AUDIO_3V3_PIN TUYA_GPIO_NUM_42

/**
 * @brief GPIO level to enable a power domain (high = enabled)
 */
#define BOARD_POWER_DOMAIN_ENABLE_LV TUYA_GPIO_LEVEL_HIGH

/**
 * @brief GPIO level to disable a power domain (low = disabled)
 */
#define BOARD_POWER_DOMAIN_DISABLE_LV TUYA_GPIO_LEVEL_LOW

/***********************************************************
***********************typedef define***********************
***********************************************************/

/**
 * @brief Power domain enumeration
 */
typedef enum {
    BOARD_POWER_DOMAIN_EPD_3V3   = 0, /**< E-paper display 3V3 power domain */
    BOARD_POWER_DOMAIN_SD_3V3    = 1, /**< SD card 3V3 power domain */
    BOARD_POWER_DOMAIN_AUDIO_3V3 = 2  /**< Audio/speaker AVDD_3V3 power domain */
} BOARD_POWER_DOMAIN_E;

/***********************************************************
********************function declaration********************
***********************************************************/

/**
 * @brief Initialize the power domain GPIO pins and enable all domains by default.
 *
 * @return Returns OPRT_OK on success, or an appropriate error code on failure.
 */
OPERATE_RET board_power_domain_init(void);

/**
 * @brief Enable the E-paper 3V3 power domain.
 * @return Returns OPRT_OK on success, or an appropriate error code on failure.
 */
OPERATE_RET board_power_domain_epd_3v3_enable(void);

/**
 * @brief Disable the E-paper 3V3 power domain.
 * @return Returns OPRT_OK on success, or an appropriate error code on failure.
 */
OPERATE_RET board_power_domain_epd_3v3_disable(void);

/**
 * @brief Enable the SD card 3V3 power domain.
 * @return Returns OPRT_OK on success, or an appropriate error code on failure.
 */
OPERATE_RET board_power_domain_sd_3v3_enable(void);

/**
 * @brief Disable the SD card 3V3 power domain.
 * @return Returns OPRT_OK on success, or an appropriate error code on failure.
 */
OPERATE_RET board_power_domain_sd_3v3_disable(void);

/**
 * @brief Enable the audio/speaker AVDD_3V3 power domain.
 * @return Returns OPRT_OK on success, or an appropriate error code on failure.
 */
OPERATE_RET board_power_domain_audio_3v3_enable(void);

/**
 * @brief Disable the audio/speaker AVDD_3V3 power domain.
 * @return Returns OPRT_OK on success, or an appropriate error code on failure.
 */
OPERATE_RET board_power_domain_audio_3v3_disable(void);

/**
 * @brief Set the power domain state.
 *
 * @param[in] domain The power domain to control
 * @param[in] enable TRUE to enable, FALSE to disable
 * @return Returns OPRT_OK on success, or an appropriate error code on failure.
 */
OPERATE_RET board_power_domain_set(BOARD_POWER_DOMAIN_E domain, BOOL_T enable);

/**
 * @brief Get the power domain state.
 *
 * @param[in] domain The power domain to query
 * @param[out] enable Pointer to store the state (TRUE = enabled, FALSE = disabled)
 * @return Returns OPRT_OK on success, or an appropriate error code on failure.
 */
OPERATE_RET board_power_domain_get(BOARD_POWER_DOMAIN_E domain, BOOL_T *enable);

/**
 * @brief Deinitialize the power domain GPIO pins.
 * @return Returns OPRT_OK on success, or an appropriate error code on failure.
 */
OPERATE_RET board_power_domain_deinit(void);

#ifdef __cplusplus
}
#endif

#endif /* __BOARD_POWER_DOMAIN_API_H__ */

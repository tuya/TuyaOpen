/**
 * @file tkl_init_common.h
 * @brief Common process
 * @version 1.0
 * @date 2021-08-06
 *
 * @copyright Copyright 2021-2030 Tuya Inc. All Rights Reserved.
 *
 */
#ifndef __TKL_INIT_COMMON_H__
#define __TKL_INIT_COMMON_H__

#include "tuya_cloud_types.h"
#ifdef ENABLE_RTC
#include "tkl_rtc.h"
#endif
#ifdef ENABLE_WATCHDOG
#include "tkl_watchdog.h"
#endif
#ifdef ENABLE_FLASH
#include "tkl_flash.h"
#endif
#ifdef __cplusplus
extern "C" {
#endif

#ifdef ENABLE_RTC
/*
 * @brief the description of tuya kernel adapter layer rtc
 */
typedef struct {
    OPERATE_RET (*init)(void);
    OPERATE_RET (*deinit)(void);
    OPERATE_RET (*time_get)(TIME_T *time_sec);
    OPERATE_RET (*time_set)(TIME_T time_sec);
} TKL_RTC_DESC_T;

/**
 * @brief register rtc description to tuya object manage
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
TKL_RTC_DESC_T *tkl_rtc_desc_get(void);
#endif

#ifdef ENABLE_WATCHDOG
/**
 * @brief the description of tuya kernel adapter layer watchdog
 */
typedef struct {
    uint32_t (*init)(TUYA_WDOG_BASE_CFG_T *cfg);
    OPERATE_RET (*deinit)(void);
    OPERATE_RET (*refresh)(void);
} TKL_WATCHDOG_DESC_T;

/**
 * @brief register watchdog description to tuya object manage
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
TKL_WATCHDOG_DESC_T *tkl_watchdog_desc_get(void);
#endif

#ifdef ENABLE_FLASH
/**
 * @brief the description of tuya kernel adapter layer flash
 */
typedef struct {
    OPERATE_RET (*read)(const uint32_t addr, uint8_t *dst, const uint32_t size);
    OPERATE_RET (*write)(const uint32_t addr, const uint8_t *src, const uint32_t size);
    OPERATE_RET (*erase)(const uint32_t addr, const uint32_t size);
    OPERATE_RET (*lock)(const uint32_t addr, const uint32_t size);
    OPERATE_RET (*unlock)(const uint32_t addr, const uint32_t size);
    OPERATE_RET (*get_one_type_info)(TUYA_FLASH_TYPE_E type, TUYA_FLASH_BASE_INFO_T *info);
} TKL_FLASH_DESC_T;

/**
 * @brief register flash description to tuya object manage
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
TKL_FLASH_DESC_T *tkl_flash_desc_get(void);
#endif

#ifdef __cplusplus
} // extern "C"
#endif

#endif // __TKL_INIT_COMMON_H__

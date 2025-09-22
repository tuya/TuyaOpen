#ifndef __TKL_WAKE_UP_H__
#define __TKL_WAKE_UP_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "tuya_cloud_types.h"
#include "tkl_gpio.h"

/**
 * @brief wake up source set
 *
 * @param[in] param: wake up source set,
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */

OPERATE_RET tkl_wakeup_source_set(const TUYA_WAKEUP_SOURCE_BASE_CFG_T *param);
/**
 * @brief wake up source clear
 *
 * @param[in] param:  wake up source clear,
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tkl_wakeup_source_clear(const TUYA_WAKEUP_SOURCE_BASE_CFG_T *param);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // __TKL_WAKE_UP_H__

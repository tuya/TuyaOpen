/**
* @file tkl_rgb.h
* @brief Common process - rgb display process
* @version 0.1
* @date 2025-03-27
*
* @copyright Copyright 2021-2025 Tuya Inc. All Rights Reserved.
*
*/
#ifndef __TKL_RGB_H__
#define __TKL_RGB_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "tuya_cloud_types.h"

typedef enum {
    RGB_OUTPUT_FINISH = 0,
} TUYA_RGB_EVENT_E;


typedef void (*TUYA_RGB_ISR_CB)(TUYA_RGB_EVENT_E event);

/**
 * @brief rgb init
 * 
 * @param[in] cfg: rgb config
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tkl_rgb_init(TUYA_RGB_BASE_CFG_T *cfg);

/**
 * @brief rgb deinit
 * 
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tkl_rgb_deinit(void);

/**
 * @brief register rgb cb
 * 
 * @param[in] cb: callback
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tkl_rgb_irq_cb_register(TUYA_RGB_ISR_CB cb);

/**
 * @brief ppi set
 * 
 * @param[in] width: ppi : width
 * @param[in] height: ppi : height
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tkl_rgb_ppi_set(uint16_t width, uint16_t height);

/**
 * @brief pixel mode set
 * 
 * @param[in] mode: mode, such as 565 or 888
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tkl_rgb_pixel_mode_set(TUYA_DISPLAY_PIXEL_FMT_E mode);//input mode set

/**
 * @brief rgb base addr set
 * 
 * @param[in] addr : base addr
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tkl_rgb_base_addr_set(uint32_t addr);

/**
 * @brief rgb transfer start
 * 
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tkl_rgb_display_transfer_start(void);

/**
 * @brief rgb transfer stop
 * 
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tkl_rgb_display_transfer_stop(void);
#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif
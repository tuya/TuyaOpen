/**
* @file tkl_8080.h
* @brief Common process - 8080 display process
* @version 0.1
* @date 2025-03-27
*
* @copyright Copyright 2021-2025 Tuya Inc. All Rights Reserved.
*
*/
#ifndef __TKL_8080_H__
#define __TKL_8080_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "tuya_cloud_types.h"

typedef enum {
    TUYA_MCU8080_OUTPUT_FINISH = 0,
} TUYA_MCU8080_EVENT_E;


typedef void (*TUYA_MCU8080_ISR_CB)(TUYA_MCU8080_EVENT_E event);

/**
 * @brief 8080 init
 * 
 * @param[in] cfg: 8080 config
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tkl_8080_init(TUYA_8080_BASE_CFG_T *cfg);

/**
 * @brief 8080 deinit
 * 
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tkl_8080_deinit(void);

/**
 * @brief register 8080 cb
 * 
 * @param[in] cb: callback
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tkl_8080_irq_cb_register(TUYA_MCU8080_ISR_CB cb);

/**
 * @brief ppi set
 * 
 * @param[in] width: ppi : width
 * @param[in] height: ppi : height
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tkl_8080_ppi_set(uint16_t width, uint16_t height);

/**
 * @brief pixel mode set
 * 
 * @param[in] mode: mode, such as 565 or 888
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tkl_8080_pixel_mode_set(TUYA_DISPLAY_PIXEL_FMT_E mode);

/**
 * @brief 8080 base addr set
 * 
 * @param[in] addr : base addr
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tkl_8080_base_addr_set(uint32_t addr);

/**
 * @brief  8080 transfer start
 * 
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tkl_8080_transfer_start(void);

/**
 * @brief  8080 transfer stop
 * 
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tkl_8080_transfer_stop(void);

/**
 * @brief 8080 cmd send
 * 
 * @param[in] cmd : cmd
 * 
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tkl_8080_cmd_send(uint32_t cmd);

/**
 * @brief 8080 cmd send(with param)
 * 
 *@param[in] cmd : cmd
 *@param[in] param : param data buf
 *@param[in] param_cnt : param cnt
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tkl_8080_cmd_send_with_param(uint32_t cmd, uint32_t *param, uint8_t param_cnt);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif

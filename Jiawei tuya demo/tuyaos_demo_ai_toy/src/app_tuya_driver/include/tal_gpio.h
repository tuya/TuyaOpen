/**
 * @file tal_gpio.h
 * @brief Common process - adapter the gpio api
 * @version 1.0
 * @date 2021-09-10
 *
 * @copyright Copyright 2021-2031 Tuya Inc. All Rights Reserved.
 *
 */

#ifndef __TAL_GPIO_H__
#define __TAL_GPIO_H__

#include "tuya_cloud_types.h"

#ifdef __cplusplus
extern "C" {
#endif


/**
 * @brief gpio init
 * 
 * @param[in] pin_id: gpio pin id, id index starts from 0
 * @param[in] cfg:  gpio config
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tal_gpio_init(UINT32_T pin_id, TUYA_GPIO_BASE_CFG_T *cfg);

/**
 * @brief gpio deinit
 * 
 * @param[in] pin_id: gpio pin id, id index starts from 0
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tal_gpio_deinit(UINT32_T pin_id);

/**
 * @brief gpio write
 * 
 * @param[in] pin_id: gpio pin id, id index starts from 0
 * @param[in] level: gpio output level value
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tal_gpio_write(UINT32_T pin_id, TUYA_GPIO_LEVEL_E level);

/**
 * @brief gpio read
 * 
 * @param[in] pin_id: gpio pin id, id index starts from 0
 * @param[out] level: gpio output level
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tal_gpio_read(UINT32_T pin_id, TUYA_GPIO_LEVEL_E *level);

/**
 * @brief gpio irq init
 * NOTE: call this API will not enable interrupt
 * 
 * @param[in] pin_id: gpio pin id, id index starts from 0
 * @param[in] cfg:  gpio irq config
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tal_gpio_irq_init(UINT32_T pin_id, TUYA_GPIO_IRQ_T *cfg);

/**
 * @brief gpio irq enable
 * 
 * @param[in] pin_id: gpio pin id, id index starts from 0
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tal_gpio_irq_enable(UINT32_T pin_id);

/**
 * @brief gpio irq disable
 * 
 * @param[in] pin_id: gpio pin id, id index starts from 0
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tal_gpio_irq_disable(UINT32_T pin_id);


#ifdef __cplusplus
} // extern "C"
#endif

#endif // __TAL_GPIO_H__


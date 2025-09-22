/**
 * @file tkl_gpio.h
 * @brief Common process - adapter the gpio api
 * @version 0.1
 * @date 2021-08-06
 *
 * @copyright Copyright 2021-2022 Tuya Inc. All Rights Reserved.
 *
 */

#ifndef __TKL_GPIO_H__
#define __TKL_GPIO_H__

#include "tuya_cloud_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief gpio init
 *
 * @param[in] pin_id: gpio pin id, id index starts at 0
 * @param[in] cfg:  gpio config
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tkl_gpio_init(TUYA_GPIO_NUM_E pin_id, const TUYA_GPIO_BASE_CFG_T *cfg);

/**
 * @brief gpio deinit
 *
 * @param[in] pin_id: gpio pin id, id index starts at 0
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tkl_gpio_deinit(TUYA_GPIO_NUM_E pin_id);

/**
 * @brief gpio write
 *
 * @param[in] pin_id: gpio pin id, id index starts at 0
 * @param[in] level: gpio output level value
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tkl_gpio_write(TUYA_GPIO_NUM_E pin_id, TUYA_GPIO_LEVEL_E level);

/**
 * @brief gpio read
 *
 * @param[in] pin_id: gpio pin id, id index starts at 0
 * @param[out] level: gpio output level
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tkl_gpio_read(TUYA_GPIO_NUM_E pin_id, TUYA_GPIO_LEVEL_E *level);

/**
 * @brief gpio irq init
 * NOTE: call this API will not enable interrupt
 *
 * @param[in] pin_id: gpio pin id, id index starts at 0
 * @param[in] cfg:  gpio irq config
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tkl_gpio_irq_init(TUYA_GPIO_NUM_E pin_id, const TUYA_GPIO_IRQ_T *cfg);

/**
 * @brief gpio irq enable
 *
 * @param[in] pin_id: gpio pin id, id index starts at 0
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tkl_gpio_irq_enable(TUYA_GPIO_NUM_E pin_id);

/**
 * @brief gpio irq disable
 *
 * @param[in] pin_id: gpio pin id, id index starts at 0
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tkl_gpio_irq_disable(TUYA_GPIO_NUM_E pin_id);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif

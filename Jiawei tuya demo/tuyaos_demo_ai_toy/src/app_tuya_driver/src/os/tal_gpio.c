/**
 * @file tal_watchdog.c
 * @brief This is tuya tal_watchdog file
 * @version 1.0
 * @date 2021-09-10
 *
 * @copyright Copyright 2021-2031 Tuya Inc. All Rights Reserved.
 *
 */

#include "tkl_gpio.h"
#include "tkl_init_common.h"



/**
 * @brief gpio init
 * 
 * @param[in] pin_id: gpio pin id,
 *                     the high 16bit - port_num, begin from 0
 *                     the low 16bit - pin_num, begin from 0
 *                     you can input like this TUYA_GPIO_PIN_ID(0, 2)
 *                     aslo you can input like this TUYA_GPIO_PIN_ID(TUYA_GPIO_PORTA, 2)
 * @param[in] cfg:  gpio config
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tal_gpio_init(UINT32_T pin_id, TUYA_GPIO_BASE_CFG_T *cfg)
{
    return tkl_gpio_init(pin_id, cfg);
}

/**
 * @brief gpio deinit
 * 
 * @param[in] pin_id: gpio pin id,
 *                     the high 16bit - port_num, begin from 0
 *                     the low 16bit - pin_num, begin from 0
 *                     you can input like this TUYA_GPIO_PIN_ID(0, 2)
 *                     aslo you can input like this TUYA_GPIO_PIN_ID(TUYA_GPIO_PORTA, 2)
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tal_gpio_deinit(UINT32_T pin_id)
{
    return tkl_gpio_deinit(pin_id);
}

/**
 * @brief gpio write
 * 
 * @param[in] pin_id: gpio pin id,
 *                     the high 16bit - port_num, begin from 0
 *                     the low 16bit - pin_num, begin from 0
 *                     you can input like this TUYA_GPIO_PIN_ID(0, 2)
 *                     aslo you can input like this TUYA_GPIO_PIN_ID(TUYA_GPIO_PORTA, 2)
 * @param[in] level: gpio output level value
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tal_gpio_write(UINT32_T pin_id, TUYA_GPIO_LEVEL_E level)
{
    return tkl_gpio_write(pin_id, level);
}

/**
 * @brief gpio read
 * 
 * @param[in] pin_id: gpio pin id,
 *                     the high 16bit - port_num, begin from 0
 *                     the low 16bit - pin_num, begin from 0
 *                     you can input like this TUYA_GPIO_PIN_ID(0, 2)
 *                     aslo you can input like this TUYA_GPIO_PIN_ID(TUYA_GPIO_PORTA, 2)
 * @param[out] level: gpio output level
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tal_gpio_read(UINT32_T pin_id, TUYA_GPIO_LEVEL_E *level)
{
    return tkl_gpio_read(pin_id, level);
}

/**
 * @brief gpio irq init
 * NOTE: call this API will not enable interrupt
 * 
 * @param[in] pin_id: gpio pin id,
 *                     the high 16bit - port_num, begin from 0
 *                     the low 16bit - pin_num, begin from 0
 *                     you can input like this TUYA_GPIO_PIN_ID(0, 2)
 *                     aslo you can input like this TUYA_GPIO_PIN_ID(TUYA_GPIO_PORTA, 2)
 * @param[in] cfg:  gpio irq config
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tal_gpio_irq_init(UINT32_T pin_id, TUYA_GPIO_IRQ_T *cfg)
{
    return tkl_gpio_irq_init(pin_id, cfg);
}

/**
 * @brief gpio irq enable
 * 
 * @param[in] pin_id: gpio pin id,
 *                     the high 16bit - port_num, begin from 0
 *                     the low 16bit - pin_num, begin from 0
 *                     you can input like this TUYA_GPIO_PIN_ID(0, 2)
 *                     aslo you can input like this TUYA_GPIO_PIN_ID(TUYA_GPIO_PORTA, 2) 
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tal_gpio_irq_enable(UINT32_T pin_id)
{
    return tkl_gpio_irq_enable(pin_id);
}

/**
 * @brief gpio irq disable
 * 
 * @param[in] pin_id: gpio pin id,
 *                     the high 16bit - port_num, begin from 0
 *                     the low 16bit - pin_num, begin from 0
 *                     you can input like this TUYA_GPIO_PIN_ID(0, 2)
 *                     aslo you can input like this TUYA_GPIO_PIN_ID(TUYA_GPIO_PORTA, 2)
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tal_gpio_irq_disable(UINT32_T pin_id)
{
    return tkl_gpio_irq_disable(pin_id);
}



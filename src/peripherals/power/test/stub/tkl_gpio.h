/* host stub: declarations only; mocks are defined in the test */
#ifndef __STUB_TKL_GPIO_H__
#define __STUB_TKL_GPIO_H__
#include "tuya_cloud_types.h"
OPERATE_RET tkl_gpio_init(TUYA_GPIO_NUM_E pin, const TUYA_GPIO_BASE_CFG_T *cfg);
OPERATE_RET tkl_gpio_deinit(TUYA_GPIO_NUM_E pin);
OPERATE_RET tkl_gpio_write(TUYA_GPIO_NUM_E pin, TUYA_GPIO_LEVEL_E level);
OPERATE_RET tkl_gpio_read(TUYA_GPIO_NUM_E pin, TUYA_GPIO_LEVEL_E *level);
OPERATE_RET tkl_gpio_irq_init(TUYA_GPIO_NUM_E pin, const TUYA_GPIO_IRQ_T *cfg);
OPERATE_RET tkl_gpio_irq_enable(TUYA_GPIO_NUM_E pin);
OPERATE_RET tkl_gpio_irq_disable(TUYA_GPIO_NUM_E pin);
#endif

#ifndef __TDD_XL9555_IO_H__
#define __TDD_XL9555_IO_H__

#include "tuya_cloud_types.h"

#ifdef __cplusplus
extern "C" {
#endif

#define XL9555_ADDR                 0x20                            /* XL9555 device 7-bit address --> Refer to manual (9.1. Device Address) */

#define XL9555_INPUT_PORT0_REG      0                               /* Input register 0 address */
#define XL9555_INPUT_PORT1_REG      1                               /* Input register 1 address */
#define XL9555_OUTPUT_PORT0_REG     2                               /* Output register 0 address */
#define XL9555_OUTPUT_PORT1_REG     3                               /* Output register 1 address */
#define XL9555_INVERSION_PORT0_REG  4                               /* Polarity inversion register 0 address */
#define XL9555_INVERSION_PORT1_REG  5                               /* Polarity inversion register 1 address */
#define XL9555_CONFIG_PORT0_REG     6                               /* Direction configuration register 0 address */
#define XL9555_CONFIG_PORT1_REG     7                               /* Direction configuration register 1 address */

/**
 * @brief Initialize XL9555 IO expander
 * 
 * @param handle I2C master bus handle
 * @param config Configuration value for PORT0
 * 
 * @return OPERATE_RET Operation result
 */
OPERATE_RET tdd_xl9555_io_init(void *handle, uint16_t config);

/**
 * @brief Set XL9555 IO pin value
 * 
 * @param pin Pin number to set
 * @param val Value to set (0 or 1)
 * 
 * @return OPERATE_RET Operation result
 */
OPERATE_RET tdd_xl9555_io_set(uint16_t pin, int val);

/**
 * @brief Read XL9555 IO pin value
 * 
 * @param pin Pin number to read
 * 
 * @return int Pin value (0 or 1)
 */
OPERATE_RET tdd_xl9555_io_get(uint16_t pin, int *val);

#ifdef __cplusplus
}
#endif

#endif /* __XL9555_IO_H__ */
#ifndef __TKL_PINMUX_H__
#define __TKL_PINMUX_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "tuya_cloud_types.h"

#define TUYA_IO_GET_PORT_ID(data) (((data) >> 8) & 0xFF)
#define TUYA_IO_GET_CHANNEL_ID(data)    (((data) & 0xFF)

/**
 * @brief tuya pin type
 */
typedef enum {
    TUYA_IO_TYPE_PWM,
    TUYA_IO_TYPE_ADC,
    TUYA_IO_TYPE_DAC,
    TUYA_IO_TYPE_UART,
    TUYA_IO_TYPE_SPI,
    TUYA_IO_TYPE_I2C,
    TUYA_IO_TYPE_I2S,
    TUYA_IO_TYPE_GPIO,
    TUYA_IO_TYPE_MAX = 0xFFFF,
} TUYA_PIN_TYPE_E;
/**
 * @brief tuya io pinmux func
 *
 * @param[in] pin: pin number
 * @param[in] pin_func: pin function
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tkl_io_pinmux_config(TUYA_PIN_NAME_E pin, TUYA_PIN_FUNC_E pin_func);

/**
 * @brief tuya multiple io pinmux func
 * @param[in] cfg: pin cfg array
 * @param[in] num: num of pin cfg array
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tkl_multi_io_pinmux_config(TUYA_MUL_PIN_CFG_T *cfg, uint16_t num);

/**
 * @brief tuya io pin to port,
 * @brief
 * @param[in] pin: pin number
 * @param[in] pin: pin type
 * @return Pin Function : Port and Channel,err < 0.
 * @return        16        8       8
 * @return[out] | rsv   |  port | channel |
 */
int32_t tkl_io_pin_to_func(uint32_t pin, TUYA_PIN_TYPE_E pin_type);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // __TKL_PINMUX_H__

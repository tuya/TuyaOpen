/**
 * @file board_com_api.h
 * @author Tuya Inc.
 * @brief Header file for common board-level hardware registration APIs.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#ifndef __BOARD_COM_API_H__
#define __BOARD_COM_API_H__

#include "tuya_cloud_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/***********************************************************
************************macro define************************
***********************************************************/
#define BOARD_BUTTON_PWR_NAME      "ws_btn_pwr"

#ifndef EXAMPLE_SYS_PWR_PIN
#define EXAMPLE_SYS_PWR_PIN TUYA_GPIO_NUM_18
#endif

#ifndef EXAMPLE_SYS_EN_PIN
#define EXAMPLE_SYS_EN_PIN TUYA_GPIO_NUM_19
#endif

#ifndef EXAMPLE_I2C_SCL_PIN
#define EXAMPLE_I2C_SCL_PIN TUYA_GPIO_NUM_20
#endif

#ifndef EXAMPLE_I2C_SDA_PIN
#define EXAMPLE_I2C_SDA_PIN TUYA_GPIO_NUM_21
#endif

#ifndef EXAMPLE_UART_TX_PIN
#define EXAMPLE_UART_TX_PIN TUYA_GPIO_NUM_41
#endif

#ifndef EXAMPLE_UART_RX_PIN
#define EXAMPLE_UART_RX_PIN TUYA_GPIO_NUM_40
#endif

#ifndef EXAMPLE_UART_PORT
#define EXAMPLE_UART_PORT TUYA_UART_NUM_2
#endif

#ifndef EXAMPLE_UART_BAUDRATE
#define EXAMPLE_UART_BAUDRATE 115200
#endif

#define EXAMPLE_BAT_CHARGE_PIN TUYA_GPIO_NUM_30

#define EXAMPLE_BAT_ADC_PIN TUYA_GPIO_NUM_13

#ifndef EXAMPLE_GPIO_47_PIN
#define EXAMPLE_GPIO_47_PIN TUYA_GPIO_NUM_47
#endif

#ifndef EXAMPLE_GPIO_17_PIN
#define EXAMPLE_GPIO_17_PIN TUYA_GPIO_NUM_17
#endif

#ifndef EXAMPLE_GPIO_16_PIN
#define EXAMPLE_GPIO_16_PIN TUYA_GPIO_NUM_16
#endif

#ifndef EXAMPLE_GPIO_15_PIN
#define EXAMPLE_GPIO_15_PIN TUYA_GPIO_NUM_15
#endif

#ifndef EXAMPLE_GPIO_14_PIN
#define EXAMPLE_GPIO_14_PIN TUYA_GPIO_NUM_14
#endif

#define ADC_CHANNEL 15
#define ADC_Ratio_Voltage 2.51/0.51 //电池分压电阻
/***********************************************************
***********************typedef define***********************
***********************************************************/

/***********************************************************
********************function declaration********************
***********************************************************/

/**
 * @brief Registers all the hardware peripherals (audio, button, LED) on the board.
 *
 * @return Returns OPERATE_RET_OK on success, or an appropriate error code on failure.
 */
OPERATE_RET board_register_hardware(void);

#ifdef __cplusplus
}
#endif

#endif /* __BOARD_COM_API_H__ */

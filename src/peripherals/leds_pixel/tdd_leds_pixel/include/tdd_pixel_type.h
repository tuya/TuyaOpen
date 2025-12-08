/**
 * @file tdd_pixel_type.h
 * @brief TDD layer common types and definitions for LED pixel drivers
 *
 * This header file defines common types, enumerations, and structures used across
 * all TDD layer LED pixel drivers. It includes configuration structures for
 * different LED controller types, color ordering modes, and PWM configuration
 * parameters that are shared by various pixel driver implementations.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 *
 */

#ifndef __TDD_PIXEL_TYPE_H__
#define __TDD_PIXEL_TYPE_H__

#include "tkl_spi.h"

#ifdef __cplusplus
extern "C" {
#endif

/***********************************************************
************************macro define************************
***********************************************************/
#define PIXEL_PWM_NUM_MAX                  2u
#define PIXEL_PWM_ID_INVALID               0xFE


#define PIXEL_PWM_CH_IDX_COLD              0  //cct: bright
#define PIXEL_PWM_CH_IDX_WARM              1  //cct: temper

#define PIXLE_PWM_DRV_TP_CW     0x00        // CW : cw & ww
#define PIXLE_PWM_DRV_TP_CCT    0x01        // CCT: not support
#define PIXLE_PWM_DRV_TP_CW_NC  0x02        // CW : cw & ww, non-complementary
#define PIXLE_PWM_DRV_TP_UNUSED 0x03        // invalid
/***********************************************************
***********************typedef define***********************
***********************************************************/
typedef unsigned char RGB_ORDER_MODE_E;
#define RGB_ORDER 0x00
#define RBG_ORDER 0x01
#define GRB_ORDER 0x02
#define GBR_ORDER 0x03
#define BRG_ORDER 0x04
#define BGR_ORDER 0x05

typedef struct {
    TUYA_SPI_NUM_E   port;
    RGB_ORDER_MODE_E line_seq;
} PIXEL_DRIVER_CONFIG_T;

typedef struct {
    uint32_t pwm_freq;                            // pwm frequency (Hz)
    BOOL_T active_level;                          // true means active high, false means active low
    uint8_t pwm_tp;         					  //PIXLE_PWM_DRV_TP_CW or PIXLE_PWM_DRV_TP_CW_NC
    uint8_t pwm_pin_arr[PIXEL_PWM_NUM_MAX];       // pwm pin of each channel
    TUYA_PWM_NUM_E pwm_ch_arr[PIXEL_PWM_NUM_MAX]; // pwm id of each channel
} PIXEL_PWM_CFG_T;
/***********************************************************
********************function declaration********************
***********************************************************/

#ifdef __cplusplus
}
#endif

#endif /* __TDD_PIXEL_TYPE_H__ */

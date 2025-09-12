/**
 * @file tdu_light_types.h
 * @author www.tuya.com
 * @brief tdl_light_types module is used to provide common
 *        types of lighting products
 * @version 0.1
 * @date 2022-08-22
 *
 * @copyright Copyright (c) tuya.inc 2022
 *
 */

#ifndef __TDU_LIGHT_TYPES_H__
#define __TDU_LIGHT_TYPES_H__

#include "tuya_cloud_types.h"
#include "tal_log.h"

#ifdef __cplusplus
extern "C" {
#endif

/***********************************************************
************************macro define************************
***********************************************************/
#define LIG_THREE_MAX(x,y,z)                      (x)>(y)?((x)>(z)?(x):(z)):((y)>(z)?(y):(z))

#define LIG_PERCENT_TO_INT(percent, base)         (((FLOAT_T)(percent)/100.0)*(base))  //Convert percentage to specific value
#define LIG_RATE_TO_TIME(rate, min_time)          ((min_time)*100/(rate))     //Convert speed ratio to specific time

#define LIGHT_COLOR_CHANNEL_MAX                   5u

/**
 * @brief light product type
 */
#define LIGHT_PRODUCT_DIMMER                     0 //Single pixel full color
#define LIGHT_PRODUCT_PIXELS                     1 //Multi-pixel fantasy color


/**
 * @brief color channel idx
 */
#define COLOR_CH_IDX_RED                          0
#define COLOR_CH_IDX_GREEN                        1
#define COLOR_CH_IDX_BLUE                         2
#define COLOR_CH_IDX_COLD                         3
#define COLOR_CH_IDX_WARM                         4

/**
 * @brief light device type - bit code
 */
#define LIGHT_R_BIT             0x01
#define LIGHT_G_BIT             0x02
#define LIGHT_B_BIT             0x04
#define LIGHT_C_BIT             0x08
#define LIGHT_W_BIT             0x10

#define LIGHT_CW_BIT           (LIGHT_C_BIT | LIGHT_W_BIT)
#define LIGHT_RGB_BIT          (LIGHT_R_BIT | LIGHT_G_BIT | LIGHT_B_BIT)
#define LIGHT_RGBCW_BIT        (LIGHT_R_BIT | LIGHT_G_BIT | LIGHT_B_BIT | LIGHT_C_BIT | LIGHT_W_BIT)

/**
 * @brief light device name length
 */
#define LIGHT_NAME_MAX_LEN      16u

/**
 * @brief light color the value hue
 */
#define LIGHT_COLOR_HUE_MAX                360
#define LIGHT_COLOR_HUE_MIN                0
#define LIGHT_COLOR_HUE_RED                0
#define LIGHT_COLOR_HUE_ORANGE             30
#define LIGHT_COLOR_HUE_YELLOW             60
#define LIGHT_COLOR_HUE_YELLOW_GREEN       90
#define LIGHT_COLOR_HUE_GREEN              120
#define LIGHT_COLOR_HUE_CYAN_GREEN         150
#define LIGHT_COLOR_HUE_CYAN               180
#define LIGHT_COLOR_HUE_CYAN_BLUE          210
#define LIGHT_COLOR_HUE_BLUE               240
#define LIGHT_COLOR_HUE_MAGENTA            270
#define LIGHT_COLOR_HUE_PURPLE             300
#define LIGHT_COLOR_HUE_PURPLE_RED         330
#define LIGHT_COLOR_HUE_PURPLE_WHITE       340
#define LIGHT_COLOR_HUE_GREEN_WHITE        146
#define LIGHT_COLOR_HUE_WHITE              0

/**
 * @brief parameter checking
 */
#define TY_APP_PARAM_CHECK(condition)                                                                                  \
    do {                                                                                                               \
        if (!(condition)) {                                                                                            \
            PR_ERR("Pre check(" #condition "):in line %d", __LINE__);                                              \
            return OPRT_INVALID_PARM;                                                                                  \
        }                                                                                                              \
    } while (0)

/**
 * @brief parameter checking without return value
 */
#define TY_APP_PARAM_CHECK_NO_RET(condition)                                                                           \
    do {                                                                                                               \
        if (!(condition)) {                                                                                            \
            PR_ERR("Pre check(" #condition "):in line %d", __LINE__);                                              \
            return ;                                                                                                   \
        }                                                                                                              \
    } while (0)


#define LIGHT_DVE_TYPE(r,g,b,c,w) (r | (g<<1) | (b<<2) | (c<<3) | (w<<4))

/***********************************************************
***********************typedef define***********************
***********************************************************/
/**
 * @brief light device type
 */
typedef uint8_t LIGHT_DEV_TP_E;
#define LIGHT_DEV_TP_C          (LIGHT_C_BIT)
#define LIGHT_DEV_TP_CW         (LIGHT_C_BIT | LIGHT_W_BIT)
#define LIGHT_DEV_TP_RGB        (LIGHT_R_BIT | LIGHT_G_BIT | LIGHT_B_BIT)
#define LIGHT_DEV_TP_RGBC       (LIGHT_R_BIT | LIGHT_G_BIT | LIGHT_B_BIT | LIGHT_C_BIT)
#define LIGHT_DEV_TP_RGBW       (LIGHT_R_BIT | LIGHT_G_BIT | LIGHT_B_BIT | LIGHT_W_BIT)
#define LIGHT_DEV_TP_RGBCW      (LIGHT_R_BIT | LIGHT_G_BIT | LIGHT_B_BIT | LIGHT_C_BIT | LIGHT_W_BIT)

typedef uint8_t LIGHT_DRV_COLOR_CH_E;
#define LIGHT_DRV_COLOR_CH_C          (LIGHT_C_BIT)
#define LIGHT_DRV_COLOR_CH_CW         (LIGHT_C_BIT | LIGHT_W_BIT)
#define LIGHT_DRV_COLOR_CH_RGB        (LIGHT_R_BIT | LIGHT_G_BIT | LIGHT_B_BIT)
#define LIGHT_DRV_COLOR_CH_RGBC       (LIGHT_R_BIT | LIGHT_G_BIT | LIGHT_B_BIT | LIGHT_C_BIT)
#define LIGHT_DRV_COLOR_CH_RGBW       (LIGHT_R_BIT | LIGHT_G_BIT | LIGHT_B_BIT | LIGHT_W_BIT)
#define LIGHT_DRV_COLOR_CH_RGBCW      (LIGHT_R_BIT | LIGHT_G_BIT | LIGHT_B_BIT | LIGHT_C_BIT | LIGHT_W_BIT)
/**
 * @brief light mode
 */
typedef uint8_t LIGHT_MODE_E;
#define LIGHT_MODE_WHITE           0x00
#define LIGHT_MODE_COLOR           0x01
#define LIGHT_MODE_SCENE           0x02
#define LIGHT_MODE_MUSIC           0x03
#define LIGHT_MODE_NUM             0x04
#define LIGHT_MODE_INVALID         LIGHT_MODE_NUM

typedef uint8_t LIGHT_PAINT_MODE_E;
#define LIGHT_PAINT_MODE_WHITE          0x00    
#define LIGHT_PAINT_MODE_COLOR          0x01
#define LIGHT_PAINT_MODE_WHITE_COLOR    0x03
#define LIGHT_PAINT_MODE_INVALID        0x04

typedef uint8_t LIGHT_COLOR_TYPE_E;
#define LIGHT_COLOR_RGBCW          0x00
#define LIGHT_COLOR_HSVBT          0x01
#define LIGHT_COLOR_AXIS           0x02

typedef uint32_t LIGHT_COLOR_E;
#define LIGHT_COLOR_C              0x00
#define LIGHT_COLOR_CW             0x01
#define LIGHT_COLOR_R              0x02
#define LIGHT_COLOR_G              0x03
#define LIGHT_COLOR_B              0x04
#define LIGHT_COLOR_RGB            0x05
#define LIGHT_COLOR_NUM_MAX        0x06

typedef uint8_t LIGHT_CTRL_TYPE_E;
#define LIGHT_CTRL_TYPE_RGBCW      0x00
#define LIGHT_CTRL_TYPE_CW         0x01
#define LIGHT_CTRL_TYPE_RGB        0x02


typedef uint8_t LIGHT_MIX_TYPE_E;
#define LIGHT_MIX_TP_NONE          0x00    // no light mixing
#define LIGHT_MIX_TP_3MIX4         0x01    // use 3 channels to mix out the effect of 1 channel of white light
#define LIGHT_MIX_TP_3MIX5         0x02    // use 3 channels to mix out the effect of 2 channel of white light
#define LIGHT_MIX_TP_4MIX5         0x03    // use 4 channels to mix out the effect of 2 channel of white light


/**
 * @brief light network channel priority
 */
typedef uint8_t LIGHT_NET_CHANNEL_PRIORITY_E;
#define LAN_MQTT_BLE 0x00       // LAN > MQTT > BLE
#define BLE_LAN_MQTT 0x01       // BLE > LAN > MQTT
#define MQTT_LAN_BLE 0x02       // MQTT > LAN > BLE
#define LAN_BLE_MQTT 0x03       // LAN > BLE > MQTT
#define BLE_MQTT_LAN 0x04       // BLE > MQTT > LAN
#define MQTT_BLE_LAN 0x05       // MQTT > BLE > LAN

/**
 * @brief xy data
 */
typedef struct {
    uint16_t  x;
    uint16_t  y;
} COLOR_XY_T, LIGHT_XY_T;

/**
 * @brief hs data
 */
typedef struct {
    uint8_t  hue;
    uint8_t  sat;
} COLOR_CHAR_HS_T, LIGHT_CHAR_HS_T;

/**
 * @brief hsv data
 */
typedef struct {
    uint16_t hue;
    uint16_t sat;
    uint16_t val;
} COLOR_HSV_T, LIGHT_HSV_T;

/**
 * @brief rgb data
 */
typedef struct {
    uint16_t red;
    uint16_t green;
    uint16_t blue;
} COLOR_RGB_T;

/**
 * @brief bt data
 */
typedef struct {
    uint16_t bright;
    uint16_t temper;
} WHITE_BT_T;

/**
 * @brief cw data
 */
typedef struct {
    uint16_t cold;
    uint16_t warm;
} WHITE_CW_T;

/**
 * @brief hsvbt data
 */
typedef struct {
    uint16_t hue;
    uint16_t sat;
    uint16_t val;
    uint16_t bright;
    uint16_t temper;
} LIGHT_HSVBT_T;

typedef struct {
    LIGHT_PAINT_MODE_E        mode;
    COLOR_HSV_T               hsv;
    WHITE_BT_T                bt;
}LIGHT_PAINT_T;

/**
 * @brief rgbcw data
 */
#pragma pack(2)
typedef struct {
    uint16_t red;
    uint16_t green;
    uint16_t blue;
    uint16_t cold;
    uint16_t warm;
}LIGHT_RGBCW_T, PIXEL_COLOR_T;

#pragma pack()
typedef union {
    LIGHT_RGBCW_T s;
    uint16_t      array[LIGHT_COLOR_CHANNEL_MAX];
}LIGHT_RGBCW_U;

typedef struct {
    BOOL_T   enable;
    uint8_t  pin;
    BOOL_T   active_lv;
}LIGHT_SWITCH_PIN_CFG_T;

/***********************************************************
********************function declaration********************
***********************************************************/



#ifdef __cplusplus
}
#endif

#endif /* __TDL_LIGHT_TYPES_H__ */

/**
 * @file tdl_pixel_driver.h
 * @brief TDL layer driver interface for LED pixel devices
 *
 * This header file provides the TDL (Tuya Device Layer) driver interface for LED pixel
 * devices. It defines the driver registration framework, device attributes, and
 * standard operation interfaces that pixel drivers must implement. The module
 * provides abstraction layer between high-level pixel management and low-level
 * hardware drivers for different LED controller types.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 *
 */

#ifndef __TDL_PIXEL_DRIVER_H__
#define __TDL_PIXEL_DRIVER_H__

#ifdef __cplusplus
extern "C" {
#endif

/***********************************************************
*************************micro define***********************
***********************************************************/
#define COLOR_R_BIT 0x01
#define COLOR_G_BIT 0x02
#define COLOR_B_BIT 0x04
#define COLOR_C_BIT 0x08
#define COLOR_W_BIT 0x10

/***********************************************************
***********************typedef define***********************
***********************************************************/
typedef unsigned char PIXEL_DRV_CMD_E;
#define DRV_CMD_GET_PWM_HARDWARE_CFG 0x01
#define DRV_CMD_SET_RGB_ORDER_CFG    0x02

typedef unsigned char PIXEL_COLOR_TP_E;
#define PIXEL_COLOR_TP_RGB   (COLOR_R_BIT | COLOR_G_BIT | COLOR_B_BIT)
#define PIXEL_COLOR_TP_RGBC  (COLOR_R_BIT | COLOR_G_BIT | COLOR_B_BIT | COLOR_C_BIT)
#define PIXEL_COLOR_TP_RGBW  (COLOR_R_BIT | COLOR_G_BIT | COLOR_B_BIT | COLOR_W_BIT)
#define PIXEL_COLOR_TP_RGBCW (COLOR_R_BIT | COLOR_G_BIT | COLOR_B_BIT | COLOR_C_BIT | COLOR_W_BIT)

typedef void *DRIVER_HANDLE_T;
typedef struct {
    int (*open)(DRIVER_HANDLE_T *handle, unsigned short pixel_num);
    int (*close)(DRIVER_HANDLE_T *handle);
    int (*output)(DRIVER_HANDLE_T handle, unsigned short *data_buf, unsigned int buf_len);
    int (*config)(DRIVER_HANDLE_T handle, unsigned char cmd, void *arg);
} PIXEL_DRIVER_INTFS_T;

typedef struct {
    PIXEL_COLOR_TP_E color_tp;
    unsigned int color_maximum;
    BOOL_T white_color_control; // Independent White Light and Color Light Control
} PIXEL_ATTR_T;

/***********************************************************
***********************function define**********************
***********************************************************/
/**
 * @brief        Register driver
 *
 * @param[in]    driver_name               Device name
 * @param[in]    intfs                     Operation interface
 * @param[in]    arrt                      Device attributes
 * @param[in]    param                     Parameters
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
int tdl_pixel_driver_register(char *driver_name, PIXEL_DRIVER_INTFS_T *intfs, PIXEL_ATTR_T *arrt, void *param);

#ifdef __cplusplus
}
#endif /* __cplusplus */
#endif /*__TDL_PIXEL_DRIVER_H__*/
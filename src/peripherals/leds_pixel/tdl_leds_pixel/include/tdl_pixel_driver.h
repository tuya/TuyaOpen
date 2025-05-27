/**
* @file tdl_pixel_driver.h
* @author www.tuya.com
* @brief tdl_pixel_driver module is used to 
* @version 0.1
* @date 2022-03-21
*
* @copyright Copyright (c) tuya.inc 2022
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
#define COLOR_R_BIT                   0x01
#define COLOR_G_BIT                   0x02
#define COLOR_B_BIT                   0x04
#define COLOR_C_BIT                   0x08
#define COLOR_W_BIT                   0x10

/***********************************************************
***********************typedef define***********************
***********************************************************/
typedef unsigned char PIXEL_DRV_CMD_E;
#define DRV_CMD_GET_PWM_HARDWARE_CFG                    0x01
#define DRV_CMD_SET_RGB_ORDER_CFG                       0x02

typedef unsigned char PIXEL_COLOR_TP_E;
#define PIXEL_COLOR_TP_RGB             (COLOR_R_BIT|COLOR_G_BIT|COLOR_B_BIT)
#define PIXEL_COLOR_TP_RGBC            (COLOR_R_BIT|COLOR_G_BIT|COLOR_B_BIT|COLOR_C_BIT)
#define PIXEL_COLOR_TP_RGBW            (COLOR_R_BIT|COLOR_G_BIT|COLOR_B_BIT|COLOR_W_BIT)
#define PIXEL_COLOR_TP_RGBCW           (COLOR_R_BIT|COLOR_G_BIT|COLOR_B_BIT|COLOR_C_BIT|COLOR_W_BIT)

typedef void* DRIVER_HANDLE_T;
typedef struct {
    int (*open)(DRIVER_HANDLE_T *handle, unsigned short pixel_num);
    int (*close)(DRIVER_HANDLE_T *handle);
    int (*output)(DRIVER_HANDLE_T handle,  unsigned short *data_buf, unsigned int buf_len);
    int (*config)(DRIVER_HANDLE_T handle, unsigned char cmd, void *arg);
}PIXEL_DRIVER_INTFS_T;

typedef struct {
    PIXEL_COLOR_TP_E color_tp;
    unsigned int     color_maximum;
    BOOL_T           white_color_control;    // Independent White Light and Color Light Control
}PIXEL_ATTR_T;
 
/***********************************************************
***********************function define**********************
***********************************************************/
/**
* @brief        注册驱动
*
* @param[in]    driver_name               设备名字
* @param[in]    intfs                     操作接口
* @param[in]    arrt                      设备属性
* @param[in]    param                     参数
*
* @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
*/
int tdl_pixel_driver_register(char *driver_name, PIXEL_DRIVER_INTFS_T *intfs, \
                              PIXEL_ATTR_T *arrt, void *param); 
 
#ifdef __cplusplus
}
#endif /* __cplusplus */
#endif /*__TDL_PIXEL_DRIVER_H__*/
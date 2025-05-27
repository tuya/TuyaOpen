/**
* @file tdl_pixel_set_color.h
* @author www.tuya.com
* @brief tdl_light_data module is used to set leds pixel color
* @version 0.1
* @date 2022-03-01
*
* @copyright Copyright (c) tuya.inc 2022
*
*/

#ifndef __TDL_PIXEL_COLOR_MANAGE_H__
#define __TDL_PIXEL_COLOR_MANAGE_H__

#include "tdl_pixel_dev_manage.h"

#ifdef __cplusplus
extern "C" {
#endif


/*********************************************************************
******************************macro define****************************
*********************************************************************/

/*********************************************************************
****************************typedef define****************************
*********************************************************************/
typedef unsigned int PIXEL_SHIFT_DIR_T;
#define PIXEL_SHIFT_RIGHT             0   //index min->max
#define PIXEL_SHIFT_LEFT              1   //index max->min

typedef unsigned int PIXEL_M_SHIFT_DIR_T;
#define PIXEL_SHIFT_CLOSE            0   //相向
#define PIXEL_SHIFT_FAR              1   //相背

/*********************************************************************
****************************function define***************************
*********************************************************************/
/**
* @brief    设置像素段颜色（单一）
*
* @param[in]    handle           设备句柄
* @param[in]    index_start      像素点起始
* @param[in]    pixel_num        像素段长度
* @param[in]    color            目标颜色
*
* @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
*/
int tdl_pixel_set_single_color(PIXEL_HANDLE_T handle, UINT_T index_start, UINT_T pixel_num, PIXEL_COLOR_T *color);

/**
* @brief        设置像素段颜色（多种）
*
* @param[in]    handle           设备句柄
* @param[in]    index_start      像素点起始
* @param[in]    pixel_num        像素段长度
* @param[in]    color_arr        目标颜色组
*
* @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
*/
int tdl_pixel_set_multi_color(PIXEL_HANDLE_T handle, UINT_T index_start, UINT_T pixel_num, PIXEL_COLOR_T *color_arr);

/**
* @brief       在背景色上设置像素段颜色
*
* @param[in]     handle           设备句柄
* @param[in]    index_start      像素点起始
* @param[in]    pixel_num        像素段长度
* @param[in]    backcolor        背景颜色
* @param[in]    color            目标颜色
*
* @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
*/
int tdl_pixel_set_single_color_with_backcolor(PIXEL_HANDLE_T handle, UINT_T index_start, UINT_T pixel_num, \
                                               PIXEL_COLOR_T *backcolor, PIXEL_COLOR_T *color);

/**
* @brief       循环平移像素颜色
*
* @param[in]    handle           设备句柄
* @param[in]    dir              移动方向 
* @param[in]    index_start      起始下标 
* @param[in]    end_start        结束下标
* @param[in]    move_step        移动步进
*
* @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
*/
int tdl_pixel_cycle_shift_color(PIXEL_HANDLE_T handle, PIXEL_SHIFT_DIR_T dir, UINT_T index_start, \
                               UINT_T index_end, UINT_T move_step);

/**
* @brief        镜像循环移动像素颜色
*
* @param[in]    handle           设备句柄
* @param[in]    dir              移动方向
* @param[in]    index_start      起始下标 
* @param[in]    end_start        结束下标
* @param[in]    move_step        移动步进
*
* @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
*/
int tdl_pixel_mirror_cycle_shift_color(PIXEL_HANDLE_T handle, PIXEL_M_SHIFT_DIR_T dir, UINT_T index_start, \
                                      UINT_T index_end, UINT_T move_step);

/**
* @brief        获得像素颜色
*
* @param[in]    handle           设备句柄
* @param[in]    index            下标
* @param[out]   color            颜色
*
* @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
*/
int tdl_pixel_get_color(PIXEL_HANDLE_T handle, UINT_T index,  PIXEL_COLOR_T *color);

/**
* @brief        复制像素颜色
*
* @param[in]    handle           设备句柄
* @param[in]    dst_idx          目标下标
* @param[in]    src_idx          源下标
* @param[in]    len              复制像素个数
*
* @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
*/
int tdl_pixel_copy_color(PIXEL_HANDLE_T handle, UINT_T dst_idx, UINT_T src_idx, UINT_T len);


/**
* @brief    设置所有像素点颜色（单一）
*
* @param[in]    handle           设备句柄
* @param[in]    color            目标颜色
*
* @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
*/
int tdl_pixel_set_single_color_all(PIXEL_HANDLE_T handle,  PIXEL_COLOR_T *color);

/**
* @brief    只设置白光颜色，不设置彩光
*
* @param[in]    handle           设备句柄
* @param[in]    color            目标颜色
*
* @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
*/
int tdl_pixel_set_single_white_all(PIXEL_HANDLE_T handle,  PIXEL_COLOR_T *color);

#ifdef __cplusplus
}
#endif /* __cplusplus */
#endif /*__TDL_PIXEL_COLOR_MANAGE_H__*/

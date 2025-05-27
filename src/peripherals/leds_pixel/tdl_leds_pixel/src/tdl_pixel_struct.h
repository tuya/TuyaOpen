/**
* @file tdl_pixel_struct.h
* @author www.tuya.com
* @brief tdl_pixel_struct module is used to 
* @version 0.1
* @date 2022-03-22
*
* @copyright Copyright (c) tuya.inc 2022
*
*/
 
#ifndef __TDL_PIXEL_STRUCT_H__
#define __TDL_PIXEL_STRUCT_H__
 
 
#ifdef __cplusplus
extern "C" {
#endif

#include <string.h>
#include "tal_semaphore.h"
#include "tal_mutex.h"
#include "tal_system.h"

#include "tdl_pixel_driver.h"
#include "tdl_pixel_dev_manage.h"

/***********************************************************
***********************typedef define***********************
***********************************************************/
typedef struct {
    UCHAR_T                is_start:        1;
}PIXEL_FLAG_T;

typedef struct pixel_dev_list {
    struct pixel_dev_list        *next;

    CHAR_T                        name[PIXEL_DEV_NAME_MAX_LEN+1];
    MUTEX_HANDLE                  mutex;

    PIXEL_FLAG_T                  flag;

	UINT_T                        pixel_num;  
    USHORT_T                      pixel_resolution;
    USHORT_T                     *pixel_buffer;                //像素缓存
    UINT_T                        pixel_buffer_len;            //像素缓存大小

    SEM_HANDLE                    send_sem;

    UCHAR_T                       color_num;                  //三路/四路/五路
    PIXEL_COLOR_TP_E              pixel_color;
    UINT_T                        color_maximum;
    DRIVER_HANDLE_T               drv_handle;
    BOOL_T                        white_color_control;         // Independent White Light and Color Light Control
    PIXEL_DRIVER_INTFS_T         *intfs;
    
}PIXEL_DEV_NODE_T, PIXEL_DEV_LIST_T; 


#ifdef __cplusplus
}
#endif /* __cplusplus */
#endif /*__TDL_PIXEL_STRUCT_H__*/
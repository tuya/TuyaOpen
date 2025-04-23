/**
* @file tdl_pixel_color_manage.c
* @author www.tuya.com
* @brief tdl_pixel_color_manage module is used to manage pixel color buff
* @version 0.1
* @date 2022-03-22
*
* @copyright Copyright (c) tuya.inc 2022
*
*/
#include <string.h>

#include "tal_log.h"
#include "tal_memory.h"
#include "tdl_pixel_color_manage.h"

/***********************************************************
*************************private include********************
***********************************************************/
#include "tdl_pixel_driver.h"
#include "tdl_pixel_struct.h"

/***********************************************************
*************************micro define***********************
***********************************************************/
 

/***********************************************************
***********************function define**********************
***********************************************************/
STATIC VOID_T __tdl_pixel_only_set_cw(PIXEL_HANDLE_T handle, USHORT_T *buff, PIXEL_COLOR_TP_E tp, UCHAR_T color_num, UINT_T index, PIXEL_COLOR_T *color)
{
    UINT_T pos = 0;
    PIXEL_DEV_NODE_T *device = (PIXEL_DEV_NODE_T *)handle;

    if(NULL == buff) {
        TAL_PR_ERR("buff is null");
        return;
    }

    if (((tp & COLOR_C_BIT) == 0) && ((tp & COLOR_W_BIT) == 0)) {
        return;
    }
    
    pos = color_num*index + 3;

	switch(tp) {
		case PIXEL_COLOR_TP_RGBC:
        	buff[pos++] = color->cold * device->color_maximum / device->pixel_resolution; 
			break;
		case PIXEL_COLOR_TP_RGBW:
        	buff[pos++] = color->warm * device->color_maximum / device->pixel_resolution;
			break;            
		case PIXEL_COLOR_TP_RGBCW:
        	buff[pos++] = color->cold * device->color_maximum / device->pixel_resolution;
			buff[pos++] = color->warm * device->color_maximum / device->pixel_resolution; 
			break;
		default:
			break;
	}


    return;
}


STATIC VOID_T __tdl_pixel_set_color(PIXEL_HANDLE_T handle, USHORT_T *buff, PIXEL_COLOR_TP_E tp, UCHAR_T color_num, UINT_T index, PIXEL_COLOR_T *color)
{
    UINT_T pos = 0;
    PIXEL_DEV_NODE_T *device = (PIXEL_DEV_NODE_T *)handle;

    if(NULL == buff) {
        TAL_PR_ERR("buff is null");
        return;
    }
    
    pos = color_num*index;
    
	switch(tp) {
		case PIXEL_COLOR_TP_RGB:
			buff[pos++] = color->red * device->color_maximum / device->pixel_resolution;
			buff[pos++] = color->green * device->color_maximum / device->pixel_resolution;
			buff[pos++] = color->blue * device->color_maximum / device->pixel_resolution;
			break;
		case PIXEL_COLOR_TP_RGBC:
			buff[pos++] = color->red * device->color_maximum / device->pixel_resolution;
			buff[pos++] = color->green * device->color_maximum / device->pixel_resolution;
			buff[pos++] = color->blue * device->color_maximum / device->pixel_resolution;
            if (!device->white_color_control) {
               buff[pos++] = color->cold * device->color_maximum / device->pixel_resolution; 
            }
			break;
		case PIXEL_COLOR_TP_RGBW:
			buff[pos++] = color->red * device->color_maximum / device->pixel_resolution;
			buff[pos++] = color->green * device->color_maximum / device->pixel_resolution;
			buff[pos++] = color->blue * device->color_maximum / device->pixel_resolution;
            if (!device->white_color_control) {
               buff[pos++] = color->warm * device->color_maximum / device->pixel_resolution; 
            }
			break;            
		case PIXEL_COLOR_TP_RGBCW:
		    buff[pos++] = color->red * device->color_maximum / device->pixel_resolution;
			buff[pos++] = color->green * device->color_maximum / device->pixel_resolution;
			buff[pos++] = color->blue * device->color_maximum / device->pixel_resolution;
            if (!device->white_color_control) {
               	buff[pos++] = color->cold * device->color_maximum / device->pixel_resolution;
			    buff[pos++] = color->warm * device->color_maximum / device->pixel_resolution; 
            }
			break;
		default:
			break;
	}

    return;
}

STATIC VOID_T __tdl_pixel_get_color(PIXEL_HANDLE_T handle, USHORT_T *buff, PIXEL_COLOR_TP_E tp, UCHAR_T color_num, UINT_T index, PIXEL_COLOR_T *color)
{
    UINT_T pos = 0;
    PIXEL_DEV_NODE_T *device = (PIXEL_DEV_NODE_T *)handle;

    if(NULL == buff) {
        TAL_PR_ERR("buff is null");
        return;
    }

    pos = color_num*index;
    
	switch(tp) {
		case PIXEL_COLOR_TP_RGB:
            color->red   = buff[pos++] * device->pixel_resolution / device->color_maximum;
            color->green = buff[pos++] * device->pixel_resolution / device->color_maximum;
            color->blue  = buff[pos++] * device->pixel_resolution / device->color_maximum;
			break;
		case PIXEL_COLOR_TP_RGBC:
            color->red   = buff[pos++] * device->pixel_resolution / device->color_maximum;
            color->green = buff[pos++] * device->pixel_resolution / device->color_maximum;
            color->blue  = buff[pos++] * device->pixel_resolution / device->color_maximum;
            color->cold  = buff[pos++] * device->pixel_resolution / device->color_maximum;
			break;
		case PIXEL_COLOR_TP_RGBW:
            color->red   = buff[pos++] * device->pixel_resolution / device->color_maximum;
            color->green = buff[pos++] * device->pixel_resolution / device->color_maximum;
            color->blue  = buff[pos++] * device->pixel_resolution / device->color_maximum;
            color->warm  = buff[pos++] * device->pixel_resolution / device->color_maximum;
			break;            
		case PIXEL_COLOR_TP_RGBCW:
            color->red   = buff[pos++] * device->pixel_resolution / device->color_maximum;
            color->green = buff[pos++] * device->pixel_resolution / device->color_maximum;
            color->blue  = buff[pos++] * device->pixel_resolution / device->color_maximum;
            color->cold  = buff[pos++] * device->pixel_resolution / device->color_maximum;
            color->warm  = buff[pos++] * device->pixel_resolution / device->color_maximum;
			break;
		default:
			break;
	}

    return;
}

STATIC OPERATE_RET __tdl_pixel_right_shift(USHORT_T *buff, UCHAR_T color_num, INT_T start, \
                                           INT_T end, INT_T step)
{
    INT_T i, temp_len = 0, rang_size=0;   
    USHORT_T *temp = NULL;
    
    if(NULL == buff || end < start || step > end-start) {
        return OPRT_INVALID_PARM;
    }

    if(end == start) {
        return OPRT_OK;
    }

    temp_len = step*color_num*sizeof(USHORT_T);
    temp = (USHORT_T *)tal_malloc(temp_len);
    if(temp == NULL) {
        TAL_PR_ERR("malloc failed !");
        
        return OPRT_MALLOC_FAILED;
    }
    memcpy(temp, buff+color_num*(end-step+1), temp_len);

    rang_size = end-start+1;
    for(i=0; i<rang_size-step; i++){
        memmove(buff+color_num*(end-i), buff+color_num*(end-step-i), color_num*sizeof(USHORT_T));
    }
    memcpy(buff+color_num*start, temp, temp_len);

    tal_free(temp);

    return OPRT_OK;
}

STATIC OPERATE_RET __tdl_pixel_left_shift(USHORT_T *buff, UCHAR_T color_num, INT_T start, \
                                          INT_T end, INT_T step)
{
    INT_T i, temp_len = 0, rang_size=0;   
    USHORT_T *temp = NULL;

    if(NULL == buff || end < start || step > end-start) {
        return OPRT_INVALID_PARM;
    }

    if(end == start) {
        return OPRT_OK;
    }

    temp_len = step*color_num*sizeof(USHORT_T);
    temp = (USHORT_T *)tal_malloc(temp_len);
    if(temp == NULL) {
        TAL_PR_ERR("malloc failed !");
        return OPRT_MALLOC_FAILED;
    }    
    memcpy(temp, buff+color_num*start, temp_len);

    rang_size = end-start+1;
	for(i=0;i<rang_size-step;i++){
		 memmove(buff+color_num *(start+i), buff+color_num*(start+i+step), color_num*sizeof(USHORT_T));
	}
    memcpy(buff+color_num*(end-step+1), temp, temp_len);

    tal_free(temp);

    return OPRT_OK;
}

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
int tdl_pixel_set_single_color(PIXEL_HANDLE_T handle, UINT_T index_start, UINT_T pixel_num, PIXEL_COLOR_T *color)
{
    INT_T i = 0;
    PIXEL_DEV_NODE_T *device = (PIXEL_DEV_NODE_T *)handle;

    if(NULL == handle || NULL == color) {
        return OPRT_INVALID_PARM;
    }

    if(0 == device->flag.is_start) {
        return OPRT_COM_ERROR;
    }

    if(index_start >= device->pixel_num || index_start+pixel_num > device->pixel_num) {
        return OPRT_INVALID_PARM;
    }

    tal_mutex_lock(device->mutex);
    for(i=0; i<pixel_num; i++) {
        __tdl_pixel_set_color(handle, device->pixel_buffer, device->pixel_color, device->color_num, index_start+i, color);
    }
    tal_mutex_unlock(device->mutex);

    return OPRT_OK;
}

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
int tdl_pixel_set_multi_color(PIXEL_HANDLE_T handle, UINT_T index_start, UINT_T pixel_num, PIXEL_COLOR_T *color_arr)
{
    INT_T i = 0;
    PIXEL_DEV_NODE_T *device = (PIXEL_DEV_NODE_T *)handle;

    if(NULL == handle || NULL == color_arr) {
        return OPRT_INVALID_PARM;
    }

    if(0 == device->flag.is_start) {
        return OPRT_COM_ERROR;
    }

    if(index_start >= device->pixel_num || index_start+pixel_num > device->pixel_num) {
        return OPRT_INVALID_PARM;
    }

    tal_mutex_lock(device->mutex);
    for(i=0; i<pixel_num; i++) {
        __tdl_pixel_set_color(handle, device->pixel_buffer, device->pixel_color,  device->color_num, index_start+i, &color_arr[i]);
    }
    tal_mutex_unlock(device->mutex);

    return OPRT_OK;  
}

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
                                               PIXEL_COLOR_T *backcolor, PIXEL_COLOR_T *color)
{
    INT_T i = 0;
    PIXEL_DEV_NODE_T *device = (PIXEL_DEV_NODE_T *)handle;

    if(NULL == handle || NULL == color || NULL == backcolor) {
        return OPRT_INVALID_PARM;
    }

    if(0 == device->flag.is_start) {
        return OPRT_COM_ERROR;
    }

    if(index_start >= device->pixel_num || index_start+pixel_num > device->pixel_num) {
        return OPRT_INVALID_PARM;
    }

    tal_mutex_lock(device->mutex);
    //background color
    for(i=0; i<device->pixel_num; i++) {
        __tdl_pixel_set_color(handle, device->pixel_buffer, device->pixel_color, device->color_num,  i, backcolor);
    }
    //dest color
    for(i=0; i<pixel_num; i++) {
        __tdl_pixel_set_color(handle, device->pixel_buffer, device->pixel_color,  device->color_num, index_start+i, color);
    }
    tal_mutex_unlock(device->mutex);

    return OPRT_OK;
}

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
                                UINT_T index_end, UINT_T move_step)
{
    PIXEL_DEV_NODE_T *device = (PIXEL_DEV_NODE_T *)handle;
    OPERATE_RET op_ret = OPRT_OK;

    if(NULL == handle || dir > PIXEL_SHIFT_LEFT) {
        return OPRT_INVALID_PARM;
    }

    if(0 == device->flag.is_start) {
        return OPRT_COM_ERROR;
    }

    if(index_start >= device->pixel_num || index_end >= device->pixel_num) {
        return OPRT_INVALID_PARM;
    }  

    tal_mutex_lock(device->mutex);
    if(PIXEL_SHIFT_RIGHT == dir){ 
       op_ret = __tdl_pixel_right_shift(device->pixel_buffer, device->color_num, \
                                        index_start, index_end, move_step);
    }else { 
        op_ret = __tdl_pixel_left_shift(device->pixel_buffer, device->color_num, \
                                        index_start, index_end, move_step);
    }
    tal_mutex_unlock(device->mutex);

    return op_ret;
}

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
                                      UINT_T index_end, UINT_T move_step)
{
    INT_T half_len = 0;
    PIXEL_DEV_NODE_T *device = (PIXEL_DEV_NODE_T *)handle;
    OPERATE_RET op_ret = OPRT_OK;

    if(NULL == handle || dir > PIXEL_SHIFT_FAR) {
        return OPRT_INVALID_PARM;
    }

    if(0 == device->flag.is_start) {
        return OPRT_COM_ERROR;
    }

    if(index_start >= device->pixel_num || index_end >= device->pixel_num ||\
       index_start >= index_end) {
        return OPRT_INVALID_PARM;
    }  

    half_len = (index_end-index_start+1)/2;

    tal_mutex_lock(device->mutex);
    if(PIXEL_SHIFT_CLOSE == dir){ 
        op_ret = __tdl_pixel_right_shift(device->pixel_buffer, device->color_num, \
                                        index_start, index_start+half_len-1, move_step);
        if(op_ret != OPRT_OK) {
            goto END;
        }

        op_ret = __tdl_pixel_left_shift(device->pixel_buffer, device->color_num, \
                                        index_start+half_len, index_start+2*half_len-1, move_step);     
        if(op_ret != OPRT_OK) {
            goto END;
        }
                                    
    }else { 
        op_ret = __tdl_pixel_left_shift(device->pixel_buffer, device->color_num, \
                                        index_start, index_start+half_len-1, move_step);
        if(op_ret != OPRT_OK) {
            goto END;
        }
                                        
        op_ret = __tdl_pixel_right_shift(device->pixel_buffer, device->color_num, \
                                        index_start+half_len, index_start+2*half_len-1, move_step);    
        if(op_ret != OPRT_OK) {
            goto END;
        }
         
    }

END:
    tal_mutex_unlock(device->mutex);

    return op_ret;
}

/**
* @brief        获得像素颜色
*
* @param[in]    handle           设备句柄
* @param[in]    index            下标
* @param[out]   color            颜色
*
* @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
*/
int tdl_pixel_get_color(PIXEL_HANDLE_T handle, UINT_T index,  PIXEL_COLOR_T *color)
{
    PIXEL_DEV_NODE_T *device = (PIXEL_DEV_NODE_T *)handle;

    if(NULL == handle || NULL == color) {
        return OPRT_INVALID_PARM;
    }

    if(0 == device->flag.is_start) {
        return OPRT_COM_ERROR;
    }

    if(index >= device->pixel_num) {
        return OPRT_INVALID_PARM;
    }  

    __tdl_pixel_get_color(handle, device->pixel_buffer, device->pixel_color, device->color_num, index, color);

    return OPRT_OK;
}

/**
* @brief    设置所有像素颜色（单一）
*
* @param[in]    handle           设备句柄
* @param[in]    color            目标颜色
*
* @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
*/
int tdl_pixel_set_single_color_all(PIXEL_HANDLE_T handle,  PIXEL_COLOR_T *color)
{
    INT_T i = 0;
    PIXEL_DEV_NODE_T *device = (PIXEL_DEV_NODE_T *)handle;

    if(NULL == handle || NULL == color) {
        return OPRT_INVALID_PARM;
    }

    if(0 == device->flag.is_start) {
        return OPRT_COM_ERROR;
    }

    tal_mutex_lock(device->mutex);
    for(i=0; i<device->pixel_num; i++) {
        __tdl_pixel_set_color(handle, device->pixel_buffer, device->pixel_color, device->color_num, i, color);
    }
    tal_mutex_unlock(device->mutex);

    return OPRT_OK;
}

/**
* @brief    只设置白光颜色，不设置彩光
*
* @param[in]    handle           设备句柄
* @param[in]    color            目标颜色
*
* @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
*/
int tdl_pixel_set_single_white_all(PIXEL_HANDLE_T handle,  PIXEL_COLOR_T *color)
{
    INT_T i = 0;
    PIXEL_DEV_NODE_T *device = (PIXEL_DEV_NODE_T *)handle;

    if(NULL == handle || NULL == color) {
        return OPRT_INVALID_PARM;
    }

    if(0 == device->flag.is_start) {
        return OPRT_COM_ERROR;
    }

    tal_mutex_lock(device->mutex);
    for(i=0; i<device->pixel_num; i++) {
        __tdl_pixel_only_set_cw(handle, device->pixel_buffer, device->pixel_color, device->color_num, i, color);
    }
    tal_mutex_unlock(device->mutex);

    return OPRT_OK;
}

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
int tdl_pixel_copy_color(PIXEL_HANDLE_T handle, UINT_T dst_idx, UINT_T src_idx, UINT_T len)
{
    INT_T copy_len = 0;

    PIXEL_DEV_NODE_T *device = (PIXEL_DEV_NODE_T *)handle;

    if(NULL == handle || 0 == len) {
        return OPRT_INVALID_PARM;
    }

    if(0 == device->flag.is_start) {
        return OPRT_COM_ERROR;
    }

    if(src_idx >= device->pixel_num || dst_idx >= device->pixel_num ||
      (src_idx+len) > device->pixel_num || (dst_idx+len) > device->pixel_num) {
        return OPRT_INVALID_PARM;
    }

    copy_len = device->color_num * sizeof(USHORT_T) * len;

    memmove((unsigned char *)&device->pixel_buffer[dst_idx*device->color_num], \
            (unsigned char *)&device->pixel_buffer[src_idx*device->color_num], copy_len);

    return OPRT_OK;       
}

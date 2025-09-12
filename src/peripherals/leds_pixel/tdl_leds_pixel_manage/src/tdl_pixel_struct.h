/**
 * @file tdl_pixel_struct.h
 * @brief TDL layer internal structures for LED pixel devices
 *
 * This header file defines internal data structures used by the TDL (Tuya Device Layer)
 * for LED pixel device management. It includes device node structures, device lists,
 * pixel buffer management, and internal state tracking. These structures are used
 * internally by the TDL layer to maintain device state and provide abstractions
 * for LED strip operations across different pixel controller types.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
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
    uint8_t is_start : 1;
} PIXEL_FLAG_T;

typedef struct pixel_dev_list {
    struct pixel_dev_list *next;

    char name[PIXEL_DEV_NAME_MAX_LEN + 1];
    MUTEX_HANDLE mutex;

    PIXEL_FLAG_T flag;

    uint32_t pixel_num;
    uint16_t pixel_resolution;
    uint16_t *pixel_buffer;    // Pixel buffer
    uint32_t pixel_buffer_len; // Pixel buffer size

    SEM_HANDLE send_sem;

    uint8_t color_num; // Three/Four/Five channels
    PIXEL_COLOR_TP_E pixel_color;
    uint32_t color_maximum;
    DRIVER_HANDLE_T drv_handle;
    BOOL_T white_color_control; // Independent White Light and Color Light Control
    PIXEL_DRIVER_INTFS_T *intfs;

} PIXEL_DEV_NODE_T, PIXEL_DEV_LIST_T;

#ifdef __cplusplus
}
#endif /* __cplusplus */
#endif /*__TDL_PIXEL_STRUCT_H__*/
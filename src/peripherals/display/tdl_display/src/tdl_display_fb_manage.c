/**
 * @file tdl_display_fb_manage.c
 * @brief Frame buffer management module for display system
 *
 * This module provides frame buffer pool management functionality, including
 * initialization, allocation, and release of frame buffers for display operations.
 * It supports multiple pixel formats and manages frame buffer lifecycle with
 * thread-safe operations.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 *
 */
#include "tal_api.h"

#include "tdl_display_manage.h"
#include "tdl_display_fb_manage.h"
/***********************************************************
************************macro define************************
***********************************************************/


/***********************************************************
***********************typedef define***********************
***********************************************************/
typedef struct {
    bool                   is_used;   
    TDL_DISP_FRAME_BUFF_T *fb;
}TDL_DISP_FB_CTRL_T;

typedef struct {
    bool               is_wait_free;
    SEM_HANDLE         free_sem;
    MUTEX_HANDLE       mutex;
    uint8_t            num;
    TDL_DISP_FB_CTRL_T arr[TDL_DISP_FB_MANAGE_MAX_NUM];
}TDL_DISP_FB_MANAGE_T;

/***********************************************************
***********************variable define**********************
***********************************************************/


/***********************************************************
***********************function define**********************
***********************************************************/
/**
 * @brief Free callback function for frame buffer
 * @brief This function is called when a frame buffer is released, marking it as unused
 *        and notifying waiting threads if any
 * @param frame_buff Pointer to the frame buffer to be freed
 */
static void __disp_frame_buff_free(TDL_DISP_FRAME_BUFF_T *frame_buff)
{
    TDL_DISP_FB_MANAGE_T *fb_manage = NULL;

    if(NULL == frame_buff) {
        return;
    }

    fb_manage = (TDL_DISP_FB_MANAGE_T *)frame_buff->sys_param;
    if(NULL == fb_manage) {
        PR_ERR("fb manage is null");
        return;
    }

    for (uint8_t i = 0; i < fb_manage->num; i++) {
        if(fb_manage->arr[i].fb == frame_buff) {
            fb_manage->arr[i].is_used = false;
            if(fb_manage->is_wait_free) {
                fb_manage->is_wait_free = false;
                tal_semaphore_post(fb_manage->free_sem);
            }

            return;
        }
    }

    PR_ERR("frame buffer not found");
}

/**
 * @brief Initialize frame buffer manager
 * @brief Allocates and initializes a new frame buffer manager instance with
 *        semaphore and mutex for thread-safe operations
 * @param handle Pointer to store the frame buffer manager handle
 * @return OPRT_OK on success, OPRT_MALLOC_FAILED on memory allocation failure
 */
OPERATE_RET tdl_disp_fb_manage_init(TDL_FB_MANAGE_HANDLE_T *handle)
{
    OPERATE_RET rt = OPRT_OK;
    TDL_DISP_FB_MANAGE_T *fb_manage = NULL;
    
    TUYA_CHECK_NULL_RETURN(handle, OPRT_MALLOC_FAILED);
    
    fb_manage = (TDL_DISP_FB_MANAGE_T *)tal_malloc(sizeof(TDL_DISP_FB_MANAGE_T));
    TUYA_CHECK_NULL_RETURN(fb_manage, OPRT_MALLOC_FAILED);
    memset(fb_manage, 0, sizeof(TDL_DISP_FB_MANAGE_T));

    TUYA_CALL_ERR_RETURN(tal_semaphore_create_init(&fb_manage->free_sem, 0 ,1));
    TUYA_CALL_ERR_RETURN(tal_mutex_create_init(&fb_manage->mutex));

    *handle = (TDL_FB_MANAGE_HANDLE_T)fb_manage;

    return rt;
}

/**
 * @brief Add a frame buffer to the manager
 * @brief Creates a new frame buffer with specified format and dimensions,
 *        and adds it to the manager's pool
 * @param handle Frame buffer manager handle
 * @param fmt Pixel format of the frame buffer
 * @param width Width of the frame buffer in pixels
 * @param height Height of the frame buffer in pixels
 * @return OPRT_OK on success, OPRT_INVALID_PARM if max number exceeded or invalid parameters,
 *         OPRT_MALLOC_FAILED on memory allocation failure
 */
OPERATE_RET tdl_disp_fb_manage_add(TDL_FB_MANAGE_HANDLE_T handle, TUYA_DISPLAY_PIXEL_FMT_E fmt,\
                                   uint16_t width, uint16_t height)
{
    uint8_t per_pixel_byte = 0, idx = 0;
    uint32_t frame_len = 0;
    TDL_DISP_FB_MANAGE_T *fb_manage = (TDL_DISP_FB_MANAGE_T *)handle;

    TUYA_CHECK_NULL_RETURN(fb_manage, OPRT_MALLOC_FAILED);

    tal_mutex_lock(fb_manage->mutex);

    if(fb_manage->num >= TDL_DISP_FB_MANAGE_MAX_NUM) {
        PR_ERR("exceed max fb num:%d curr num:%d", TDL_DISP_FB_MANAGE_MAX_NUM, fb_manage->num);
        tal_mutex_unlock(fb_manage->mutex);
        return OPRT_INVALID_PARM;
    }

    idx = fb_manage->num;

    if(fmt == TUYA_PIXEL_FMT_MONOCHROME) {
        frame_len = (width + 7) / 8 * height;
    } else if(fmt == TUYA_PIXEL_FMT_I2){
        frame_len = (width + 3) / 4 * height;
    }else {
        per_pixel_byte = (tdl_disp_get_fmt_bpp(fmt) +7)/8;
        frame_len = width * height * per_pixel_byte;
    }

    if(fb_manage->arr[idx].fb) {
        tdl_disp_free_frame_buff(fb_manage->arr[idx].fb);
        fb_manage->arr[idx].fb = NULL;
    }

    fb_manage->arr[idx].fb = tdl_disp_create_frame_buff(DISP_FB_TP_PSRAM, frame_len);
    if(fb_manage->arr[idx].fb == NULL) {
        PR_ERR("create display frame buff failed");
        tal_mutex_unlock(fb_manage->mutex);
        return OPRT_MALLOC_FAILED;
    }

    fb_manage->arr[idx].is_used = false;

    fb_manage->arr[idx].fb->fmt       = fmt;
    fb_manage->arr[idx].fb->width     = width;
    fb_manage->arr[idx].fb->height    = height;
    fb_manage->arr[idx].fb->free_cb   = __disp_frame_buff_free;
    fb_manage->arr[idx].fb->sys_param = (void *)fb_manage;

    fb_manage->num++;

    tal_mutex_unlock(fb_manage->mutex);

    return OPRT_OK;
}

/**
 * @brief Get a free frame buffer from the manager
 * @brief Searches for an available frame buffer in the pool. If none is available,
 *        waits until one becomes free. Marks the returned buffer as used.
 * @param handle Frame buffer manager handle
 * @return Pointer to an available frame buffer, or NULL on error or if no buffer
 *         becomes available after waiting
 */
TDL_DISP_FRAME_BUFF_T *tdl_disp_get_free_fb(TDL_FB_MANAGE_HANDLE_T handle)
{
    OPERATE_RET rt = OPRT_OK;
    TDL_DISP_FB_MANAGE_T *fb_manage = (TDL_DISP_FB_MANAGE_T *)handle;

    TUYA_CHECK_NULL_RETURN(fb_manage, NULL);

    tal_mutex_lock(fb_manage->mutex);

    for (uint8_t i = 0; i < fb_manage->num; i++) {
        if(false == fb_manage->arr[i].is_used) {
            fb_manage->arr[i].is_used = true;
            tal_mutex_unlock(fb_manage->mutex);
            return fb_manage->arr[i].fb;
        }
    }

    fb_manage->is_wait_free = true;
    tal_mutex_unlock(fb_manage->mutex);
    TUYA_CALL_ERR_LOG(tal_semaphore_wait(fb_manage->free_sem, 5000));
    tal_mutex_lock(fb_manage->mutex);

    for (uint8_t i = 0; i < fb_manage->num; i++) {
        if(false == fb_manage->arr[i].is_used) {
            fb_manage->arr[i].is_used = true;
            tal_mutex_unlock(fb_manage->mutex);
            return fb_manage->arr[i].fb;
        }
    }

    PR_ERR("no free frame buffer available after wait");
    tal_mutex_unlock(fb_manage->mutex);

    return NULL;
}

/**
 * @brief Release frame buffer manager and free all resources
 * @brief Waits for all frame buffers to be released, then frees all frame buffers,
 *        destroys semaphore and mutex, and frees the manager structure
 * @param handle Pointer to the frame buffer manager handle (will be set to NULL)
 * @return OPRT_OK on success, OPRT_MALLOC_FAILED if handle is invalid,
 *         or timeout error if frame buffers are not released in time
 */
OPERATE_RET tdl_disp_fb_manage_release(TDL_FB_MANAGE_HANDLE_T *handle)
{
    OPERATE_RET rt = OPRT_OK;
    TDL_DISP_FB_MANAGE_T *fb_manage = (TDL_DISP_FB_MANAGE_T *)(*handle);

    TUYA_CHECK_NULL_RETURN(fb_manage, OPRT_MALLOC_FAILED);

    tal_mutex_lock(fb_manage->mutex);

    for (uint8_t i = 0; i < fb_manage->num; i++) {
        if(fb_manage->arr[i].fb) {
            while(true == fb_manage->arr[i].is_used){
                rt = tal_semaphore_wait(fb_manage->free_sem, 1000);
                if(rt != OPRT_OK && fb_manage->arr[i].is_used) {
                    PR_ERR("wait fb free sem timeout");
                    tal_mutex_unlock(fb_manage->mutex);
                    return rt;
                }
            }

            tdl_disp_free_frame_buff(fb_manage->arr[i].fb);
        }
    }

    if(fb_manage->free_sem) {
        tal_semaphore_release(fb_manage->free_sem);
        fb_manage->free_sem = NULL;
    }

    tal_mutex_unlock(fb_manage->mutex);

    tal_mutex_release(fb_manage->mutex);
    fb_manage->mutex = NULL;

    tal_free(fb_manage);
    *handle = NULL;

    return OPRT_OK;
}


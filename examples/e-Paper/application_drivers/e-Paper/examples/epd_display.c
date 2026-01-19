/*****************************************************************************
 * @file        epd_display.c
 * @author      Custom
 * @brief       墨水屏统一显示管理模块实现
 * @version     1.0
 * @date        2025-01-01
 * 
 *****************************************************************************/

#include "epd_display.h"
#include "epd_mutex.h"
#include "tal_log.h"
#include <stdlib.h>
#include <string.h>

/*============================================================================
                            私有变量
============================================================================*/
static UBYTE *sg_screen_buffer = NULL;      /**< 全屏缓冲区 */
static BOOL_T sg_initialized = FALSE;       /**< 初始化标志 */
static BOOL_T sg_dirty = FALSE;             /**< 脏区域标志 */

/*============================================================================
                            公共函数实现
============================================================================*/

/**
 * @brief 初始化显示管理器
 */
OPERATE_RET epd_display_init(VOID_T)
{
    if (sg_initialized) {
        return OPRT_OK;
    }
    
    TAL_PR_NOTICE("[EPD_DISPLAY] Initializing...");
    
    /* 初始化互斥锁 */
    epd_mutex_init();
    
    /* 分配全屏缓冲区 */
    /* 800 x 480 / 8 = 48000 字节 */
    UINT32_T buf_size = (EPD_SCREEN_WIDTH * EPD_SCREEN_HEIGHT) / 8;
    sg_screen_buffer = (UBYTE *)malloc(buf_size);
    if (sg_screen_buffer == NULL) {
        TAL_PR_ERR("[EPD_DISPLAY] malloc failed! size=%d", buf_size);
        return OPRT_MALLOC_FAILED;
    }
    
    /* 初始化为白色 */
    memset(sg_screen_buffer, 0xFF, buf_size);
    
    sg_initialized = TRUE;
    sg_dirty = FALSE;
    
    TAL_PR_NOTICE("[EPD_DISPLAY] Initialized, buffer=%d bytes", buf_size);
    return OPRT_OK;
}

/**
 * @brief 释放显示管理器资源
 */
OPERATE_RET epd_display_deinit(VOID_T)
{
    if (!sg_initialized) {
        return OPRT_OK;
    }
    
    if (sg_screen_buffer != NULL) {
        free(sg_screen_buffer);
        sg_screen_buffer = NULL;
    }
    
    sg_initialized = FALSE;
    TAL_PR_NOTICE("[EPD_DISPLAY] Deinitialized");
    return OPRT_OK;
}

/**
 * @brief 获取全屏缓冲区指针
 */
UBYTE* epd_display_get_buffer(VOID_T)
{
    return sg_screen_buffer;
}

/**
 * @brief 选择全屏画布用于绘图
 */
VOID_T epd_display_select_canvas(VOID_T)
{
    if (sg_screen_buffer == NULL) {
        return;
    }
    
    Paint_NewImage(sg_screen_buffer, EPD_SCREEN_WIDTH, EPD_SCREEN_HEIGHT, ROTATE_0, WHITE);
    Paint_SelectImage(sg_screen_buffer);
}

/**
 * @brief 清空整个画布
 */
VOID_T epd_display_clear(VOID_T)
{
    if (sg_screen_buffer == NULL) {
        return;
    }
    
    UINT32_T buf_size = (EPD_SCREEN_WIDTH * EPD_SCREEN_HEIGHT) / 8;
    memset(sg_screen_buffer, 0xFF, buf_size);
    sg_dirty = TRUE;
}

/**
 * @brief 清空信息栏区域
 */
VOID_T epd_display_clear_info(VOID_T)
{
    if (sg_screen_buffer == NULL) {
        return;
    }
    
    epd_display_select_canvas();
    
    /* 填充信息栏区域为白色 */
    Paint_ClearWindows(EPD_INFO_X, EPD_INFO_Y, 
                       EPD_INFO_X + EPD_INFO_WIDTH, 
                       EPD_INFO_Y + EPD_INFO_HEIGHT, WHITE);
    sg_dirty = TRUE;
}

/**
 * @brief 清空宠物区域
 */
VOID_T epd_display_clear_pet(VOID_T)
{
    if (sg_screen_buffer == NULL) {
        return;
    }
    
    epd_display_select_canvas();
    
    /* 填充宠物区域为白色 */
    Paint_ClearWindows(EPD_PET_AREA_X, EPD_PET_AREA_Y, 
                       EPD_PET_AREA_X + EPD_PET_AREA_WIDTH, 
                       EPD_PET_AREA_Y + EPD_PET_AREA_HEIGHT, WHITE);
    sg_dirty = TRUE;
}

/**
 * @brief 刷新整个屏幕到墨水屏
 */
VOID_T epd_display_refresh(VOID_T)
{
    if (sg_screen_buffer == NULL) {
        return;
    }
    
    TAL_PR_NOTICE("[EPD_DISPLAY] Refreshing screen...");
    
    /* 获取互斥锁 */
    epd_mutex_lock();
    
    /* 使用快速刷新模式（约1.5秒，而不是标准的4秒） */
    EPD_4in26_Display_Fast(sg_screen_buffer);
    
    /* 释放互斥锁 */
    epd_mutex_unlock();
    
    sg_dirty = FALSE;
    
    TAL_PR_NOTICE("[EPD_DISPLAY] Refresh done");
}

/**
 * @brief 标记需要刷新
 */
VOID_T epd_display_mark_dirty(VOID_T)
{
    sg_dirty = TRUE;
}

/**
 * @brief 检查是否需要刷新
 */
BOOL_T epd_display_is_dirty(VOID_T)
{
    return sg_dirty;
}

/**
 * @brief 如果有脏区域则刷新
 */
VOID_T epd_display_flush_if_dirty(VOID_T)
{
    if (sg_dirty) {
        epd_display_refresh();
    }
}


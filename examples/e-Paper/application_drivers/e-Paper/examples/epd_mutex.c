/*****************************************************************************
 * @file        epd_mutex.c
 * @author      Custom
 * @brief       墨水屏访问互斥锁模块实现
 * @version     1.0
 * @date        2025-01-01
 * 
 *****************************************************************************/

#include "epd_mutex.h"
#include "tal_mutex.h"
#include "tal_log.h"

/*============================================================================
                            私有变量
============================================================================*/
static MUTEX_HANDLE sg_epd_mutex = NULL;
static BOOL_T sg_mutex_initialized = FALSE;

/*============================================================================
                            公共函数实现
============================================================================*/

/**
 * @brief 初始化EPD互斥锁
 */
OPERATE_RET epd_mutex_init(VOID_T)
{
    if (sg_mutex_initialized) {
        return OPRT_OK;
    }
    
    OPERATE_RET ret = tal_mutex_create_init(&sg_epd_mutex);
    if (ret != OPRT_OK) {
        TAL_PR_ERR("EPD mutex create failed, ret=%d", ret);
        return ret;
    }
    
    sg_mutex_initialized = TRUE;
    TAL_PR_NOTICE("EPD mutex initialized");
    return OPRT_OK;
}

/**
 * @brief 获取EPD互斥锁
 */
OPERATE_RET epd_mutex_lock(VOID_T)
{
    if (!sg_mutex_initialized || sg_epd_mutex == NULL) {
        /* 未初始化时自动初始化 */
        OPERATE_RET ret = epd_mutex_init();
        if (ret != OPRT_OK) {
            return ret;
        }
    }
    
    return tal_mutex_lock(sg_epd_mutex);
}

/**
 * @brief 释放EPD互斥锁
 */
OPERATE_RET epd_mutex_unlock(VOID_T)
{
    if (!sg_mutex_initialized || sg_epd_mutex == NULL) {
        return OPRT_COM_ERROR;
    }
    
    return tal_mutex_unlock(sg_epd_mutex);
}

/**
 * @brief 销毁EPD互斥锁
 */
OPERATE_RET epd_mutex_deinit(VOID_T)
{
    if (!sg_mutex_initialized) {
        return OPRT_OK;
    }
    
    if (sg_epd_mutex != NULL) {
        tal_mutex_release(sg_epd_mutex);
        sg_epd_mutex = NULL;
    }
    
    sg_mutex_initialized = FALSE;
    TAL_PR_NOTICE("EPD mutex destroyed");
    return OPRT_OK;
}


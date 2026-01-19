/*****************************************************************************
 * @file        epd_mutex.h
 * @author      Custom
 * @brief       墨水屏访问互斥锁模块
 * @version     1.0
 * @date        2025-01-01
 * 
 * @details     解决多线程同时操作墨水屏导致显示冲突的问题
 *              - epd_clock 和 epd_pet 线程需要共享这个互斥锁
 *              - 所有墨水屏刷新操作必须先获取锁
 * 
 *****************************************************************************/
#ifndef __EPD_MUTEX_H__
#define __EPD_MUTEX_H__

#include "tuya_cloud_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 初始化EPD互斥锁
 * @return OPRT_OK: 成功
 */
OPERATE_RET epd_mutex_init(VOID_T);

/**
 * @brief 获取EPD互斥锁（阻塞等待）
 * @return OPRT_OK: 成功获取
 */
OPERATE_RET epd_mutex_lock(VOID_T);

/**
 * @brief 释放EPD互斥锁
 * @return OPRT_OK: 成功释放
 */
OPERATE_RET epd_mutex_unlock(VOID_T);

/**
 * @brief 销毁EPD互斥锁
 * @return OPRT_OK: 成功
 */
OPERATE_RET epd_mutex_deinit(VOID_T);

#ifdef __cplusplus
}
#endif

#endif /* __EPD_MUTEX_H__ */


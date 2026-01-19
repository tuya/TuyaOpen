/*****************************************************************************
 * @file        epd_display.h
 * @author      Custom
 * @brief       墨水屏统一显示管理模块
 * @version     1.0
 * @date        2025-01-01
 * 
 * @details     解决多区域刷新互相覆盖的问题
 *              - 管理一个全屏缓冲区
 *              - 各模块绘制到全屏缓冲区的不同区域
 *              - 统一刷新，避免区域互相覆盖
 * 
 *****************************************************************************/
#ifndef __EPD_DISPLAY_H__
#define __EPD_DISPLAY_H__

#include "tuya_cloud_types.h"
#include "EPD_4in26.h"
#include "GUI_Paint.h"

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
                            显示区域定义
============================================================================*/
/**
 * @brief 屏幕总尺寸
 */
#define EPD_SCREEN_WIDTH    800
#define EPD_SCREEN_HEIGHT   480

/**
 * @brief 信息栏区域（视觉上方）
 *        信息栏在 Y=0~120
 */
#define EPD_INFO_X          0
#define EPD_INFO_Y          0       /* 信息栏顶边Y=0，范围0~120 */
#define EPD_INFO_WIDTH      800
#define EPD_INFO_HEIGHT     120

/**
 * @brief 宠物区域（视觉下方）
 *        宠物在 Y=140~480
 */
#define EPD_PET_AREA_X      0
#define EPD_PET_AREA_Y      140     /* 宠物区域 140~480，留20像素间距 */
#define EPD_PET_AREA_WIDTH  800
#define EPD_PET_AREA_HEIGHT 340

/*============================================================================
                            函数声明
============================================================================*/

/**
 * @brief 初始化显示管理器
 * @return OPRT_OK: 成功
 */
OPERATE_RET epd_display_init(VOID_T);

/**
 * @brief 释放显示管理器资源
 * @return OPRT_OK: 成功
 */
OPERATE_RET epd_display_deinit(VOID_T);

/**
 * @brief 获取全屏缓冲区指针
 * @return 缓冲区指针，NULL表示未初始化
 */
UBYTE* epd_display_get_buffer(VOID_T);

/**
 * @brief 获取画布（Paint对象），用于绘图
 * @note 调用此函数后，画布已选中全屏缓冲区
 */
VOID_T epd_display_select_canvas(VOID_T);

/**
 * @brief 清空整个画布（填充白色）
 */
VOID_T epd_display_clear(VOID_T);

/**
 * @brief 清空信息栏区域
 */
VOID_T epd_display_clear_info(VOID_T);

/**
 * @brief 清空宠物区域
 */
VOID_T epd_display_clear_pet(VOID_T);

/**
 * @brief 刷新整个屏幕到墨水屏
 * @note 将全屏缓冲区内容发送到墨水屏并刷新
 */
VOID_T epd_display_refresh(VOID_T);

/**
 * @brief 标记需要刷新
 * @note 用于延迟刷新，多个区域更新后再统一刷新
 */
VOID_T epd_display_mark_dirty(VOID_T);

/**
 * @brief 检查是否需要刷新
 * @return TRUE: 需要刷新
 */
BOOL_T epd_display_is_dirty(VOID_T);

/**
 * @brief 如果有脏区域则刷新
 */
VOID_T epd_display_flush_if_dirty(VOID_T);

#ifdef __cplusplus
}
#endif

#endif /* __EPD_DISPLAY_H__ */


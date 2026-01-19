/*****************************************************************************
 * @file        epd_clock.c
 * @author      Custom
 * @brief       电子墨水屏信息栏显示模块（使用统一缓冲区版本）
 * @version     1.2
 * @date        2025-12-12
 * 
 * @details     功能概述：
 *              本模块实现了一个电子墨水屏（E-Paper/EPD）信息栏显示系统，
 *              并统一管理整个屏幕的刷新，避免多区域刷新冲突。
 *              
 *              【主要特性】
 *              1. 自动刷新：独立线程每分钟检测时间变化并刷新
 *              2. 光照感应：根据光敏传感器读数显示不同天气图标
 *              3. 统一缓冲区：信息栏和宠物区共用全屏缓冲区
 *              4. 同步刷新：一次刷新整个屏幕，避免区域互相覆盖
 *              5. 时间同步：等待网络时间同步后才显示正确时间
 * 
 *****************************************************************************/

#include "epd_clock.h"          /* 本模块头文件 */
#include "epd_display.h"        /* 统一显示管理 */
#include "epd_pet.h"            /* 宠物模块（用于绘制）*/
#include "epd_mutex.h"          /* 墨水屏互斥锁 */
#include "EPD_4in26.h"          /* 4.26寸墨水屏驱动 */
#include "GUI_Paint.h"          /* 图形绘制库 */
#include "tal_thread.h"         /* TuyaOS 线程API */
#include "tal_system.h"         /* TuyaOS 系统API（延时等）*/
#include "tal_time_service.h"   /* TuyaOS 时间服务API */
#include "tal_log.h"            /* TuyaOS 日志打印API */
#include "light_sensor.h"       /* 光敏传感器驱动 */
#include "soil_moisture.h"      /* 土壤湿度传感器驱动 */
#include <stdio.h>              /* 标准输入输出（snprintf）*/
#include <stdlib.h>             /* 标准库（malloc/free）*/
#include <string.h>             /* 字符串处理（memset）*/

/*============================================================================
                            私有宏定义
============================================================================*/

/**
 * @brief 判断时间是否已同步的年份阈值
 */
#define TIME_SYNC_YEAR_THRESHOLD    2024

/*============================================================================
                            私有变量定义
============================================================================*/

static THREAD_HANDLE sg_clock_thread = NULL;
static BOOL_T sg_clock_running = FALSE;
static BOOL_T sg_clock_stop_flag = FALSE;
static BOOL_T sg_epd_initialized = FALSE;
static INT_T sg_last_display_min = -1;
static BOOL_T sg_time_synced = FALSE;

/*============================================================================
                            私有函数声明
============================================================================*/

static VOID_T _clock_thread_func(PVOID_T args);
static VOID_T _draw_info_bar(POSIX_TM_S *tm);
static VOID_T _draw_waiting_sync(VOID_T);
static VOID_T _refresh_all(POSIX_TM_S *tm);
static BOOL_T _is_time_synced(POSIX_TM_S *tm);

/*============================================================================
                            公共函数实现
============================================================================*/

/**
 * @brief 启动信息栏显示
 */
OPERATE_RET epd_clock_start(VOID_T)
{
    OPERATE_RET ret = OPRT_OK;
    
    if (sg_clock_running) {
        TAL_PR_WARN("EPD info bar is already running");
        return OPRT_OK;
    }
    
    TAL_PR_NOTICE("=== Starting EPD Info Bar (Unified Buffer) ===");
    
    /* 步骤1: 初始化统一显示管理器 */
    TAL_PR_NOTICE("Step 1: Initializing display manager...");
    ret = epd_display_init();
    if (ret != OPRT_OK) {
        TAL_PR_ERR("Display manager init failed, ret=%d", ret);
        return ret;
    }
    
    /* 步骤2: 初始化底层硬件 */
    TAL_PR_NOTICE("Step 2: DEV_Module_Init...");
    if (DEV_Module_Init() != 0) {
        TAL_PR_ERR("DEV_Module_Init failed");
        return OPRT_COM_ERROR;
    }
    TAL_PR_NOTICE("Hardware init OK");
    
    /* 步骤3: 初始化墨水屏（快速模式） */
    TAL_PR_NOTICE("Step 3: EPD_4in26_Init_Fast...");
    EPD_4in26_Init_Fast();
    TAL_PR_NOTICE("EPD init OK (fast mode)");
    
    /* 步骤4: 清屏 */
    TAL_PR_NOTICE("Step 4: EPD_4in26_Clear...");
    EPD_4in26_Clear();
    TAL_PR_NOTICE("Clear OK");
    
    /* 步骤5: 启动光敏传感器 */
    TAL_PR_NOTICE("Step 5: Starting light sensor...");
    ret = light_sensor_start_periodic(1000, NULL);
    if (ret != OPRT_OK) {
        TAL_PR_WARN("Light sensor start failed, ret=%d", ret);
    } else {
        TAL_PR_NOTICE("Light sensor started OK");
    }
    tal_system_sleep(100);
    
    /* 步骤6: 显示初始内容 */
    TAL_PR_NOTICE("Step 6: Showing initial display...");
    POSIX_TM_S tm;
    memset(&tm, 0, sizeof(tm));
    tal_time_get_local_time_custom(0, &tm);
    
    sg_time_synced = _is_time_synced(&tm);
    sg_last_display_min = tm.tm_min;
    
    /* 绘制并刷新（包括信息栏和宠物） */
    _refresh_all(&tm);
    TAL_PR_NOTICE("Initial display OK (time_synced=%d)", sg_time_synced);
    
    sg_clock_running = TRUE;
    sg_epd_initialized = TRUE;
    
    /* 步骤7: 创建刷新线程 */
    TAL_PR_NOTICE("Step 7: Creating refresh thread...");
    sg_clock_stop_flag = FALSE;
    
    THREAD_CFG_T thread_cfg = {
        .stackDepth = 4096,
        .priority = THREAD_PRIO_3,
        .thrdname = "epd_refresh"
    };
    
    ret = tal_thread_create_and_start(
        &sg_clock_thread, NULL, NULL,
        _clock_thread_func, NULL, &thread_cfg
    );
    
    if (ret != OPRT_OK) {
        TAL_PR_WARN("Thread create failed, ret=%d", ret);
    } else {
        TAL_PR_NOTICE("Refresh thread started");
    }
    
    TAL_PR_NOTICE("=== EPD Info Bar Ready ===");
    return OPRT_OK;
}

/**
 * @brief 停止信息栏显示
 */
OPERATE_RET epd_clock_stop(VOID_T)
{
    if (!sg_clock_running) {
        return OPRT_OK;
    }
    
    sg_clock_stop_flag = TRUE;
    
    UINT32_T timeout = 0;
    while (sg_clock_running && timeout < 100) {
        tal_system_sleep(100);
        timeout++;
    }
    
    if (sg_clock_thread != NULL) {
        tal_thread_delete(sg_clock_thread);
        sg_clock_thread = NULL;
    }
    
    light_sensor_stop_periodic();
    
    if (sg_epd_initialized) {
        EPD_4in26_Sleep();
        DEV_Module_Exit();
        sg_epd_initialized = FALSE;
    }
    
    epd_display_deinit();
    
    sg_clock_running = FALSE;
    TAL_PR_NOTICE("EPD info bar stopped");
    return OPRT_OK;
}

/**
 * @brief 检查是否运行中
 */
BOOL_T epd_clock_is_running(VOID_T)
{
    return sg_clock_running;
}

/**
 * @brief 立即刷新显示
 */
OPERATE_RET epd_clock_refresh_now(VOID_T)
{
    if (!sg_clock_running) {
        return OPRT_COM_ERROR;
    }
    
    POSIX_TM_S tm;
    memset(&tm, 0, sizeof(tm));
    tal_time_get_local_time_custom(0, &tm);
    
    _refresh_all(&tm);
    
    return OPRT_OK;
}

/*============================================================================
                            私有函数实现
============================================================================*/

/**
 * @brief 检查时间是否已同步
 */
static BOOL_T _is_time_synced(POSIX_TM_S *tm)
{
    if (tm == NULL) {
        return FALSE;
    }
    return ((tm->tm_year + 1900) >= TIME_SYNC_YEAR_THRESHOLD);
}

/**
 * @brief 绘制"等待时间同步"界面到全屏缓冲区的信息栏区域
 */
static VOID_T _draw_waiting_sync(VOID_T)
{
    /* 选择全屏画布 */
    epd_display_select_canvas();
    
    /* 清空信息栏区域 */
    Paint_ClearWindows(EPD_INFO_X, EPD_INFO_Y, 
                       EPD_INFO_X + EPD_INFO_WIDTH, 
                       EPD_INFO_Y + EPD_INFO_HEIGHT, WHITE);
    
    /* 绘制边框 */
    Paint_DrawRectangle(EPD_INFO_X + 2, EPD_INFO_Y + 2, 
                        EPD_INFO_X + EPD_INFO_WIDTH - 3, EPD_INFO_Y + EPD_INFO_HEIGHT - 3, 
                        BLACK, DOT_PIXEL_1X1, DRAW_FILL_EMPTY);
    Paint_DrawRectangle(EPD_INFO_X + 5, EPD_INFO_Y + 5, 
                        EPD_INFO_X + EPD_INFO_WIDTH - 6, EPD_INFO_Y + EPD_INFO_HEIGHT - 6, 
                        BLACK, DOT_PIXEL_1X1, DRAW_FILL_EMPTY);
    
    /* 时钟图标 */
    UINT16_T icon_x = EPD_INFO_X + 60;
    UINT16_T icon_y = EPD_INFO_Y + 60;
    Paint_DrawCircle(icon_x, icon_y, 30, BLACK, DOT_PIXEL_2X2, DRAW_FILL_EMPTY);
    Paint_DrawCircle(icon_x, icon_y, 3, BLACK, DOT_PIXEL_1X1, DRAW_FILL_FULL);
    Paint_DrawLine(icon_x, icon_y, icon_x, icon_y - 20, BLACK, DOT_PIXEL_2X2, LINE_STYLE_SOLID);
    Paint_DrawLine(icon_x, icon_y, icon_x + 15, icon_y, BLACK, DOT_PIXEL_2X2, LINE_STYLE_SOLID);
    
    /* 分隔线 */
    Paint_DrawLine(EPD_INFO_X + 120, EPD_INFO_Y + 15, 
                   EPD_INFO_X + 120, EPD_INFO_Y + EPD_INFO_HEIGHT - 15, 
                   BLACK, DOT_PIXEL_1X1, LINE_STYLE_DOTTED);
    
    /* 文字 */
    Paint_DrawString_EN(EPD_INFO_X + 140, EPD_INFO_Y + 25, 
                        "Waiting for time sync...", &Font24, WHITE, BLACK);
    Paint_DrawString_EN(EPD_INFO_X + 140, EPD_INFO_Y + 65, 
                        "Please ensure network connection", &Font20, WHITE, BLACK);
    
    TAL_PR_NOTICE("Display: Waiting for time sync...");
}

/**
 * @brief 绘制信息栏到全屏缓冲区
 */
static VOID_T _draw_info_bar(POSIX_TM_S *tm)
{
    CHAR_T time_str[64];
    CHAR_T light_str[64];
    
    if (tm == NULL) {
        return;
    }
    
    /* 选择全屏画布 */
    epd_display_select_canvas();
    
    /* 清空信息栏区域 */
    Paint_ClearWindows(EPD_INFO_X, EPD_INFO_Y, 
                       EPD_INFO_X + EPD_INFO_WIDTH, 
                       EPD_INFO_Y + EPD_INFO_HEIGHT, WHITE);
    
    /* 星期名称 */
    static const CHAR_T *weekday_names[] = {
        "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"
    };
    
    /* 格式化时间 */
    snprintf(time_str, sizeof(time_str), 
             "%04d-%02d-%02d %s %02d:%02d",
             tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday,
             weekday_names[tm->tm_wday], tm->tm_hour, tm->tm_min);
    
    /* 获取光照 */
    UINT8_T light_percent = light_sensor_get_light_percent();
    
    /* 生成进度条 */
    CHAR_T progress_bar[13];
    INT_T filled = light_percent / 10;
    if (filled > 10) filled = 10;
    progress_bar[0] = '[';
    for (INT_T i = 0; i < 10; i++) {
        progress_bar[1 + i] = (i < filled) ? '#' : '-';
    }
    progress_bar[11] = ']';
    progress_bar[12] = '\0';
    
    /* 光照状态 */
    if (light_percent >= 71) {
        snprintf(light_str, sizeof(light_str), "%s %d%% SUNNY", progress_bar, light_percent);
    } else if (light_percent >= 40) {
        snprintf(light_str, sizeof(light_str), "%s %d%% CLOUDY", progress_bar, light_percent);
    } else {
        snprintf(light_str, sizeof(light_str), "%s %d%% DARK", progress_bar, light_percent);
    }
    
    TAL_PR_NOTICE("Display: %s | Light:%d%% | Soil:%d%%", time_str, light_percent, soil_moisture_get_percent());
    
    /* 绘制边框 */
    Paint_DrawRectangle(EPD_INFO_X + 2, EPD_INFO_Y + 2, 
                        EPD_INFO_X + EPD_INFO_WIDTH - 3, EPD_INFO_Y + EPD_INFO_HEIGHT - 3, 
                        BLACK, DOT_PIXEL_1X1, DRAW_FILL_EMPTY);
    Paint_DrawRectangle(EPD_INFO_X + 5, EPD_INFO_Y + 5, 
                        EPD_INFO_X + EPD_INFO_WIDTH - 6, EPD_INFO_Y + EPD_INFO_HEIGHT - 6, 
                        BLACK, DOT_PIXEL_1X1, DRAW_FILL_EMPTY);
    
    /* 天气图标 */
    UINT16_T icon_x = EPD_INFO_X + 45;
    UINT16_T icon_y = EPD_INFO_Y + 60;
    
    if (light_percent >= 71) {
        /* 太阳 */
        Paint_DrawCircle(icon_x, icon_y, 15, BLACK, DOT_PIXEL_1X1, DRAW_FILL_FULL);
        Paint_DrawLine(icon_x, icon_y - 25, icon_x, icon_y - 35, BLACK, DOT_PIXEL_2X2, LINE_STYLE_SOLID);
        Paint_DrawLine(icon_x, icon_y + 25, icon_x, icon_y + 35, BLACK, DOT_PIXEL_2X2, LINE_STYLE_SOLID);
        Paint_DrawLine(icon_x - 25, icon_y, icon_x - 35, icon_y, BLACK, DOT_PIXEL_2X2, LINE_STYLE_SOLID);
        Paint_DrawLine(icon_x + 25, icon_y, icon_x + 35, icon_y, BLACK, DOT_PIXEL_2X2, LINE_STYLE_SOLID);
        Paint_DrawLine(icon_x - 18, icon_y - 18, icon_x - 25, icon_y - 25, BLACK, DOT_PIXEL_2X2, LINE_STYLE_SOLID);
        Paint_DrawLine(icon_x + 18, icon_y - 18, icon_x + 25, icon_y - 25, BLACK, DOT_PIXEL_2X2, LINE_STYLE_SOLID);
        Paint_DrawLine(icon_x - 18, icon_y + 18, icon_x - 25, icon_y + 25, BLACK, DOT_PIXEL_2X2, LINE_STYLE_SOLID);
        Paint_DrawLine(icon_x + 18, icon_y + 18, icon_x + 25, icon_y + 25, BLACK, DOT_PIXEL_2X2, LINE_STYLE_SOLID);
    } else if (light_percent >= 40) {
        /* 多云 */
        Paint_DrawCircle(icon_x - 15, icon_y + 5, 18, BLACK, DOT_PIXEL_1X1, DRAW_FILL_FULL);
        Paint_DrawCircle(icon_x + 5, icon_y + 8, 15, BLACK, DOT_PIXEL_1X1, DRAW_FILL_FULL);
        Paint_DrawCircle(icon_x + 20, icon_y + 5, 12, BLACK, DOT_PIXEL_1X1, DRAW_FILL_FULL);
        Paint_DrawCircle(icon_x - 5, icon_y - 5, 14, BLACK, DOT_PIXEL_1X1, DRAW_FILL_FULL);
        Paint_DrawCircle(icon_x + 10, icon_y - 3, 12, BLACK, DOT_PIXEL_1X1, DRAW_FILL_FULL);
        Paint_DrawCircle(icon_x - 12, icon_y + 8, 12, WHITE, DOT_PIXEL_1X1, DRAW_FILL_FULL);
        Paint_DrawCircle(icon_x + 5, icon_y + 10, 10, WHITE, DOT_PIXEL_1X1, DRAW_FILL_FULL);
        Paint_DrawCircle(icon_x - 3, icon_y, 10, WHITE, DOT_PIXEL_1X1, DRAW_FILL_FULL);
        Paint_DrawCircle(icon_x - 25, icon_y - 20, 10, BLACK, DOT_PIXEL_1X1, DRAW_FILL_FULL);
        Paint_DrawLine(icon_x - 25, icon_y - 35, icon_x - 25, icon_y - 40, BLACK, DOT_PIXEL_2X2, LINE_STYLE_SOLID);
        Paint_DrawLine(icon_x - 40, icon_y - 20, icon_x - 45, icon_y - 20, BLACK, DOT_PIXEL_2X2, LINE_STYLE_SOLID);
        Paint_DrawLine(icon_x - 35, icon_y - 30, icon_x - 40, icon_y - 35, BLACK, DOT_PIXEL_2X2, LINE_STYLE_SOLID);
    } else {
        /* 月亮 */
        Paint_DrawCircle(icon_x, icon_y, 20, BLACK, DOT_PIXEL_1X1, DRAW_FILL_FULL);
        Paint_DrawCircle(icon_x + 12, icon_y - 5, 18, WHITE, DOT_PIXEL_1X1, DRAW_FILL_FULL);
        Paint_DrawPoint(icon_x - 25, icon_y - 25, BLACK, DOT_PIXEL_3X3, DOT_STYLE_DFT);
        Paint_DrawPoint(icon_x + 30, icon_y - 20, BLACK, DOT_PIXEL_2X2, DOT_STYLE_DFT);
        Paint_DrawPoint(icon_x - 20, icon_y + 20, BLACK, DOT_PIXEL_2X2, DOT_STYLE_DFT);
        Paint_DrawPoint(icon_x + 25, icon_y + 15, BLACK, DOT_PIXEL_2X2, DOT_STYLE_DFT);
    }
    
    /* 分隔线 */
    Paint_DrawLine(EPD_INFO_X + 90, EPD_INFO_Y + 15, 
                   EPD_INFO_X + 90, EPD_INFO_Y + EPD_INFO_HEIGHT - 15, 
                   BLACK, DOT_PIXEL_1X1, LINE_STYLE_DOTTED);
    
    /* 时间日期 */
    Paint_DrawString_EN(EPD_INFO_X + 100, EPD_INFO_Y + 25, time_str, &Font24, WHITE, BLACK);
    
    /* 光照状态 */
    Paint_DrawString_EN(EPD_INFO_X + 100, EPD_INFO_Y + 55, light_str, &Font16, WHITE, BLACK);
    
    /* 获取土壤湿度 */
    UINT8_T soil_percent = soil_moisture_get_percent();
    CHAR_T soil_str[48];
    
    /* 生成土壤湿度进度条 */
    CHAR_T soil_bar[13];
    INT_T soil_filled = soil_percent / 10;
    if (soil_filled > 10) soil_filled = 10;
    soil_bar[0] = '[';
    for (INT_T i = 0; i < 10; i++) {
        soil_bar[1 + i] = (i < soil_filled) ? '#' : '-';
    }
    soil_bar[11] = ']';
    soil_bar[12] = '\0';
    
    /* 土壤湿度状态 */
    if (soil_percent >= 70) {
        snprintf(soil_str, sizeof(soil_str), "Soil:%s %d%% WET", soil_bar, soil_percent);
    } else if (soil_percent >= 30) {
        snprintf(soil_str, sizeof(soil_str), "Soil:%s %d%% OK", soil_bar, soil_percent);
    } else {
        snprintf(soil_str, sizeof(soil_str), "Soil:%s %d%% DRY!", soil_bar, soil_percent);
    }
    
    /* 土壤湿度显示 */
    Paint_DrawString_EN(EPD_INFO_X + 100, EPD_INFO_Y + 80, soil_str, &Font16, WHITE, BLACK);
    
    /* 励志短语 */
    static const CHAR_T *phrases[] = {
        "Have a nice day!", "Keep smiling!", "Stay positive!", "You are awesome!",
        "Enjoy today!", "Be happy!", "Good vibes only!", "Make it count!"
    };
    static const CHAR_T *happy_faces[] = {"^_^", "^o^", ":D", "=)"};
    static const CHAR_T *neutral_faces[] = {"-_-", "._.", ":|", "=|"};
    static const CHAR_T *sad_faces[] = {"T_T", ";_;", ":(", "='("};
    
    INT_T phrase_idx = tm->tm_min % 8;
    INT_T face_idx = tm->tm_min % 4;
    
    Paint_DrawString_EN(EPD_INFO_X + 500, EPD_INFO_Y + 25, phrases[phrase_idx], &Font20, WHITE, BLACK);
    
    const CHAR_T *face;
    if (light_percent >= 71) {
        face = happy_faces[face_idx];
    } else if (light_percent >= 40) {
        face = neutral_faces[face_idx];
    } else {
        face = sad_faces[face_idx];
    }
    Paint_DrawString_EN(EPD_INFO_X + 560, EPD_INFO_Y + 70, face, &Font24, WHITE, BLACK);
    
    /* 角落装饰 */
    Paint_DrawLine(EPD_INFO_X + EPD_INFO_WIDTH - 30, EPD_INFO_Y + 10, 
                   EPD_INFO_X + EPD_INFO_WIDTH - 10, EPD_INFO_Y + 10, BLACK, DOT_PIXEL_2X2, LINE_STYLE_SOLID);
    Paint_DrawLine(EPD_INFO_X + EPD_INFO_WIDTH - 10, EPD_INFO_Y + 10, 
                   EPD_INFO_X + EPD_INFO_WIDTH - 10, EPD_INFO_Y + 30, BLACK, DOT_PIXEL_2X2, LINE_STYLE_SOLID);
    Paint_DrawLine(EPD_INFO_X + EPD_INFO_WIDTH - 30, EPD_INFO_Y + EPD_INFO_HEIGHT - 10, 
                   EPD_INFO_X + EPD_INFO_WIDTH - 10, EPD_INFO_Y + EPD_INFO_HEIGHT - 10, BLACK, DOT_PIXEL_2X2, LINE_STYLE_SOLID);
    Paint_DrawLine(EPD_INFO_X + EPD_INFO_WIDTH - 10, EPD_INFO_Y + EPD_INFO_HEIGHT - 30, 
                   EPD_INFO_X + EPD_INFO_WIDTH - 10, EPD_INFO_Y + EPD_INFO_HEIGHT - 10, BLACK, DOT_PIXEL_2X2, LINE_STYLE_SOLID);
}

/**
 * @brief 刷新整个屏幕（信息栏 + 宠物）
 */
static VOID_T _refresh_all(POSIX_TM_S *tm)
{
    UBYTE *buffer = epd_display_get_buffer();
    if (buffer == NULL) {
        return;
    }
    
    /* 1. 绘制信息栏 */
    if (!_is_time_synced(tm)) {
        _draw_waiting_sync();
    } else {
        sg_time_synced = TRUE;
    _draw_info_bar(tm);
    }
    
    /* 2. 绘制宠物（如果宠物模块运行中） */
    if (epd_pet_is_running()) {
        epd_pet_draw_to_buffer();
    }
    
    /* 3. 刷新到墨水屏 */
    epd_display_refresh();
}

/**
 * @brief 刷新线程
 */
static VOID_T _clock_thread_func(PVOID_T args)
{
    (VOID_T)args;
    
    TAL_PR_NOTICE("EPD refresh thread started");
    
    POSIX_TM_S tm;
    UINT32_T loop_count = 0;
    
    while (!sg_clock_stop_flag) {
        tal_system_sleep(1000);
        loop_count++;
        
        memset(&tm, 0, sizeof(tm));
        OPERATE_RET ret = tal_time_get_local_time_custom(0, &tm);
        if (ret != OPRT_OK) {
            TAL_PR_WARN("Failed to get time, ret=%d", ret);
            continue;
        }
        
        /* 每10秒打印一次状态 */
        if (loop_count % 10 == 0) {
            TAL_PR_DEBUG("EPD thread alive: %04d-%02d-%02d %02d:%02d:%02d, synced=%d, last_min=%d",
                         tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
                         tm.tm_hour, tm.tm_min, tm.tm_sec,
                         sg_time_synced, sg_last_display_min);
        }
        
        /* 检查时间同步状态变化 */
        BOOL_T now_synced = _is_time_synced(&tm);
        if (!sg_time_synced && now_synced) {
            TAL_PR_NOTICE("Time synced! Year=%d, Refreshing...", tm.tm_year + 1900);
            sg_time_synced = TRUE;
            sg_last_display_min = tm.tm_min;
            _refresh_all(&tm);
            continue;
        }
        
        /* 检查分钟变化 */
        if (tm.tm_min != sg_last_display_min) {
            TAL_PR_NOTICE("Time changed: min %02d -> %02d, refreshing...", 
                          sg_last_display_min, tm.tm_min);
            sg_last_display_min = tm.tm_min;
            _refresh_all(&tm);
        }
        
        /* 检查是否有脏区域需要刷新（如宠物状态变化） */
        if (epd_display_is_dirty()) {
            TAL_PR_NOTICE("Dirty flag set, refreshing...");
            memset(&tm, 0, sizeof(tm));
            tal_time_get_local_time_custom(0, &tm);
            _refresh_all(&tm);
        }
    }
    
    TAL_PR_NOTICE("EPD refresh thread exiting");
    sg_clock_running = FALSE;
}

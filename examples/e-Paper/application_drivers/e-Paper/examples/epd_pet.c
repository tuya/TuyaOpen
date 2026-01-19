/*****************************************************************************
 * @file        epd_pet.c
 * @author      Custom
 * @brief       电子墨水屏虚拟宠物模块实现（使用统一缓冲区版本）
 * @version     1.1
 * @date        2025-01-01
 * 
 * @details     实现虚拟电子宠物的完整功能：
 *              - 状态管理（饥饿/心情/精力）
 *              - 绘制到全屏缓冲区（由epd_clock统一刷新）
 *              - App远程喂食（DP点）
 *              - 语音喂食（关键词检测）
 *              - 数据持久化（Flash存储）
 * 
 *****************************************************************************/

#include "epd_pet.h"
#include "epd_display.h"        /* 统一显示管理 */
#include "EPD_4in26.h"
#include "GUI_Paint.h"
#include "tal_thread.h"
#include "tal_system.h"
#include "tal_time_service.h"
#include "tal_log.h"
#include "tuya_ws_db.h"
#include "light_sensor.h"
#include "tuya_iot_com_api.h"   /* DP上报 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*============================================================================
                            私有宏定义
============================================================================*/
#define PET_TAG                 "PET"
#define PET_SAVE_KEY            "pet_data"
#define PET_UPDATE_INTERVAL_MS  (60 * 1000)

/*============================================================================
                            喂食关键词定义
============================================================================*/
static const CHAR_T *sg_feed_keywords[] = {
    "喂食", "吃饭", "吃东西", "喂饭", "饿了吗", "给你吃",
    "喂猫", "喂狗", "开饭", "投食", "喂它",
    "feed", "eat", "food", "hungry", "dinner", "lunch", "breakfast",
    NULL
};

static const CHAR_T *sg_play_keywords[] = {
    "摸摸", "抚摸", "玩", "陪你", "乖乖", "好可爱", "真乖",
    "play", "pet", "cute", "good",
    NULL
};

/*============================================================================
                            私有变量
============================================================================*/
static PET_DATA_T       sg_pet_data;
static THREAD_HANDLE    sg_pet_thread = NULL;
static BOOL_T           sg_pet_running = FALSE;
static BOOL_T           sg_pet_stop_flag = FALSE;
static PET_EVENT_CB     sg_event_cb = NULL;
static UINT32_T         sg_last_feed_tick = 0;
static BOOL_T           sg_initialized = FALSE;

/*============================================================================
                            私有函数声明
============================================================================*/
static VOID_T _pet_thread_func(PVOID_T args);
static VOID_T _pet_update_state(VOID_T);
static VOID_T _pet_save_data(VOID_T);
static VOID_T _pet_load_data(VOID_T);
static PET_STATE_E _pet_calculate_state(VOID_T);
static VOID_T _pet_draw_cat(UINT16_T x, UINT16_T y, PET_STATE_E state);
static VOID_T _pet_draw_status_bars(VOID_T);
static VOID_T _pet_draw_info(VOID_T);
static BOOL_T _check_keyword(CONST CHAR_T *text, CONST CHAR_T **keywords);

/*============================================================================
                            公共函数实现
============================================================================*/

/**
 * @brief 初始化虚拟宠物模块
 */
OPERATE_RET epd_pet_init(VOID_T)
{
    if (sg_initialized) {
        TAL_PR_WARN("[%s] Already initialized", PET_TAG);
        return OPRT_OK;
    }
    
    TAL_PR_NOTICE("[%s] Initializing virtual pet...", PET_TAG);
    
    _pet_load_data();
    
    sg_initialized = TRUE;
    TAL_PR_NOTICE("[%s] Pet initialized: hunger=%d, mood=%d, energy=%d, age=%d days",
                  PET_TAG, sg_pet_data.hunger, sg_pet_data.mood, 
                  sg_pet_data.energy, sg_pet_data.age_days);
    
    return OPRT_OK;
}

/**
 * @brief 启动虚拟宠物
 */
OPERATE_RET epd_pet_start(VOID_T)
{
    OPERATE_RET ret = OPRT_OK;
    
    if (sg_pet_running) {
        TAL_PR_WARN("[%s] Already running", PET_TAG);
        return OPRT_OK;
    }
    
    if (!sg_initialized) {
        ret = epd_pet_init();
        if (ret != OPRT_OK) {
            return ret;
        }
    }
    
    TAL_PR_NOTICE("[%s] Starting virtual pet...", PET_TAG);
    
    sg_pet_running = TRUE;
    
    /* 创建状态更新线程（只更新状态，不刷新显示） */
    sg_pet_stop_flag = FALSE;
    THREAD_CFG_T thread_cfg = {
        .stackDepth = 2048,
        .priority = THREAD_PRIO_4,
        .thrdname = "epd_pet_state"
    };
    
    ret = tal_thread_create_and_start(&sg_pet_thread, NULL, NULL,
                                      _pet_thread_func, NULL, &thread_cfg);
    if (ret != OPRT_OK) {
        TAL_PR_WARN("[%s] State thread create failed, ret=%d", PET_TAG, ret);
    }
    
    /* 标记需要刷新 */
    epd_display_mark_dirty();
    
    TAL_PR_NOTICE("[%s] Virtual pet started!", PET_TAG);
    return OPRT_OK;
}

/**
 * @brief 停止虚拟宠物
 */
OPERATE_RET epd_pet_stop(VOID_T)
{
    if (!sg_pet_running) {
        return OPRT_OK;
    }
    
    TAL_PR_NOTICE("[%s] Stopping virtual pet...", PET_TAG);
    
    _pet_save_data();
    
    sg_pet_stop_flag = TRUE;
    UINT32_T timeout = 0;
    while (sg_pet_running && timeout < 50) {
        tal_system_sleep(100);
        timeout++;
    }
    
    if (sg_pet_thread) {
        tal_thread_delete(sg_pet_thread);
        sg_pet_thread = NULL;
    }
    
    sg_pet_running = FALSE;
    TAL_PR_NOTICE("[%s] Virtual pet stopped", PET_TAG);
    
    return OPRT_OK;
}

/**
 * @brief 绘制宠物到全屏缓冲区（由epd_clock调用）
 */
VOID_T epd_pet_draw_to_buffer(VOID_T)
{
    if (!sg_pet_running) {
        return;
    }
    
    UBYTE *buffer = epd_display_get_buffer();
    if (buffer == NULL) {
        return;
    }
    
    /* 选择全屏画布 */
    epd_display_select_canvas();
    
    /* 清空宠物区域 */
    Paint_ClearWindows(EPD_PET_AREA_X, EPD_PET_AREA_Y, 
                       EPD_PET_AREA_X + EPD_PET_AREA_WIDTH, 
                       EPD_PET_AREA_Y + EPD_PET_AREA_HEIGHT, WHITE);
    
    /* 绘制边框 */
    Paint_DrawRectangle(EPD_PET_AREA_X + 5, EPD_PET_AREA_Y + 5, 
                        EPD_PET_AREA_X + EPD_PET_AREA_WIDTH - 6, 
                        EPD_PET_AREA_Y + EPD_PET_AREA_HEIGHT - 6, 
                        BLACK, DOT_PIXEL_1X1, DRAW_FILL_EMPTY);
    
    /* 绘制宠物（左侧区域，避免与状态条重叠） */
    _pet_draw_cat(EPD_PET_AREA_X + 150, EPD_PET_AREA_Y + 180, sg_pet_data.state);
    
    /* 绘制状态条（右侧） */
    _pet_draw_status_bars();
    
    /* 绘制信息（底部） */
    _pet_draw_info();
}

/**
 * @brief 喂食
 */
OPERATE_RET epd_pet_feed(VOID_T)
{
    UINT32_T now = tal_system_get_millisecond();
    
    if (now - sg_last_feed_tick < PET_FEED_COOLDOWN_MS) {
        UINT32_T remaining = (PET_FEED_COOLDOWN_MS - (now - sg_last_feed_tick)) / 1000;
        TAL_PR_WARN("[%s] Feed cooldown, please wait %d seconds", PET_TAG, remaining);
        
        /* 冷却中也触发全屏刷新，显示最新倒计时 */
        epd_display_mark_dirty();
        
        return OPRT_RESOURCE_NOT_READY;
    }
    
    TAL_PR_NOTICE("[%s] Feeding pet!", PET_TAG);
    
    sg_pet_data.hunger += PET_FEED_HUNGER_GAIN;
    if (sg_pet_data.hunger > 100) sg_pet_data.hunger = 100;
    
    sg_pet_data.mood += PET_FEED_MOOD_GAIN;
    if (sg_pet_data.mood > 100) sg_pet_data.mood = 100;
    
    sg_pet_data.energy += PET_FEED_ENERGY_GAIN;
    if (sg_pet_data.energy > 100) sg_pet_data.energy = 100;
    
    sg_pet_data.feed_count++;
    sg_pet_data.last_feed_time = tal_time_get_posix();
    sg_last_feed_tick = now;
    
    sg_pet_data.state = PET_STATE_EATING;
    
    _pet_save_data();
    
    /* 标记需要刷新 */
    epd_display_mark_dirty();
    
    /* 立即上报状态到App */
    epd_pet_report_status();
    
    if (sg_event_cb) {
        sg_event_cb(&sg_pet_data);
    }
    
    TAL_PR_NOTICE("[%s] Fed! hunger=%d, mood=%d, energy=%d, total_feeds=%d",
                  PET_TAG, sg_pet_data.hunger, sg_pet_data.mood, sg_pet_data.energy, sg_pet_data.feed_count);
    
    return OPRT_OK;
}

/**
 * @brief 互动/抚摸
 */
OPERATE_RET epd_pet_play(VOID_T)
{
    TAL_PR_NOTICE("[%s] Playing with pet!", PET_TAG);
    
    sg_pet_data.mood += PET_PLAY_MOOD_GAIN;
    if (sg_pet_data.mood > 100) sg_pet_data.mood = 100;
    
    if (sg_pet_data.energy > PET_PLAY_ENERGY_COST) {
        sg_pet_data.energy -= PET_PLAY_ENERGY_COST;
    } else {
        sg_pet_data.energy = 0;
    }
    
    sg_pet_data.play_count++;
    sg_pet_data.last_play_time = tal_time_get_posix();
    
    sg_pet_data.state = PET_STATE_PLAYING;
    
    _pet_save_data();
    
    /* 标记需要刷新 */
    epd_display_mark_dirty();
    
    /* 立即上报状态到App */
    epd_pet_report_status();
    
    TAL_PR_NOTICE("[%s] Played! mood=%d, energy=%d, total_plays=%d",
                  PET_TAG, sg_pet_data.mood, sg_pet_data.energy, sg_pet_data.play_count);
    
    return OPRT_OK;
}

/**
 * @brief 处理DP点指令
 */
BOOL_T epd_pet_dp_handler(UINT8_T dpid, INT_T value)
{
    switch (dpid) {
        case DP_PET_FEED:
            if (value) {
                TAL_PR_NOTICE("[%s] DP feed command received!", PET_TAG);
                epd_pet_feed();
            }
            return TRUE;
            
        case DP_PET_PLAY:
            if (value) {
                TAL_PR_NOTICE("[%s] DP play command received!", PET_TAG);
                epd_pet_play();
            }
            return TRUE;
            
        default:
            return FALSE;
    }
}

/**
 * @brief 处理语音识别结果
 */
BOOL_T epd_pet_voice_handler(CONST CHAR_T *asr_text)
{
    if (asr_text == NULL || strlen(asr_text) == 0) {
        return FALSE;
    }
    
    TAL_PR_DEBUG("[%s] Checking voice: %s", PET_TAG, asr_text);
    
    if (_check_keyword(asr_text, sg_feed_keywords)) {
        TAL_PR_NOTICE("[%s] Voice feed command detected: %s", PET_TAG, asr_text);
        epd_pet_feed();
        return TRUE;
    }
    
    if (_check_keyword(asr_text, sg_play_keywords)) {
        TAL_PR_NOTICE("[%s] Voice play command detected: %s", PET_TAG, asr_text);
        epd_pet_play();
        return TRUE;
    }
    
    return FALSE;
}

/**
 * @brief 获取当前宠物数据
 */
CONST PET_DATA_T* epd_pet_get_data(VOID_T)
{
    return &sg_pet_data;
}

/**
 * @brief 强制刷新显示
 */
OPERATE_RET epd_pet_refresh(VOID_T)
{
    if (!sg_pet_running) {
        return OPRT_COM_ERROR;
    }
    
    epd_display_mark_dirty();
    return OPRT_OK;
}

/**
 * @brief 设置事件回调
 */
VOID_T epd_pet_set_callback(PET_EVENT_CB cb)
{
    sg_event_cb = cb;
}

/**
 * @brief 检查是否运行中
 */
BOOL_T epd_pet_is_running(VOID_T)
{
    return sg_pet_running;
}

/**
 * @brief 上报宠物状态到App
 * @return OPRT_OK: 成功, 其他: 失败
 */
OPERATE_RET epd_pet_report_status(VOID_T)
{
    CHAR_T *devid = tuya_iot_get_gw_id();
    if (devid == NULL) {
        TAL_PR_WARN("[%s] Device not online, skip report", PET_TAG);
        return OPRT_COM_ERROR;
    }
    
    /* 上报3个DP点：饥饿度(103)、心情(104)、状态(105) */
    TY_OBJ_DP_S dps[3];
    
    /* DP 103: 饥饿度 */
    dps[0].dpid = DP_PET_HUNGER;
    dps[0].type = PROP_VALUE;
    dps[0].time_stamp = 0;
    dps[0].value.dp_value = sg_pet_data.hunger;
    
    /* DP 104: 心情 */
    dps[1].dpid = DP_PET_MOOD;
    dps[1].type = PROP_VALUE;
    dps[1].time_stamp = 0;
    dps[1].value.dp_value = sg_pet_data.mood;
    
    /* DP 105: 状态枚举 */
    dps[2].dpid = DP_PET_STATE;
    dps[2].type = PROP_ENUM;
    dps[2].time_stamp = 0;
    dps[2].value.dp_enum = (UINT_T)sg_pet_data.state;
    
    OPERATE_RET ret = tuya_report_dp_async(devid, dps, 3, NULL);
    
    if (ret == OPRT_OK) {
        TAL_PR_NOTICE("[%s] DP reported: hunger=%d, mood=%d, state=%d",
                      PET_TAG, sg_pet_data.hunger, sg_pet_data.mood, sg_pet_data.state);
    } else {
        TAL_PR_ERR("[%s] DP report failed: %d", PET_TAG, ret);
    }
    
    return ret;
}

/*============================================================================
                            私有函数实现
============================================================================*/

/**
 * @brief 宠物状态更新线程（只更新状态，不刷新显示）
 */
static VOID_T _pet_thread_func(PVOID_T args)
{
    (VOID_T)args;
    
    TAL_PR_NOTICE("[%s] Pet state thread started", PET_TAG);
    
    UINT32_T last_update = 0;
    UINT32_T now;
    
    while (!sg_pet_stop_flag) {
        tal_system_sleep(1000);
        now = tal_system_get_millisecond();
        
        /* 每分钟更新状态 */
        if (now - last_update >= PET_UPDATE_INTERVAL_MS) {
            last_update = now;
            PET_STATE_E old_state = sg_pet_data.state;
            _pet_update_state();
            _pet_save_data();
            
            /* 如果状态变化了，触发屏幕刷新 */
            if (sg_pet_data.state != old_state) {
                TAL_PR_NOTICE("[%s] State changed: %d -> %d, refreshing display",
                              PET_TAG, old_state, sg_pet_data.state);
                epd_display_mark_dirty();
            }
        }
    }
    
    TAL_PR_NOTICE("[%s] Pet state thread exiting", PET_TAG);
    sg_pet_running = FALSE;
}

/**
 * @brief 更新宠物状态
 */
static VOID_T _pet_update_state(VOID_T)
{
    POSIX_TM_S tm;
    memset(&tm, 0, sizeof(tm));
    tal_time_get_local_time_custom(0, &tm);
    
    UINT8_T light = light_sensor_get_light_percent();
    BOOL_T is_night = (tm.tm_hour >= 22 || tm.tm_hour < 6);
    
    static UINT8_T minute_counter = 0;
    minute_counter++;
    
    if (minute_counter >= 60) {
        minute_counter = 0;
        
        if (sg_pet_data.hunger > PET_HUNGER_DECAY_PER_HOUR) {
            sg_pet_data.hunger -= PET_HUNGER_DECAY_PER_HOUR;
        } else {
            sg_pet_data.hunger = 0;
        }
        
        if (sg_pet_data.mood > PET_MOOD_DECAY_PER_HOUR) {
            sg_pet_data.mood -= PET_MOOD_DECAY_PER_HOUR;
        } else {
            sg_pet_data.mood = 0;
        }
        
        if (is_night) {
            sg_pet_data.energy += PET_ENERGY_GAIN_NIGHT;
            if (sg_pet_data.energy > 100) sg_pet_data.energy = 100;
        } else {
            if (sg_pet_data.energy > PET_ENERGY_DECAY_DAY) {
                sg_pet_data.energy -= PET_ENERGY_DECAY_DAY;
            } else {
                sg_pet_data.energy = 0;
            }
        }
        
        UINT32_T now = tal_time_get_posix();
        sg_pet_data.age_days = (now - sg_pet_data.birth_time) / (24 * 3600);
        
        TAL_PR_DEBUG("[%s] Hourly update: hunger=%d, mood=%d, energy=%d",
                     PET_TAG, sg_pet_data.hunger, sg_pet_data.mood, sg_pet_data.energy);
    }
    
    if (is_night && light < 20) {
        sg_pet_data.state = PET_STATE_SLEEPING;
    } else {
        sg_pet_data.state = _pet_calculate_state();
    }
}

/**
 * @brief 计算宠物状态
 */
static PET_STATE_E _pet_calculate_state(VOID_T)
{
    UINT32_T now = tal_time_get_posix();
    
    if (sg_pet_data.state == PET_STATE_EATING) {
        if (now - sg_pet_data.last_feed_time > 60) {
            /* 吃完了 */
        } else {
            return PET_STATE_EATING;
        }
    }
    if (sg_pet_data.state == PET_STATE_PLAYING) {
        if (now - sg_pet_data.last_play_time > 60) {
            /* 玩完了 */
        } else {
            return PET_STATE_PLAYING;
        }
    }
    
    if (sg_pet_data.hunger < 30) {
        return PET_STATE_HUNGRY;
    }
    
    if (sg_pet_data.energy < 20) {
        return PET_STATE_SLEEPY;
    }
    
    if (sg_pet_data.mood < 30) {
        return PET_STATE_SAD;
    }
    
    if (sg_pet_data.hunger >= 70 && sg_pet_data.mood >= 70 && sg_pet_data.energy >= 50) {
        return PET_STATE_HAPPY;
    }
    
    return PET_STATE_NORMAL;
}

/**
 * @brief 绘制猫咪
 */
static VOID_T _pet_draw_cat(UINT16_T x, UINT16_T y, PET_STATE_E state)
{
    const CHAR_T *line1, *line2, *line3, *line4, *line5, *dialog;
    
    switch (state) {
        case PET_STATE_HAPPY:
            line1 = "  /\\_/\\  ";
            line2 = " ( ^.^ ) ";
            line3 = "  > ^ <  ";
            line4 = " /|   |\\ ";
            line5 = "(_|   |_)";
            dialog = "I'm so happy! ^_^";
            break;
            
        case PET_STATE_HUNGRY:
            line1 = "  /\\_/\\  ";
            line2 = " ( T.T ) ";
            line3 = "  > o <  ";
            line4 = " /|   |\\ ";
            line5 = "(_|   |_)";
            dialog = "I'm hungry... Feed me!";
            break;
            
        case PET_STATE_EATING:
            line1 = "  /\\_/\\  ";
            line2 = " ( @.@ ) ";
            line3 = "  >mmm<  ";
            line4 = " /|   |\\ ";
            line5 = "(_|   |_)";
            dialog = "Yummy! *nom nom*";
            break;
            
        case PET_STATE_SLEEPY:
            line1 = "  /\\_/\\  ";
            line2 = " ( -.- ) ";
            line3 = "  > ~ <  ";
            line4 = " /|   |\\ ";
            line5 = "(_|   |_)";
            dialog = "So tired... zzZ";
            break;
            
        case PET_STATE_SLEEPING:
            line1 = "  /\\_/\\  ";
            line2 = " ( -.- ) ";
            line3 = "  > ~ <  ";
            line4 = " /|   |\\ ";
            line5 = "(_|___|_)";
            dialog = "zzZ... Sleeping...";
            break;
            
        case PET_STATE_PLAYING:
            line1 = "  /\\_/\\  ";
            line2 = " ( >w< ) ";
            line3 = "  > v <  ";
            line4 = " \\|   |/ ";
            line5 = " (_\\_/_) ";
            dialog = "Let's play! Wheee~";
            break;
            
        case PET_STATE_SAD:
            line1 = "  /\\_/\\  ";
            line2 = " ( ;_; ) ";
            line3 = "  > n <  ";
            line4 = " /|   |\\ ";
            line5 = "(_|   |_)";
            dialog = "I feel lonely...";
            break;
            
        default:
            line1 = "  /\\_/\\  ";
            line2 = " ( o.o ) ";
            line3 = "  > ^ <  ";
            line4 = " /|   |\\ ";
            line5 = "(_|   |_)";
            dialog = "Meow~";
            break;
    }
    
    Paint_DrawString_EN(x - 60, y - 80, line1, &Font24, WHITE, BLACK);
    Paint_DrawString_EN(x - 60, y - 50, line2, &Font24, WHITE, BLACK);
    Paint_DrawString_EN(x - 60, y - 20, line3, &Font24, WHITE, BLACK);
    Paint_DrawString_EN(x - 60, y + 10, line4, &Font24, WHITE, BLACK);
    Paint_DrawString_EN(x - 60, y + 40, line5, &Font24, WHITE, BLACK);
    
    /* 对话气泡 */
    Paint_DrawRectangle(x + 80, y - 60, x + 350, y, BLACK, DOT_PIXEL_1X1, DRAW_FILL_EMPTY);
    Paint_DrawString_EN(x + 90, y - 45, dialog, &Font20, WHITE, BLACK);
}

/**
 * @brief 绘制状态条
 */
static VOID_T _pet_draw_status_bars(VOID_T)
{
    UINT16_T bar_x = EPD_PET_AREA_X + 500;
    UINT16_T bar_y = EPD_PET_AREA_Y + 80;
    UINT16_T bar_width = 200;
    UINT16_T bar_height = 20;
    UINT16_T bar_spacing = 50;
    
    CHAR_T percent_str[16];
    
    /* 饥饿度 */
    Paint_DrawString_EN(bar_x, bar_y, "Hunger:", &Font20, WHITE, BLACK);
    Paint_DrawRectangle(bar_x, bar_y + 25, bar_x + bar_width, bar_y + 25 + bar_height, 
                        BLACK, DOT_PIXEL_1X1, DRAW_FILL_EMPTY);
    UINT16_T hunger_fill = (sg_pet_data.hunger * bar_width) / 100;
    if (hunger_fill > 0) {
        Paint_DrawRectangle(bar_x + 2, bar_y + 27, bar_x + 2 + hunger_fill, bar_y + 23 + bar_height, 
                            BLACK, DOT_PIXEL_1X1, DRAW_FILL_FULL);
    }
    snprintf(percent_str, sizeof(percent_str), "%d%%", sg_pet_data.hunger);
    Paint_DrawString_EN(bar_x + bar_width + 10, bar_y + 25, percent_str, &Font16, WHITE, BLACK);
    
    /* 心情 */
    bar_y += bar_spacing + bar_height;
    Paint_DrawString_EN(bar_x, bar_y, "Mood:", &Font20, WHITE, BLACK);
    Paint_DrawRectangle(bar_x, bar_y + 25, bar_x + bar_width, bar_y + 25 + bar_height, 
                        BLACK, DOT_PIXEL_1X1, DRAW_FILL_EMPTY);
    UINT16_T mood_fill = (sg_pet_data.mood * bar_width) / 100;
    if (mood_fill > 0) {
        Paint_DrawRectangle(bar_x + 2, bar_y + 27, bar_x + 2 + mood_fill, bar_y + 23 + bar_height, 
                            BLACK, DOT_PIXEL_1X1, DRAW_FILL_FULL);
    }
    snprintf(percent_str, sizeof(percent_str), "%d%%", sg_pet_data.mood);
    Paint_DrawString_EN(bar_x + bar_width + 10, bar_y + 25, percent_str, &Font16, WHITE, BLACK);
    
    /* 精力 */
    bar_y += bar_spacing + bar_height;
    Paint_DrawString_EN(bar_x, bar_y, "Energy:", &Font20, WHITE, BLACK);
    Paint_DrawRectangle(bar_x, bar_y + 25, bar_x + bar_width, bar_y + 25 + bar_height, 
                        BLACK, DOT_PIXEL_1X1, DRAW_FILL_EMPTY);
    UINT16_T energy_fill = (sg_pet_data.energy * bar_width) / 100;
    if (energy_fill > 0) {
        Paint_DrawRectangle(bar_x + 2, bar_y + 27, bar_x + 2 + energy_fill, bar_y + 23 + bar_height, 
                            BLACK, DOT_PIXEL_1X1, DRAW_FILL_FULL);
    }
    snprintf(percent_str, sizeof(percent_str), "%d%%", sg_pet_data.energy);
    Paint_DrawString_EN(bar_x + bar_width + 10, bar_y + 25, percent_str, &Font16, WHITE, BLACK);
}

/**
 * @brief 绘制底部信息
 */
static VOID_T _pet_draw_info(VOID_T)
{
    CHAR_T info_str[128];
    UINT32_T now = tal_system_get_millisecond();
    UINT32_T elapsed = now - sg_last_feed_tick;
    
    /* 统计信息 + 冷却状态，同一行 */
    if (elapsed < PET_FEED_COOLDOWN_MS) {
        UINT32_T remaining_sec = (PET_FEED_COOLDOWN_MS - elapsed) / 1000;
        snprintf(info_str, sizeof(info_str), 
                 "Age:%d  Fed:%d  Played:%d  |  Cooldown:%ds",
                 sg_pet_data.age_days, sg_pet_data.feed_count, 
                 sg_pet_data.play_count, remaining_sec);
    } else {
        snprintf(info_str, sizeof(info_str), 
                 "Age:%d  Fed:%d  Played:%d  |  Feed Ready!",
                 sg_pet_data.age_days, sg_pet_data.feed_count, 
                 sg_pet_data.play_count);
    }
    
    Paint_DrawString_EN(EPD_PET_AREA_X + 20, EPD_PET_AREA_Y + EPD_PET_AREA_HEIGHT - 30, 
                        info_str, &Font16, WHITE, BLACK);
}

/**
 * @brief 保存宠物数据
 */
static VOID_T _pet_save_data(VOID_T)
{
    sg_pet_data.last_update_time = tal_time_get_posix();
    
    OPERATE_RET ret = wd_common_write(PET_SAVE_KEY, (BYTE_T *)&sg_pet_data, sizeof(PET_DATA_T));
    if (ret != OPRT_OK) {
        TAL_PR_WARN("[%s] Save data failed, ret=%d", PET_TAG, ret);
    } else {
        TAL_PR_DEBUG("[%s] Data saved", PET_TAG);
    }
}

/**
 * @brief 加载宠物数据
 */
static VOID_T _pet_load_data(VOID_T)
{
    BYTE_T *data = NULL;
    UINT_T len = 0;
    OPERATE_RET ret = wd_common_read(PET_SAVE_KEY, &data, &len);
    
    if (ret != OPRT_OK || data == NULL || len != sizeof(PET_DATA_T)) {
        TAL_PR_NOTICE("[%s] No save data, creating new pet!", PET_TAG);
        memset(&sg_pet_data, 0, sizeof(PET_DATA_T));
        sg_pet_data.hunger = 80;
        sg_pet_data.mood = 80;
        sg_pet_data.energy = 100;
        sg_pet_data.state = PET_STATE_HAPPY;
        sg_pet_data.birth_time = tal_time_get_posix();
        sg_pet_data.age_days = 0;
        _pet_save_data();
    } else {
        memcpy(&sg_pet_data, data, sizeof(PET_DATA_T));
        
        /* 检查 birth_time 有效性（应该大于 2024-01-01 的时间戳 1704067200） */
        UINT32_T min_valid_time = 1704067200;  /* 2024-01-01 00:00:00 UTC */
        if (sg_pet_data.birth_time < min_valid_time) {
            TAL_PR_WARN("[%s] Invalid birth_time=%u, resetting pet!", PET_TAG, sg_pet_data.birth_time);
            sg_pet_data.birth_time = tal_time_get_posix();
            sg_pet_data.age_days = 0;
            sg_pet_data.feed_count = 0;
            sg_pet_data.play_count = 0;
            _pet_save_data();
        }
        
        TAL_PR_NOTICE("[%s] Loaded save data, pet age=%d days", PET_TAG, sg_pet_data.age_days);
    }
    
    if (data != NULL) {
        wd_common_free_data(data);
    }
}

/**
 * @brief 检查关键词
 */
static BOOL_T _check_keyword(CONST CHAR_T *text, CONST CHAR_T **keywords)
{
    if (text == NULL || keywords == NULL) {
        return FALSE;
    }
    
    for (INT_T i = 0; keywords[i] != NULL; i++) {
        if (strstr(text, keywords[i]) != NULL) {
            return TRUE;
        }
    }
    
    return FALSE;
}


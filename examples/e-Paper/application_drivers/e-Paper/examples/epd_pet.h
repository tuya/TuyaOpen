/*****************************************************************************
 * @file        epd_pet.h
 * @author      Custom
 * @brief       电子墨水屏虚拟宠物模块
 * @version     1.0
 * @date        2025-01-01
 * 
 * @details     功能概述：
 *              在墨水屏下方区域显示一只可爱的虚拟电子宠物，具备：
 *              - 饥饿度、心情、精力等状态属性
 *              - 多种状态表情（开心/饥饿/困倦/睡觉等）
 *              - App远程喂食（通过DP点）
 *              - 语音喂食（说"喂食"/"吃饭"/"feed"等）
 *              - 光照感应（根据光照变化状态）
 *              
 *              【状态系统】
 *              - 饥饿度: 0-100，每小时-5，喂食+25
 *              - 心情:   0-100，每小时-2，互动+15
 *              - 精力:   0-100，白天-3/h，晚上+5/h
 *              
 *              【喂食方式】
 *              1. App下发DP点 101
 *              2. 语音说"喂食"/"吃饭"/"feed"等关键词
 *              3. 定时喂食提醒
 * 
 *****************************************************************************/
#ifndef __EPD_PET_H__
#define __EPD_PET_H__

#include "tuya_cloud_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
                            DP点定义
============================================================================*/
/**
 * @brief 宠物相关DP点ID
 * @note 需要在涂鸦IoT平台添加对应的DP点
 */
#define DP_PET_FEED         101     /**< 喂食指令 (bool: true=喂食一次) */
#define DP_PET_PLAY         102     /**< 互动指令 (bool: true=互动一次) */
#define DP_PET_HUNGER       103     /**< 饥饿度上报 (value: 0-100) */
#define DP_PET_MOOD         104     /**< 心情上报 (value: 0-100) */
#define DP_PET_STATE        105     /**< 状态上报 (enum: 0=开心,1=普通,2=饥饿,3=困倦,4=睡觉) */

/*============================================================================
                            配置参数
============================================================================*/
/**
 * @brief 宠物显示区域（与 epd_display.h 保持一致）
 * @note 墨水屏分辨率：800 × 480
 *       信息栏占用：Y=0~120（视觉上方，高度120）
 *       宠物区域：Y=140~480（视觉下方，高度340）
 */
#define EPD_PET_X           0       /**< 宠物区域X坐标 */
#define EPD_PET_Y           140     /**< 宠物区域Y坐标（信息栏下方，留20像素间距）*/
#define EPD_PET_WIDTH       800     /**< 宠物区域宽度 */
#define EPD_PET_HEIGHT      340     /**< 宠物区域高度 */

/**
 * @brief 状态变化速率（每小时变化量）
 */
#define PET_HUNGER_DECAY_PER_HOUR   5   /**< 饥饿度每小时下降 */
#define PET_MOOD_DECAY_PER_HOUR     2   /**< 心情每小时下降 */
#define PET_ENERGY_DECAY_DAY        3   /**< 精力白天每小时下降 */
#define PET_ENERGY_GAIN_NIGHT       5   /**< 精力夜晚每小时恢复 */

/**
 * @brief 喂食/互动效果
 */
#define PET_FEED_HUNGER_GAIN        25  /**< 喂食增加饥饿度 */
#define PET_FEED_MOOD_GAIN          10  /**< 喂食增加心情 */
#define PET_FEED_ENERGY_GAIN        5   /**< 喂食增加精力 */
#define PET_PLAY_MOOD_GAIN          15  /**< 互动增加心情 */
#define PET_PLAY_ENERGY_COST        5   /**< 互动消耗精力 */

/**
 * @brief 喂食冷却时间（防止刷喂食）
 */
#define PET_FEED_COOLDOWN_MS        (30 * 1000)  /**< 喂食冷却30秒 */

/*============================================================================
                            数据类型定义
============================================================================*/

/**
 * @brief 宠物状态枚举
 */
typedef enum {
    PET_STATE_HAPPY = 0,    /**< 😊 开心 - 所有属性良好 */
    PET_STATE_NORMAL,       /**< 😐 普通 - 属性一般 */
    PET_STATE_HUNGRY,       /**< 😿 饥饿 - 饥饿度低 */
    PET_STATE_SLEEPY,       /**< 😴 困倦 - 精力低 */
    PET_STATE_SLEEPING,     /**< 💤 睡觉 - 夜间休息 */
    PET_STATE_EATING,       /**< 😋 吃东西 - 刚喂食后 */
    PET_STATE_PLAYING,      /**< 😸 玩耍 - 互动中 */
    PET_STATE_SAD,          /**< 😢 难过 - 心情低 */
    PET_STATE_MAX
} PET_STATE_E;

/**
 * @brief 宠物数据结构
 */
typedef struct {
    UINT8_T hunger;             /**< 饥饿度 0-100 (0=饿死, 100=吃饱) */
    UINT8_T mood;               /**< 心情 0-100 (0=抑郁, 100=开心) */
    UINT8_T energy;             /**< 精力 0-100 (0=疲惫, 100=精神) */
    PET_STATE_E state;          /**< 当前状态 */
    UINT32_T last_feed_time;    /**< 上次喂食时间戳 */
    UINT32_T last_play_time;    /**< 上次互动时间戳 */
    UINT32_T last_update_time;  /**< 上次状态更新时间戳 */
    UINT32_T birth_time;        /**< 宠物"出生"时间 */
    UINT16_T age_days;          /**< 宠物年龄（天） */
    UINT16_T feed_count;        /**< 累计喂食次数 */
    UINT16_T play_count;        /**< 累计互动次数 */
} PET_DATA_T;

/**
 * @brief 喂食事件回调函数类型
 * @param data 宠物当前数据
 */
typedef VOID_T (*PET_EVENT_CB)(PET_DATA_T *data);

/*============================================================================
                            函数声明
============================================================================*/

/**
 * @brief 初始化虚拟宠物模块
 * @return OPRT_OK: 成功, 其他: 失败
 * 
 * @note 会自动从Flash加载保存的宠物数据
 *       如果没有存档，会创建新宠物
 */
OPERATE_RET epd_pet_init(VOID_T);

/**
 * @brief 启动虚拟宠物（开始显示和状态更新）
 * @return OPRT_OK: 成功, 其他: 失败
 */
OPERATE_RET epd_pet_start(VOID_T);

/**
 * @brief 停止虚拟宠物
 * @return OPRT_OK: 成功, 其他: 失败
 */
OPERATE_RET epd_pet_stop(VOID_T);

/**
 * @brief 喂食
 * @return OPRT_OK: 成功, OPRT_RESOURCE_NOT_READY: 冷却中
 * 
 * @note 可被App/语音/定时调用
 */
OPERATE_RET epd_pet_feed(VOID_T);

/**
 * @brief 互动/抚摸
 * @return OPRT_OK: 成功
 */
OPERATE_RET epd_pet_play(VOID_T);

/**
 * @brief 处理DP点指令
 * @param dpid DP点ID
 * @param value DP点值
 * @return TRUE: 已处理, FALSE: 非宠物相关DP
 * 
 * @note 在 ty_ai_toy_dp_cmd_cb() 中调用
 */
BOOL_T epd_pet_dp_handler(UINT8_T dpid, INT_T value);

/**
 * @brief 处理语音识别结果，检测喂食关键词
 * @param asr_text 语音识别文字
 * @return TRUE: 检测到喂食指令, FALSE: 未检测到
 * 
 * @note 在ASR结果回调中调用，检测"喂食"/"吃饭"/"feed"等关键词
 */
BOOL_T epd_pet_voice_handler(CONST CHAR_T *asr_text);

/**
 * @brief 获取当前宠物数据
 * @return 宠物数据指针（只读）
 */
CONST PET_DATA_T* epd_pet_get_data(VOID_T);

/**
 * @brief 强制刷新显示
 * @return OPRT_OK: 成功
 */
OPERATE_RET epd_pet_refresh(VOID_T);

/**
 * @brief 绘制宠物到全屏缓冲区
 * @note 由 epd_clock 模块调用，统一刷新时使用
 */
VOID_T epd_pet_draw_to_buffer(VOID_T);

/**
 * @brief 设置事件回调
 * @param cb 回调函数
 */
VOID_T epd_pet_set_callback(PET_EVENT_CB cb);

/**
 * @brief 检查宠物是否在运行
 * @return TRUE: 运行中, FALSE: 未运行
 */
BOOL_T epd_pet_is_running(VOID_T);

/**
 * @brief 上报宠物状态到App
 * @return OPRT_OK: 成功, 其他: 失败
 * 
 * @note 上报 DP 103(饥饿度)、104(心情)、105(状态)
 */
OPERATE_RET epd_pet_report_status(VOID_T);

#ifdef __cplusplus
}
#endif

#endif /* __EPD_PET_H__ */


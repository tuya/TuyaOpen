#ifndef __TUYA_AI_TOY_H__
#define __TUYA_AI_TOY_H__

#include "tuya_cloud_types.h"
#include "tkl_audio.h"
#include "ty_cJSON.h"
#include "tuya_ai_debug.h"

typedef enum {
    TY_AI_TRIGGER_MODE_HOLD,        // 长按触发模式
    TY_AI_TRIGGER_MODE_ONE_SHOT,    // 单次按键，回合制对话模式
    TY_AI_TRIGGER_MODE_WAKEUP,      // 关键词唤醒模式
    TY_AI_TRIGGER_MODE_FREE,        // 关键词唤醒和自由对话模式
} TY_AI_TRIGGER_MODE_E;

typedef enum {
    TY_AI_NET_STATUS_UNCFG = 0,
    TY_AI_NET_STATUS_DISCONNECTED,
    TY_AI_NET_STATUS_CONNECTED,
    TY_AI_NET_STATUS_CONNECTING,
    TY_AI_NET_STATUS_DISCONNECTING,
    TY_AI_NET_STATUS_MAX,
} TY_AI_NET_STATUS_E;


typedef struct {
    TUYA_GPIO_NUM_E         spk_en_pin;         // speaker enable pin
    TUYA_GPIO_NUM_E         led_pin;            // led pin
    TUYA_GPIO_NUM_E         audio_trigger_pin;  // audio trigger pin
    TY_AI_TRIGGER_MODE_E    trigger_mode;       // trigger mode, 0: hold trigger mode, 1: one shot trigger mode, 2: free conversation trigger mode
} TY_AI_TOY_CFG_T;


// toy alert type
typedef enum {
    TOY_ALERT_TYPE_NORMAL = 0,
    TOY_ALERT_TYPE_POWER_ON,             /* 开机上电播报 */
    TOY_ALERT_TYPE_NOT_ACTIVE,           /* 还未配网 请先配网 */
    TOY_ALERT_TYPE_NETWORK_CFG,          /* 进入配网状态，开始配网 */
    TOY_ALERT_TYPE_NETWORK_CONNECTED,    /* 联网成功 */
    TOY_ALERT_TYPE_NETWORK_FAIL,         /* 联网失败 重试 */
    TOY_ALERT_TYPE_NETWORK_DISCONNECT,   /* 掉网 */
    TOY_ALERT_TYPE_BATTERY_LOW,          /* 电量不足 */
    TOY_ALERT_TYPE_PLEASE_AGAIN,         /* 请再说一遍 */
    TOY_ALART_TYPE_WAKEUP,               /* 你好我在*/
    TOY_ALART_TYPE_LONG_KEY_TALK,        /* 长按键对话 */
    TOY_ALART_TYPE_KEY_TALK,             /* 按键对话 */
    TOY_ALART_TYPE_WAKEUP_TALK,          /* 唤醒对话 */
    TOY_ALART_TYPE_RANDOM_TALK,          /* 任意对话 */
    TOY_ALERT_TYPE_MAX,
} TY_AI_TOY_ALERT_TYPE;


/**
 * @brief AI toy init
 * 
 * @return int 0: success, -1: fail
 */
OPERATE_RET ty_ai_toy_init(TY_AI_TOY_CFG_T *cfg);

OPERATE_RET ty_ai_toy_alert(TY_AI_TOY_ALERT_TYPE type, BOOL_T send_eof);

#endif /* __TUYA_AI_TOY_H__ */

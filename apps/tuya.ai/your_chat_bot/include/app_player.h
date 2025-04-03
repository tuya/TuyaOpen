/**
 * @file app_player.h
 * @brief app_player module is used to
 * @version 0.1
 * @date 2025-03-25
 */

#ifndef __APP_PLAYER_H__
#define __APP_PLAYER_H__

#include "tuya_cloud_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/***********************************************************
************************macro define************************
***********************************************************/

/***********************************************************
***********************typedef define***********************
***********************************************************/
typedef enum {
    APP_PLAYER_STAT_IDLE = 0,
    APP_PLAYER_STAT_START,
    APP_PLAYER_STAT_PLAY,
    APP_PLAYER_STAT_STOP,
    APP_PLAYER_STAT_MAX,
} APP_PLAYER_STATE;

typedef enum {
    APP_ALERT_TYPE_NORMAL = 0,
    APP_ALERT_TYPE_POWER_ON,
    APP_ALERT_TYPE_NOT_ACTIVE,
    APP_ALERT_TYPE_NETWORK_CFG,
    APP_ALERT_TYPE_NETWORK_CONNECTED,
    APP_ALERT_TYPE_NETWORK_FAIL,
    APP_ALERT_TYPE_NETWORK_DISCONNECT,
    APP_ALERT_TYPE_BATTERY_LOW,
    APP_ALERT_TYPE_PLEASE_AGAIN,
    APP_ALERT_TYPE_MAX,
} APP_ALERT_TYPE;

/***********************************************************
********************function declaration********************
***********************************************************/

OPERATE_RET app_player_init(void);

OPERATE_RET app_player_start(void);

OPERATE_RET app_player_data_write(uint8_t *data, uint32_t len, uint8_t is_eof);

OPERATE_RET app_player_stop(void);

OPERATE_RET app_player_play_alert(APP_ALERT_TYPE type);

uint8_t app_player_is_playing(void);

#ifdef __cplusplus
}
#endif

#endif /* __APP_PLAYER_H__ */

/**
 * @file local_alert_src.h
 * @version 0.1
 * @copyright Copyright (c) 2021-2026 Tuya Inc. All Rights Reserved.
 */

#ifndef __LOCAL_ALERT_SRC_H__
#define __LOCAL_ALERT_SRC_H__

#include "tuya_cloud_types.h"

#ifdef __cplusplus
extern "C" {
#endif

#if defined(AI_PLAYER_ALERT_SOURCE_LOCAL) && (AI_PLAYER_ALERT_SOURCE_LOCAL == 1)

/***********************************************************
************************macro define************************
***********************************************************/


/***********************************************************
***********************extern define***********************
***********************************************************/
#if defined(ENABLE_AI_LANGUAGE_CHINESE) && (ENABLE_AI_LANGUAGE_CHINESE == 1)

extern const char media_src_ai_zh[4257];
extern const char media_src_free_chat_zh[10413];
extern const char media_src_long_press_zh[10413];
extern const char media_src_low_battery_zh[11024];
extern const char media_src_network_config_zh[13760];
extern const char media_src_network_conn_failed_zh[13472];
extern const char media_src_network_conn_success_zh[12896];
extern const char media_src_network_conn_zh[15776];
extern const char media_src_network_reconfigure_zh[18080];
extern const char media_src_please_again_zh[13472];
extern const char media_src_press_talk_zh[10413];
extern const char media_src_prologue_zh[16640];
extern const char media_src_wakeup_chat_zh[11277];
extern const char media_src_wakeup_zh[3933];
#endif

#if defined(ENABLE_AI_LANGUAGE_ENGLISH) && (ENABLE_AI_LANGUAGE_ENGLISH == 1)

extern const char media_src_ai_en[7605];
extern const char media_src_free_chat_en[8469];
extern const char media_src_long_press_en[9549];
extern const char media_src_low_battery_en[14768];
extern const char media_src_network_config_en[15920];
extern const char media_src_network_conn_en[20240];
extern const char media_src_network_conn_failed_en[15056];
extern const char media_src_network_conn_success_en[16928];
extern const char media_src_network_reconfigure_en[18368];
extern const char media_src_please_again_en[12608];
extern const char media_src_press_talk_en[8037];
extern const char media_src_prologue_en[16352];
extern const char media_src_wakeup_chat_en[8685];

#endif

/***********************************************************
********************function declaration********************
***********************************************************/

#endif

#ifdef __cplusplus
}
#endif

#endif /* __LOCAL_MEDIA_SRC_H__ */

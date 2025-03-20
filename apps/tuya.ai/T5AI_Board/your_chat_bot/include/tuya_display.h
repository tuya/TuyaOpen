/**
 * @file tuya_display.h
 * @version 0.1
 * @date 2025-03-17
 */

#ifndef __TUYA_DISPLAY_H__
#define __TUYA_DISPLAY_H__

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
typedef unsigned int  TY_DISPLAY_TYPE_E;
#define TY_DISPLAY_TP_HUMAN_CHAT           0
#define TY_DISPLAY_TP_AI_CHAT              1
#define TY_DISPLAY_TP_AI_THINKING          2

#define TY_DISPLAY_TP_STAT_LISTEN          3
#define TY_DISPLAY_TP_STAT_SPEAK           4
#define TY_DISPLAY_TP_STAT_IDLE            5

#define TY_DISPLAY_TP_STAT_NETCFG          6
#define TY_DISPLAY_TP_STAT_POWERON         7
#define TY_DISPLAY_TP_STAT_ONLINE          8

#define TY_DISPLAY_TP_STAT_SLEEP           9
#define TY_DISPLAY_TP_STAT_WAKEUP          10

#define TY_DISPLAY_TP_MAX                  11


/***********************************************************
********************function declaration********************
***********************************************************/
OPERATE_RET tuya_display_init(void);

OPERATE_RET tuya_display_send_msg(TY_DISPLAY_TYPE_E tp, char *data, int len);


#ifdef __cplusplus
}
#endif

#endif /* __TUYA_DISPLAY_H__ */

/**
 * @file app_media_alert.h
 * @brief app_media_alert module is used to
 * @version 0.1
 * @date 2025-03-26
 */

#ifndef __APP_MEDIA_ALERT_H__
#define __APP_MEDIA_ALERT_H__

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
extern const uint8_t media_src_power_on[16640];
extern const uint8_t media_src_not_active[15776];
extern const uint8_t media_src_netcfg_mode[13760];
extern const uint8_t media_src_network_conencted[12896];
extern const uint8_t media_src_network_fail[13472];
extern const uint8_t media_src_network_disconnect[18080];
extern const uint8_t media_src_battery_low[11024];
extern const uint8_t media_src_please_again[13472];

/***********************************************************
********************function declaration********************
***********************************************************/

#ifdef __cplusplus
}
#endif

#endif /* __APP_MEDIA_ALERT_H__ */

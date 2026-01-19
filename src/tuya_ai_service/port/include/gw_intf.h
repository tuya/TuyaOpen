/**
 * @file gw_intf.h
 * @brief gw_intf module is used to 
 * @version 0.1
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#ifndef __GW_INTF_H__
#define __GW_INTF_H__

#include "tuya_cloud_types.h"

#include "tuya_ai_types.h"

#include "tuya_iot.h"

#ifdef __cplusplus
extern "C" {
#endif

/***********************************************************
************************macro define************************
***********************************************************/
#define LOCAL_KEY_LEN           16      // max string length of LOCAL_KEY

/***********************************************************
***********************typedef define***********************
***********************************************************/
/**
 * @brief Definition gw BASE information
 */
typedef struct {
    int a;
} GW_BASE_IF_S;


/**
 * @brief Definition of active info
 */
typedef struct {
    /** key used by lan/mqtt/... */
    char local_key[LOCAL_KEY_LEN + 1];
} GW_ACTV_IF_S;

/**
 * @brief Definition of gateway callback funtions
 */
typedef struct {
    /** status update */
    int a;
} TY_IOT_CBS_S;

// gw register status
typedef uint8_t GW_WORK_STAT_T;
#define UNREGISTERED 0 // unregistered
#define REGISTERED 1 // registered & activate start
#define ACTIVATED 2 // already active
#define PROXY_ACTIVING    (3)//start ble active
#define PROXY_ACTIVATED   (4) // ble actived

typedef struct {
    GW_WORK_STAT_T stat;
} GW_WORK_STAT_MAG_S;

/**
 * @brief Definition of core device management
 */
typedef struct {
    /** Inited or not */
    BOOL_T is_init;

    /** device active info, see GW_ACTV_IF_S */
    GW_ACTV_IF_S gw_actv;

    GW_WORK_STAT_MAG_S gw_wsm;
} GW_CNTL_S;

/***********************************************************
********************function declaration********************
***********************************************************/
/**
 * @brief Get gateway cntl
 *
 * @return Gateway cntl, see GW_CNTL_S
 */
GW_CNTL_S *get_gw_cntl(VOID);

#ifdef __cplusplus
}
#endif

#endif /* __GW_INTF_H__ */

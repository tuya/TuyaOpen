/**
 * @file tuya_svc_netmgr_linkage.h
 * @brief tuya_svc_netmgr_linkage module is used to
 * @version 0.1
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#ifndef __TUYA_SVC_NETMGR_LINKAGE_H__
#define __TUYA_SVC_NETMGR_LINKAGE_H__

#include "tuya_cloud_types.h"

#include "tuya_ai_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/***********************************************************
************************macro define************************
***********************************************************/
typedef uint32_t LINKAGE_CAP_E;
#define LINKAGE_CAP_LINK_EVENT     BIT(0)
#define LINKAGE_CAP_ACTIVATE_TOKEN BIT(1)
#define LINKAGE_CAP_ACTIVATE_META  BIT(2)
#define LINKAGE_CAP_LAN            BIT(3)
#define LINKAGE_CAP_ACTIVATE       (LINKAGE_CAP_ACTIVATE_TOKEN | LINKAGE_CAP_ACTIVATE_META)

/***********************************************************
***********************typedef define***********************
***********************************************************/
/* tuya sdk gateway reset type */
typedef enum {
    GW_LOCAL_RESET_FACTORY = 0,   //(cb/event)
    GW_REMOTE_UNACTIVE,           //(cb/event)
    GW_LOCAL_UNACTIVE,            //(cb/event)
    GW_REMOTE_RESET_FACTORY,      //(cb/event)
    GW_RESET_DATA_FACTORY,        // need clear local data when active(cb/event)
    GW_REMOTE_RESET_DATA_FACTORY, // need clear local data when active(event only)
} GW_RESET_TYPE_E;

typedef enum {
    LINKAGE_TYPE_DEFAULT,    // keep it first(current active linkage)
    LINKAGE_TYPE_WIRED,      // Wired
    LINKAGE_TYPE_WIFI,       // Wi-Fi
    LINKAGE_TYPE_BT,         // BLE
    LINKAGE_TYPE_CAT1,       // CN/4G
    LINKAGE_TYPE_NB,         // NB-IoT
    LINKAGE_TYPE_EXT_MODULE, // 插件
    LINKAGE_TYPE_EXT2,       // reserved
    LINKAGE_TYPE_EXT3,       // reserved
    LINKAGE_TYPE_VIRTUAL,    // keep it last

    LINKAGE_TYPE_MAX
} LINKAGE_TYPE_E;

typedef enum {
    LINKAGE_CFG_LOWPOWER, // BOOL_T
    LINKAGE_CFG_IP,       // NW_IP_S
    LINKAGE_CFG_MAC,      // NW_MAC_S
    LINKAGE_CFG_RSSI,     // SCHAR_T
    LINKAGE_CFG_STATUS,   // uint8_t (linkage customized status)

    LINKAGE_CFG_MAX
} LINKAGE_CFG_E;

typedef struct {
    LINKAGE_TYPE_E type;
    LINKAGE_CAP_E capability;
    OPERATE_RET (*open)(LINKAGE_CAP_E cap);
    OPERATE_RET (*close)(VOID);
    OPERATE_RET (*reset)(IN GW_RESET_TYPE_E reset_type);
    OPERATE_RET (*set)(IN LINKAGE_CFG_E cfg, IN VOID *data);
    OPERATE_RET (*get)(IN LINKAGE_CFG_E cfg, OUT VOID *data);
} netmgr_linkage_t;

/***********************************************************
********************function declaration********************
***********************************************************/
/**
 * @brief Get a linkage with specific type
 *
 * @param[in] type see LINKAGE_TYPE_E
 *
 * @return linkage on success, NULL on error
 */
netmgr_linkage_t *tuya_svc_netmgr_linkage_get(IN LINKAGE_TYPE_E type);

#ifdef __cplusplus
}
#endif

#endif /* __TUYA_SVC_NETMGR_LINKAGE_H__ */

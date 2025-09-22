/**
 * @file tuya_cloud_com_defs.h
 * @brief This file contains the definitions and declarations for the Tuya
 * Cloud.
 *
 * @copyright Copyright 2016-2021 Tuya Inc. All Rights Reserved.
 */

#ifndef TUYA_CLOUD_COM_DEFS_H
#define TUYA_CLOUD_COM_DEFS_H

#include "tuya_cloud_types.h"
#include "cJSON.h"

#ifdef __cplusplus
extern "C" {
#endif

// clang-format off

// tuya sdk constants definitions
#define ENCRYPT_KEY_LEN 16 // max string length of storage encrypt
#define GW_UUID_LEN     25 // max string length of UUID
#define AUTH_KEY_LEN    32 // max string length of AUTH_KEY
#define PSK_KEY_LEN     64 // max string length of PSK_KEY
#define PRODUCT_KEY_LEN 16 // max string length of PRODUCT_KEY
#define SW_VER_LEN      10 // max string length of VERSION
#define SW_MD5_LEN      32 // max string length of attatch MD5

#define BT_HID_LEN 19 // max string length of HID
#define BT_MAC_LEN 12 // max string length of MAC

#define TUYA_PSK_LEN   32 // max string length of PSK
#define TUYA_PSK21_LEN 44 // max string length of PSK21
#define TUYA_IMEI_LEN  18 // max string length of IMEI
#define TUYA_SN_LEN    20 // max string length of SN

#define SYS_ENV_LEN        20  // max string length of ENV
#define LOG_SEQ_PATH_LEN   128 // max string length of LOG SEQ PATH
#define GW_ID_LEN          25  // max string length of GW_ID
#define DEV_UUID_LEN       25  // max string length of DEV UUID
#define DEV_ID_LEN         25  // max string length of DEV_ID
#define SCHEMA_ID_LEN      16  // max string length of SCHEMA
#define SCHEMA_VER_LEN     16  // max string length of SCHEMA VERSION
#define BIND_TOKEN_LEN     16  // max string length of BIND_TOKEN
#define TOKEN_LEN          8   // max string length of TOKEN
#define REGION_LEN         2   // max string length of REGIN IN TOKEN
#define REGIST_KEY_LEN     4   // max string length of REGIST_KEY IN TOKEN
#define HTTP_URL_LMT       128 // max string length of URL
#define MQ_URL_LMT         128 // max string length of MQTT_URL
#define TIME_ZONE_LEN      10  // max string length of TIME_ZONE
#define SUMMER_TM_ZONE_LEN 256 // max string length of SUMMER_TIME_ZONE
#define COUNTRY_CODE_LEN   8   // max string length of COUNTRY_CODE
#define COUNTRY_CODE_LEN   8   // max string length of COUNTRY_CODE
#define IPV4_LEN           15  // max string length of IPV4_LEN 255.255.255.255

#define SEC_KEY_LEN   16 // max string length of SECURITY_KEY
#define LOCAL_KEY_LEN 16 // max string length of LOCAL_KEY

#define WXAPPID_LEN 32 // max string length of wechat_appid
#define WXUUID_LEN  32 // max string length of wechat_uuid

#define GRP_ID_LEN   5 // max string length of group id range:1-65535
#define SCENE_ID_LEN 3 // max string length of scene id range:1-255

#define NET_MODE_LEN 32 // max string length of NET MODE

#define FW_URL_LEN  255 // max length of firmware download url
#define FW_HMAC_LEN 64  // max length of firmware hmac

#define LC_GW_SLEEP_HB_LMT_S 12 * 3600 // 12h
// group and scene error code
#define M_SCE_SUCCESS     0 // success
#define M_SCE_ERR_EXCEED  1 // space not enough
#define M_SCE_ERR_TIMEOUT 2 // timeout
#define M_SCE_ERR_PARAM   3 // param not in range
#define M_SCE_WRITE_FILE  4 // failed to write db
#define M_SCE_ERR_OTHER   5 // other failure

// Product Function Specification
typedef uint8_t GW_ABI;
#define GW_VIRTUAL         0x00 // product doesn't have subdevices functions,it only has device function. For example: SOC or MCU
#define GW_ENTITY_ATTH_DEV 0x01 // Product has subdevices functions and it also has device function.
#define GW_ENTITY          0x02 // Product only has subdevices functions.

// Product Network Connection
typedef uint8_t GW_NW_STAT_T;
#define GNS_UNVALID   0 // product is offline in LAN and WAN
#define GNS_LAN_VALID 1 // product is online in LAN but offline in WAN
#define GNS_WAN_VALID 2 // product is online in LAN and WAN

// Product Extend State
typedef uint8_t GW_EXT_STAT_T;
#define EXT_UNAUTH         0 // UNAUTH
#define EXT_PROD_TEST      1 // PROD_TEST
#define EXT_NORMAL_S       2 // NORMAL_S
#define EXT_GW_UPGRD       3 // GW_UPGRD
#define EXT_DEV_UPGRD      4 // DEV_UPGRD
#define EXT_DEV_ADD        5 // DEV_ADD
#define EXT_REG_FAIL       6 // REG_FAIL
#define EXT_NET_FAIL       7 // NET_FAIL
#define EXT_CONFIG_BACK_UP 8 // CONFIG_BACK_UP
#define EXT_CONFIG_RESTORE 9 // CONFIG_RESTORE

/**
 * @brief Definition of device ota channel
 *
 * @note 0~9 are consistent among all TUYA devices
 * 10~19 are customized by device itself
 */
typedef uint8_t DEV_TYPE_T;
#define DEV_NM_ATH_SNGL     0  // main netlink module
#define DEV_BLE_SNGL        1  // ble
#define DEV_ZB_SNGL         3  // ZigBee
#define DEV_NM_NOT_ATH_SNGL 9  // MCU
#define DEV_ATTACH_MOD_1    10 // attach 1
#define DEV_ATTACH_MOD_2    11 // attach 2
#define DEV_ATTACH_MOD_3    12 // attach 3
#define DEV_ATTACH_MOD_4    13 // attach 4
#define DEV_ATTACH_MOD_5    14 // attach 5
#define DEV_ATTACH_MOD_6    15 // attach 6
#define DEV_ATTACH_MOD_7    16 // attach 7
#define DEV_ATTACH_MOD_8    17 // attach 8
#define DEV_ATTACH_MOD_9    18 // attach 9
#define DEV_ATTACH_MOD_10   19 // attach 10

// sub-device detail type
typedef uint32_t USER_DEV_DTL_DEF_T; // user detial type define

/**
 * @brief Definition of all attache types
 */
typedef uint8_t GW_PERMIT_DEV_TP_T;
#define GP_DEV_DEF    0xFF                // default device type
#define GP_DEV_ZB     DEV_ZB_SNGL         // zigbee
#define GP_DEV_BLE    DEV_BLE_SNGL        // ble
#define GP_DEV_MCU    DEV_NM_NOT_ATH_SNGL // mcu
#define GP_DEV_ATH_1  DEV_ATTACH_MOD_1    // attach 1
#define GP_DEV_ATH_2  DEV_ATTACH_MOD_2    // attach 2
#define GP_DEV_ATH_3  DEV_ATTACH_MOD_3    // attach 3
#define GP_DEV_ATH_4  DEV_ATTACH_MOD_4    // attach 4
#define GP_DEV_ATH_5  DEV_ATTACH_MOD_5    // attach 5
#define GP_DEV_ATH_6  DEV_ATTACH_MOD_6    // attach 6
#define GP_DEV_ATH_7  DEV_ATTACH_MOD_7    // attach 7
#define GP_DEV_ATH_8  DEV_ATTACH_MOD_8    // attach 8
#define GP_DEV_ATH_9  DEV_ATTACH_MOD_9    // attach 9
#define GP_DEV_ATH_10 DEV_ATTACH_MOD_10   // attach 10

#define GP_DEV_SUPPORT_MAX GP_DEV_ATH_10 // max attach id

typedef uint8_t TY_DP_REPT_CHAN_TP_T;
#define TY_DP_REPT_CHAN_LAN       0
#define TY_DP_REPT_CHAN_MQTT      1
#define TY_DP_REPT_CHAN_HTTP      2
#define TY_DP_REPT_CHAN_BLE       3
#define TY_DP_REPT_CHAN_SIGMESH   4
#define TY_DP_REPT_CHAN_TUYA_MESH 5
#define TY_DP_REPT_CHAN_BEACON    6
#define TY_DP_REPT_CHAN_MAX       7

// clang-format on

#ifdef __cplusplus
}
#endif

#endif

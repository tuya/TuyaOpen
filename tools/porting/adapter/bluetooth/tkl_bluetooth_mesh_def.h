/**
 * @file tkl_bluetooth_mesh_def.h
 * @brief This is tuya tal_adc file
 * @version 1.0
 * @date 2021-09-10
 *
 * @copyright Copyright 2021-2031 Tuya Inc. All Rights Reserved.
 *
 */

#ifndef __TKL_BLUETOOTH_MESH_DEF_H__
#define __TKL_BLUETOOTH_MESH_DEF_H__

#include "tuya_cloud_types.h"
#include "tuya_error_code.h"
#include "tkl_bluetooth.h"

#ifndef _PACKED_
#define _PACKED_ __attribute__((packed))
#endif

#ifndef _WEAK_
#define _WEAK_ __attribute__((weak))
#endif

typedef struct {
    uint8_t *data;     /**< Mesh data. */
    uint16_t data_len; /**< Mesh data lenth. */
} TKL_MESH_PROXY_DATA_T;

/**@brief mesh access msg parameters. */
typedef struct {
    uint32_t opcode;   /**< Mesh opcode. */
    uint8_t *data;     /**< Mesh data. */
    uint16_t data_len; /**< Mesh data lenth. */
} TKL_MESH_ACCESS_MSG_T;

typedef struct {
    uint8_t adv_data[31]; /**< Mesh Connect Adv Data. */
    uint16_t adv_len;     /**< Mesh Connect Adv Data Len. */

    uint8_t rsp_data[31]; /**< Rsp Data. */
    uint16_t rsp_len;     /**< Rsp Data Len. */

    uint8_t count;          /**< The number of transmissions is the Transmit Count + 1 */
    uint8_t interval_steps; /**< Transmission interval = (Network Retransmit Interval Steps + 1) * 10 */
} TKL_MESH_CONNECT_ADV_PARAM_T;

/**@brief network parameters. */
typedef struct {
    uint16_t src_addr;      /**< Source unicast address. */
    uint16_t dst_addr;      /**< Destination  unicast address. */
    uint8_t model_index;    /**< Msg model index. */
    uint32_t seq;           /**< Sequence num of this msg. */
    uint8_t ttl;            /**< Time To Live. */
    uint16_t app_key_index; /**< The appkey index of this msg ues. */
    uint16_t net_key_index; /**< The networkkey index of this msg ues. */
    char rssi;              /**< used when rx in adv bearer. */
} TKL_MESH_NET_PARAM_T;

/**@brief network provision data. */
typedef struct {
    uint8_t net_key[16];      /**< Network key. */
    uint16_t key_index;       /**< Network key index. */
    uint8_t flags;            /**< Network key flag. */
    uint8_t iv_index[4];      /**< Network IV index. */
    uint16_t unicast_address; /**< Node unicast address. */
} _PACKED_ TKL_NET_PROV_DATA_T;

/**@brief app key data. */
typedef struct {
    uint8_t net_app_idx[3]; /**< Network key index. */
    uint8_t app_key[16];    /**< App key. */
} _PACKED_ TKL_APP_KEY_DATA_T;

typedef OPERATE_RET (*TKL_MESH_MSG_RECV_CB)(TKL_MESH_ACCESS_MSG_T *msg_raw, TKL_MESH_NET_PARAM_T *net_param);

/**@brief model handle. */
typedef struct {
    uint32_t model_id;                  /**< Model identifier. */
    TKL_MESH_MSG_RECV_CB model_receive; /**< Model receive callback. */
    uint16_t model_handle;              /**< Model handel or model index. */
} TKL_MESH_MODEL_HADLE_T;

/**@brief mesh node feature. */
typedef enum {
    MESH_FEATURE_RELAY = 0x00, /**< Relay. */
    MESH_FEATURE_PROXY,        /**< Proxy. */
    MESH_FEATURE_FRIEND,       /**< Friend. */
    MESH_FEATURE_LPN,          /**< LPN. */
} MESH_FEATURE_T;

typedef enum {
    TKL_MESH_EVT_STACK_INIT = 0x01, /**< Mesh Stack Initial Callback Event */

    TKL_MESH_EVT_STACK_DEINIT, /**< Mesh Stack Deinit Callback Event */

    TKL_MESH_EVT_STACK_RESET, /**< Mesh Stack Reset Callback Event */

    TKL_MESH_EVT_UNPROV_BEACON_REPORT, /**< Receive Mesh Unprovisioned Beacon Data */

    TKL_MESH_EVT_PRIVATE_BEACON_REPORT, /**< Receive Tuya-Private Beacon Data */

    TKL_MESH_EVT_INVITE_EVT, /**< Invite Mesh Device Event */

    TKL_MESH_EVT_CONFIG_DATA_RX, /**< Receive Mesh Configuration Message */

    TKL_MESH_EVT_MODEL_DATA_RX, /**< Receive Mesh Model Message */
} TKL_MESH_EVT_TYPE_E;

typedef struct {
    uint8_t peer_addr[6];  /**< Mesh Device Mac Address */
    uint8_t peer_uuid[16]; /**< Mesh Device UUID */

    uint16_t node_addr;      /**< Mesh Node Address */
    uint8_t node_devkey[16]; /**< Mesh Device key */
} TKL_MESH_DEVICE_INFO_T;

typedef struct {
    uint16_t local_addr; /**< Mesh Provisioner Local Address */

    uint8_t netkey[16]; /**< Mesh Provisioner Local Netkey */
    uint8_t appkey[16]; /**< Mesh Provisioner Local Appkey */
} TKL_MESH_LOCAL_INFO_T;

typedef struct {
    uint8_t mac[6];    /**< Mesh Provisioner Scan Adv Mac Address */
    uint8_t uuid[16];  /**< Mesh Provisioner Scan Mesh Device UUID */
    uint16_t oob;      /**< Mesh Provisioner Scan Mesh Device OOB */
    uint32_t uri_hash; /**< Mesh Provisioner Scan Mesh Device URI Hash */
    char rssi;         /**< Mesh Provisioner Scan Mesh Device Rssi */
} TKL_MESH_UNPROV_BEACON_T;

typedef struct {
    uint8_t mac[6];  /**< Beacon Central Scan Adv Mac Address while in mesh mode*/
    uint8_t length;  /**< Beacon Central Scan advertising data length*/
    uint8_t *p_data; /**< Beacon Central Scan advertising data */
    char rssi;       /**< Beacon Central Scan advertising Rssi */
} TKL_PRIVATE_BEACON_T;

typedef struct {
    uint32_t opcode;        /**< Mesh opcode. Indicate the mesh data with opcode */
    uint8_t count;          /**< [Mesh& Gateway Special] The number of transmissions is the Transmit Count + 1 */
    uint8_t interval_steps; /**< [Mesh& Gateway Special] Transmission interval = (Network Retransmit Interval Steps + 1)
                             * 10 */

    uint16_t data_len; /**< Mesh Data Length */
    uint8_t *p_data;   /**< Pointer For Mesh Data */
} TKL_MESH_DATA_T;

typedef struct {
    uint32_t opcode;    /**< Mesh opcode. Point the mesh opcode while receiving data. */
    uint16_t node_addr; /**< Sending Node Address */
    uint16_t dest_addr; /**< Receive Destination Address */
    uint8_t recv_ttl;   /**< Receive TTL */

    uint16_t data_len; /**< Recevie Mesh Data Length */
    uint8_t *p_data;   /**< Recevie Pointer of Mesh Data */
} TKL_MESH_DATA_RECEIVE_T;

typedef struct {
    uint16_t node_addr; /**< Assign mesh Node Address */
    uint8_t devkey[16]; /**< Get Mesh Node Dev-key After Provision*/
} TKL_MESH_PROV_T;

struct TKL_MESH_DFU_TARGET_PARAM_T {
    uint16_t target_addr; /**< Mesh DFU Target-Node Addr */
    uint8_t img_idx;      /**< Mesh DFU Target image index, default: 0*/
    uint8_t result;       /**< Mesh DFU Target Finish Result*/
} __attribute__((packed));

struct TKL_MESH_DFU_BLOB_CONFIG_T {
    uint16_t group_addr; /**< Mesh DFU Target Group Addr or Single Target Address*/

    uint16_t area_id;   /**< Mesh DFU Image ID */
    uint8_t blob_id[8]; /**< Mesh BLOB ID */
    uint32_t blob_size; /**< Mesh BLOB Size, Image Size*/
} __attribute__((packed));

typedef struct {
    uint16_t number;                            /**< Mesh DFU Target Number */
    struct TKL_MESH_DFU_TARGET_PARAM_T *target; /**< Mesh DFU Target List for Result */
} TKL_MESH_DFU_TARGET_LIST_T;

typedef enum {
    TKL_MESH_DFU_OP_TARGET_ADD = 0x01, /**< TKL Mesh Provision Add Target for BLOB Client*/

    TKL_MESH_DFU_OP_PROGRESS_CHECK, /**< TKL Mesh Provision Check Progress*/

    TKL_MESH_DFU_OP_PROGRESS_GET, /**< TKL Mesh Provision DFU Progress Report*/

    TKL_MESH_DFU_OP_RESET, /**< TKL Mesh Provision DFU Operations: Reset Current Status and data */

    TKL_MESH_DFU_OP_MAX, /**< TKL Mesh Provision DFU Event Max*/
} TKL_MESH_DFU_OPERATION_E;

typedef struct {
    /* Called when the reader is opened for reading.*/
    OPERATE_RET (*dfu_open)(uint16_t area_id, void *user_data);

    /* Used by the BLOB Transfer Client to fetch outgoing data.*/
    uint32_t (*dfu_read)(uint16_t area_id, uint32_t offset, uint8_t *pbuff, uint32_t size);

    /* Called when the reader is closed. */
    OPERATE_RET (*dfu_close)(uint16_t area_id);

    /* Called when the client report @TKL_MESH_DFU_STATUS_E status. */
    OPERATE_RET (*dfu_start)(uint16_t group_addr, int result);

    /* Called when the client report result. */
    OPERATE_RET (*dfu_result)(uint16_t group_addr, TKL_MESH_DFU_TARGET_LIST_T target_list);
} TKL_MESH_DFU_IO_T;

typedef struct {
    TKL_MESH_EVT_TYPE_E type; /**< Mesh Event Type */
    int state;                /**< Mesh Event States */

    union {
        TKL_MESH_UNPROV_BEACON_T unprov_report; /**< Receive Mesh Unprovisioned Beacon Data */
        TKL_PRIVATE_BEACON_T
        beacon_report; /**< Receive Tuya Private Beacon Data while running beacon cental in mesh mode*/

        TKL_MESH_PROV_T prov;                  /**< Provision Callback State */
        TKL_MESH_DATA_RECEIVE_T config_report; /**< Report Mesh Model Configuration Data */
        TKL_MESH_DATA_RECEIVE_T model_report;  /**< Report Mesh Model Message Data */
    } mesh_event;
} TKL_MESH_EVT_PARAMS_T;

/**< Define Event Callback for mesh*/
typedef void (*TKL_MESH_EVT_FUNC_CB)(TKL_MESH_EVT_PARAMS_T *p_event);

typedef enum {
    TKL_MESH_PROVISION_SUCCESS = 0,
    TKL_MESH_PROVISION_START,
    TKL_MESH_PROVISION_TIMEOUT,
    TKL_MESH_RESET,
    TKL_MESH_RESET_IN_RAM,
    TKL_MESH_REVERT_IN_MESH,
    TKL_MESH_GROUP_SUB_ADD,
    TKL_MESH_GROUP_SUB_DEL,

    TKL_OTA_START,
    TKL_OTA_SUCCESS,
    TKL_OTA_FAIL,
} TKL_MESH_STATE_T;

typedef void (*TKL_MESH_NET_STATE_CB_T)(TKL_MESH_STATE_T state);

#endif

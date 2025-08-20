/**
 * @file tkl_cellular_comm.h
 * @brief
 * @version 0.1
 * @date 2023-8-22
 *
 * @copyright Copyright (c) 2021-2023 Tuya Inc. All Rights Reserved.
 *
 * Permission is hereby granted, to any person obtaining a copy of this software and
 * associated documentation files (the "Software"), Under the premise of complying
 * with the license of the third-party open source software contained in the software,
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software.
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 */
#ifndef __TKL_CELLULAR_COMM_H
#define __TKL_CELLULAR_COMM_H

#include <time.h>
#include "tuya_cloud_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct{
    int cid;
    uint32_t rx_count;
    uint32_t tx_count;
}TKL_CELL_DATA_STATISTICS;

typedef enum
{
    TUYA_CELL_NET_TYPE_UNKNOWN = -1,
    TUYA_CELL_NET_TYPE_GSM = 1,
    TUYA_CELL_NET_TYPE_LTE,
    TUYA_CELL_NET_TYPE_3G,
    TUYA_CELL_NET_TYPE_CATM, //CATM1
    TUYA_CELL_NET_TYPE_NB,
}TUYA_CELLULAR_RAT_E;

typedef struct {
    int8_t rat;             // -1 invalid
    int8_t act1;            // -1 invalid
    int8_t act2;            // -1 invalid
    int8_t gsm_band_count;
    uint16_t gsm_band[4];   //850/900/1800/1900MHz
    int8_t lte_band_count;
    uint8_t lte_band[11];   //1~85, supports setting 11
}TKL_GTACT_T;

typedef struct
{
    int cur_plmn;         //Current device registered PLMN
    int8_t plmn_list_count; //Number of PLMNs in the country supported by the current device
    int *plmn_list;       //PLMNs in the country supported by the current device, such as 46000
}PLMN_LIST_T;

/**
 * @brief Virtual AT receive response callback function
 * @param resp Returned AT response
 * @param len  Length of returned AT response data
 */
typedef void (*TKL_VIRTAT_RESP)(char *resp,int len);

/**
 * @brief Control cellular AP LOG output
 * @param enable FALSE: output off, TRUE: output on
 * @return 0 success, others failure
 */
OPERATE_RET tkl_cellular_comm_ctrl_ap_trace(bool enable);

/**
 * @brief Power key shutdown function enable control
 * @param enable TRUE: allow, FALSE: disable
 * @return 0 success, others failure
 */
OPERATE_RET tkl_cellular_comm_ctrl_powerkey(bool enable);

/**
 * @brief Enable AT port function
 * @param enable TRUE: allow serial AT, FALSE: disable serial AT
 * @return 0 success, others failure
 */
OPERATE_RET tkl_cellular_comm_ctrl_at_port(bool enable);

/**
 * @brief Get RSSI dBm value
 * @param sim_id Corresponding SIM card ID (0~1)
 * @param rssi_dBm: Signal strength pointer, unit dBm
 * @return 0 success, others failure
 */
OPERATE_RET tkl_cellular_comm_get_rssi_dBm(uint8_t sim_id,int *rssi_dBm);

/**
 * @brief Get the current network type after the current cellular device successfully registers to the network
 * @param sim_id sim id
 * @param net_type Pointer to the obtained network type
 * @return 0 success, others failure
 */
OPERATE_RET tkl_cellular_comm_get_nettype(uint8_t sim_id, TUYA_CELLULAR_RAT_E *net_type);

/**
 * @brief Get the reference signal quality of the current cellular device, refer to AT+CSQ
            <rssi>:
                0 113 dBm or less
                1 111 dBm
                2. . . 30 109. . . 53 dBm
                31 51 dBm or greater
                99 not known or not detectable
 * @param rssi Pointer to the obtained signal strength
 * @return 0 success, others failure
 */
OPERATE_RET tkl_cellular_comm_get_rssi(uint8_t sim_id,int *rssi);

/**
 * @brief Get the current Unix timestamp (in seconds)
 * @param epoch_sec Pointer used to store the Unix timestamp. After successful function execution, the variable pointed to by this pointer will be set to the current Unix timestamp (in seconds)
 * @return OPERATE_RET type, 0 indicates success, other values indicate failure
 */
OPERATE_RET tkl_cellular_comm_get_epoch_time(uint64_t *epoch_sec);

/**
 * @brief Get the cumulative time since system power-on, unit ms
 * @param up_ms Pointer used to store the cumulative power-on time
 * @return OPERATE_RET type, 0 indicates success, other values indicate failure
 */
OPERATE_RET tkl_cellular_comm_get_poweron_time(uint64_t *up_ms);

/**
 * @brief Get the local time of the cellular device based on the time synchronized with the base station
 * @param local_tm Pointer used to store local time. After successful function execution, the structure pointed to by this pointer will be set to the current local time
 * @return OPERATE_RET type, 0 indicates success, other values indicate failure
 */
OPERATE_RET tkl_cellular_comm_ctrl_get_localtime(struct tm *local_tm);

/**
 * @brief Get timezone information based on the time synchronized with the base station
 * @param timezone Pointer to the obtained timezone
 * @return OPERATE_RET type, 0 indicates success, other values indicate failure
 */
OPERATE_RET tkl_cellular_comm_get_timezone(int *timezone);

/**
 * @brief Get cellular SN number
 * @param sn Set SN string
 * @return OPERATE_RET type, 0 indicates success, other values indicate failure
 */
OPERATE_RET tkl_cellular_comm_get_sn(char sn[25]);

/**
 * @brief Set cellular SN number
 * @param sim_id (0~1)
 * @param sn Obtained SN string
 * @return OPERATE_RET type, 0 indicates success, other values indicate failure
 */
OPERATE_RET tkl_cellular_comm_set_sn(char sn[25]);

/**
 * @brief Set the delay time for RRC connect to switch to idle
 * @param time Unit seconds.
 * @return OPERATE_RET type, 0 indicates success, other values indicate failure
 */
OPERATE_RET tkl_cellular_comm_set_rrc_release_time(uint32_t time);

/**
 * @brief Get the current device RRC connect to idle delay time
 * @param time Unit seconds.
 * @return OPERATE_RET type, 0 indicates success, other values indicate failure
 */
OPERATE_RET tkl_cellular_comm_get_rrc_release_time(uint32_t *time);

/**
 * @brief Get the version number of the base firmware. This function is generally used for dual firmware, the application firmware calls this API to get the version number of the system firmware
 * @param ver Obtained version number string
 * @return OPERATE_RET type, 0 indicates success, other values indicate failure
 */
OPERATE_RET tkl_cellular_comm_get_sysfw_ver(char ver[32]);

/**
 * @brief Get module model
 * @param module Get module model name
 * @return OPERATE_RET type, 0 indicates success, other values indicate failure
 */
OPERATE_RET tkl_cellular_comm_get_module(char module[32]);

/**
 * @brief Set PLMN
 * @note  Specify the module to register to a specific operator. When the module fails to register to the specified operator, the module automatically searches for registrable operators
 *        Same function as AT+COPS=4,2,"46000"
 * @param plmn Numeric format operator code string, such as "46000"
 * @return OPERATE_RET type, 0 indicates success, other values indicate failure
 */
OPERATE_RET tkl_cellular_comm_set_plmn(char *plmn);

/**
 * @brief Set the callback function for virtual AT command response
 * @param resp_callback Set callback function
 * @return OPERATE_RET type, 0 indicates success, other values indicate failure
 */
OPERATE_RET tkl_cellular_comm_set_virtual_at_recv_cb(TKL_VIRTAT_RESP resp_callback);

/**
 * @brief Send AT command through virtual AT
 * @param at_cmd AT command string
 * @return OPERATE_RET type, 0 indicates success, other values indicate failure
 */
OPERATE_RET tkl_cellular_comm_send_virtual_at(char *at_cmd);



#ifdef __cplusplus
}
#endif
#endif

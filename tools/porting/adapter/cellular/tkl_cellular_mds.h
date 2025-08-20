/**
 * @file tkl_cellular_mds.h
 * @author www.tuya.com
 * @brief Cellular module data API implementation interface.
 *
 * @copyright Copyright (c) tuya.inc 2021
 */

#ifndef __TKL_CELLULAR_MDS_H__
#define __TKL_CELLULAR_MDS_H__

#include "tuya_cloud_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Cellular mobile data authentication status
 */
typedef enum
{
    TUYA_CELLULAR_MDS_STATUS_UNKNOWN = 0,
    TUYA_CELLULAR_MDS_STATUS_IDLE = 1,  /*< Idle status */
    TUYA_CELLULAR_MDS_STATUS_REG,       /*< PS domain registration */
    TUYA_CELLULAR_MDS_STATUS_ACTIVE,    /*< PDP activation */
    TUYA_CELLULAR_MDS_STATUS_CAMPED,    /*< Registration rejected */
}TUYA_CELLULAR_MDS_STATUS_E;

/**
 * @brief Cellular network status
 */
typedef enum
{
    TUYA_CELLULAR_MDS_NET_DISCONNECT = 0,
    TUYA_CELLULAR_MDS_NET_CONNECT = 1,
}TUYA_CELLULAR_MDS_NET_STATUS_E;

/**
 * @brief Cellular network IP type
 */
typedef enum{
    TUYA_MDS_PDP_IPV4 = 0,
    TUYA_MDS_PDP_IPV6,
    TUYA_MDS_PDP_IPV4V6
}TUYA_MDS_PDP_TYPE_E;

/**
 * @brief Cellular network status change notification function prototype, this interface is for svc_netmgr adaptation
 * @param simId SIM card ID
 * @param state Cellular network status, see @TUYA_CELLULAR_MDS_NET_STATUS_E definition
 */
typedef void (*TKL_MDS_NOTIFY)(uint8_t sim_id, TUYA_CELLULAR_MDS_NET_STATUS_E st);

/**
 * @brief Initialize cellular mobile data service
 * @param simId SIM card ID
 * @return 0 success, others failure
 */
OPERATE_RET tkl_cellular_mds_init(uint8_t sim_id);

/**
 * @brief Get the authentication status of cellular mobile data service
 * @param simId SIM card ID
 * @return Cellular mobile data authentication status, see @TUYA_CELLULAR_MDS_STATUS_E definition
 */
TUYA_CELLULAR_MDS_STATUS_E tkl_cellular_mds_get_status(uint8_t sim_id);

/**
 * @brief Get the authentication status of cellular mobile data service
 * @param simId SIM card ID
 * @return Cellular mobile data authentication status, see @TUYA_CELLULAR_MDS_STATUS_E definition
 */
TUYA_CELLULAR_MDS_STATUS_E tkl_cellular_mds_adv_get_status(uint8_t sim_id,uint8_t cid);

/**
 * @brief Cellular mobile data PDP activation, default using CID 1
 * @param simId SIM card ID
 * @param apn Carrier APN setting
 * @param username Username
 * @param password Password
 *
 * @return 0 success, others failure
 */
OPERATE_RET tkl_cellular_mds_pdp_active(uint8_t sim_id,char * apn, char * username, char * password);

/**
 * @brief Cellular mobile data specified CID PDP activation
 * @param simId SIM card ID
 * @param cid Specify the PDP Context Identifier
 * @param apn Carrier APN setting
 * @param username Username
 * @param password Password
 *
 * @return 0 success, others failure
 */
OPERATE_RET tkl_cellular_mds_adv_pdp_active(uint8_t sim_id,uint8_t cid,TUYA_MDS_PDP_TYPE_E pdp_type,char * apn, char * username, char * password);

/**
 * @brief Cellular mobile data PDP deactivation, default using CID 1
 * @param simId SIM card ID
 * @return 0 success, others failure
 */
OPERATE_RET tkl_cellular_mds_pdp_deactive(uint8_t sim_id);

/**
 * @brief Cellular mobile data specified CID PDP deactivation
 * @param simId SIM card ID
 * @param cid Specify the PDP Context Identifier
 * @return 0 success, others failure
 */
OPERATE_RET tkl_cellular_mds_adv_pdp_deactive(uint8_t sim_id,uint8_t cid);

/**
 * @brief Cellular mobile data PDP automatic reactivation setting
 * @param simId SIM card ID
 * @param enable TRUE enable automatic reactivation, FALSE disable automatic reactivation
 *
 * @return 0 success, others failure
 */
OPERATE_RET tkl_cellular_mds_pdp_auto_reactive(uint8_t sim_id,bool enable);

/**
 * @brief Register cellular data service status change notification function
 * @param fun Status change notification function
 * @return 0 success, others failure
 */
OPERATE_RET tkl_cellular_mds_register_state_notify(uint8_t sim_id, TKL_MDS_NOTIFY fun);

/**
 * @brief   Get device ip address.
 * @param   ip: The type of NW_IP_S
 * @return  OPRT_OK: success  Other: fail
 */
OPERATE_RET tkl_cellular_mds_get_ip(uint8_t sim_id,NW_IP_S *ip);

/**
 * @brief   Get device ip address.
 * @param   ip: The type of NW_IP_S
 * @return  OPRT_OK: success  Other: fail
 */
OPERATE_RET tkl_cellular_mds_adv_get_ip(uint8_t sim_id,uint8_t cid,NW_IP_S *ip);

#ifdef __cplusplus
}
#endif

#endif

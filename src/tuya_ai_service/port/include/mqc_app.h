/**
 * @file mqc_app.h
 * @brief mqc_app module is used to
 * @version 0.1
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#ifndef __MQC_APP_H__
#define __MQC_APP_H__

#include "tuya_cloud_types.h"

#include "tuya_ai_types.h"

#include "http_manager.h"
#include "tuya_svc_netmgr_linkage.h"

#ifdef __cplusplus
extern "C" {
#endif

/***********************************************************
************************macro define************************
***********************************************************/

/***********************************************************
***********************typedef define***********************
***********************************************************/
/**
 * @brief Callback to handle protocol data
 *
 * @param[in] root_json Json encoded protocol data
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
typedef OPERATE_RET (*mqc_protocol_handler_cb)(IN ty_cJSON *root_json);

/***********************************************************
********************function declaration********************
***********************************************************/
/**
 * @brief Register handler for specific protocol
 *
 * @param[in] mq_pro Protocol ID, see PRO_XX defined above
 * @param[in] handler Callback when protocol data got
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET mqc_app_register_cb(uint32_t mq_pro, mqc_protocol_handler_cb handler);

/**
 * @brief Unregister handler for specific protocol
 *
 * @param[in] mq_pro Protocol ID, see PRO_XX defined above
 * @param[in] handler Callback when protocol data got
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET mqc_app_unregister_cb(uint32_t mq_pro, mqc_protocol_handler_cb handler);

/**
 * @brief Publish protocol data to the default topic
 *
 * @param[in] protocol Protocol ID, see PRO_XX defined above
 * @param[in] p_data The data to be published
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET mqc_send_custom_mqtt_msg(IN CONST uint32_t protocol, IN CONST uint8_t *p_data);

/**
 * @brief Get current linakge for mqtt connection
 *
 * @param[in] linkage Current linkage
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET mqc_get_connection_linkage(netmgr_linkage_t **linkage);

#ifdef __cplusplus
}
#endif

#endif /* __MQC_APP_H__ */

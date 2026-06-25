/**
 * @file app_ir_service.h
 * @brief IR cloud service integration for the smart_speaker application.
 * @copyright Copyright (c) 2026 Tuya Inc. All Rights Reserved.
 */

#ifndef __APP_IR_SERVICE_H__
#define __APP_IR_SERVICE_H__

#include "tuya_cloud_types.h"
#include "dp_schema.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize the IR cloud service: register the IR hardware driver and
 *        start the cloud IR control/study/download service.
 *        Call after board_register_hardware().
 *
 * @return OPRT_OK on success. Others on error.
 */
OPERATE_RET app_ir_service_init(void);

/**
 * @brief Notify the IR service that MQTT is connected. Selects the cloud IR
 *        code library (requires the device to be online). Safe to call more
 *        than once; the library is selected only once.
 */
void app_ir_service_on_mqtt_connected(void);

/**
 * @brief Route a received object dp batch to the IR system dp path (DP-201).
 *        Call from the TUYA_EVENT_DP_RECEIVE_OBJ handler.
 *
 * @param[in] dpobj: received object dp batch
 *
 * @return OPRT_OK on success. Others on error.
 */
OPERATE_RET app_ir_dp_process(dp_obj_recv_t *dpobj);

#ifdef __cplusplus
}
#endif

#endif /* __APP_IR_SERVICE_H__ */

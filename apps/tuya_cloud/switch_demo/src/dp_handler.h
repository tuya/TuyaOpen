#ifndef DP_HANDLER_H
#define DP_HANDLER_H

#include "tuya_iot.h"
#include "tuya_iot_dp.h"

#ifdef __cplusplus
extern "C" {
#endif

VOID dp_handler_mqtt_connected(void);
OPERATE_RET dp_handler_init(tuya_iot_client_t *client);
VOID dp_handler_deinit(void);
OPERATE_RET dp_obj_cmd_handler(tuya_iot_client_t *client, dp_obj_recv_t *dpobj);

#ifdef __cplusplus
}
#endif

#endif

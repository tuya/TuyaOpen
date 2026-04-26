/**
 * @file dp_handler.c
 * @brief DP点数据处理模块 - 处理云端下发的控制指令和传感器数据上报
 */

#include "dp_handler.h"
#include "tal_api.h"
#include "tal_thread.h"
#include "tal_system.h"
#include "../sensor/Sensor_Manager.h"

#define DP_ID_TEMP      101
#define DP_ID_HUM       102
#define DP_ID_LIGHT     103
#define DP_ID_PERSON    104
#define DP_ID_FAN       105
#define DP_ID_LED       106
#define DP_ID_SERVO     107
#define DP_ID_CO2       108

STATIC THREAD_HANDLE g_sensor_thread = NULL;
STATIC BOOL_T g_sensor_thread_running = FALSE;
STATIC tuya_iot_client_t *g_tuya_client = NULL;
STATIC TIME_MS g_mqtt_connected_time = 0;

extern BOOL_T g_tuya_mqtt_connected;

VOID dp_handler_mqtt_connected(void)
{
    g_mqtt_connected_time = tal_system_get_millisecond();
}

STATIC VOID sensor_report_once(void)
{
    OPERATE_RET op_ret = Sensor_Manager_Update();
    if (op_ret != OPRT_OK) {
        PR_WARN("Sensor update failed: %d", op_ret);
        return;
    }

    SensorData_t *data = Sensor_Manager_GetData();
    if (data == NULL || data->valid == FALSE) {
        PR_WARN("Sensor data invalid");
        return;
    }

    dp_obj_t dps[5] = {0};
    UINT_T dp_cnt = 0;

    dps[dp_cnt].id = DP_ID_TEMP;
    dps[dp_cnt].type = PROP_VALUE;
    dps[dp_cnt].time_stamp = data->timestamp;
    dps[dp_cnt].value.dp_value = (INT_T)data->temperature;
    dp_cnt++;

    dps[dp_cnt].id = DP_ID_HUM;
    dps[dp_cnt].type = PROP_VALUE;
    dps[dp_cnt].time_stamp = data->timestamp;
    dps[dp_cnt].value.dp_value = (INT_T)data->humidity;
    dp_cnt++;

    dps[dp_cnt].id = DP_ID_LIGHT;
    dps[dp_cnt].type = PROP_VALUE;
    dps[dp_cnt].time_stamp = data->timestamp;
    dps[dp_cnt].value.dp_value = data->light;
    dp_cnt++;

    dps[dp_cnt].id = DP_ID_PERSON;
    dps[dp_cnt].type = PROP_BOOL;
    dps[dp_cnt].time_stamp = data->timestamp;
    dps[dp_cnt].value.dp_bool = data->person_detect;
    dp_cnt++;

    dps[dp_cnt].id = DP_ID_CO2;
    dps[dp_cnt].type = PROP_VALUE;
    dps[dp_cnt].time_stamp = data->timestamp;
    dps[dp_cnt].value.dp_value = data->co2;
    dp_cnt++;

    if (g_tuya_client == NULL || g_tuya_mqtt_connected == FALSE || g_tuya_client->is_activated == FALSE ||
        g_tuya_client->activate.devid[0] == '\0') {
        PR_DEBUG("Tuya not ready, skip DP report");
        return;
    }

    if (g_mqtt_connected_time != 0 && (tal_system_get_millisecond() - g_mqtt_connected_time) < 5000) {
        PR_DEBUG("Tuya MQTT just connected, delay DP report");
        return;
    }

    op_ret = tuya_iot_dp_obj_report(g_tuya_client, g_tuya_client->activate.devid, dps, dp_cnt, 0);
    if (op_ret != OPRT_OK) {
        PR_ERR("DP report failed: %d", op_ret);
    } else {
        PR_NOTICE("DP reported: temp=%.1f hum=%.1f light=%u pir=%d co2=%u",
                  data->temperature, data->humidity, data->light, data->person_detect, data->co2);
    }
}

STATIC VOID sensor_report_thread(void *arg)
{
    (void)arg;

    while (g_sensor_thread_running) {
        sensor_report_once();
        tal_system_sleep(2000);
    }
}

OPERATE_RET dp_obj_cmd_handler(tuya_iot_client_t *client, dp_obj_recv_t *dpobj)
{
    (void)client;

    if (dpobj == NULL || dpobj->dps == NULL) {
        return OPRT_INVALID_PARM;
    }

    for (UINT_T i = 0; i < dpobj->dpscnt; i++) {
        dp_obj_t *dp = &dpobj->dps[i];

        switch (dp->id) {
        case DP_ID_FAN:
            if (dp->type == PROP_BOOL) {
                Sensor_Ctrl_Fan(dp->value.dp_bool);
            }
            break;

        case DP_ID_LED:
            if (dp->type == PROP_VALUE) {
                Sensor_Ctrl_Led((UINT_T)dp->value.dp_value);
            }
            break;

        case DP_ID_SERVO:
            if (dp->type == PROP_VALUE) {
                UINT_T angle = (UINT_T)dp->value.dp_value;
                if (angle > 180) {
                    angle = 180;
                }
                Sensor_Ctrl_Servo(angle);
            }
            break;

        default:
            PR_WARN("Unhandled DP ID: %d", dp->id);
            break;
        }
    }

    return OPRT_OK;
}

OPERATE_RET dp_handler_init(tuya_iot_client_t *client)
{
    OPERATE_RET op_ret = Sensor_Manager_Init();
    if (op_ret != OPRT_OK) {
        PR_ERR("Sensor manager init failed: %d", op_ret);
        return op_ret;
    }

    g_tuya_client = client;
    if (g_sensor_thread != NULL) {
        return OPRT_OK;
    }

    g_sensor_thread_running = TRUE;
    THREAD_CFG_T thread_cfg = {
        .stackDepth = 1024 * 4,
        .priority = THREAD_PRIO_2,
        .thrdname = "sensor_report",
    };

    op_ret = tal_thread_create_and_start(&g_sensor_thread, NULL, NULL, sensor_report_thread, NULL, &thread_cfg);
    if (op_ret != OPRT_OK) {
        PR_ERR("Sensor report thread create failed: %d", op_ret);
        g_sensor_thread_running = FALSE;
        g_sensor_thread = NULL;
        return op_ret;
    }

    PR_NOTICE("DP handler initialized successfully");
    return OPRT_OK;
}

VOID dp_handler_deinit(void)
{
    if (g_sensor_thread != NULL) {
        g_sensor_thread_running = FALSE;
        tal_thread_delete(g_sensor_thread);
        g_sensor_thread = NULL;
    }
    g_tuya_client = NULL;
}

/**
 * @file tuya_iot_dp.c
 * @brief Implementation of Tuya IoT data point (DP) synchronization functions.
 *
 * This file contains the implementation of functions for synchronizing data
 * points (DPs) between Tuya IoT devices and the Tuya cloud platform. It
 * includes mechanisms for initiating DP synchronization, handling DP
 * synchronization callbacks, and setting the status of DPs based on the
 * synchronization result. The file supports conditional compilation for
 * Bluetooth-enabled devices, allowing for DP synchronization over Bluetooth
 * when available.
 *
 * The DP synchronization process is crucial for ensuring that the state of IoT
 * devices is consistent with the Tuya cloud platform, enabling real-time
 * monitoring and control of devices from remote locations.
 *
 * @copyright Copyright (c) 2021-2024 Tuya Inc. All Rights Reserved.
 *
 */

#include "dp_schema.h"
#include "tuya_iot_dp.h"
#include "tal_log.h"
#include "tuya_lan.h"
#include "tal_api.h"
#include "mix_method.h"

#ifdef ENABLE_BLUETOOTH
#include "ble_mgr.h"
#include "ble_dp.h"
#endif

static DELAYED_WORK_HANDLE s_tmm_dp_sync = NULL;

int tuya_iot_dp_sync_start(tuya_iot_client_t *client, uint32_t timeout_s);

static void dp_sync_cb(int result, void *user_data)
{
    dp_rept_valid_t *dpvalid = (dp_rept_valid_t *)user_data;

    if (OPRT_OK == result) {
        for (int i = 0; i < dpvalid->num; i++) {
            dp_pv_stat_set(dpvalid->schema, dpvalid->dpid[i], PV_STAT_CLOUD);
        }
    } else {
        //! start mqtt cloud sync
        tuya_iot_dp_sync_start(tuya_iot_client_get(), 5);
    }

    tal_free(dpvalid);
}

/**
 * @brief Processes the synchronization of device data points.
 *
 * This function is responsible for processing the synchronization of device
 * data points. It takes a pointer to the data as a parameter.
 *
 * @param data A pointer to the data to be processed.
 */
void tuya_iot_dp_sync_process(void *data)
{
    tuya_iot_client_t *client = (tuya_iot_client_t *)data;

    if (!tuya_iot_is_connected()) {
        //! continue;
        tal_workq_start_delayed(s_tmm_dp_sync, 5000, LOOP_ONCE);
        return;
    }

    dp_rept_valid_t *dpvalid = NULL;
    char *dpsjson = NULL;

    int ret = dp_obj_dump_stat_local_json(client->activate.devid, &dpvalid, &dpsjson, 0);
    if (OPRT_OK != ret) {
        PR_ERR("dp sync stat local failed %d", ret);
        tal_workq_start_delayed(s_tmm_dp_sync, 5000, LOOP_ONCE);
        return;
    }

    tuya_iot_dp_report_json_async(client, dpsjson, NULL, dp_sync_cb, dpvalid, 5000);
    tal_free(dpsjson);
}

/**
 * @brief Initiates the synchronization of device data points with the Tuya IoT
 * platform.
 *
 * This function is used to start the synchronization of device data points with
 * the Tuya IoT platform.
 *
 * @param client Pointer to the Tuya IoT client structure.
 * @param timeout_s Timeout value in seconds for the synchronization process.
 *
 * @return Returns 0 on success, or a negative error code on failure.
 */
int tuya_iot_dp_sync_start(tuya_iot_client_t *client, uint32_t timeout_s)
{
    int ret = OPRT_OK;

    if (NULL == s_tmm_dp_sync) {
        ret = tal_workq_init_delayed(WORKQ_HIGHTPRI, tuya_iot_dp_sync_process, client, &s_tmm_dp_sync);
        if (OPRT_OK != ret) {
            PR_DEBUG("do sync worqk init failed\r\n");
            return ret;
        }
    }

    return tal_workq_start_delayed(s_tmm_dp_sync, timeout_s * 1000, LOOP_ONCE);
}

/**
 * @brief Dispatches an event for the Tuya IoT data point (DP).
 *
 * This function is responsible for dispatching an event for the Tuya IoT data
 * point (DP). It takes the type of the DP, the DP data, and user-specific data
 * as parameters.
 *
 * @param type The type of the DP.
 * @param dp_data The DP data.
 * @param user_data User-specific data.
 */
void tuya_iot_dp_event_dispatch(dp_type_t type, void *dp_data, void *user_data)
{
    tuya_iot_client_t *client = (tuya_iot_client_t *)user_data;

    if (client->config.event_handler) {
        tuya_event_msg_t event;

        event.id = T_OBJ == type ? TUYA_EVENT_DP_RECEIVE_OBJ : TUYA_EVENT_DP_RECEIVE_RAW;
        if (T_OBJ == type) {
            event.id = TUYA_EVENT_DP_RECEIVE_OBJ;
            event.value.dpobj = (dp_obj_recv_t *)dp_data;
        } else if (T_RAW == type) {
            event.id = TUYA_EVENT_DP_RECEIVE_RAW;
            event.value.dpraw = (dp_raw_recv_t *)dp_data;
        } else {
            return;
        }
        client->config.event_handler(client, &event);
    }
}

static void tuya_iot_dp_parse_on_worq(void *args)
{
    dp_recv_msg_t *msg = (dp_recv_msg_t *)args;

    int op_ret = dp_data_recv_parse(msg, tuya_iot_dp_event_dispatch);
    if (OPRT_OK != op_ret) {
        PR_ERR("handle_recv_dp err:%d", op_ret);
    }

    cJSON_Delete(msg->data_js);
    tal_free(msg);
}

/**
 * @brief Parses the device data point command received from the Tuya IoT
 * platform.
 *
 * This function is responsible for parsing the device data point command
 * received from the Tuya IoT platform.
 *
 * @param client The Tuya IoT client instance.
 * @param cmd_tp The type of the data point command.
 * @param cmd_js The JSON object containing the data point command.
 *
 * @return The status of the parsing operation.
 *     - 0: Success
 *     - Other values: Error codes
 */
int tuya_iot_dp_parse(tuya_iot_client_t *client, dp_cmd_type_t cmd_tp, cJSON *cmd_js)
{
    cJSON *data = cmd_js;
    if (data == NULL) {
        PR_ERR("data null");
        return OPRT_CJSON_GET_ERR;
    }
    char *devId = NULL;
    cJSON *item = cJSON_GetObjectItem(data, "devId");
    if (NULL == item) {
        PR_WARN("devid is null");
        devId = client->activate.devid;
    } else {
        devId = item->valuestring;
    }

    dp_recv_msg_t *msg = tal_malloc(sizeof(dp_recv_msg_t));
    if (NULL == msg) {
        return OPRT_MALLOC_FAILED;
    }
    msg->cmd = cmd_tp;
    msg->devid = devId;
    msg->dt_tp = DTT_SCT_UNC;
    msg->data_js = cmd_js;
    msg->user_data = client;

    return tal_workq_schedule(WORKQ_HIGHTPRI, tuya_iot_dp_parse_on_worq, msg);
}

/**
 * @brief Reports device object data to the Tuya IoT cloud service.
 *
 * This function is used to report the device object data to the Tuya IoT cloud
 * service.
 *
 * @param client The Tuya IoT client instance.
 * @param devid The device ID.
 * @param dps An array of device object data.
 * @param dpscnt The number of device object data elements in the array.
 * @param flags Additional flags for the report.
 *
 * @return The result of the operation. Returns 0 on success, or a negative
 * error code on failure.
 */
int tuya_iot_dp_obj_report(tuya_iot_client_t *client, const char *devid, dp_obj_t *dps, uint16_t dpscnt, int flags)
{
    int ret = OPRT_OK;

    if (!client->is_activated) {
        PR_DEBUG("client no active");
        return OPRT_COM_ERROR;
    }
    if (NULL == dps || 0 == dpscnt) {
        return OPRT_INVALID_PARM;
    }

    dp_schema_t *schema = dp_schema_find(devid);
    if (NULL == schema) {
        return OPRT_INVALID_PARM;
    }

    dp_rept_valid_t *dpvalid = tal_malloc(sizeof(dp_rept_valid_t) + sizeof(uint8_t) * dpscnt);
    if (NULL == dpvalid) {
        return OPRT_MALLOC_FAILED;
    }
    memset(dpvalid, 0, sizeof(sizeof(dp_rept_valid_t) + sizeof(uint8_t) * dpscnt));

    PR_DEBUG("dp report: devid %s, dps 0x%08x, dpscnt %d, flags %d", devid ? devid : "null", dps, dpscnt, flags);

    dp_rept_in_t dpin;

    dpin.dps = dps;
    dpin.dpscnt = dpscnt;
    dpin.flags = flags;
    dpin.rept_type = T_OBJ_REPT;

    ret = dp_rept_valid_check(schema, &dpin, dpvalid);
    if (OPRT_OK != ret) {
        tal_free(dpvalid);
        return ret;
    }

#ifdef ENABLE_BLUETOOTH
    //! BLE TLV FORMAT
    if (tuya_ble_is_connected()) {
        dp_rept_in_t *ble_dpin = NULL;

        ble_dpin = tal_malloc(sizeof(dp_rept_in_t) + sizeof(dp_obj_t) * dpvalid->num);
        if (NULL == ble_dpin) {
            tal_free(dpvalid);
            return OPRT_MALLOC_FAILED;
        }
        ble_dpin->flags = flags;
        ble_dpin->rept_type = T_OBJ_REPT;
        ble_dpin->dpscnt = dpvalid->num;
        ble_dpin->dps = (dp_obj_t *)(ble_dpin + sizeof(dp_rept_in_t));
        //! copy vaild dpid
        int i, j;
        for (i = 0; i < dpvalid->num; i++) {
            for (j = 0; j < dpin.dpscnt; j++) {
                if (dpvalid->dpid[i] != dpin.dps[j].id) {
                    continue;
                }
                memcpy(&ble_dpin->dps[i], &dpin.dps[j], sizeof(dp_obj_t));
            }
        }
        PR_DEBUG("ble channel report");
        ret = tuya_ble_dp_report(ble_dpin);
        tal_free(ble_dpin);
        tal_free(dpvalid);
        tuya_iot_dp_sync_start(client, 5);

        return ret;
    }
#endif

    dp_rept_out_t dpout;

    memset(&dpout, 0, sizeof(dpout));

    ret = dp_rept_json_output(schema, &dpin, dpvalid, &dpout);
    if (OPRT_OK != ret) {
        PR_DEBUG("dp rept json output error %d", ret);
        return ret;
    }

    if (tuya_lan_is_connected()) {
        char *out = NULL;
        PR_DEBUG("lan channel report");
        dp_rept_json_append(schema, dpout.dpsjson, NULL, NULL, 0, &out);
        ret = tuya_lan_dp_report(out);
        tal_free(out);
        tal_free(dpvalid);
        tuya_iot_dp_sync_start(client, 5);
    } else if (tuya_iot_is_connected()) {
        PR_DEBUG("mqtt channel report");
        ret = tuya_iot_dp_report_json_with_notify(client, dpout.dpsjson, NULL, dp_sync_cb, dpvalid, 5000);
    } else {
        PR_ERR("no channel for connect");
    }

    if (dpout.dpsjson) {
        tal_free(dpout.dpsjson);
    }
    if (dpout.timejson) {
        tal_free(dpout.timejson);
    }

    return ret;
}

/**
 * @brief Dumps the object representation of the Tuya IoT data point (DP) for a
 * specific device.
 *
 * This function is used to retrieve the object representation of the Tuya IoT
 * data point (DP) for a specific device. The object representation includes
 * information such as the device ID, client information, and flags.
 *
 * @param client The Tuya IoT client.
 * @param devid The device ID for which the DP object is to be dumped.
 * @param flags The flags to control the dumping process.
 *
 * @return A pointer to the dumped DP object.
 */
char *tuya_iot_dp_obj_dump(tuya_iot_client_t *client, char *devid, int flags)
{
    if (!client->is_activated) {
        return NULL;
    }

    if (NULL == devid) {
        devid = client->activate.devid;
    }

    return dp_obj_dump_all_json(devid, flags);
}

/**
 * @brief Dumps the local status of the device's data points.
 *
 * This function is used to dump the local status of the device's data points.
 *
 * @param client The Tuya IoT client.
 * @param dpvalid The structure containing the validity information of the data
 * points.
 * @param flags The flags indicating additional options for dumping the local
 * status.
 *
 * @return The status of the operation.
 *         - 0: Success.
 *         - Other values: Error code.
 */
int tuya_iot_dp_stat_local_dump(tuya_iot_client_t *client, dp_rept_valid_t *dpvalid, int flags)
{
    return OPRT_OK;
}

static void dp_raw_async_cb(int result, void *user_data)
{
    PR_DEBUG("mqtt raw dp send result %d", result);
    //! TODO: event report
}

/**
 * @brief Reports raw data points to the Tuya IoT platform.
 *
 * This function is used to report raw data points to the Tuya IoT platform.
 *
 * @param client The Tuya IoT client instance.
 * @param devid The device ID.
 * @param dp The raw data points to be reported.
 * @param timeout The timeout value for the report operation.
 *
 * @return The result of the report operation.
 *         - 0: Success.
 *         - Other values: Error codes.
 */
int tuya_iot_dp_raw_report(tuya_iot_client_t *client, const char *devid, dp_raw_t *dp, uint32_t timeout)
{
    int ret = OPRT_OK;

    if (!client->is_activated) {
        PR_DEBUG("client no active");
        return OPRT_COM_ERROR;
    }

    dp_schema_t *schema = dp_schema_find(devid ? devid : client->activate.devid);
    if (NULL == schema) {
        return OPRT_INVALID_PARM;
    }

#ifdef ENABLE_BLUETOOTH
    //! BLE TLV FORMAT
    if (tuya_ble_is_connected()) {
        dp_rept_in_t dpin;

        dpin.dp = dp;
        dpin.rept_type = T_RAW_REPT;

        ret = tuya_ble_dp_report(&dpin);
        return ret;
    }
#endif

    uint32_t encode_len = (dp->len / 3) * 4 + ((dp->len % 3) ? 4 : 0) + 20 + 1;

    dp_rept_out_t dpout;

    memset(&dpout, 0, sizeof(dpout));

    dpout.dpsjson = tal_malloc(encode_len);

    if (NULL == dpout.dpsjson) {
        return OPRT_MALLOC_FAILED;
    }

    uint32_t offset = sprintf(dpout.dpsjson, "{\"%d\":\"", dp->id);
    tuya_base64_encode(dp->data, dpout.dpsjson + offset, dp->len);
    strcpy(dpout.dpsjson + strlen(dpout.dpsjson), "\"}");

    if (tuya_lan_is_connected()) {
        char *out = NULL;
        dp_rept_json_append(schema, dpout.dpsjson, NULL, NULL, 0, &out);
        ret = tuya_lan_dp_report(out);
        tal_free(out);
    } else if (tuya_iot_is_connected()) {
        ret = tuya_iot_dp_report_json_async(client, dpout.dpsjson, NULL, dp_raw_async_cb, NULL, timeout);
    } else {
        PR_ERR("no channel for connect");
    }

    if (dpout.dpsjson) {
        tal_free(dpout.dpsjson);
    }

    return ret;
}

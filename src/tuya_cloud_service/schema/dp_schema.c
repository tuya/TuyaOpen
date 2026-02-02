/**
 * @file dp_schema.c
 * @brief This file contains the implementation of device property (DP) schema
 * management and processing functions. It includes functions for DP schema
 * creation, finding, updating, and reporting DP changes in JSON format.
 *        Additionally, it handles the parsing of received DP data and the
 * generation of responses based on the schema definitions. The file is part of
 * a larger framework aimed at device management and communication in IoT
 * applications.
 *
 * @copyright Copyright (c) 2021-2024 Tuya Inc. All Rights Reserved.
 *
 */

#include <inttypes.h>
#include <stdarg.h>
#include "tuya_cloud_types.h"
#include "dp_schema.h"
#include "cJSON.h"
#include "mix_method.h"
#include "tal_api.h"

#define MAX_ITEM_LEN 1024

#define MAX_TRANS_TYPE_NUM (DTT_SCT_SCENE + 1)

#define DP_SCHEMA_NUM_MAX 1

typedef struct {
    // DELAYED_WORK_HANDLE tmm_dp_sync;
    uint16_t serial_no;
    MUTEX_HANDLE mutex;
    uint8_t schema_num;
    dp_schema_t *schema_list[DP_SCHEMA_NUM_MAX];
} dp_schema_mgr_t;

static dp_schema_mgr_t s_dsmgr = {0};

static bool dp_snprintf_append(char *buf, size_t buf_len, size_t *offset, const char *fmt, ...)
{
    if (buf == NULL || offset == NULL || *offset >= buf_len) {
        return false;
    }

    va_list args;
    va_start(args, fmt);
    int ret = vsnprintf(buf + *offset, buf_len - *offset, fmt, args);
    va_end(args);

    if (ret < 0 || (size_t)ret >= buf_len - *offset) {
        return false;
    }

    *offset += (size_t)ret;
    return true;
}

/**
 * @brief Appends a JSON string to the given data with the specified time, type,
 * and repetition sequence.
 *
 * This function is used to append a JSON string to the provided data, along
 * with the specified time, type, and repetition sequence.
 *
 * @param schema Pointer to the dp_schema_t structure.
 * @param data Pointer to the data string to which the JSON string will be
 * appended.
 * @param time Pointer to the time string to be included in the JSON string.
 * @param type Pointer to the type string to be included in the JSON string.
 * @param rept_seq The repetition sequence number to be included in the JSON
 * string.
 * @param pp_out Pointer to the output string that will contain the appended
 * JSON string.
 *
 * @return Returns 0 on success, or a negative error code on failure.
 */
int dp_rept_json_append(dp_schema_t *schema, char *data, char *time, char *type, uint8_t rept_seq, char **pp_out)
{
    if (schema == NULL || data == NULL || NULL == pp_out) {
        PR_ERR("para null");
        return OPRT_INVALID_PARM;
    }

    int len = 0;

    if (NULL == time) {
        len = strlen(data) + 128;
    } else {
        len = strlen(data) + strlen(time) + 128;
    }
    char *tmp = tal_malloc(len);
    if (NULL == tmp) {
        PR_ERR("tal_malloc err:%d", len);
        return OPRT_MALLOC_FAILED;
    }
    memset(tmp, 0, len);

    int offset = 0;
    int ret = 0;
    ret = snprintf(tmp + offset, len - offset, "{\"dps\":%s,\"devId\":\"%s\"", data, schema->devid);
    if (ret < 0 || ret >= len - offset) {
        goto __err_exit;
    }
    offset += ret;
    if (time) {
        ret = snprintf(tmp + offset, len - offset, ",\"t\":%s", time);
        if (ret < 0 || ret >= len - offset) {
            goto __err_exit;
        }
        offset += ret;
    }
    if (rept_seq > 0) {
        ret = snprintf(tmp + offset, len - offset, ",\"seq\":\"%u\"", rept_seq);
        if (ret < 0 || ret >= len - offset) {
            goto __err_exit;
        }
        offset += ret;
    }
    if (type) {
        ret = snprintf(tmp + offset, len - offset, ",\"type\":\"%s\"", type);
        if (ret < 0 || ret >= len - offset) {
            goto __err_exit;
        }
        offset += ret;
    }
    tmp[offset] = '}';
    *pp_out = tmp;

    return OPRT_OK;

__err_exit:
    tal_free(tmp);
    PR_ERR("snprintf %d", offset);
    return OPRT_COM_ERROR;
}

/**
 * Finds a DP node in the given schema based on the provided ID.
 *
 * @param schema The DP schema to search in.
 * @param id The ID of the DP node to find.
 * @return A pointer to the found DP node, or NULL if not found.
 */
dp_node_t *dp_node_find(dp_schema_t *schema, int id)
{
    int i;
    dp_node_t *dpnode = NULL;

    for (i = 0; i < schema->num; i++) {
        if (schema->node[i].desc.id == id) {
            dpnode = &schema->node[i];
            break;
        }
    }
    return dpnode;
}

/**
 * Finds the data point schema for a given device ID.
 *
 * @param devid The device ID to search for.
 * @return A pointer to the data point schema if found, or NULL if not found.
 */
dp_schema_t *dp_schema_find(const char *devid)
{
    int i = 0;

    PR_TRACE("try to find schema devid %s", devid);
    dp_schema_mgr_t *dsmgr = &s_dsmgr;
    for (i = 0; i < DP_SCHEMA_NUM_MAX; i++) {
        if (NULL == dsmgr->schema_list[i]) {
            continue;
        }
        if (0 == strcmp(devid, dsmgr->schema_list[i]->devid)) {
            return dsmgr->schema_list[i];
        }

        PR_TRACE("find schema devid %s, not match!", dsmgr->schema_list[i]->devid);
    }

    return NULL;
}

/**
 * Finds a DP node by device ID and ID.
 *
 * This function searches for a DP node in the schema based on the given device
 * ID and ID.
 *
 * @param devid The device ID to search for.
 * @param id The ID of the DP node to search for.
 * @return A pointer to the found DP node, or NULL if not found.
 */
dp_node_t *dp_node_find_by_devid(char *devid, int id)
{
    int i;
    dp_node_t *dpnode = NULL;

    dp_schema_t *schema = dp_schema_find(devid);
    if (NULL == schema) {
        return NULL;
    }
    for (i = 0; i < schema->num; i++) {
        if (schema->node[i].desc.id == id) {
            dpnode = &schema->node[i];
            break;
        }
    }
    return dpnode;
}

static __attribute__((unused)) OPERATE_RET dp_obj_equal_resp(dp_schema_t *schema, uint8_t *dpid, uint8_t num,
                                                             dp_cmd_type_t cmd_tp)
{
    if (NULL == schema || 0 == num) {
        PR_ERR("para err");
        return OPRT_INVALID_PARM;
    }
    OPERATE_RET op_ret = OPRT_OK;

    cJSON *qr_data = cJSON_CreateObject();
    if (NULL == qr_data) {
        PR_ERR("json err");
        return OPRT_CR_CJSON_ERR;
    }

    int i;
    tal_mutex_lock(schema->mutex);
    for (i = 0; i < num; i++) {
        int id = dpid[i];
        dp_node_t *dpnode = dp_node_find(schema, id);
        if (dpnode == NULL) {
            PR_ERR("dp id Invalid %d", id);
            continue;
        }

        if ((dpnode->desc.mode == M_WR) || (dpnode->desc.type != T_OBJ) || (dpnode->pv_stat == PV_STAT_INVALID) ||
            ((schema->actv.preprocess == TRUE) && (dpnode->desc.passive == PSV_TRUE))) {
            PR_ERR("dp id %d Skip", id);
            continue;
        }

        char dpid[10];
        snprintf(dpid, 10, "%d", id);

        switch (dpnode->desc.prop_tp) {
        case PROP_BOOL: {
            cJSON_AddBoolToObject(qr_data, dpid, dpnode->prop.prop_bool.value);
        } break;
        case PROP_VALUE: {
            cJSON_AddNumberToObject(qr_data, dpid, dpnode->prop.prop_int.value);
        } break;
        case PROP_STR: {
            tal_mutex_lock(dpnode->prop.prop_str.dp_str_mutex);
            if (dpnode->prop.prop_str.value) {
                cJSON_AddStringToObject(qr_data, dpid, dpnode->prop.prop_str.value);
            } else {
                PR_WARN("DP:%d dpnode null", dpid);
            }
            tal_mutex_unlock(dpnode->prop.prop_str.dp_str_mutex);
        } break;
        case PROP_ENUM: {
            int cnt = dpnode->prop.prop_enum.value;
            cJSON_AddStringToObject(qr_data, dpid, dpnode->prop.prop_enum.pp_enum[cnt]);
        } break;
        case PROP_BITMAP: {
            cJSON_AddNumberToObject(qr_data, dpid, dpnode->prop.prop_bitmap.value);
        } break;

        default:
            break;
        }
    }
    tal_mutex_unlock(schema->mutex);

    char *tmp = NULL;
    tmp = cJSON_PrintUnformatted(qr_data);
    cJSON_Delete(qr_data);
    if (NULL == tmp) {
        PR_ERR("json err");
        return OPRT_MALLOC_FAILED;
    }
    cJSON_free(tmp);

    //! FIXME:

    // char *out = NULL;
    // op_ret = make_dpstr(schema, tmp, NULL, "query", 0, 0, &out);
    // tal_free(tmp);
    // if (OPRT_OK != op_ret) {
    //     PR_ERR("add_devid_time_to_dpstr err:%d", op_ret);
    //     return op_ret;
    // }
    // PR_DEBUG("resp:%s", out);

    //! FIXME:
    //     SMARTPOINTER_T *rfc_da = NULL;
    //     op_ret = __sf_mk_rfc_msg_data((uint8_t *)out, strlen(out),
    //     &rfc_da);//root ref tal_free(out); if (OPRT_OK != op_ret) {
    //         PR_ERR("mk_rfc_msg_data err:%d", op_ret);
    //         return op_ret;
    //     }

    // #if defined(ENABLE_TUYA_LAN) && (ENABLE_TUYA_LAN==1)
    //     // lan dp report
    //     tuya_smartpointer_get(rfc_da); //lan ref
    //     op_ret = tuya_svc_lan_dp_report(rfc_da, sizeof(SMARTPOINTER_T *));
    //     if (op_ret != OPRT_OK) {
    //         if (OPRT_SVC_LAN_NO_CLIENT_CONNECTED != op_ret) {
    //             PR_ERR("tuya_svc_lan_dp_report err:%d", op_ret);
    //         }
    //         tuya_smartpointer_put(rfc_da); //lan ref
    //     }
    // #endif

    //     op_ret = mqc_obj_dp_query(rfc_da);
    //     if (op_ret != OPRT_OK) {
    //         PR_ERR("mqc_obj_dp_query err:%d", op_ret);
    //     }

    //     tuya_smartpointer_put(rfc_da); //root ref

    return op_ret;
}

/**
 * Parses the received data and invokes the callback function.
 *
 * @param msg The pointer to the received message.
 * @param dp_recv_cb The callback function to be invoked.
 * @return The result of the parsing operation.
 */
int dp_data_recv_parse(dp_recv_msg_t *msg, dp_recv_cb_t dp_recv_cb)
{
    OPERATE_RET op_ret = OPRT_OK;
    uint16_t dpscnt = 0;
    dp_obj_recv_t *dpobj = NULL;
    dp_node_t *dpnode = NULL;
    cJSON *item = NULL;
    dp_schema_t *schema = dp_schema_find(msg->devid);
    cJSON *dps_js = cJSON_GetObjectItem(msg->data_js, "dps");

    if (NULL == schema || NULL == dps_js) {
        PR_ERR("dev null or no dps");
        return OPRT_COM_ERROR;
    }

    tal_mutex_lock(schema->mutex);
    for (item = dps_js->child; item != NULL; item = item->next) {
        dpnode = dp_node_find(schema, atoi(item->string));
        if (dpnode == NULL) {
            PR_ERR("DP ID %d Invalid", atoi(item->string));
            continue;
            ;
        }
        if ((schema->actv.preprocess == TRUE) && (dpnode->desc.passive == PSV_TRUE)) {
            dpnode->desc.passive = PSV_F_ONCE;
        }
        if (T_OBJ == dpnode->desc.type) {
            dpscnt++;
        }
    }
    tal_mutex_unlock(schema->mutex);

    if (dpscnt) {
        dpobj = (dp_obj_recv_t *)tal_malloc(sizeof(dp_obj_recv_t) + (dpscnt * sizeof(dp_obj_t)));
        if (NULL == dpobj) {
            PR_ERR("malloc err:%d", dpscnt);
            op_ret = OPRT_MALLOC_FAILED;
            goto __err_exit;
        }
        memset(dpobj, 0, (sizeof(dp_obj_recv_t) + (dpscnt * sizeof(dp_obj_t))));
        dpobj->cmd_tp = msg->cmd;
        dpobj->dtt_tp = (dp_trans_type_t)msg->dt_tp;
        dpobj->devid = msg->devid;
        dpobj->dpscnt = dpscnt;
    }
    /*
        Determine if the received DP is the same as the locally stored DP to
       decide whether to reply directly.
        */
    int i = 0;
    tal_mutex_lock(schema->mutex);
    for (item = dps_js->child; item != NULL; item = item->next) {
        dpnode = dp_node_find(schema, atoi(item->string));
        if (NULL == dpnode) {
            PR_ERR("DP ID %d Invalid", atoi(item->string));
            continue;
        }
        if (T_RAW == dpnode->desc.type && cJSON_String == item->type) { // raw dp process
            // dp_raw_t
            int data_len = sizeof(dp_raw_recv_t) + strlen(item->valuestring);
            dp_raw_recv_t *dpraw = tal_malloc(data_len);
            if (NULL == dpraw) {
                tal_mutex_unlock(schema->mutex);
                goto __err_exit;
            }
            memset(dpraw, 0, data_len);

            dpraw->devid = msg->devid;
            dpraw->cmd_tp = msg->cmd;
            dpraw->dp.id = dpnode->desc.id;
            dpraw->dtt_tp = msg->dt_tp;
            dpraw->dp.len = tuya_base64_decode(item->valuestring, dpraw->dp.data);

            if (dp_recv_cb) {
                tal_mutex_unlock(schema->mutex);
                dp_recv_cb(T_RAW, dpraw, msg->user_data);
                tal_mutex_lock(schema->mutex);
            }
            tal_free(dpraw);
            continue;
        }

        dpnode->pv_stat = PV_STAT_LOCAL;

        switch (dpnode->desc.prop_tp) {
        case PROP_BOOL: {
            if (!cJSON_IsBool(item)) {
                continue;
            }
            //! set value;
            if (cJSON_True == item->type) {
                dpobj->dps[i].value.dp_bool = TRUE;
            } else {
                dpobj->dps[i].value.dp_bool = FALSE;
            }
            break;
        }

        case PROP_VALUE: {
            if (cJSON_Number != item->type) {
                continue;
            }
            dpobj->dps[i].value.dp_value = item->valueint;
            break;
        }

        case PROP_STR: {
            if (item->type != cJSON_String) {
                continue;
            }
            dpobj->dps[i].value.dp_str = item->valuestring;
            break;
        }

        case PROP_ENUM: {
            if (item->type != cJSON_String) {
                break;
            }
            int j = 0;
            for (j = 0; j < dpnode->prop.prop_enum.cnt; j++) {
                if (0 == strcmp(dpnode->prop.prop_enum.pp_enum[j], item->valuestring)) {
                    break;
                }
            }
            if (j >= dpnode->prop.prop_enum.cnt) {
                PR_ERR("dp enum value[%s] invalid", item->valuestring);
                continue;
            }
            dpobj->dps[i].value.dp_enum = j;
            break;
        }

        case PROP_BITMAP: {
            dpobj->dps[i].value.dp_value = item->valueint;
            break;
        }

        default: {
            PR_ERR("dp prop_tp[%d] invalude", dpnode->desc.prop_tp);
            continue;
        }
        } /* end of switch */
        dpobj->dps[i].id = dpnode->desc.id;
        dpobj->dps[i].type = dpnode->desc.prop_tp;
        dpobj->dps[i].time_stamp = tal_time_get_posix();
        i++;
    } /* end of for */
    tal_mutex_unlock(schema->mutex);

    if (dpobj) {
        if (dp_recv_cb) {
            // tal_mutex_unlock(schema->mutex);
            dp_recv_cb(T_OBJ, dpobj, msg->user_data);
            // tal_mutex_lock(schema->mutex);
        }
    }

    op_ret = OPRT_OK;

__err_exit:
    if (dpobj) {
        tal_free(dpobj);
    }

    return op_ret;
}

/**
 * Retrieves the PV (Property Value) status for a specific data point (DP)
 * schema.
 *
 * @param schema The DP schema to retrieve the PV status from.
 * @param id The ID of the data point to retrieve the PV status for.
 * @return The PV status of the specified data point.
 */
dp_pv_stat_t dp_pv_stat_get(dp_schema_t *schema, uint8_t id)
{
    dp_node_t *dpnode = dp_node_find(schema, id);

    if (dpnode) {
        return dpnode->pv_stat;
    }

    return PV_STAT_INVALID;
}

/**
 * @brief Sets the PV (Present Value) status for a specific data point in the
 * schema.
 *
 * This function updates the PV status of a data point in the schema structure.
 *
 * @param schema Pointer to the schema structure.
 * @param id The ID of the data point.
 * @param pv_stat The PV status to be set.
 */
void dp_pv_stat_set(dp_schema_t *schema, uint8_t id, dp_pv_stat_t pv_stat)
{
    dp_node_t *dpnode = dp_node_find(schema, id);

    if (dpnode) {
        dpnode->pv_stat = pv_stat;
        PR_TRACE("dp[%d] pv stat %d", id, pv_stat);
    }
}

static bool dp_rept_update(dp_rept_type_t rept_type, dp_obj_t *dp, dp_node_t *dpnode, int flags)
{
    bool is_need_update = FALSE;

    switch (dpnode->desc.type) {
    case T_OBJ: {                                  /* obj type */
        if ((PV_STAT_INVALID == dpnode->pv_stat)   /* Local data status is invalid */
            || (PV_STAT_LOCAL == dpnode->pv_stat)  /* Local data status not uploaded */
            || (TRIG_DIRECT == dpnode->desc.trig)  /* Forced upload type */
            || (rept_type == T_STAT_REPT)          /* Statistical type */
            || (DP_REPT_NO_FILTER_FLAG & flags)) { /* User forced upload */
            is_need_update = TRUE;
        }
        PR_DEBUG("dp<%d> check. need_update:%d pv_stat:%d trig_t:%d type:%d "
                 "force_send:%d prop_tp:%d",
                 dpnode->desc.id, is_need_update, dpnode->pv_stat, dpnode->desc.trig, rept_type,
                 DP_REPT_NO_FILTER_FLAG & flags, dpnode->desc.prop_tp);

        switch (dpnode->desc.prop_tp) {
        case PROP_BOOL: {
            PR_DEBUG("dp<%d> bool: %d, new: %d", dpnode->desc.id, dpnode->prop.prop_bool.value, dp->value.dp_bool);
            if (dpnode->prop.prop_bool.value != dp->value.dp_bool) {
                is_need_update = TRUE;
            }
            if (is_need_update == TRUE) { /* If an update is needed, then update the local cache
                                             and refresh the flow control status */
                dpnode->prop.prop_bool.value = dp->value.dp_bool;
            }
            break;
        }
        case PROP_VALUE: {
            PR_DEBUG("dp<%d> value: %d, new: %d", dpnode->desc.id, dpnode->prop.prop_int.value, dp->value.dp_value);
            if (dpnode->prop.prop_int.value != dp->value.dp_value) {
                is_need_update = TRUE;
            }
            if (is_need_update == TRUE) { /* If an update is needed, then update the local cache
                                             and refresh the flow control status */
                dpnode->prop.prop_int.value = dp->value.dp_value;
            }
            break;
        }
        case PROP_STR: {
            tal_mutex_lock(dpnode->prop.prop_str.dp_str_mutex);
            PR_DEBUG("dp<%d> str: %s, new: %s", dpnode->desc.id, dpnode->prop.prop_str.value, dp->value.dp_str);
            if ((NULL == dpnode->prop.prop_str.value) ||
                (strlen(dpnode->prop.prop_str.value) != strlen(dp->value.dp_str)) ||
                strcmp(dpnode->prop.prop_str.value, dp->value.dp_str)) {
                is_need_update = TRUE;
            }
            if (is_need_update == TRUE) { /* If an update is needed, then update the local cache
                                             and refresh the flow control status */
                if ((NULL == dpnode->prop.prop_str.value) ||
                    (dpnode->prop.prop_str.cur_len < strlen(dp->value.dp_str))) {
                    if (NULL != dpnode->prop.prop_str.value) {
                        tal_free(dpnode->prop.prop_str.value);
                        dpnode->prop.prop_str.value = NULL;
                    }
                    dpnode->prop.prop_str.cur_len = strlen(dp->value.dp_str);
                    dpnode->prop.prop_str.value = tal_malloc(dpnode->prop.prop_str.cur_len + 1);
                    if (dpnode->prop.prop_str.value) {
                        memcpy(dpnode->prop.prop_str.value, dp->value.dp_str, dpnode->prop.prop_str.cur_len);
                        dpnode->prop.prop_str.value[dpnode->prop.prop_str.cur_len] = '\0';
                    } else {
                        PR_ERR("dp str malloc err, cache loss");
                        is_need_update = FALSE;
                        // dpnode->pv_stat = dp_pv_stat_get(dp_rept);;
                        // dpnode->pv_stat = PV_STAT_LOCAL;
                    }
                } else {
                    size_t copy_len = strlen(dp->value.dp_str);
                    if (copy_len > (size_t)dpnode->prop.prop_str.cur_len) {
                        copy_len = dpnode->prop.prop_str.cur_len;
                    }
                    memcpy(dpnode->prop.prop_str.value, dp->value.dp_str, copy_len);
                    dpnode->prop.prop_str.value[copy_len] = '\0';
                }

                // Statistical type DP Indicates the record time stamp
                if (rept_type == T_STAT_REPT) {
                    dpnode->time_stamp = dp->time_stamp;
                }
            }
            tal_mutex_unlock(dpnode->prop.prop_str.dp_str_mutex);
            break;
        }
        case PROP_ENUM: {
            if (dp->value.dp_enum >= dpnode->prop.prop_enum.cnt) {
                PR_ERR("dp enum exceed:%d", dp->value.dp_enum);
                break;
            }
            PR_DEBUG("dp<%d> enum: %s, new: %s", dpnode->desc.id,
                     dpnode->prop.prop_enum.pp_enum[dpnode->prop.prop_enum.value],
                     dpnode->prop.prop_enum.pp_enum[dp->value.dp_enum]);
            if (dpnode->prop.prop_enum.value != dp->value.dp_enum) {
                is_need_update = TRUE;
            }
            if (is_need_update == TRUE) { /* If an update is needed, then update the local cache
                                             and refresh the flow control status */
                dpnode->prop.prop_enum.value = dp->value.dp_enum;
            }
            break;
        }
        case PROP_BITMAP: {
            if (dpnode->prop.prop_bitmap.value != dp->value.dp_bitmap) {
                is_need_update = TRUE;
            }
            if (is_need_update == TRUE) { /* If an update is needed, then update the local cache
                                             and refresh the flow control status */
                dpnode->prop.prop_bitmap.value = dp->value.dp_bitmap;
            }
            break;
        }
        default:
            break;
        }
        break;
    }
    case T_RAW: {
        is_need_update = TRUE;
        dpnode->pv_stat = PV_STAT_LOCAL;
        break;
    }
    case T_FILE:
    default: {
        PR_ERR("prop_tp err:%d", dpnode->desc.prop_tp);
        break;
    }
    } /* end of switch */

    if (is_need_update) {
        dpnode->pv_stat = PV_STAT_LOCAL;
        // Record timestamp for statistical type DP
        if (rept_type == T_STAT_REPT) {
            dpnode->time_stamp = dp->time_stamp;
        }
    }

    // if (is_need_update && (VALID_ULING == dpnode->pv_stat)) {
    //     dpnode->uling_cnt = 0;
    // }
    PR_TRACE("is_need_update:%d, pv_stat:%u", is_need_update, dpnode->pv_stat);

    return (FALSE == is_need_update);
}

static bool dp_type_check(dp_rept_type_t dp_rept_type, dp_obj_t *dp, dp_node_t *node)
{
    if ((node->desc.mode == M_WR) ||                                    /* DP is write-only, verification failed */
        ((node->desc.type != T_OBJ) && (dp_rept_type == T_OBJ_REPT)) || /* Non-object DP mixed in object-DP interface */
        ((node->desc.type != T_RAW) && (dp_rept_type == T_RAW_REPT))) { /* Non-raw DP mixed in raw-DP interface */
        PR_ERR("DP ID:%d Verify Fail:%d %d %d", node->desc.id, node->desc.mode, dp_rept_type, node->desc.type);
        return FALSE;
    }

    // Verifying dp validity
    switch (node->desc.type) {
    case T_OBJ: {
        switch (node->desc.prop_tp) {
        case PROP_BOOL: {
            if ((TRUE != dp->value.dp_bool) && (FALSE != dp->value.dp_bool)) {
                PR_ERR("bool check err,val:%u", dp->value.dp_bool);
                return FALSE;
            }
            break;
        }

        case PROP_VALUE: {
            if (dp->value.dp_value > node->prop.prop_int.max || dp->value.dp_value < node->prop.prop_int.min) {
                PR_ERR("value check err:%d[%d,%d]", dp->value.dp_value, node->prop.prop_int.min,
                       node->prop.prop_int.max);
                return FALSE;
            }
            break;
        }

        case PROP_STR: {
            if (strlen(dp->value.dp_str) > node->prop.prop_str.max_len) {
                PR_ERR("str check err %s %d", dp->value.dp_str, node->prop.prop_str.max_len);
                return FALSE;
            }
            break;
        }

        case PROP_ENUM: {
            if (dp->value.dp_enum >= node->prop.prop_enum.cnt) {
                PR_ERR("enum check err:%d %d", dp->value.dp_enum, node->prop.prop_enum.cnt);
                return FALSE;
            }
            break;
        }

        case PROP_BITMAP:
            if (dp->type != cJSON_Number) {
                PR_ERR("bitmap check fail %d %d %d", dp->type, dp->value.dp_bitmap, node->prop.prop_bitmap.max_len);
                return FALSE;
            }
            break;

        default: {
            PR_ERR("prop_tp err:%d", node->desc.prop_tp);
            return FALSE;
        }
        }
        break;
    }
    case T_RAW:

        break;

    case T_FILE: {
        PR_ERR("file not support");
        return FALSE;
    }

    default: {
        PR_ERR("type err:%d", node->desc.type);
        return FALSE;
    }
    }

    return TRUE;
}

/**
 * @brief Performs a validity check on the given data point (DP) repetition.
 *
 * This function checks if the provided data point repetition is valid according
 * to the given schema.
 *
 * @param schema The schema to validate against.
 * @param dpin The input data point repetition to be checked.
 * @param dpvalid The structure to store the validity result.
 *
 * @return Returns 0 if the data point repetition is valid, or a negative error
 * code if it is invalid.
 */
int dp_rept_valid_check(dp_schema_t *schema, dp_rept_in_t *dpin, dp_rept_valid_t *dpvalid)
{
    int i;

    tal_mutex_lock(schema->mutex);
    for (i = 0; i < dpin->dpscnt; i++) {
        dp_obj_t *dp = &(dpin->dps[i]);
        dp_node_t *dpnode = dp_node_find(schema, dp->id);
        if (NULL == dpnode) {
            PR_ERR("dpnode[%d]: dpid %d not find", i, dp->id);
            continue;
        }
        if (dp->type != dpnode->desc.prop_tp) {
            PR_ERR("dparr[%d] type not match:%d %d", i, dp->type, dpnode->desc.prop_tp);
            tal_mutex_unlock(schema->mutex);
            return OPRT_SVC_DP_TP_NOT_MATCH;
        }

        // Only the reported interface needs to be checked. Retransmission does not require verification and flow
        // control
        if (dpin->rept_type != T_RE_TRANS_REPT) {
            if (FALSE == dp_type_check(dpin->rept_type, dp, dpnode)) {
                continue;
            }

            if ((schema->actv.preprocess == TRUE) && (dpnode->desc.passive == PSV_TRUE)) {
                PR_DEBUG("dp passive:true");
                continue;
            }
            if (dp_rept_update(dpin->rept_type, dp, dpnode, dpin->flags)) {
                PR_DEBUG("dp np update");
                continue;
            }
        }

        switch (dp->type) {
        case PROP_BOOL:
        case PROP_VALUE:
        case PROP_BITMAP: {
            dpvalid->len += 20;
            break;
        }

        case PROP_STR: {
            dpvalid->len += 2 * strlen(dp->value.dp_str) + 15;
        } break;

        case PROP_ENUM: {
            if (dp->value.dp_enum >= dpnode->prop.prop_enum.cnt) {
                PR_ERR("dparr[%d] enum not match:%d %d", i, dp->value.dp_enum, dpnode->prop.prop_enum.cnt);
                tal_mutex_unlock(schema->mutex);
                return OPRT_SVC_DP_TYPE_PROP_ILLEGAL;
            }
            dpvalid->len += strlen(dpnode->prop.prop_enum.pp_enum[dp->value.dp_enum]) + 15;
        } break;

        default: {
            PR_ERR("dparr[%d] type invalid %d", i, dp->type);
            tal_mutex_unlock(schema->mutex);
            return OPRT_COM_ERROR;
        }
        }
        if (dp->time_stamp) {
            dpvalid->timelen += 30;
        }
        dpvalid->dpid[dpvalid->num++] = dp->id;
    }
    tal_mutex_unlock(schema->mutex);

    if (0 == dpvalid->num) {
        PR_DEBUG("no vaild dp to rept");
        return OPRT_SVC_DP_ID_NOT_FOUND;
    }

    dpvalid->schema = schema;

    return OPRT_OK;
}

/**
 * @brief Outputs the JSON representation of a device property (DP) schema.
 *
 * This function takes a DP schema, input data, validation information, and
 * output data as parameters. It generates the JSON representation of the DP
 * schema based on the provided input data and validation information, and
 * stores the result in the output data structure.
 *
 * @param schema Pointer to the DP schema structure.
 * @param dpin Pointer to the input data structure.
 * @param dpvalid Pointer to the validation information structure.
 * @param dpout Pointer to the output data structure.
 * @return Integer value indicating the success or failure of the operation.
 */
int dp_rept_json_output(dp_schema_t *schema, dp_rept_in_t *dpin, dp_rept_valid_t *dpvalid, dp_rept_out_t *dpout)
{
    uint16_t i, j;
    size_t offset = 0;
    size_t time_offset = 0;
    OPERATE_RET op_ret = OPRT_OK;
    char *dpstr = NULL;
    char *dptimestr = NULL;
    bool is_need_time = false;

    dpstr = (char *)tal_malloc(dpvalid->len);
    if (NULL == dpstr) {
        PR_ERR("malloc err:%d", dpvalid->len);
        return OPRT_MALLOC_FAILED;
    }
    // STAT type DP needs to assemble a timestamp
    if ((T_STAT_REPT == dpin->rept_type) && dpvalid->timelen && dpout->timejson) {
        dptimestr = (char *)tal_malloc(dpvalid->timelen);
        if (NULL == dptimestr) {
            PR_ERR("malloc err:%d", dpvalid->timelen);
            op_ret = OPRT_MALLOC_FAILED;
            goto __err_exit;
        }
        is_need_time = true;
    }
    if (dpvalid->len == 0) {
        op_ret = OPRT_BUFFER_NOT_ENOUGH;
        goto __err_exit;
    }

    dpstr[offset++] = '{';
    if (is_need_time) {
        if (dpvalid->timelen == 0) {
            op_ret = OPRT_BUFFER_NOT_ENOUGH;
            goto __err_exit;
        }
        dptimestr[time_offset++] = '{';
    }

    for (i = 0; i < dpvalid->num; i++) {
        dp_obj_t *dp = NULL;
        for (j = 0; j < dpin->dpscnt; j++) {
            if (dpvalid->dpid[i] == dpin->dps[j].id) {
                dp = &dpin->dps[j];
                break;
            }
        }
        if (NULL == dp) {
            PR_DEBUG("dp not found");
            op_ret = OPRT_SVC_DP_ID_NOT_FOUND;
            goto __err_exit;
        }
        // dp_obj_t  *dp     = &dpin->dps[dpvalid->dpid[i]];
        dp_node_t *dpnode = dp_node_find(schema, dp->id);
        if (NULL == dpnode) {
            PR_DEBUG("dp->id = %d not found", dp->id);
            op_ret = OPRT_SVC_DP_ID_NOT_FOUND;
            goto __err_exit;
        }

        if (dp->type != dpnode->desc.prop_tp) {
            op_ret = OPRT_SVC_DP_TP_NOT_MATCH;
            goto __err_exit;
        }

        switch (dp->type) {
        case PROP_BOOL: {
            if (TRUE == dp->value.dp_bool) {
                if (!dp_snprintf_append(dpstr, dpvalid->len, &offset, "\"%d\":true,", dp->id)) {
                    op_ret = OPRT_BUFFER_NOT_ENOUGH;
                    goto __err_exit;
                }
            } else {
                if (!dp_snprintf_append(dpstr, dpvalid->len, &offset, "\"%d\":false,", dp->id)) {
                    op_ret = OPRT_BUFFER_NOT_ENOUGH;
                    goto __err_exit;
                }
            }
            break;
        }

        case PROP_VALUE: {
            if (!dp_snprintf_append(dpstr, dpvalid->len, &offset, "\"%d\":%d,", dp->id, dp->value.dp_value)) {
                op_ret = OPRT_BUFFER_NOT_ENOUGH;
                goto __err_exit;
            }
            break;
        }

        case PROP_BITMAP: {
            if (!dp_snprintf_append(dpstr, dpvalid->len, &offset, "\"%d\":%" PRIu32 ",", dp->id,
                                    dp->value.dp_bitmap)) {
                op_ret = OPRT_BUFFER_NOT_ENOUGH;
                goto __err_exit;
            }
            break;
        }

        case PROP_STR: {
            cJSON *temp_str = cJSON_CreateString(dp->value.dp_str);
            char *tmp_data = cJSON_PrintUnformatted(temp_str);
            if (tmp_data) {
                if (!dp_snprintf_append(dpstr, dpvalid->len, &offset, "\"%d\":%s,", dp->id, tmp_data)) {
                    cJSON_free(tmp_data);
                    cJSON_Delete(temp_str);
                    op_ret = OPRT_BUFFER_NOT_ENOUGH;
                    goto __err_exit;
                }
            }
            cJSON_free(tmp_data);
            cJSON_Delete(temp_str);
            break;
        }

        case PROP_ENUM: {
            if (!dp_snprintf_append(dpstr, dpvalid->len, &offset, "\"%d\":\"%s\",", dp->id,
                                    dpnode->prop.prop_enum.pp_enum[dp->value.dp_enum])) {
                op_ret = OPRT_BUFFER_NOT_ENOUGH;
                goto __err_exit;
            }
        } break;
        }

        if (is_need_time && dp->time_stamp) {
            if (!dp_snprintf_append(dptimestr, dpvalid->timelen, &time_offset, "\"%d\":%u,", dp->id, dp->time_stamp)) {
                op_ret = OPRT_BUFFER_NOT_ENOUGH;
                goto __err_exit;
            }
        }
    }

    if (offset == 0 || offset + 1 >= dpvalid->len) {
        op_ret = OPRT_BUFFER_NOT_ENOUGH;
        goto __err_exit;
    }

    dpstr[offset - 1] = '}';
    dpstr[offset] = 0;

    dpout->dpsjson = dpstr;

    PR_DEBUG("dp rept out: %s", dpstr);

    if (is_need_time) {
        if (time_offset == 0 || time_offset + 1 >= dpvalid->timelen) {
            op_ret = OPRT_BUFFER_NOT_ENOUGH;
            goto __err_exit;
        }
        dptimestr[time_offset - 1] = '}';
        dptimestr[time_offset] = 0;
        PR_DEBUG("dptimestr:%s", dptimestr);
        dpout->timejson = dptimestr;
    }

    return OPRT_OK;

__err_exit:
    tal_free(dpstr);
    if (is_need_time) {
        tal_free(dptimestr);
    }

    return op_ret;
}

// int dp_rept_json_output(dp_schema_t *schema, dp_rept_in_t *dpin,
// dp_rept_out_t *dpout)
// {
//     OPERATE_RET op_ret = OPRT_OK;

//     if (NULL == schema || NULL == dpin || NULL == dpout) {
//         return OPRT_INVALID_PARM;
//     }

//     dp_rept_valid_t *dpvaild = (dp_rept_valid_t
//     *)tal_malloc(sizeof(dp_rept_valid_t) + sizeof(uint8_t) * dpin->dpscnt);
//     if (NULL == dpvaild) {
//         return OPRT_MALLOC_FAILED;
//     }
//     memset(dpvaild, 0, sizeof(dp_rept_valid_t) + sizeof(uint8_t) *
//     dpin->dpscnt);

//     op_ret = dp_obj_valid_check(schema, dpin, dpvaild);
//     if (OPRT_OK != op_ret) {
//         goto __err_exit;
//     }

//     //! TODO:
//     op_ret = dp_obj_output_to_json(schema, dpin, dpvaild, dpout);
//     if (OPRT_OK != op_ret) {
//         goto __err_exit;
//     }

// __err_exit:
//     if (dpvaild) {
//         tal_free(dpvaild);
//     }

//     return op_ret;
// }

static int dp_obj_json_create(cJSON *root, dp_node_t *dpnode)
{
    int length = 0;

    char dpid[10] = {0};
    snprintf(dpid, 10, "%d", dpnode->desc.id);

    switch (dpnode->desc.prop_tp) {
    case PROP_BOOL: {
        length += 10;
        cJSON_AddBoolToObject(root, dpid, dpnode->prop.prop_bool.value);
        break;
    }

    case PROP_VALUE: {
        length += 10;
        cJSON_AddNumberToObject(root, dpid, dpnode->prop.prop_int.value);
        break;
    }

    case PROP_STR: {
        tal_mutex_lock(dpnode->prop.prop_str.dp_str_mutex);
        if (dpnode->prop.prop_str.value) {
            length += (10 + strlen(dpnode->prop.prop_str.value));
            cJSON_AddStringToObject(root, dpid, dpnode->prop.prop_str.value);
        }
        tal_mutex_unlock(dpnode->prop.prop_str.dp_str_mutex);
        break;
    }

    case PROP_ENUM: {
        int value = dpnode->prop.prop_enum.value;
        length += (10 + strlen(dpnode->prop.prop_enum.pp_enum[value]));
        cJSON_AddStringToObject(root, dpid, dpnode->prop.prop_enum.pp_enum[value]);
        break;
    }

    case PROP_BITMAP: {
        length += 10;
        cJSON_AddNumberToObject(root, dpid, dpnode->prop.prop_bitmap.value);
        break;
    }

    default: {
        PR_ERR("dp type err:%d", dpnode->desc.prop_tp);
        break;
    }
    }

    return length;
}

/**
 * @brief Dumps the status of a device object to a local JSON string.
 *
 * This function takes a device ID, a pointer to a dp_rept_valid_t structure, a
 * pointer to a char pointer, and flags as input parameters. It generates a JSON
 * string representing the status of the device object and stores it in the
 * `outjson` parameter.
 *
 * @param devid The device ID.
 * @param outdpvalid A pointer to a dp_rept_valid_t structure that contains the
 * valid data points.
 * @param outjson A pointer to a char pointer that will store the generated JSON
 * string.
 * @param flags Flags to control the behavior of the function.
 *
 * @return An integer value indicating the success or failure of the function.
 *         - Returns 0 on success.
 *         - Returns a negative value on failure.
 */
int dp_obj_dump_stat_local_json(char *devid, dp_rept_valid_t **outdpvalid, char **outjson, int flags)
{
    int i;
    size_t length = 0;
    dp_schema_t *schema = dp_schema_find(devid);
    uint8_t dp_stat_local_num = 0;

    for (i = 0; i < schema->num; i++) {
        dp_node_t *dpnode = &(schema->node[i]);
        if (dpnode == NULL) {
            PR_DEBUG("dpnode NULL");
            break;
        }
        if (T_OBJ == dpnode->desc.type && PV_STAT_CLOUD != dpnode->pv_stat) {
            dp_stat_local_num++;
            continue;
        }
    }

    if (0 == dp_stat_local_num) {
        return OPRT_OK;
    }

    cJSON *cjson = cJSON_CreateObject();
    if (NULL == cjson) {
        PR_ERR("json err");
        return OPRT_MALLOC_FAILED;
    }

    dp_rept_valid_t *dpvaild = tal_malloc(sizeof(dp_rept_valid_t) + sizeof(uint8_t) * dp_stat_local_num);
    if (NULL == dpvaild) {
        cJSON_Delete(cjson);
        return OPRT_MALLOC_FAILED;
    }
    memset(dpvaild, 0, sizeof(dp_rept_valid_t) + sizeof(uint8_t) * dp_stat_local_num);
    dpvaild->schema = schema;

    for (i = 0; i < schema->num; i++) {
        dp_node_t *dpnode = &(schema->node[i]);
        if (dpnode == NULL) {
            PR_DEBUG("dpnode NULL");
            break;
        }
        if (T_OBJ == dpnode->desc.type && PV_STAT_CLOUD == dpnode->pv_stat) {
            continue;
        }
        if (dpvaild->num < dp_stat_local_num) {
            dpvaild->dpid[dpvaild->num++] = dpnode->desc.id;
            length += dp_obj_json_create(cjson, dpnode);
        }
    }

    if (length == 0) {
        PR_DEBUG("Nothing To Pack");
        cJSON_Delete(cjson);
        return OPRT_SVC_DP_ID_NOT_FOUND;
    }
    char *jsonstr = cJSON_PrintUnformatted(cjson);
    cJSON_Delete(cjson);
    if (NULL == jsonstr) {
        tal_free(dpvaild);
        PR_ERR("Json err");
        return OPRT_CR_CJSON_ERR;
    }

    char *out = NULL;
    if (flags & DP_APPEND_HEADER_FLAG) {
        dp_rept_json_append(schema, jsonstr, NULL, NULL, 0, &out);
        cJSON_free(jsonstr);
    }else {
        size_t jsonstr_len = strlen(jsonstr);
        out = tal_malloc(jsonstr_len + 1);
        if(out) {
            memset(out, 0, jsonstr_len + 1);
            memcpy(out, jsonstr, jsonstr_len);
        }
        cJSON_free(jsonstr);
    }

    if (outjson) {
        *outjson = out;
    } else {
        tal_free(out);
    }

    if (outdpvalid) {
        *outdpvalid = dpvaild;
    } else {
        tal_free(dpvaild);
    }

    return OPRT_OK;
}

/**
 * @brief Dumps all the DP objects in JSON format.
 *
 * This function takes a device ID and flags as input parameters and returns a
 * JSON string containing all the DP objects for the specified device. The JSON
 * string can be used for debugging or other purposes.
 *
 * @param devid The device ID for which to dump the DP objects.
 * @param flags Additional flags to control the behavior of the function
 * (optional).
 *
 * @return A pointer to the JSON string containing the dumped DP objects.
 */
char *dp_obj_dump_all_json(char *devid, int flags)
{
    cJSON *cjson = cJSON_CreateObject();
    if (NULL == cjson) {
        PR_ERR("json err");
        return NULL;
    }

    size_t length = 0;
    dp_schema_t *schema = dp_schema_find(devid);
    if (NULL == schema) {
        cJSON_Delete(cjson);
        PR_ERR("schema err");
        return NULL;
    }

    int i;

    for (i = 0; i < schema->num; i++) {
        dp_node_t *dpnode = &(schema->node[i]);
        if (dpnode == NULL) {
            PR_DEBUG("dpnode NULL");
            break;
        }
        if (DP_DUMP_STAT_LOCAL_FLAG & flags) {
            if (T_OBJ == dpnode->desc.type && PV_STAT_CLOUD == dpnode->pv_stat) {
                continue;
            }
        }
        length += dp_obj_json_create(cjson, dpnode);
    } /* end of for */

    if (length == 0) {
        PR_DEBUG("Nothing To Pack");
        cJSON_Delete(cjson);
        return NULL;
    }

    char *tmp = cJSON_PrintUnformatted(cjson);
    cJSON_Delete(cjson);
    if (NULL == tmp) {
        PR_ERR("Json err");
        return NULL;
    }

    char *out_str = tal_malloc(strlen(tmp) + 1);
    if (NULL == out_str) {
        PR_ERR("malloc err");
        cJSON_free(tmp);
        return NULL;
    }else {
        memset(out_str, 0, strlen(tmp) + 1);
        strcpy(out_str, tmp);
        cJSON_free(tmp);
    }

    char *out = out_str;

    if (flags & DP_APPEND_HEADER_FLAG) {
        dp_rept_json_append(schema, out, NULL, NULL, 0, &out);
    }

    return out;
}

typedef struct {
    uint16_t start;
    uint16_t end;
} dp_node_pos_t;

#define MAX_ITEM_LEN 1024

static int dp_node_pos_decode(char *schema_json, dp_node_pos_t pos[], int pos_num)
{
    if (!schema_json) {
        return 0;
    }

    int i = 0, brace = 0, n = 0;

    while (*schema_json) {
        if (*schema_json == '{') {
            if (brace == 0) {
                pos[n].start = i;
            }
            brace++;
        } else if (*schema_json == '}') {
            brace--;
            if (brace == 0) {
                pos[n++].end = i;
            }

            if (n >= pos_num) {
                return n;
            }
        }
        i++;
        schema_json++;
    }

    return n;
}

static OPERATE_RET dp_node_parse(char *schema_json, dp_node_pos_t *nodepos, uint16_t nodenum, dp_node_t *dpnode,
                                 SCHEMA_OTHER_ATTR_S *other_attr)
{
    OPERATE_RET op_ret = OPRT_OK;
    dp_desc_t *dp_desc;
    dp_prop_vaule_t *prop;

    cJSON *cjson = NULL;
    cJSON *item = NULL;
    char *pBuf = NULL;

    pBuf = (char *)tal_malloc(MAX_ITEM_LEN);
    if (NULL == pBuf) {
        PR_ERR("malloc fail");
        op_ret = OPRT_COM_ERROR;
        goto __exit;
    }

    int i = 0;

    for (i = 0; i < nodenum; i++) {
        dp_desc = &(dpnode[i].desc);
        prop = &(dpnode[i].prop);

        memset(pBuf, 0, MAX_ITEM_LEN);
        memcpy(pBuf, schema_json + nodepos[i].start, nodepos[i].end - nodepos[i].start + 1);
        cjson = cJSON_Parse(pBuf);
        if (NULL == cjson) {
            PR_ERR("cjson NULL:%s", pBuf);
            op_ret = OPRT_CJSON_PARSE_ERR;
            goto __exit;
        }
        item = cJSON_GetObjectItem(cjson, "id"); // id
        if (NULL == item) {
            PR_ERR("get id null");
            op_ret = OPRT_CJSON_GET_ERR;
            goto __exit;
        }
        if (item->type == cJSON_String) {
            dp_desc->id = atoi(item->valuestring);
        } else {
            dp_desc->id = item->valueint;
        }
        //! mode
        item = cJSON_GetObjectItem(cjson, "mode");
        if (NULL == item) {
            PR_ERR("get mode null");
            op_ret = OPRT_CJSON_GET_ERR;
            goto __exit;
        }
        if (!strcmp(item->valuestring, "rw")) {
            dp_desc->mode = M_RW;
        } else if (!strcmp(item->valuestring, "ro")) {
            dp_desc->mode = M_RO;
        } else {
            dp_desc->mode = M_WR;
        }
        //! passive
        item = cJSON_GetObjectItem(cjson, "passive");
        if (item == NULL) {
            dp_desc->passive = PSV_FALSE;
        } else {
            other_attr->preprocess = TRUE;
            // passive processing is disabled first. The default value is false
            dp_desc->passive = PSV_FALSE;
        }
        //! trigger
        item = cJSON_GetObjectItem(cjson, "trigger");
        if (NULL == item) {
            dp_desc->trig = TRIG_PULSE;
        } else {
            if (!strcmp(item->valuestring, "pulse")) {
                dp_desc->trig = TRIG_PULSE;
            } else {
                dp_desc->trig = TRIG_DIRECT;
            }
        }
        // route
        item = cJSON_GetObjectItem(cjson, "route");
        if (NULL == item) {
            dp_desc->route_t = ROUTE_DEFAULT;
        } else {
            if (item->valueint == 2) {
                dp_desc->route_t = ROUTE_FORCE_BT;
            }

            else if (item->valueint == 1) {
                dp_desc->route_t = ROUTE_BLE_FIRST;
            } else {
                dp_desc->route_t = ROUTE_DEFAULT;
            }
        }
        // stat //todo
        item = cJSON_GetObjectItem(cjson, "stat");
        if (NULL == item) {
            dp_desc->stat = DST_NONE;
        } else {
            if (!strcmp(item->valuestring, "total")) {
                dp_desc->stat = DST_TOTAL;
            } else {
                dp_desc->stat = DST_INC;
            }
        }
        // type
        item = cJSON_GetObjectItem(cjson, "type");
        if (NULL == item) {
            dp_desc->type = T_OBJ;
        } else {
            if (!strcmp(item->valuestring, "obj")) {
                dp_desc->type = T_OBJ;
            } else {
                if (!strcmp(item->valuestring, "raw")) {
                    dp_desc->type = T_RAW;
                } else {
                    dp_desc->type = T_FILE;
                }
                cJSON_Delete(cjson);
                continue;
            }
        }
        // property
        item = cJSON_GetObjectItem(cjson, "property");
        if (NULL == item) {
            PR_ERR("get property null");
            op_ret = OPRT_CJSON_GET_ERR;
            goto __exit;
        }
        cJSON *child;
        child = cJSON_GetObjectItem(item, "type");
        if (NULL == child) {
            PR_ERR("get type null");
            op_ret = OPRT_CJSON_GET_ERR;
            goto __exit;
        }
        if (!strcmp(child->valuestring, "bool")) {
            dp_desc->prop_tp = PROP_BOOL;
        } else if (!strcmp(child->valuestring, "value")) {
            dp_desc->prop_tp = PROP_VALUE;
            char *str[] = {"max", "min", "scale"};
            int i;
            for (i = 0; i < CNTSOF(str); i++) {
                child = cJSON_GetObjectItem(item, str[i]);
                if (NULL == child && (i != CNTSOF(str) - 1)) {
                    PR_ERR("get property null");
                    op_ret = OPRT_CJSON_GET_ERR;
                    goto __exit;
                } else if (NULL == child && (i == CNTSOF(str) - 1)) {
                    prop->prop_int.scale = 0;
                } else {
                    switch (i) {
                    case 0:
                        prop->prop_int.max = child->valueint;
                        break;
                    case 1:
                        prop->prop_int.min = child->valueint;
                        break;
                    case 2:
                        prop->prop_int.scale = child->valueint;
                        break;
                    }
                }
            }
        } else if (!strcmp(child->valuestring, "string")) {
            dp_desc->prop_tp = PROP_STR;
            child = cJSON_GetObjectItem(item, "maxlen");
            if (NULL == child) {
                PR_ERR("get maxlen null");
                op_ret = OPRT_CJSON_GET_ERR;
                goto __exit;
            }
            prop->prop_str.max_len = child->valueint;
            prop->prop_str.value = NULL;
            prop->prop_str.cur_len = 0;
            op_ret = tal_mutex_create_init(&prop->prop_str.dp_str_mutex);
            if (OPRT_OK != op_ret) {
                PR_ERR("mutex init fail:%d", op_ret);
                op_ret = OPRT_CR_MUTEX_ERR;
                goto __exit;
            }
        } else if (!strcmp(child->valuestring, "enum")) {
            dp_desc->prop_tp = PROP_ENUM;
            child = cJSON_GetObjectItem(item, "range");
            if (NULL == child) {
                PR_ERR("get range null");
                op_ret = OPRT_CJSON_GET_ERR;
                goto __exit;
            }
            int i, num;
            num = cJSON_GetArraySize(child);
            if (num == 0) {
                PR_ERR("get array size null");
                op_ret = OPRT_CJSON_GET_ERR;
                goto __exit;
            }
            prop->prop_enum.pp_enum = tal_malloc(num * sizeof(char *));
            if (NULL == prop->prop_enum.pp_enum) {
                PR_ERR("malloc fail");
                op_ret = OPRT_MALLOC_FAILED;
                goto __exit;
            }
            prop->prop_enum.cnt = num;
            for (i = 0; i < num; i++) {
                cJSON *c_child = cJSON_GetArrayItem(child, i);
                if (NULL == c_child) {
                    PR_ERR("get array null");
                    op_ret = OPRT_CJSON_GET_ERR;
                    goto __exit;
                }
                prop->prop_enum.pp_enum[i] = mm_strdup(c_child->valuestring);
                if (NULL == prop->prop_enum.pp_enum[i]) {
                    PR_ERR("malloc fail");
                    op_ret = OPRT_MALLOC_FAILED;
                    goto __exit;
                }
            }
        } else if (!strcmp(child->valuestring, "bitmap")) {
            dp_desc->prop_tp = PROP_BITMAP;
            child = cJSON_GetObjectItem(item, "maxlen");
            if (NULL == child) {
                PR_ERR("get maxlen null");
                op_ret = OPRT_CJSON_GET_ERR;
                goto __exit;
            }

            prop->prop_bitmap.max_len = child->valueint;
        } else {
            op_ret = OPRT_SVC_DEVOS_SCMA_INVALID;
            goto __exit;
        }

        cJSON_Delete(cjson);
        cjson = NULL;
    }

    tal_free(pBuf);

    return OPRT_OK;

__exit:
    if (pBuf) {
        tal_free(pBuf);
    }

    if (cjson) {
        cJSON_Delete(cjson);
    }

    return op_ret;
}

/**
 * @brief Creates a new data point schema for a device.
 *
 * This function creates a new data point schema for a device identified by the
 * given device ID. The schema is defined by the provided JSON string.
 *
 * @param devid The device ID for which the schema is being created.
 * @param schema_json The JSON string defining the data point schema.
 * @param dp_schema_out A pointer to a variable that will hold the created data
 * point schema. This variable should be allocated by the caller.
 *
 * @return 0 if the schema was successfully created, or an error code if an
 * error occurred.
 */
int dp_schema_create(char *devid, char *schema_json, dp_schema_t **dp_schema_out)
{
    OPERATE_RET op_ret = OPRT_OK;
    dp_node_pos_t *nodepos = NULL;
    int nodenum;

    nodepos = tal_malloc(sizeof(dp_node_pos_t) * 255);
    if (NULL == nodepos) {
        PR_ERR("malloc fail");
        return OPRT_MALLOC_FAILED;
    }
    PR_DEBUG("devid %s, schema_json %s", devid, schema_json);

    nodenum = dp_node_pos_decode(schema_json, nodepos, 255);
    if (0 == nodenum || nodenum >= 255) {
        PR_ERR("dp num parse err:%d", nodenum);
        tal_free(nodepos);
        return OPRT_SVC_DEVOS_DEV_DP_CNT_INVALID;
    }
    dp_schema_t *dp_schema = (dp_schema_t *)tal_malloc(sizeof(dp_schema_t) + nodenum * sizeof(dp_node_t));
    if (NULL == dp_schema) {
        tal_free(nodepos);
        PR_ERR("malloc fail:%d", nodenum);
        return OPRT_MALLOC_FAILED;
    }
    memset(dp_schema, 0, sizeof(dp_schema_t) + nodenum * sizeof(dp_node_t));
    op_ret = tal_mutex_create_init(&(dp_schema->mutex));
    if (OPRT_OK != op_ret) {
        PR_ERR("mutex create fail:%d", op_ret);
        goto __exit;
    }
    dp_schema->num = nodenum;
    // dp schema parse
    SCHEMA_OTHER_ATTR_S other_attr;
    memset(&other_attr, 0, sizeof(other_attr));
    tal_mutex_lock(dp_schema->mutex);
    op_ret = dp_node_parse(schema_json, nodepos, nodenum, dp_schema->node, &other_attr);
    tal_mutex_unlock(dp_schema->mutex);
    if (OPRT_OK != op_ret) {
        PR_ERR("dp_node_parse fail:%d", op_ret);
        goto __exit;
    }
    dp_schema->actv.preprocess = other_attr.preprocess;
    dp_schema->actv.attach_dp_if = TRUE;
    strncpy(dp_schema->devid, devid, DEV_ID_LEN);
    if (dp_schema_out) {
        *dp_schema_out = dp_schema;
    }
    if (s_dsmgr.schema_num < DP_SCHEMA_NUM_MAX) {
        s_dsmgr.schema_list[s_dsmgr.schema_num] = dp_schema;
        s_dsmgr.schema_num++;
    }
    PR_DEBUG("create dp_schema Success ");
    tal_free(nodepos);

    return OPRT_OK;

__exit:
    tal_mutex_release(dp_schema->mutex);
    tal_free(dp_schema);
    tal_free(nodepos);
    return op_ret;
}

/**
 * @brief Deletes the data point schema for a device.
 *
 * This function deletes the data point schema associated with the specified
 * device.
 *
 * @param devid The device ID for which the data point schema needs to be
 * deleted.
 * @return Returns 0 on success, or a negative error code on failure.
 */
int dp_schema_delete(char *devid)
{
    int i = 0;

    PR_TRACE("try to delete schema devid %s", devid);
    dp_schema_mgr_t *dsmgr = &s_dsmgr;
    for (i = 0; i < DP_SCHEMA_NUM_MAX; i++) {
        if (NULL == dsmgr->schema_list[i]) {
            continue;
        }

        if (0 == strcmp(devid, dsmgr->schema_list[i]->devid)) {
            tal_mutex_release(dsmgr->schema_list[i]->mutex);
            tal_free(dsmgr->schema_list[i]);
            dsmgr->schema_list[i] = NULL;
            dsmgr->schema_num--;
            return OPRT_OK;
        }
    }

    return OPRT_OK;
}

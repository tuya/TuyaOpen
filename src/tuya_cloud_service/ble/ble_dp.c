/**
 * @file ble_dp.C
 * @brief This file contains functions to manage BLE data points (DPs) including
 * creating, parsing, and freeing KLV (Key-Length-Value) lists for BLE
 * communication. It provides mechanisms to serialize and deserialize data
 * points for transmission over BLE, handling different data types like enums,
 * booleans, and various sized integers. Additionally, it includes utilities for
 * managing memory and ensuring data integrity during the process.
 *
 * @copyright Copyright (c) 2021-2024 Tuya Inc. All Rights Reserved.
 *
 */

#include "tal_api.h"
#include "ble_protocol.h"
#include "ble_trsmitr.h"
#include "ble_dp.h"
#include "ble_mgr.h"
#include "tuya_protocol.h"
#include "tuya_iot_dp.h"
#include "mix_method.h"

// dp type describe
typedef uint8_t dp_type;
#define DT_RAW    0
#define DT_BOOL   1
#define DT_VALUE  2
#define DT_INT    DT_VALUE
#define DT_STRING 3
#define DT_ENUM   4
#define DT_BITMAP 5
#define DT_CHAR   7
#define DT_UCHAR  8
#define DT_SHORT  9
#define DT_USHORT 10
#define DT_LMT    DT_USHORT

#define DT_VALUE_LEN  4 // int
#define DT_BOOL_LEN   4
#define DT_ENUM_LEN   4
#define DT_BITMAP_MAX 4 // 1/2/4
#define DT_STR_MAX    255
#define DT_RAW_MAX    255
#define DT_INT_LEN    DT_VALUE_LEN

typedef struct s_klv_node {
    struct s_klv_node *next;
    uint8_t id;
    dp_type type;
    uint16_t len;
    uint8_t *data;
} klv_node_s;

void tuya_change_bt_dp_tlv(dp_type type, void *data, uint16_t *len)
{
    if (DT_ENUM == type) {
        uint32_t enum_value = *(uint32_t *)data;
        if (enum_value <= 0xff) {
            uint8_t enum_byte = *(uint32_t *)data;
            *len = 1;
            memcpy(data, &enum_byte, *len);
        } else if (enum_value <= 0xffff) {
            uint16_t enum_short = *(uint32_t *)data;
            *len = 2;
            enum_short = UNI_HTONS(enum_short);
            memcpy(data, &enum_short, *len);
        } else {
            uint32_t enum_int = *(uint32_t *)data;
            *len = 4;
            enum_int = UNI_HTONL(enum_int);
            memcpy(data, &enum_int, *len);
        }
    } else if (DT_BOOL == type) {
        uint8_t bool_value = *(uint32_t *)data;
        *len = 1;
        memcpy(data, &bool_value, *len);
    }
}

void free_klv_list(klv_node_s *list)
{
    if (NULL == list) {
        return;
    }

    klv_node_s *node = list;
    klv_node_s *next_node = NULL;

    do {
        next_node = node->next;
        // free(node);
        tal_free((uint8_t *)(node->data));
        tal_free((uint8_t *)node);
        node = next_node;
    } while (node);
}

klv_node_s *make_klv_list(klv_node_s *list, uint8_t id, dp_type type, void *data, uint16_t len)
{
    klv_node_s *node;
    void *p_data = NULL;

    if (NULL == data || type >= DT_LMT) {
        PR_ERR("input invalid");
        return NULL;
    }

    if (DT_VALUE == type && DT_VALUE_LEN != len) {
        goto err_ret;
    } else if (DT_BITMAP == type && len > DT_BITMAP_MAX) {
        goto err_ret;
    } else if (DT_BOOL == type && DT_BOOL_LEN != len) {
        goto err_ret;
    } else if (DT_ENUM == type && DT_ENUM_LEN != len) {
        goto err_ret;
    }

    if (len > 0) {
        p_data = tal_malloc(len);
        if (NULL == p_data) {
            goto err_ret;
        }
        memcpy(p_data, data, len);
    }

    tuya_change_bt_dp_tlv(type, p_data, &len);

    node = (klv_node_s *)tal_malloc(sizeof(klv_node_s));
    if (NULL == node) {
        goto err_ret;
    }
    memset(node, 0, sizeof(klv_node_s));
    if (len > 0) {
        node->data = tal_malloc(len);
        if (node->data == NULL) {
            tal_free((uint8_t *)node);
            goto err_ret;
        }
    }
    node->id = id;
    node->len = len;
    node->type = type;

    if (DT_VALUE == type || DT_BITMAP == type) {
        // change to big-end
        unsigned int tmp = *(unsigned int *)p_data;
        // unsigned char shift = 0;
        node->data[0] = (tmp >> 24) & 0xff;
        node->data[1] = (tmp >> 16) & 0xff;
        node->data[2] = (tmp >> 8) & 0xff;
        node->data[3] = (tmp >> 0) & 0xff;
    } else {
        // Enumerations and BOOL have been converted to byte order in tuya_change_bt_dp_tlv. You can copy them directly
        if (len > 0) {
            memcpy((void *)node->data, (unsigned char *)p_data, len);
        }
    }
    node->next = list;
    tal_free(p_data);
    return node;

err_ret:
    PR_ERR("input err");
    free_klv_list(list);
    tal_free(p_data);
    return NULL;
}

OPERATE_RET klvlist_2_data(klv_node_s *list, uint8_t **data, uint32_t *len, uint32_t *time_stamp, BOOL_T query,
                           uint8_t flag)
{
    if (NULL == list || NULL == data || NULL == len) {
        PR_ERR("para err");
        return OPRT_INVALID_PARM;
    }

    static uint32_t sn = 1;
    klv_node_s *node = list;
    // type: 1: 4.x protocol dp report
    uint8_t type = 1;

    // count data len
    unsigned int mk_data_len = 0;

    if (1 == type) {
        mk_data_len += 7; // version(1)+sn(4)+type(1)+flag(1)
        if (NULL != time_stamp) {
            mk_data_len += 5; // lenth of timeType+time_data
        }
    } else if (2 == type) {
        mk_data_len += 3; // sn(2)+flag(1)
    }

    while (node) {
        mk_data_len += sizeof(klv_node_s) + node->len - sizeof(struct s_klv_node *);
        node = node->next;
    }

    unsigned char *mk_data = (unsigned char *)tal_malloc(mk_data_len);
    if (NULL == mk_data) {
        return OPRT_MALLOC_FAILED;
    }

    unsigned int offset = 0;
    // fill v4 head info
    if (1 == type) {
        mk_data[offset++] = 0;
        mk_data[offset++] = (sn & 0xff000000) >> 24;
        mk_data[offset++] = (sn & 0xff0000) >> 16;
        mk_data[offset++] = (sn & 0xff00) >> 8;
        mk_data[offset++] = (sn & 0xff);
        mk_data[offset++] = (query ? 1 : 0); // type
        mk_data[offset++] = flag;
        // fill time
        if (NULL != time_stamp) {
            mk_data[offset++] = 1;
            memcpy(&mk_data[offset], time_stamp, 4);
            offset += 4;
        }
    } else if (2 == type) {
        mk_data[offset++] = (sn & 0xff00) >> 8;
        mk_data[offset++] = (sn & 0xff);
        mk_data[offset++] = flag;
    }

    // fill data
    node = list;
    while (node) {
        mk_data[offset++] = node->id;
        mk_data[offset++] = node->type;
        if (1 == type) {
            mk_data[offset++] = 0xff & (node->len >> 8);
            mk_data[offset++] = node->len;
        } else {
            mk_data[offset++] = node->len;
        }
        if (node->len > 0) {
            memcpy(&mk_data[offset], node->data, node->len);
        }
        offset += node->len;
        node = node->next;
    }
    *len = offset;
    *data = mk_data;

    // if(ty_get_sync_rpt_flag() && (FALSE == query)){
    //     ty_upd_dp_sync_sn(sn);
    // }

    sn++;
    return OPRT_OK;
}

OPERATE_RET data_2_klvlist(uint8_t *data, uint32_t len, klv_node_s **list)
{
    if (NULL == data || NULL == list) {
        return OPRT_INVALID_PARM;
    }

    uint8_t type = 1;

    tuya_ble_raw_print("data_2_klvlist", 16, data, len);

    unsigned int offset = 0;
    klv_node_s *klv_list = NULL;
    klv_node_s *node = NULL;
    do {
        // not full klv
        if (type == 1) {
            if ((len - offset) < 4) {
                free_klv_list(klv_list);
                return OPRT_COM_ERROR;
            }
        } else {
            if ((len - offset) < 3) {
                free_klv_list(klv_list);
                return OPRT_COM_ERROR;
            }
        }

        node = (klv_node_s *)tal_malloc(sizeof(klv_node_s));
        if (NULL == node) {
            free_klv_list(klv_list);
            return OPRT_MALLOC_FAILED;
        }
        memset(node, 0, sizeof(klv_node_s));

        node->id = data[offset++];
        node->type = data[offset++];
        if (1 == type) {
            node->len = data[offset++];
            node->len = (node->len << 8) + data[offset++];
        } else {
            node->len = data[offset++];
        }

        if (node->len > 0) {
            node->data = tal_malloc(node->len + 1);
            if (node->data == NULL) {
                tal_free((uint8_t *)node);
                free_klv_list(klv_list);
                return OPRT_MALLOC_FAILED;
            }

            if ((len - offset) < node->len) { // is remain data len enougn?
                tal_free(node->data);
                tal_free((uint8_t *)node);
                free_klv_list(klv_list);
                return OPRT_COM_ERROR;
            }

            memcpy(node->data, &data[offset], node->len);
            node->data[node->len] = 0;
        }

        offset += node->len;
        node->next = klv_list;
        klv_list = node;
    } while (offset < len);

    if (NULL == klv_list) {
        return OPRT_COM_ERROR;
    }
    *list = klv_list;
    return OPRT_OK;
}

static OPERATE_RET __result_code_resp(uint16_t type, uint32_t ack_sn, uint8_t result_code)
{
    return tuya_ble_send(type, ack_sn, &result_code, 1);
}

static OPERATE_RET __result_code_resp_v4(uint16_t type, uint32_t ack_sn, uint8_t *data, uint8_t result_code)
{
    uint8_t data_code[6] = {0}; // version(1byte)+R_SN(4byte)+STATE(1byte)
    memcpy(data_code, data, 5);
    data_code[5] = result_code;

    return tuya_ble_send(type, ack_sn, data_code, 6);
}

static OPERATE_RET __dp_data_report_data(uint16_t type, uint8_t *p_data, uint32_t len)
{
    if ((p_data == NULL) || (len > TUYA_BLE_TRANSMISSION_MAX_DATA_LEN) || (len == 0)) {
        PR_ERR("dp data len err:%u", len);
        return OPRT_INVALID_PARM;
    }
    tuya_ble_raw_print("__dp_data_report_data", 64, p_data, len);
    return tuya_ble_send(type, 0, p_data, len);
}

klv_node_s *__make_obj_dp_klv_list(dp_rept_in_t *dpin)
{
    klv_node_s *p_node = NULL;
    int index = 0;

    for (index = 0; index < dpin->dpscnt; index++) {
        dp_obj_t *p_dp = dpin->dps + index;
        dp_type new_type = 0;
        void *new_data = NULL;
        uint16_t new_len = 0;

        switch (p_dp->type) {
        case PROP_BOOL: {
            new_type = DT_BOOL;
            new_data = &(p_dp->value.dp_bool);
            new_len = DT_BOOL_LEN;
            break;
        }
        case PROP_VALUE: {
            new_type = DT_VALUE;
            new_data = &(p_dp->value.dp_value);
            new_len = DT_VALUE_LEN;
            break;
        }
        case PROP_STR: {
            new_type = DT_STRING;
            new_data = p_dp->value.dp_str;
            new_len = strlen(p_dp->value.dp_str);
            break;
        }
        case PROP_ENUM: {
            new_type = DT_ENUM;
            new_data = &(p_dp->value.dp_enum);
            new_len = DT_ENUM_LEN;
            break;
        }
        case PROP_BITMAP: {
            new_type = DT_BITMAP;
            new_data = &(p_dp->value.dp_bitmap);
            new_len = DT_BITMAP_MAX;
            break;
        }
        default:
            PR_ERR("p_dp->type:%d invalid", p_dp->type);
            continue;
        }
        p_node = make_klv_list(p_node, p_dp->id, new_type, new_data, new_len);
    }

    return p_node;
}

OPERATE_RET ty_bt_dp_data_report(klv_node_s *p_node, uint32_t time_stamp)
{
    OPERATE_RET ret = OPRT_OK;
    uint16_t type = FRM_STAT_REPORT;

    if (NULL == p_node) {
        return OPRT_INVALID_PARM;
    }

    time_stamp = UNI_HTONL(time_stamp);
    uint32_t *p_time_stamp = (time_stamp > 0) ? &time_stamp : NULL;
    uint8_t *p_new_data = NULL;
    uint32_t new_data_len = 0;
    klvlist_2_data(p_node, &p_new_data, &new_data_len, p_time_stamp, FALSE, 0);
    free_klv_list(p_node);

    type = (NULL != p_time_stamp) ? FRM_DP_STAT_REPORT_WITH_TIME_V4 : FRM_DP_STAT_REPORT_V4;
    ret = __dp_data_report_data(type, p_new_data, new_data_len);
    tal_free(p_new_data);
    return ret;
}

static __attribute__((unused)) klv_node_s *__get_response_query_dp_data(const uint8_t *dpid, const uint8_t num)
{
    uint16_t i;
    klv_node_s *p_node = NULL;
    dp_schema_t *schema = tuya_iot_client_get()->schema;

    tal_mutex_lock(schema->mutex);
    for (i = 0; i < num; i++) {
        int id = dpid[i];
        dp_type new_type = 0;
        void *new_data = NULL;
        uint16_t new_len = 0;

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

        switch (dpnode->desc.prop_tp) {
        case PROP_BOOL: {
            new_type = DT_BOOL;
            new_data = &(dpnode->prop.prop_bool.value);
            new_len = DT_BOOL_LEN;
        } break;
        case PROP_VALUE: {
            new_type = DT_VALUE;
            new_data = &(dpnode->prop.prop_int.value);
            new_len = DT_VALUE_LEN;
        } break;
        case PROP_STR: {
            new_type = DT_STRING;
            new_data = dpnode->prop.prop_str.value;
            new_len = strlen(dpnode->prop.prop_str.value);
        } break;
        case PROP_ENUM: {
            new_type = DT_ENUM;
            new_data = &(dpnode->prop.prop_enum.value);
            new_len = DT_ENUM_LEN;
        } break;
        case PROP_BITMAP: {
            new_type = DT_BITMAP;
            new_data = &(dpnode->prop.prop_bitmap.value);
            new_len = DT_BITMAP_MAX;
        } break;
        default:
            break;
        }
        p_node = make_klv_list(p_node, dpnode->desc.id, new_type, new_data, new_len);
    }
    tal_mutex_unlock(schema->mutex);
    return p_node;
}

uint32_t __dp_get_time_stamp(dp_obj_t *dp_data, const uint32_t cnt)
{
    // time_stamp:0 indicates the current time.
    if (NULL == dp_data || 0 == cnt || 0 == dp_data[0].time_stamp) {
        return tal_time_get_posix();
    }
    // In the Bluetooth protocol, timestamps are not assigned to each DP
    // individually; therefore, when packaging DPs, only the timestamp of the
    // first DP is taken.
    return dp_data[0].time_stamp;
}

static int ble_dp_report(const dp_rept_in_t *dpin)
{
    klv_node_s *p_node = NULL;
    uint32_t time_stamp = 0;

    if (NULL == dpin) {
        return OPRT_INVALID_PARM;
    }

    switch (dpin->rept_type) {
    case T_OBJ_REPT: {
        time_stamp = __dp_get_time_stamp(dpin->dps, dpin->dpscnt);
        p_node = __make_obj_dp_klv_list((dp_rept_in_t *)dpin);
        break;
    }
    case T_STAT_REPT: {
        //! TODO:
        break;
    }
    case T_RAW_REPT: {
        p_node = make_klv_list(NULL, dpin->dp->id, DT_RAW, dpin->dp->data, dpin->dp->len);
        break;
    }
    default:
        return OPRT_INVALID_PARM;
    }

    return ty_bt_dp_data_report(p_node, time_stamp);
}

static int ble_dp_req(ble_packet_t *req, void *priv_data)
{
    uint8_t *data = NULL;
    uint16_t len = 0;

    tuya_ble_raw_print("ble dp", 32, req->data, req->len);

    if (req->type == FRM_DP_CMD_SEND_V4) {
        __result_code_resp_v4(FRM_DP_CMD_SEND_V4, req->sn, req->data, 0);
        data = req->data + 5;
        len = req->len - 5;
    } else {
        return OPRT_NOT_SUPPORTED;
    }

    cJSON *p_root = cJSON_CreateObject();
    if (NULL == p_root) {
        PR_DEBUG("json err");
        return OPRT_CR_CJSON_ERR;
    }
    cJSON *p_dps = cJSON_CreateObject();
    if (NULL == p_dps) {
        PR_DEBUG("json err");
        cJSON_Delete(p_root);
        return OPRT_CR_CJSON_ERR;
    }
    cJSON_AddItemToObject(p_root, "dps", p_dps);
    klv_node_s *list = NULL;
    int ret = data_2_klvlist(data, len, &list);
    if (OPRT_OK != ret) {
        PR_ERR("parse err:%d", ret);
        cJSON_Delete(p_root);
        return OPRT_CJSON_PARSE_ERR;
    }

    klv_node_s *p_tmp = list;
    while (p_tmp != NULL) {
        PR_DEBUG("ble dp id:%d type:%d len:%d", p_tmp->id, p_tmp->type, p_tmp->len);
        char dp_id_str[5] = {0};
        snprintf(dp_id_str, 5, "%d", p_tmp->id);
        switch (p_tmp->type) {
        case DT_RAW: {
            char *p_base64 = tal_malloc(p_tmp->len / 3 * 4 + 5);
            tuya_base64_encode(p_tmp->data, p_base64, p_tmp->len);
            cJSON_AddStringToObject(p_dps, dp_id_str, p_base64);
            tal_free(p_base64);
            break;
        }
        case DT_BOOL: {
            cJSON_AddBoolToObject(p_dps, dp_id_str, *(p_tmp->data));
            break;
        }
        case DT_BITMAP:
        case DT_VALUE: {
            int val = (p_tmp->data[0] << 24) + (p_tmp->data[1] << 16) + (p_tmp->data[2] << 8) + (p_tmp->data[3] << 0);
            cJSON_AddNumberToObject(p_dps, dp_id_str, val);
            break;
        }
        case DT_ENUM: {
            int val = p_tmp->data[0];
            dp_node_t *dpnode = dp_node_find(tuya_iot_client_get()->schema, p_tmp->id);
            if (NULL == dpnode) {
                PR_ERR("invalid dp id[%d]", p_tmp->id);
                break;
            }
            cJSON_AddStringToObject(p_dps, dp_id_str, dpnode->prop.prop_enum.pp_enum[val]);
            break;
        }

        case DT_STRING: {
            // In the Bluetooth protocol, empty strings do not include a
            // terminator and require special handling.
            char *str_val = (NULL == p_tmp->data) ? "" : (char *)(p_tmp->data);
            cJSON_AddStringToObject(p_dps, dp_id_str, str_val);
            break;
        }
        default:
            PR_NOTICE("type not support:%d", p_tmp->type);
            break;
        }
        p_tmp = p_tmp->next;
    }

    free_klv_list(list);

    return tuya_iot_dp_parse(tuya_iot_client_get(), DP_CMD_BT, p_root);
}

static int ble_dp_query(ble_packet_t *req, void *priv_data)
{
    PR_NOTICE("ble recv dp query");
    tuya_ble_raw_print("ble dp query", 16, req->data, req->len);
    __result_code_resp(req->type, req->sn, 0);

    dp_schema_t *schema = dp_schema_find(tuya_iot_client_get()->activate.devid);

    int i;
    klv_node_s *p_node = NULL;
    if (schema == NULL) {
        PR_DEBUG("schema null");
        return OPRT_INVALID_PARM;
    }
    tal_mutex_lock(schema->mutex);
    for (i = 0; i < schema->num; i++) {
        dp_node_t *dpnode = &(schema->node[i]);
        if (dpnode->desc.mode == M_WR) {
            PR_TRACE("Skip DP ID %d", dpnode->desc.id);
            continue;
        }
        if (dpnode->desc.type == T_RAW) {
            // do nth for now
        }
        if (dpnode->desc.type == T_OBJ) {
            dp_type new_type = 0;
            void *new_data = NULL;
            uint16_t new_len = 0;

            switch (dpnode->desc.prop_tp) {
            case PROP_BOOL: {
                new_type = DT_BOOL;
                new_data = &(dpnode->prop.prop_bool.value);
                new_len = DT_BOOL_LEN;
            } break;
            case PROP_VALUE: {
                new_type = DT_VALUE;
                new_data = &(dpnode->prop.prop_int.value);
                new_len = DT_VALUE_LEN;
            } break;
            case PROP_STR: {
                new_type = DT_STRING;
                new_data = dpnode->prop.prop_str.value;
                new_len = strlen(dpnode->prop.prop_str.value);
            } break;
            case PROP_ENUM: {
                new_type = DT_ENUM;
                new_data = &(dpnode->prop.prop_enum.value);
                new_len = DT_ENUM_LEN;
            } break;
            case PROP_BITMAP: {
                new_type = DT_BITMAP;
                new_data = &(dpnode->prop.prop_bitmap.value);
                new_len = DT_BITMAP_MAX;
            } break;
            default: {
                PR_ERR("unsupport dp type:%d", dpnode->desc.prop_tp);
            } break;
            }
            p_node = make_klv_list(p_node, dpnode->desc.id, new_type, new_data, new_len);
        }

    } /* end of for */
    tal_mutex_unlock(schema->mutex);

    if (p_node != NULL) {
        uint16_t type = FRM_DP_STAT_REPORT_V4;
        uint8_t *p_new_data = NULL;
        uint32_t new_data_len = 0;

        klvlist_2_data(p_node, &p_new_data, &new_data_len, NULL, TRUE, 0);
        free_klv_list(p_node);
        __dp_data_report_data(type, p_new_data, new_data_len);
        tal_free(p_new_data);
    }

    return OPRT_OK;
}

/**
 * @brief Reports the data point (DP) to the Tuya BLE device.
 *
 * This function is used to report the data point (DP) to the Tuya BLE device.
 *
 * @param[in] dpin Pointer to the structure containing the DP data to be
 * reported.
 * @return Returns the status of the DP report operation.
 *     - 0: Success
 *     - Other values: Error codes
 */
int tuya_ble_dp_report(dp_rept_in_t *dpin)
{
    return ble_dp_report(dpin);
}

/**
 * @brief Processes the BLE session data point (DP) packet.
 *
 * This function is responsible for processing the BLE session data point
 * packet. It determines the type of the packet and performs the corresponding
 * action.
 *
 * @param packet The BLE packet to process.
 * @param priv_data A pointer to private data associated with the packet.
 */
void ble_session_dp_process(ble_packet_t *packet, void *priv_data)
{
    int rt;

    switch (packet->type) {

    case FRM_DP_CMD_SEND_V4:
        TUYA_CALL_ERR_LOG(ble_dp_req(packet, priv_data));
        break;

    case FRM_STATE_QUERY:
        TUYA_CALL_ERR_LOG(ble_dp_query(packet, priv_data));
        break;

    case FRM_DP_STAT_REPORT_V4:
    case FRM_DP_STAT_REPORT_WITH_TIME_V4:
        PR_DEBUG("ble dp report ack");
        break;

    default:
        PR_TRACE("bt_dp can not process cmd: 0x%x ", packet->type);
        break;
    }
}

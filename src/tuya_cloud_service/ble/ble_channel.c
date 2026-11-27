/**
 * @file ble_channel.C
 * @brief Implementation of BLE channel management for data transmission and
 * reception. This file provides functionalities for BLE channel creation,
 * deletion, data packet processing, and response handling in a BLE
 * communication scenario. It supports large data packet handling by splitting
 * data into subpackets, managing subpacket transmission, and reassembling
 * received subpackets.
 *
 * @copyright Copyright (c) 2021-2024 Tuya Inc. All Rights Reserved.
 *
 */

#include "tal_api.h"
#include "ble_channel.h"

#define SUBPACKET_RECV_ALL_DONE         0
#define SUBPACKET_RECV_ONE_AND_NEXT     1
#define SUBPACKET_RECV_ERROR_RESTART    2
#define SUBPACKET_RECV_ERROR_DISCONNECT 3

#pragma pack(1)
typedef struct subpacketAck {
    uint16_t flag;
    uint8_t  status;
    uint16_t curSubpacketNo;
    uint16_t cursubpacketLen;
    uint32_t receivedLen;
    uint32_t totalLen;
} ble_channel_ack_t;
#pragma pack()

typedef struct {
    uint8_t *rsp_data;
    uint8_t *subpack_data;
    uint32_t subpack_no;
    uint32_t subpack_sent;
    uint32_t subpack_len;
} ble_channel_mgr_t;

typedef struct {
    uint8_t *buf;
    uint32_t total_len;
    uint32_t received_len;
    uint32_t expected_subpacket_no;
} ble_channel_rx_t;

static ble_channel_mgr_t s_ble_channel_mgr   = {0}; // APP downlink channel transparent transmission
static ble_channel_rx_t  s_ble_channel_rx    = {0}; // downlink subpacket reassembly state
static MUTEX_HANDLE      s_ble_channel_mutex = NULL;

typedef struct {
    ble_channel_fn_t function;
    void            *priv_data;
} ble_channel_t;

static ble_channel_t s_ble_channel[BLE_CHANNEL_MAX];
static bool          s_ble_wifi_list_cap = false;

/**
 * @brief Adds a BLE channel.
 *
 * This function adds a BLE channel of the specified type with the provided
 * function and private data.
 *
 * @param type The type of the BLE channel.
 * @param fn The function to be associated with the BLE channel.
 * @param priv_data The private data to be associated with the BLE channel.
 * @return Returns OPRT_OK if the BLE channel was added successfully, or
 * OPRT_INVALID_PARM if the type is invalid.
 */
int ble_channel_add(ble_channel_type_t type, ble_channel_fn_t fn, void *priv_data)
{
    ble_channel_t *channel = s_ble_channel;

    if (NULL == s_ble_channel_mutex) {
        if (OPRT_OK != tal_mutex_create_init(&s_ble_channel_mutex)) {
            PR_ERR("create ble channel mutex failed");
        }
    }

    if (type < BLE_CHANNEL_MAX) {
        channel[type].function  = fn;
        channel[type].priv_data = priv_data;

        return OPRT_OK;
    }

    return OPRT_INVALID_PARM;
}

/**
 * @brief Deletes a BLE channel.
 *
 * This function deletes a BLE channel of the specified type.
 *
 * @param type The type of the BLE channel to delete.
 * @return Returns OPRT_OK if the channel is successfully deleted, or
 * OPRT_INVALID_PARM if the type is invalid.
 */
int ble_channel_del(ble_channel_type_t type)
{
    ble_channel_t *channel = s_ble_channel;

    if (type < BLE_CHANNEL_MAX) {
        channel[type].function  = NULL;
        channel[type].priv_data = NULL;
        return OPRT_OK;
    }

    return OPRT_INVALID_PARM;
}

void ble_channel_reset_rx(void)
{
    if (s_ble_channel_rx.buf != NULL) {
        tal_free(s_ble_channel_rx.buf);
    }
    memset(&s_ble_channel_rx, 0, sizeof(s_ble_channel_rx));
}

void ble_channel_deinit(void)
{
    ble_channel_reset_rx();
    if (NULL != s_ble_channel_mutex) {
        tal_mutex_lock(s_ble_channel_mutex);
    }
    if (s_ble_channel_mgr.rsp_data != NULL) {
        tal_free(s_ble_channel_mgr.rsp_data);
    }
    memset(&s_ble_channel_mgr, 0, sizeof(s_ble_channel_mgr));
    if (NULL != s_ble_channel_mutex) {
        tal_mutex_unlock(s_ble_channel_mutex);
    }
    memset(s_ble_channel, 0, sizeof(s_ble_channel));
    s_ble_wifi_list_cap = false;
}

bool ble_channel_is_registered(ble_channel_type_t type)
{
    if (type < BLE_CHANNEL_MAX) {
        return (NULL != s_ble_channel[type].function);
    }
    return false;
}

void ble_channel_wifi_list_cap_set(bool enable)
{
    s_ble_wifi_list_cap = enable;
}

uint8_t ble_channel_combos_flag_get(void)
{
    uint8_t flag = 0;

    if (ble_channel_is_registered(BLE_CHANNLE_WIFI_LST) && s_ble_wifi_list_cap) {
        flag |= (uint8_t)(1 << 0);
    }
    if (ble_channel_is_registered(BLE_CHANNLE_NETCFG_STAT)) {
        flag |= (uint8_t)(1 << 1);
    }

    return flag;
}

static void ble_channel_process(void *data, uint32_t len)
{
    uint8_t *user_data = (uint8_t *)data;
    uint16_t type      = 0;

    ble_channel_t *channel = s_ble_channel;

    if (NULL == user_data || len < 2) {
        return;
    }

    type = (user_data[0] << 8) | (user_data[1]);

    PR_DEBUG("ble channel type:%x", type);

    if (type < BLE_CHANNEL_MAX) {
        if (channel[type].function) {
            channel[type].function(user_data + 2, len - 2, channel[type].priv_data);
        } else {
            PR_DEBUG("ble channel not add :%x", type);
        }
    }
}

// raw_len keeps the varint from reading past the received bytes; returns 0 on truncation
static uint32_t __extract_packet_len(const uint8_t *raw_data, uint32_t raw_len, uint32_t *pNum)
{
    uint32_t i          = 0;
    uint32_t multiplier = 1;
    uint8_t  digit      = 0;
    uint32_t offset     = 0;
    uint32_t num        = 0;

    *pNum = 0;

    for (i = 0; i < 4 && offset < raw_len; i++) {
        digit = raw_data[offset++];
        num += (uint32_t)(digit & 0x7f) * multiplier;
        multiplier *= 0x80;

        if (0 == (digit & 0x80)) {
            *pNum = num;
            return offset;
        }
    }

    return 0;
}

static void __response_to_app_by_subpack(uint16_t type)
{
    uint32_t subpack_sent = 0;
    uint32_t subpack_len  = s_ble_channel_mgr.subpack_len;
    uint32_t subpack_no   = 0;
    uint16_t pkg_offset   = 0;
    uint16_t pkg_data_len = 0;
    uint8_t *pkg_buff     = NULL;
    uint8_t  flag         = 0x00;

    if (NULL == s_ble_channel_mgr.rsp_data) {
        PR_ERR("no subpack data");
        return;
    }

    pkg_buff = tal_malloc(TUYA_BLE_TRANS_DATA_SUBPACK_LEN);
    if (NULL == pkg_buff) {
        PR_ERR("malloc error in __response_to_app_by_subpack");
        return;
    }
    memset(pkg_buff, 0, TUYA_BLE_TRANS_DATA_SUBPACK_LEN);

    if (subpack_len > (TUYA_BLE_TRANS_DATA_SUBPACK_LEN - 2)) {
        flag = 0x02;
    }

    while (subpack_sent < subpack_len) { // has subpack data not sent yet
        pkg_offset             = 0;
        pkg_buff[pkg_offset++] = 0x00;
        pkg_buff[pkg_offset++] = flag;
        if (flag & 0x02) {
            pkg_buff[pkg_offset++] = subpack_no;
            if (0 == subpack_no) {
                uint32_t tmp = subpack_len;
                for (int i = 0; i < 4; i++) {
                    pkg_buff[pkg_offset] = tmp % 0x80;
                    if (tmp / 0x80) {
                        pkg_buff[pkg_offset] |= 0x80;
                    }
                    pkg_offset++;
                    tmp /= 0x80;
                    if (0 == tmp) {
                        break;
                    }
                }
                pkg_buff[pkg_offset++] = (TUYA_BLE_PROTOCOL_VERSION_HIGN << 0x04);
            }
        }

        pkg_data_len = TUYA_BLE_TRANS_DATA_SUBPACK_LEN - pkg_offset;
        if ((subpack_len - subpack_sent) < pkg_data_len) {
            pkg_data_len = subpack_len - subpack_sent;
        }
        memcpy(pkg_buff + pkg_offset, s_ble_channel_mgr.subpack_data + subpack_sent, pkg_data_len);
        tuya_ble_send(type, 0, pkg_buff, pkg_offset + pkg_data_len);

        subpack_sent += pkg_data_len;
        subpack_no++;
    }

    tal_free(pkg_buff);
}

/**
 * @brief Sends a response to the app by subpack.
 *
 * This function is responsible for sending a response to the app by subpack.
 * It takes the type, data, and length of the response as parameters.
 * The function first checks if there is any previous subpack data, and if so,
 * it frees the memory. Then, it initializes the s_ble_channel_mgr structure,
 * sets the response data, subpack data, and subpack length. Finally, it prints
 * a debug message and calls the __response_to_app_by_subpack() function.
 *
 * @param type The type of the response.
 * @param data The data of the response.
 * @param len The length of the response.
 */
void ble_channle_ack(uint16_t type, uint8_t *data, uint32_t len)
{
    if (NULL == data || len < 2) {
        return;
    }

    if (NULL == s_ble_channel_mutex) {
        tal_mutex_create_init(&s_ble_channel_mutex);
    }

    /* the reply may come from another task, serialize the buffer and cursor */
    if (NULL != s_ble_channel_mutex) {
        tal_mutex_lock(s_ble_channel_mutex);
    }

    if (s_ble_channel_mgr.rsp_data) {
        tal_free(s_ble_channel_mgr.rsp_data);
    }

    memset(&s_ble_channel_mgr, 0, sizeof(s_ble_channel_mgr));
    s_ble_channel_mgr.rsp_data     = data;
    s_ble_channel_mgr.subpack_data = data + 2;
    s_ble_channel_mgr.subpack_len  = len - 2;

    PR_DEBUG("start to send subpack cmd:%x, subcmd:%x", type, (len > 3) ? data[3] : 0);
    __response_to_app_by_subpack(type);

    tal_free(s_ble_channel_mgr.rsp_data);
    memset(&s_ble_channel_mgr, 0, sizeof(s_ble_channel_mgr));

    if (NULL != s_ble_channel_mutex) {
        tal_mutex_unlock(s_ble_channel_mutex);
    }
}

void ble_channel_reply_buf(uint16_t subcmd, uint8_t *buf, uint32_t payload_len)
{
    if (NULL == buf) {
        return;
    }

    buf[0] = 0x00;
    buf[1] = 0x00;
    buf[2] = (uint8_t)((subcmd >> 8) & 0xFF);
    buf[3] = (uint8_t)(subcmd & 0xFF);

    ble_channle_ack(FRM_UPLINK_TRANSPARENT_SPEC_REQ, buf, BLE_CHANNEL_REPLY_HDR_LEN + payload_len);
}

OPERATE_RET ble_channel_reply(uint16_t subcmd, const uint8_t *data, uint32_t len)
{
    uint8_t *resp = NULL;

    if (NULL == data || 0 == len) {
        return OPRT_INVALID_PARM;
    }

    resp = (uint8_t *)tal_malloc(BLE_CHANNEL_REPLY_HDR_LEN + len);
    if (NULL == resp) {
        PR_ERR("ble channel reply malloc err");
        return OPRT_MALLOC_FAILED;
    }

    memcpy(resp + BLE_CHANNEL_REPLY_HDR_LEN, data, len);
    ble_channel_reply_buf(subcmd, resp, len);

    return OPRT_OK;
}

/**
 * @brief Processes the BLE session channel.
 *
 * This function is responsible for processing the BLE session channel based on
 * the received BLE packet. It handles both downlink and uplink transparent
 * requests and transparent specific requests. For downlink requests, it checks
 * if the packet is a subpacket or not and processes accordingly. For uplink
 * requests, it handles different status codes and takes appropriate actions.
 *
 * @param req The BLE packet to be processed.
 * @param user_data User data associated with the BLE packet.
 */
void ble_session_channel_process(ble_packet_t *req, void *user_data)
{
    (void)user_data;

    if (NULL == req || NULL == req->data) {
        return;
    }

    if (!(req->type == FRM_DOWNLINK_TRANSPARENT_REQ || req->type == FRM_DOWNLINK_TRANSPARENT_SPEC_REQ ||
          req->type == FRM_UPLINK_TRANSPARENT_REQ || req->type == FRM_UPLINK_TRANSPARENT_SPEC_REQ)) {
        return;
    }

    uint8_t *pRawData = req->data;
    uint16_t pRawLen  = req->len;

    // the parse below reads pRawData[1] and pRawData[2]
    if (pRawLen < 3) { // flag(2) + sub-packet no(1)
        PR_ERR("ble channel frame too short:%d", pRawLen);
        return;
    }

    if (FRM_DOWNLINK_TRANSPARENT_REQ == req->type || FRM_DOWNLINK_TRANSPARENT_SPEC_REQ == req->type) {
        tuya_ble_raw_print("recv_downlink_frame", 16, pRawData, pRawLen);

        // [0~1]: flag
        // bit0: 0 - not need response, 1 - need response
        // bit1: 0 - not subpacket, 1 - subpacket
        if (pRawData[1] & 0x02) { // subpacket process
            uint32_t offset          = 0;
            uint32_t len_bytes       = 0;
            uint32_t curSubpacketNo  = 0;
            uint32_t curSubpacketLen = 0;

            if (pRawData[2] == 0) { // first subpacket
                uint32_t totalLen = 0;
                ble_channel_reset_rx();
                offset    = 3; // skip flag and first subpacket no, total 3B
                len_bytes = __extract_packet_len(pRawData + offset, pRawLen - offset, &totalLen);
                if (0 == len_bytes || offset + len_bytes + 1 > pRawLen || totalLen < 2 ||
                    totalLen > TUYA_BLE_TRANS_DATA_TOTAL_LEN_MAX) {
                    PR_ERR("ble channel first subpacket invalid, totalLen:%u, rawLen:%u", (unsigned int)totalLen,
                           (unsigned int)pRawLen);
                    return;
                }
                offset += len_bytes + 1; // skip length varint + version/reserve (1B)
                curSubpacketLen = pRawLen - offset;
                if (curSubpacketLen > totalLen) {
                    PR_ERR("ble channel subpacket len err:%u/%u", (unsigned int)pRawLen, (unsigned int)totalLen);
                    return;
                }
                PR_DEBUG("first downlink subpacket, totalLen:%u", (unsigned int)totalLen);

                // spare byte terminates the payload for cJSON_Parse
                s_ble_channel_rx.buf = tal_malloc(totalLen + 1);
                if (NULL == s_ble_channel_rx.buf) {
                    PR_ERR("malloc error for pBuffer");
                    return;
                }
                s_ble_channel_rx.buf[totalLen]         = 0;
                s_ble_channel_rx.total_len             = totalLen;
                s_ble_channel_rx.received_len          = 0;
                s_ble_channel_rx.expected_subpacket_no = 1;

                memcpy(s_ble_channel_rx.buf, pRawData + offset, curSubpacketLen);
                s_ble_channel_rx.received_len = curSubpacketLen;
                curSubpacketNo                = 0;
            } else { // subsequent subpackets
                if (NULL == s_ble_channel_rx.buf) {
                    PR_ERR("subsequent downlink subpacket without buffer");
                    return;
                }

                offset    = 2; // skip flag, total 2B
                len_bytes = __extract_packet_len(pRawData + offset, pRawLen - offset, &curSubpacketNo);
                if (0 == len_bytes || offset + len_bytes > pRawLen) {
                    PR_ERR("invalid subsequent downlink subpacket header");
                    ble_channel_reset_rx();
                    return;
                }
                offset += len_bytes;
                if (curSubpacketNo != s_ble_channel_rx.expected_subpacket_no) {
                    PR_ERR("ble channel subpacket no err:%u/%u", (unsigned int)curSubpacketNo,
                           (unsigned int)s_ble_channel_rx.expected_subpacket_no);
                    return;
                }
                curSubpacketLen = pRawLen - offset;
                if (s_ble_channel_rx.received_len > s_ble_channel_rx.total_len ||
                    curSubpacketLen > (s_ble_channel_rx.total_len - s_ble_channel_rx.received_len)) {
                    PR_ERR("ble channel subpacket overflow: %u + %u > %u", (unsigned int)s_ble_channel_rx.received_len,
                           (unsigned int)curSubpacketLen, (unsigned int)s_ble_channel_rx.total_len);
                    ble_channel_reset_rx();
                    return;
                }
                memcpy(s_ble_channel_rx.buf + s_ble_channel_rx.received_len, pRawData + offset, curSubpacketLen);
                s_ble_channel_rx.received_len += curSubpacketLen;
                s_ble_channel_rx.expected_subpacket_no++;
            }

            PR_DEBUG("rece downlink subpacket, curSubpacketNo:%u, "
                     "curSubpacketLen:%u, receivedLen:%u, totalLen:%u",
                     (unsigned int)curSubpacketNo, (unsigned int)curSubpacketLen,
                     (unsigned int)s_ble_channel_rx.received_len, (unsigned int)s_ble_channel_rx.total_len);

            ble_channel_ack_t *pAck = tal_malloc(sizeof(ble_channel_ack_t));
            if (pAck == NULL) {
                PR_ERR("malloc error for pAck");
                ble_channel_reset_rx();
                return;
            }
            pAck->flag            = (pRawData[1] << 8) | pRawData[0];
            pAck->curSubpacketNo  = curSubpacketNo;
            pAck->cursubpacketLen = curSubpacketLen;
            pAck->receivedLen     = s_ble_channel_rx.received_len;
            pAck->totalLen        = s_ble_channel_rx.total_len;

            if (s_ble_channel_rx.received_len < s_ble_channel_rx.total_len) {
                pAck->status = SUBPACKET_RECV_ONE_AND_NEXT;
                tuya_ble_send(req->type, req->sn, (uint8_t *)pAck, sizeof(ble_channel_ack_t));
                tal_free(pAck);
            } else {
                pAck->status = SUBPACKET_RECV_ALL_DONE;
                tuya_ble_send(req->type, req->sn, (uint8_t *)pAck, sizeof(ble_channel_ack_t));
                tal_free(pAck);

                tuya_ble_raw_print("recv_donwlink_cmd", 16, s_ble_channel_rx.buf, s_ble_channel_rx.total_len);
                ble_channel_process(s_ble_channel_rx.buf, s_ble_channel_rx.total_len);
                ble_channel_reset_rx();
            }
        } else { // not subpacket process
            // ble_channel_process() reads the channel type at pRawData[2..3]
            if (pRawLen < 4) {
                PR_ERR("ble channel frame too short:%d", pRawLen);
                return;
            }
            tuya_ble_raw_print("recv_downlink_cmd", 16, pRawData, pRawLen);
            ble_channel_process(pRawData + 2, pRawLen - 2);
        }
    } else if (FRM_UPLINK_TRANSPARENT_REQ == req->type || FRM_UPLINK_TRANSPARENT_SPEC_REQ == req->type) {
        tuya_ble_raw_print("recv_uplink_frame", 16, pRawData, pRawLen);
        uint8_t status = pRawData[2];
        if (NULL != s_ble_channel_mutex) {
            tal_mutex_lock(s_ble_channel_mutex);
        }
        if (status == 0) { // complete subpack send
            if (s_ble_channel_mgr.rsp_data) {
                tal_free(s_ble_channel_mgr.rsp_data);
            }
            memset(&s_ble_channel_mgr, 0, sizeof(s_ble_channel_mgr));
        } else if (status == 1) { // continue subpack send
            __response_to_app_by_subpack(req->type);
        } else if (status == 2) { // restart subpack send
            s_ble_channel_mgr.subpack_no   = 0;
            s_ble_channel_mgr.subpack_sent = 0;
            __response_to_app_by_subpack(req->type);
        } else {
        }
        if (NULL != s_ble_channel_mutex) {
            tal_mutex_unlock(s_ble_channel_mutex);
        }
    }
}

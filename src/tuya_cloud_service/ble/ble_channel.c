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
    uint8_t status;
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

static ble_channel_mgr_t s_ble_channel_mgr = {0}; // APP downlink channel transparent transmission

typedef struct {
    ble_channel_fn_t function;
    void *priv_data;
} ble_channel_t;

static ble_channel_t s_ble_channel[BLE_CHANNEL_MAX];

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

    if (type < BLE_CHANNEL_MAX) {
        channel[type].function = fn;
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
        channel[type].function = NULL;
        channel[type].priv_data = NULL;
        return OPRT_OK;
    }

    return OPRT_INVALID_PARM;
}

static void ble_channel_process(void *data)
{
    if (NULL == data) {
        return;
    }
    uint8_t *user_data = (uint8_t *)data;
    uint16_t type = (user_data[0] << 8) | (user_data[1]);

    ble_channel_t *channel = s_ble_channel;

    PR_DEBUG("ble channel type:%x", type);

    if (type < BLE_CHANNEL_MAX) {
        if (channel[type].function) {
            channel[type].function(user_data + 2, channel[type].priv_data);
        } else {
            PR_DEBUG("ble channel not add :%x", type);
        }
    }
}

static uint32_t s_channel_total_len = 0;
static uint8_t *s_channel_buffer = NULL;
static uint32_t s_channel_received_len = 0;

void ble_channel_reset(void)
{
    if (s_channel_buffer) {
        tal_free(s_channel_buffer);
        s_channel_buffer = NULL;
    }
    s_channel_total_len = 0;
    s_channel_received_len = 0;
    if (s_ble_channel_mgr.rsp_data) {
        tal_free(s_ble_channel_mgr.rsp_data);
        s_ble_channel_mgr.rsp_data = NULL;
    }
    memset(&s_ble_channel_mgr, 0, sizeof(s_ble_channel_mgr));
}

uint32_t __extract_packet_len(uint8_t *raw_data, uint32_t max_len, uint32_t *pNum)
{
    uint8_t i = 0;
    uint32_t multiplier = 1;
    uint8_t digit;
    uint8_t offset = 0;
    uint32_t num = 0;

    if (NULL == raw_data || NULL == pNum || max_len == 0) {
        if (pNum) {
            *pNum = 0;
        }
        return 0;
    }

    for (i = 0; i < 4 && offset < max_len; i++) {
        digit = raw_data[offset++];
        num += (digit & 0x7f) * multiplier;
        multiplier *= 0x80;

        if (0 == (digit & 0x80)) {
            *pNum = num;
            return offset;
        }
    }
    *pNum = 0;
    return 0;
}

static void __response_to_app_by_subpack(uint16_t type)
{
    uint32_t subpack_sent = s_ble_channel_mgr.subpack_sent;
    uint32_t subpack_len = s_ble_channel_mgr.subpack_len;
    uint32_t subpack_no = s_ble_channel_mgr.subpack_no;
    uint16_t pkg_offset = 0;
    uint16_t pkg_data_len = 0;
    uint8_t *pkg_buff = NULL;

    if (NULL == s_ble_channel_mgr.rsp_data) {
        PR_ERR("no subpack data");
        return;
    }

    pkg_buff = tal_malloc(TUYA_BLE_TRANS_DATA_SUBPACK_LEN);
    if (NULL == pkg_buff) {
        PR_ERR("malloc error in __get_timer_data");
        return;
    }
    memset(pkg_buff, 0, TUYA_BLE_TRANS_DATA_SUBPACK_LEN);

    if (subpack_sent < subpack_len) { // has subpack data not sent yet
        pkg_offset = 0;
        pkg_buff[pkg_offset++] = 0x00;
        pkg_buff[pkg_offset++] = 0x03; // flag: set as need subpacket, need response
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

        pkg_data_len = TUYA_BLE_TRANS_DATA_SUBPACK_LEN - pkg_offset;
        if ((subpack_len - subpack_sent) < pkg_data_len) {
            pkg_data_len = subpack_len - subpack_sent;
        }
        memcpy(pkg_buff + pkg_offset, s_ble_channel_mgr.subpack_data + subpack_sent, pkg_data_len);
        tuya_ble_send(type, 0, pkg_buff, pkg_offset + pkg_data_len);

        s_ble_channel_mgr.subpack_sent += pkg_data_len;
        s_ble_channel_mgr.subpack_no++;
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
    if (s_ble_channel_mgr.rsp_data) {
        PR_ERR("pre subpack data overwrite!");
        tal_free(s_ble_channel_mgr.rsp_data);
    }

    memset(&s_ble_channel_mgr, 0, sizeof(s_ble_channel_mgr));
    s_ble_channel_mgr.rsp_data = data;
    s_ble_channel_mgr.subpack_data = data + 2;
    s_ble_channel_mgr.subpack_len = len - 2;

    PR_DEBUG("start to send subpack cmd:%x, subcmd:%x", type, data[3]);
    __response_to_app_by_subpack(type);
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
    if (NULL == req || NULL == req->data || req->len < 3) {
        return;
    }

    if (!(req->type == FRM_DOWNLINK_TRANSPARENT_REQ || req->type == FRM_DOWNLINK_TRANSPARENT_SPEC_REQ ||
          req->type == FRM_UPLINK_TRANSPARENT_REQ || req->type == FRM_UPLINK_TRANSPARENT_SPEC_REQ)) {
        return;
    }

    uint8_t *pRawData = req->data;
    uint16_t pRawLen = req->len;

    if (FRM_DOWNLINK_TRANSPARENT_REQ == req->type || FRM_DOWNLINK_TRANSPARENT_SPEC_REQ == req->type) {
        tuya_ble_raw_print("recv_downlink_frame", 16, pRawData, pRawLen);

        // [0~1]: flag
        // bit0: 0 - not need response, 1 - need response
        // bit1: 0 - not subpacket, 1 - subpacket
        if (pRawData[1] & 0x02) { // subpacket process
            uint32_t offset = 0;
            uint32_t curSubpacketNo = 0;
            uint32_t curSubpacketLen = 0;

            if (pRawData[2] == 0) { // first subpacket
                offset = 3;         // skip flag and first subpacket no, total 3B
                ble_channel_reset();

                if (offset >= pRawLen) {
                    PR_ERR("pRawLen %u <= offset %u", pRawLen, offset);
                    return;
                }
                uint32_t extracted_len =
                    __extract_packet_len(pRawData + offset, pRawLen - offset, &s_channel_total_len);
                if (extracted_len == 0 || s_channel_total_len == 0 || s_channel_total_len > TUYA_BLE_AIR_FRAME_MAX) {
                    PR_ERR("invalid channel totalLen: %u", s_channel_total_len);
                    ble_channel_reset();
                    return;
                }
                offset += extracted_len;
                PR_DEBUG("first downlink subpacket, totalLen:%u", s_channel_total_len);

                s_channel_buffer = tal_malloc(s_channel_total_len + 1);
                if (NULL == s_channel_buffer) {
                    PR_ERR("malloc error for s_channel_buffer");
                    ble_channel_reset();
                    return;
                }
                memset(s_channel_buffer, 0, s_channel_total_len + 1);

                offset += 1; // skip version and reserve, total 1B
                if (offset > pRawLen) {
                    PR_ERR("offset %u > pRawLen %u", offset, pRawLen);
                    ble_channel_reset();
                    return;
                }
                curSubpacketLen = pRawLen - offset;
                if (curSubpacketLen > s_channel_total_len) {
                    PR_ERR("curSubpacketLen %u > totalLen %u", curSubpacketLen, s_channel_total_len);
                    ble_channel_reset();
                    return;
                }
                memcpy(s_channel_buffer + s_channel_received_len, pRawData + offset, curSubpacketLen);
                s_channel_received_len += curSubpacketLen;
                curSubpacketNo = 0;
            } else { // subsequent subpackets
                if (NULL == s_channel_buffer || s_channel_total_len == 0) {
                    PR_ERR("s_channel_buffer is NULL in subsequent subpacket");
                    return;
                }

                offset = 2; // skip flag, total 2B
                if (offset >= pRawLen) {
                    PR_ERR("pRawLen %u <= offset %u", pRawLen, offset);
                    ble_channel_reset();
                    return;
                }
                uint32_t extracted_len = __extract_packet_len(pRawData + offset, pRawLen - offset, &curSubpacketNo);
                if (extracted_len == 0) {
                    PR_ERR("extract curSubpacketNo failed");
                    ble_channel_reset();
                    return;
                }
                offset += extracted_len;
                if (offset > pRawLen) {
                    PR_ERR("offset %u > pRawLen %u", offset, pRawLen);
                    ble_channel_reset();
                    return;
                }
                curSubpacketLen = pRawLen - offset;
                if (curSubpacketLen > s_channel_total_len - s_channel_received_len) {
                    PR_ERR("curSubpacketLen %u > remaining space %u", curSubpacketLen,
                           s_channel_total_len - s_channel_received_len);
                    ble_channel_reset();
                    return;
                }
                memcpy(s_channel_buffer + s_channel_received_len, pRawData + offset, curSubpacketLen);
                s_channel_received_len += curSubpacketLen;
            }

            PR_DEBUG("rece downlink subpacket, curSubpacketNo:%u, "
                     "curSubpacketLen:%u, receivedLen:%u, totalLen:%u",
                     curSubpacketNo, curSubpacketLen, s_channel_received_len, s_channel_total_len);

            ble_channel_ack_t *pAck = tal_malloc(sizeof(ble_channel_ack_t));
            if (pAck == NULL) {
                PR_ERR("malloc error for pAck");
                ble_channel_reset();
                return;
            }
            pAck->flag = (pRawData[1] << 8) | pRawData[0];
            pAck->curSubpacketNo = curSubpacketNo;
            pAck->cursubpacketLen = curSubpacketLen;
            pAck->receivedLen = s_channel_received_len;
            pAck->totalLen = s_channel_total_len;

            if (s_channel_received_len < s_channel_total_len) {
                pAck->status = SUBPACKET_RECV_ONE_AND_NEXT;
                tuya_ble_send(req->type, req->sn, (uint8_t *)pAck, sizeof(ble_channel_ack_t));
                tal_free(pAck);
            } else {
                pAck->status = SUBPACKET_RECV_ALL_DONE;
                tuya_ble_send(req->type, req->sn, (uint8_t *)pAck, sizeof(ble_channel_ack_t));
                tal_free(pAck);

                s_channel_buffer[s_channel_total_len] = 0;
                tuya_ble_raw_print("recv_donwlink_cmd", 16, s_channel_buffer, s_channel_total_len);
                ble_channel_process(s_channel_buffer);
                ble_channel_reset();
            }
        } else { // not subpacket process
            if (pRawLen >= 2) {
                tuya_ble_raw_print("recv_downlink_cmd", 16, pRawData, pRawLen);
                ble_channel_process(pRawData + 2);
            }
        }
    } else if (FRM_UPLINK_TRANSPARENT_REQ == req->type || FRM_UPLINK_TRANSPARENT_SPEC_REQ == req->type) {
        if (pRawLen >= 3) {
            tuya_ble_raw_print("recv_uplink_frame", 16, pRawData, pRawLen);
            uint8_t status = pRawData[2];
            if (status == 0) { // complete subpack send
                if (s_ble_channel_mgr.rsp_data) {
                    tal_free(s_ble_channel_mgr.rsp_data);
                }
                memset(&s_ble_channel_mgr, 0, sizeof(s_ble_channel_mgr));
            } else if (status == 1) { // continue subpack send
                __response_to_app_by_subpack(req->type);
            } else if (status == 2) { // restart subpack send
                s_ble_channel_mgr.subpack_no = 0;
                s_ble_channel_mgr.subpack_sent = 0;
                __response_to_app_by_subpack(req->type);
            } else {
            }
        }
    }
}

/**
 * @file ble_channel.c
 * @brief BLE transparent-channel framing and reassembly.
 *
 * @copyright Copyright (c) 2021-2026 Tuya Inc. All Rights Reserved.
 */

#include "tal_api.h"
#include "ble_channel.h"

#define SUBPACKET_RECV_ALL_DONE     0
#define SUBPACKET_RECV_ONE_AND_NEXT 1

#pragma pack(1)
typedef struct {
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
} ble_channel_tx_t;

typedef struct {
    uint8_t *buffer;
    uint32_t capacity;
    uint32_t received;
    uint32_t expected_no;
} ble_channel_rx_t;

typedef struct {
    ble_channel_fn_t function;
    void            *priv_data;
} ble_channel_t;

static ble_channel_tx_t s_ble_channel_tx;
static ble_channel_rx_t s_ble_channel_rx;
static ble_channel_t    s_ble_channel[BLE_CHANNEL_MAX];

static int ble_channel_varint_decode(const uint8_t *data, uint32_t data_len, uint32_t *offset, uint32_t *value)
{
    uint32_t result = 0;

    for (uint8_t i = 0; i < 4; i++) {
        if (*offset >= data_len) {
            return OPRT_INVALID_PARM;
        }
        uint8_t digit = data[(*offset)++];
        result |= (uint32_t)(digit & 0x7f) << (i * 7);
        if ((digit & 0x80) == 0) {
            *value = result;
            return OPRT_OK;
        }
    }

    return OPRT_INVALID_PARM;
}

static int ble_channel_varint_encode(uint32_t value, uint8_t *data, uint32_t capacity, uint32_t *offset)
{
    do {
        if (*offset >= capacity) {
            return OPRT_INVALID_PARM;
        }
        uint8_t digit = value & 0x7f;
        value >>= 7;
        data[(*offset)++] = digit | (value != 0 ? 0x80 : 0);
    } while (value != 0);

    return OPRT_OK;
}

void ble_channel_rx_reset(void)
{
    if (s_ble_channel_rx.buffer != NULL) {
        tal_free(s_ble_channel_rx.buffer);
    }
    memset(&s_ble_channel_rx, 0, sizeof(s_ble_channel_rx));
}

void ble_channel_tx_reset(void)
{
    if (s_ble_channel_tx.rsp_data != NULL) {
        tal_free(s_ble_channel_tx.rsp_data);
    }
    memset(&s_ble_channel_tx, 0, sizeof(s_ble_channel_tx));
}

void ble_channel_reset(void)
{
    ble_channel_rx_reset();
    ble_channel_tx_reset();
}

int ble_channel_add(ble_channel_type_t type, ble_channel_fn_t fn, void *priv_data)
{
    if (type >= BLE_CHANNEL_MAX) {
        return OPRT_INVALID_PARM;
    }
    s_ble_channel[type].function  = fn;
    s_ble_channel[type].priv_data = priv_data;
    return OPRT_OK;
}

int ble_channel_del(ble_channel_type_t type)
{
    if (type >= BLE_CHANNEL_MAX) {
        return OPRT_INVALID_PARM;
    }
    s_ble_channel[type].function  = NULL;
    s_ble_channel[type].priv_data = NULL;
    return OPRT_OK;
}

static void ble_channel_process(uint8_t *data, uint32_t len)
{
    if (data == NULL || len < 2) {
        return;
    }

    uint16_t type = ((uint16_t)data[0] << 8) | data[1];
    if (type < BLE_CHANNEL_MAX && s_ble_channel[type].function != NULL) {
        s_ble_channel[type].function(data + 2, s_ble_channel[type].priv_data);
    }
}

static void ble_channel_response_by_subpack(uint16_t type)
{
    if (s_ble_channel_tx.rsp_data == NULL || s_ble_channel_tx.subpack_sent > s_ble_channel_tx.subpack_len ||
        (s_ble_channel_tx.subpack_sent == s_ble_channel_tx.subpack_len && s_ble_channel_tx.subpack_no != 0)) {
        return;
    }

    uint8_t *packet = tal_malloc(TUYA_BLE_TRANS_DATA_SUBPACK_LEN);
    if (packet == NULL) {
        return;
    }

    uint32_t offset  = 0;
    packet[offset++] = 0;
    packet[offset++] = 3;
    if (ble_channel_varint_encode(s_ble_channel_tx.subpack_no, packet, TUYA_BLE_TRANS_DATA_SUBPACK_LEN, &offset) !=
        OPRT_OK) {
        tal_free(packet);
        return;
    }
    if (s_ble_channel_tx.subpack_no == 0) {
        if (ble_channel_varint_encode(s_ble_channel_tx.subpack_len, packet, TUYA_BLE_TRANS_DATA_SUBPACK_LEN, &offset) !=
                OPRT_OK ||
            offset >= TUYA_BLE_TRANS_DATA_SUBPACK_LEN) {
            tal_free(packet);
            return;
        }
        packet[offset++] = TUYA_BLE_PROTOCOL_VERSION_HIGN << 4;
    }

    uint32_t remaining = s_ble_channel_tx.subpack_len - s_ble_channel_tx.subpack_sent;
    uint32_t available = TUYA_BLE_TRANS_DATA_SUBPACK_LEN - offset;
    uint32_t data_len  = remaining < available ? remaining : available;
    if (data_len != 0) {
        memcpy(packet + offset, s_ble_channel_tx.subpack_data + s_ble_channel_tx.subpack_sent, data_len);
    }
    tuya_ble_send(type, 0, packet, offset + data_len);
    s_ble_channel_tx.subpack_sent += data_len;
    s_ble_channel_tx.subpack_no++;
    tal_free(packet);
}

void ble_channle_ack(uint16_t type, uint8_t *data, uint32_t len)
{
    if (data == NULL || len < 2 || len - 2 >= 0x10000000) {
        if (data != NULL) {
            tal_free(data);
        }
        return;
    }

    ble_channel_tx_reset();
    s_ble_channel_tx.rsp_data     = data;
    s_ble_channel_tx.subpack_data = data + 2;
    s_ble_channel_tx.subpack_len  = len - 2;
    ble_channel_response_by_subpack(type);
}

static void ble_channel_ack_send(ble_packet_t *req, uint32_t subpacket_no, uint32_t subpacket_len, uint8_t status)
{
    ble_channel_ack_t ack = {0};
    ack.flag              = ((uint16_t)req->data[1] << 8) | req->data[0];
    ack.status            = status;
    ack.curSubpacketNo    = subpacket_no;
    ack.cursubpacketLen   = subpacket_len;
    ack.receivedLen       = s_ble_channel_rx.received;
    ack.totalLen          = s_ble_channel_rx.capacity;
    tuya_ble_send(req->type, req->sn, (uint8_t *)&ack, sizeof(ack));
}

static void ble_channel_downlink_process(ble_packet_t *req)
{
    uint8_t *data = req->data;
    uint32_t len  = req->len;
    if (len < 2) {
        ble_channel_rx_reset();
        return;
    }

    if ((data[1] & 0x02) == 0) {
        if (len >= 4) {
            ble_channel_process(data + 2, len - 2);
        }
        return;
    }

    uint32_t offset       = 2;
    uint32_t subpacket_no = 0;
    if (ble_channel_varint_decode(data, len, &offset, &subpacket_no) != OPRT_OK) {
        ble_channel_rx_reset();
        return;
    }

    uint32_t fragment_len;
    if (subpacket_no == 0) {
        uint32_t total_len = 0;
        if (s_ble_channel_rx.buffer != NULL || ble_channel_varint_decode(data, len, &offset, &total_len) != OPRT_OK ||
            total_len < 2 || total_len > TUYA_BLE_AIR_FRAME_MAX || offset >= len) {
            ble_channel_rx_reset();
            return;
        }
        offset++;
        fragment_len = len - offset;
        if (fragment_len > total_len) {
            ble_channel_rx_reset();
            return;
        }
        s_ble_channel_rx.buffer = tal_malloc(total_len + 1);
        if (s_ble_channel_rx.buffer == NULL) {
            ble_channel_rx_reset();
            return;
        }
        s_ble_channel_rx.capacity    = total_len;
        s_ble_channel_rx.expected_no = 1;
    } else {
        if (s_ble_channel_rx.buffer == NULL || subpacket_no != s_ble_channel_rx.expected_no) {
            ble_channel_rx_reset();
            return;
        }
        fragment_len = len - offset;
        if (s_ble_channel_rx.received > s_ble_channel_rx.capacity ||
            fragment_len > s_ble_channel_rx.capacity - s_ble_channel_rx.received) {
            ble_channel_rx_reset();
            return;
        }
        s_ble_channel_rx.expected_no++;
    }

    if (fragment_len != 0) {
        memcpy(s_ble_channel_rx.buffer + s_ble_channel_rx.received, data + offset, fragment_len);
    } else if (s_ble_channel_rx.received < s_ble_channel_rx.capacity && subpacket_no != 0) {
        ble_channel_rx_reset();
        return;
    }
    s_ble_channel_rx.received += fragment_len;
    s_ble_channel_rx.buffer[s_ble_channel_rx.received] = '\0';

    if (s_ble_channel_rx.received < s_ble_channel_rx.capacity) {
        ble_channel_ack_send(req, subpacket_no, fragment_len, SUBPACKET_RECV_ONE_AND_NEXT);
        return;
    }

    ble_channel_ack_send(req, subpacket_no, fragment_len, SUBPACKET_RECV_ALL_DONE);
    ble_channel_process(s_ble_channel_rx.buffer, s_ble_channel_rx.capacity);
    ble_channel_rx_reset();
}

void ble_session_channel_process(ble_packet_t *req, void *user_data)
{
    (void)user_data;
    if (req == NULL || req->data == NULL) {
        return;
    }

    if (req->type == FRM_DOWNLINK_TRANSPARENT_REQ || req->type == FRM_DOWNLINK_TRANSPARENT_SPEC_REQ) {
        ble_channel_downlink_process(req);
        return;
    }
    if (req->type != FRM_UPLINK_TRANSPARENT_REQ && req->type != FRM_UPLINK_TRANSPARENT_SPEC_REQ) {
        return;
    }
    if (req->len < 3) {
        return;
    }

    uint8_t status = req->data[2];
    if (status == 0) {
        ble_channel_tx_reset();
    } else if (status == 1) {
        ble_channel_response_by_subpack(req->type);
    } else if (status == 2) {
        s_ble_channel_tx.subpack_no   = 0;
        s_ble_channel_tx.subpack_sent = 0;
        ble_channel_response_by_subpack(req->type);
    }
}

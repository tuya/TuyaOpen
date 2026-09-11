/**
 * @file ble_trsmitr.C
 * @brief Bluetooth Low Energy (BLE) data transmission module.
 * This module provides functions to create and manage BLE data transmission,
 * including encoding and decoding BLE packets for communication between
 * devices.
 *
 * @copyright Copyright (c) 2021-2026 Tuya Inc. All Rights Reserved.
 *
 */

#include "tal_api.h"
#include "ble_trsmitr.h"
#include "ble_mgr.h"
#include "ble_dp.h"

#include <stdbool.h>

#define __MUTLI_TSF_PROTOCOL_GLOBALS

/***********************************************************
*************************micro define***********************
***********************************************************/

/***********************************************************
*************************variable define********************
***********************************************************/
static ble_frame_seq_t s_ble_frame_seq        = 0;
static uint16_t        s_ble_frame_packet_len = 1024;

static int ble_frame_varint_decode(const uint8_t *data, uint16_t data_len, uint16_t *offset, uint32_t *value)
{
    uint32_t result = 0;

    for (uint8_t i = 0; i < 4; i++) {
        if (*offset >= data_len) {
            return OPRT_SVC_BT_API_TRSMITR_ERROR;
        }

        uint8_t digit = data[(*offset)++];
        result |= (uint32_t)(digit & 0x7f) << (i * 7);
        if ((digit & 0x80) == 0) {
            *value = result;
            return OPRT_OK;
        }
    }

    return OPRT_SVC_BT_API_TRSMITR_ERROR;
}

static int ble_frame_varint_encode(uint32_t value, uint8_t *data, uint16_t capacity, uint16_t *offset)
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

/***********************************************************
*************************function define********************
***********************************************************/
/**
 * @brief Creates a new instance of the ble_frame_trsmitr_t structure.
 *
 * This function allocates memory for the ble_frame_trsmitr_t structure and
 * initializes its members. It also allocates memory for the subpkg member based
 * on the value returned by ble_frame_packet_len_get().
 *
 * @return A pointer to the newly created ble_frame_trsmitr_t structure, or NULL
 * if memory allocation fails.
 */
ble_frame_trsmitr_t *ble_frame_trsmitr_create(void)
{
    ble_frame_trsmitr_t *trsmitr = (ble_frame_trsmitr_t *)tal_malloc(sizeof(ble_frame_trsmitr_t));
    if (NULL == trsmitr) {
        PR_DEBUG("malloc err");
        return NULL;
    }
    memset(trsmitr, 0, sizeof(ble_frame_trsmitr_t));
    trsmitr->subpkg_capacity = ble_frame_packet_len_get();
    if (trsmitr->subpkg_capacity == 0 || trsmitr->subpkg_capacity > TUYA_BLE_AIR_FRAME_MAX) {
        tal_free(trsmitr);
        return NULL;
    }

    trsmitr->subpkg = (uint8_t *)tal_malloc(trsmitr->subpkg_capacity);
    if (trsmitr->subpkg == NULL) {
        PR_ERR("malloc err:%d", ble_frame_packet_len_get());
        tal_free(trsmitr);
        return NULL;
    }
    memset(trsmitr->subpkg, 0, trsmitr->subpkg_capacity);

    return trsmitr;
}

/**
 * @brief Deletes a BLE frame transmitter.
 *
 * This function frees the memory allocated for a BLE frame transmitter and its
 * subpackage.
 *
 * @param trsmitr Pointer to the BLE frame transmitter to be deleted.
 */
void ble_frame_trsmitr_delete(ble_frame_trsmitr_t *trsmitr)
{
    if (trsmitr == NULL) {
        return;
    }

    tal_free(trsmitr->subpkg);
    tal_free(trsmitr);
}

void ble_frame_trsmitr_reset(ble_frame_trsmitr_t *trsmitr)
{
    if (trsmitr == NULL) {
        return;
    }
    uint8_t *subpkg   = trsmitr->subpkg;
    uint32_t capacity = trsmitr->subpkg_capacity;
    memset(trsmitr, 0, sizeof(*trsmitr));
    trsmitr->subpkg          = subpkg;
    trsmitr->subpkg_capacity = capacity;
}

/**
 * @brief Get the length of the subpacket in a BLE frame transmitter.
 *
 * This function returns the length of the subpacket in a BLE frame transmitter.
 *
 * @param trsmitr The BLE frame transmitter.
 * @return The length of the subpacket.
 */
ble_frame_subpkg_len_t ble_frame_subpacket_len_get(ble_frame_trsmitr_t *trsmitr)
{
    return trsmitr->subpkg_len;
}

/**
 * @brief Get the length of the BLE frame packet.
 *
 * This function returns the length of the BLE frame packet.
 *
 * @return The length of the BLE frame packet.
 */
uint16_t ble_frame_packet_len_get(void)
{
    return s_ble_frame_packet_len;
}

/**
 * @brief Sets the length of the BLE frame packet.
 *
 * This function sets the length of the BLE frame packet to the specified value.
 *
 * @param len The length of the BLE frame packet.
 */
void ble_frame_packet_len_set(uint16_t len)
{
    s_ble_frame_packet_len = len;
    PR_DEBUG("ble sub packet lenth set:%d", s_ble_frame_packet_len);
}

/**
 * @brief Retrieves the subpacket from the given BLE frame transmitter.
 *
 * This function returns a pointer to the subpacket data stored in the specified
 * BLE frame transmitter.
 *
 * @param trsmitr The BLE frame transmitter from which to retrieve the
 * subpacket.
 * @return A pointer to the subpacket data.
 */
unsigned char *ble_frame_subpacket_get(ble_frame_trsmitr_t *trsmitr)
{
    return trsmitr->subpkg;
}

static ble_frame_seq_t ble_frame_seq_get(void)
{
    ble_frame_seq_t seq = s_ble_frame_seq;
    s_ble_frame_seq     = (s_ble_frame_seq + 1) % BLE_FRAME_SEQ_LMT;
    return seq;
}

/**
 * @brief Encodes and sends a package over BLE.
 *
 * This function encodes and sends a package over BLE. It takes the version,
 * buffer, and length of the package as input parameters. The function also
 * updates the package descriptor, subpackage number, and package transmission
 * count.
 *
 * @param trsmitr Pointer to the ble_frame_trsmitr_t structure.
 * @param version The version of the package.
 * @param buf Pointer to the buffer containing the package data.
 * @param len The length of the package data.
 * @return Returns OPRT_INVALID_PARM if trsmitr is NULL, OPRT_COM_ERROR if the
 * subpackage number or length exceeds the limit,
 *         OPRT_SVC_BT_API_TRSMITR_CONTINUE if there are more subpackages to
 * send, or OPRT_OK if the package transmission is complete.
 */
int ble_frame_trsmitr_send_pkg_encode(ble_frame_trsmitr_t *trsmitr, unsigned char version, unsigned char *buf,
                                      unsigned int len)
{
    if (trsmitr == NULL || trsmitr->subpkg == NULL || (buf == NULL && len != 0)) {
        return OPRT_INVALID_PARM;
    }

    if (trsmitr->subpkg_capacity < BLE_FRAME_HEADER_MAX_LEN + 1U || trsmitr->subpkg_capacity > TUYA_BLE_AIR_FRAME_MAX) {
        return OPRT_INVALID_PARM;
    }
    if (len == 0 || len > TUYA_BLE_AIR_FRAME_MAX) {
        return OPRT_COM_ERROR;
    }

    bool is_first = (BLE_FRAME_PKG_INIT == trsmitr->pkg_desc);
    if (!is_first &&
        (trsmitr->total != len || trsmitr->pkg_trsmitr_cnt > len || trsmitr->pkg_desc == BLE_FRAME_PKG_END)) {
        return OPRT_INVALID_PARM;
    }

    uint32_t subpkg_num    = is_first ? 0 : trsmitr->subpkg_num;
    uint16_t subpkg_offset = 0;
    int      rt = ble_frame_varint_encode(subpkg_num, trsmitr->subpkg, trsmitr->subpkg_capacity, &subpkg_offset);
    if (rt != OPRT_OK) {
        return rt;
    }

    if (is_first) {
        rt = ble_frame_varint_encode(len, trsmitr->subpkg, trsmitr->subpkg_capacity, &subpkg_offset);
        if (rt != OPRT_OK || subpkg_offset >= trsmitr->subpkg_capacity) {
            return OPRT_INVALID_PARM;
        }
    }

    uint32_t sent      = is_first ? 0 : trsmitr->pkg_trsmitr_cnt;
    uint32_t remaining = len - sent;
    if (remaining != 0 && subpkg_offset >= trsmitr->subpkg_capacity - (is_first ? 1 : 0)) {
        return OPRT_INVALID_PARM;
    }

    ble_frame_seq_t seq = is_first ? ble_frame_seq_get() : trsmitr->seq;
    if (is_first) {
        trsmitr->subpkg[subpkg_offset++] = (version << 4) | (seq & 0x0f);
    }

    uint32_t available = trsmitr->subpkg_capacity - subpkg_offset;
    uint16_t send_data = remaining < available ? remaining : available;
    PR_TRACE("pkg max len:%d, subpkg_offset:%d, send_data:%d", trsmitr->subpkg_capacity, subpkg_offset, send_data);
    if (send_data != 0) {
        memcpy(&trsmitr->subpkg[subpkg_offset], buf + sent, send_data);
    }

    if (is_first) {
        trsmitr->total      = len;
        trsmitr->version    = version;
        trsmitr->seq        = seq;
        trsmitr->subpkg_num = 0;
    }
    trsmitr->subpkg_len      = subpkg_offset + send_data;
    trsmitr->pkg_trsmitr_cnt = sent + send_data;
    trsmitr->pkg_desc        = is_first ? BLE_FRAME_PKG_FIRST : BLE_FRAME_PKG_MIDDLE;

    if (trsmitr->pkg_trsmitr_cnt < trsmitr->total) {
        trsmitr->subpkg_num++;
        return OPRT_SVC_BT_API_TRSMITR_CONTINUE;
    }

    trsmitr->pkg_desc = BLE_FRAME_PKG_END;
    return OPRT_OK;
}

/**
 * @brief Decodes the received package and updates the ble_frame_trsmitr_t
 * structure.
 *
 * This function decodes the received package data and updates the fields of the
 * ble_frame_trsmitr_t structure. It checks for invalid parameters and validates
 * the subpackage number and package description. It also decodes the frame
 * length, frame type, and frame sequence. Finally, it copies the decoded data
 * to the transmitter subpackage buffer and updates the package transmit count.
 *
 * @param trsmitr Pointer to the ble_frame_trsmitr_t structure.
 * @param raw_data Pointer to the raw data of the received package.
 * @param raw_data_len Length of the raw data.
 * @return Returns the operation result status.
 *     - OPRT_INVALID_PARM: If the raw_data or trsmitr pointer is NULL, or if
 * the raw_data_len is greater than the maximum packet length.
 *     - OPRT_COM_ERROR: If the subpackage number exceeds the maximum value.
 *     - OPRT_SVC_BT_API_TRSMITR_ERROR: If the received subpackage number is
 * less than the current subpackage number.
 *     - OPRT_SVC_BT_API_TRSMITR_CONTINUE: If the received subpackage number is
 * the same as the current subpackage number.
 *     - OPRT_OK: If the decoding and updating process is successful.
 */
int ble_frame_trsmitr_recv_pkg_decode(ble_frame_trsmitr_t *trsmitr, unsigned char *raw_data, uint16_t raw_data_len)
{
    if (raw_data == NULL || trsmitr == NULL || trsmitr->subpkg == NULL || raw_data_len == 0 ||
        raw_data_len > trsmitr->subpkg_capacity || raw_data_len > TUYA_BLE_AIR_FRAME_MAX) {
        return OPRT_INVALID_PARM;
    }

    uint16_t               subpkg_offset = 0;
    ble_frame_subpkg_num_t subpkg_num    = 0;
    if (ble_frame_varint_decode(raw_data, raw_data_len, &subpkg_offset, &subpkg_num) != OPRT_OK) {
        return OPRT_SVC_BT_API_TRSMITR_ERROR;
    }

    bool              is_first = (subpkg_num == 0);
    ble_frame_total_t total    = trsmitr->total;
    uint32_t          received = trsmitr->pkg_trsmitr_cnt;
    uint8_t           version  = trsmitr->version;
    ble_frame_seq_t   seq      = trsmitr->seq;
    if (is_first) {
        if (ble_frame_varint_decode(raw_data, raw_data_len, &subpkg_offset, &total) != OPRT_OK || total == 0 ||
            total > TUYA_BLE_AIR_FRAME_MAX || subpkg_offset >= raw_data_len) {
            return OPRT_SVC_BT_API_TRSMITR_ERROR;
        }

        version  = (raw_data[subpkg_offset] & BLE_FRAME_VERSION_OFFSET) >> 4;
        seq      = raw_data[subpkg_offset++] & BLE_FRAME_SEQ_OFFSET;
        received = 0;
    }

    uint16_t recv_data       = raw_data_len - subpkg_offset;
    bool     exact_duplicate = trsmitr->pkg_desc != BLE_FRAME_PKG_INIT && subpkg_num == trsmitr->subpkg_num &&
                           total == trsmitr->total && version == trsmitr->version && seq == trsmitr->seq &&
                           recv_data == trsmitr->subpkg_len &&
                           (recv_data == 0 || memcmp(trsmitr->subpkg, &raw_data[subpkg_offset], recv_data) == 0);
    if (exact_duplicate) {
        return OPRT_SVC_BT_API_TRSMITR_CONTINUE;
    }

    if (received > total || recv_data > total - received || recv_data > trsmitr->subpkg_capacity) {
        return OPRT_SVC_BT_API_TRSMITR_ERROR;
    }
    if (recv_data == 0 && received < total) {
        return OPRT_SVC_BT_API_TRSMITR_ERROR;
    }

    if (is_first) {
        if (trsmitr->pkg_desc != BLE_FRAME_PKG_INIT && trsmitr->pkg_desc != BLE_FRAME_PKG_END) {
            return OPRT_SVC_BT_API_TRSMITR_ERROR;
        }
    } else if ((trsmitr->pkg_desc != BLE_FRAME_PKG_FIRST && trsmitr->pkg_desc != BLE_FRAME_PKG_MIDDLE) ||
               trsmitr->subpkg_num >= 0x0fffffff || subpkg_num != trsmitr->subpkg_num + 1) {
        return OPRT_SVC_BT_API_TRSMITR_ERROR;
    }

    if (recv_data != 0) {
        memcpy(trsmitr->subpkg, &raw_data[subpkg_offset], recv_data);
    }
    trsmitr->total           = total;
    trsmitr->version         = version;
    trsmitr->seq             = seq;
    trsmitr->subpkg_num      = subpkg_num;
    trsmitr->subpkg_len      = recv_data;
    trsmitr->pkg_trsmitr_cnt = received + recv_data;
    trsmitr->pkg_desc        = is_first ? BLE_FRAME_PKG_FIRST : BLE_FRAME_PKG_MIDDLE;

    if (trsmitr->pkg_trsmitr_cnt < trsmitr->total) {
        return OPRT_SVC_BT_API_TRSMITR_CONTINUE;
    }
    trsmitr->pkg_desc = BLE_FRAME_PKG_END;

    return OPRT_OK;
}

/**
 * @file ble_mgr_bounds.c
 * @brief Bounds and allocation helpers for BLE manager negotiation.
 *
 * @copyright Copyright (c) 2021-2026 Tuya Inc. All Rights Reserved.
 */

#include "ble_mgr_bounds.h"

#include "ble_protocol.h"
#include "tal_api.h"

int ble_mgr_packet_length_parse(const uint8_t *data, uint16_t len, uint16_t *packet_len)
{
    if (data == NULL || len < 2 || packet_len == NULL) {
        return OPRT_INVALID_PARM;
    }

    uint16_t candidate = ((uint16_t)data[0] << 8) | data[1];
    if (candidate < BLE_MGR_PACKET_LEN_MIN || candidate > TUYA_BLE_AIR_FRAME_MAX) {
        return OPRT_INVALID_PARM;
    }

    *packet_len = candidate;
    return OPRT_OK;
}

int ble_mgr_subpkg_resize(uint8_t **buffer, uint32_t *capacity, uint16_t new_capacity)
{
    if (buffer == NULL || *buffer == NULL || capacity == NULL || new_capacity < BLE_MGR_PACKET_LEN_MIN ||
        new_capacity > TUYA_BLE_AIR_FRAME_MAX) {
        return OPRT_INVALID_PARM;
    }
    if (*capacity == new_capacity) {
        return OPRT_OK;
    }

    uint8_t *replacement = tal_malloc(new_capacity);
    if (replacement == NULL) {
        return OPRT_MALLOC_FAILED;
    }
    memset(replacement, 0, new_capacity);
    tal_free(*buffer);
    *buffer   = replacement;
    *capacity = new_capacity;
    return OPRT_OK;
}

bool ble_mgr_dev_info_buffer_valid(const void *manager, const uint8_t *buffer, uint32_t capacity)
{
    return manager != NULL && buffer != NULL && capacity >= BLE_MGR_DEV_INFO_LEN;
}

uint32_t ble_mgr_u32_from_big_endian(const uint8_t data[4])
{
    return ((uint32_t)data[0] << 24) | ((uint32_t)data[1] << 16) | ((uint32_t)data[2] << 8) | (uint32_t)data[3];
}

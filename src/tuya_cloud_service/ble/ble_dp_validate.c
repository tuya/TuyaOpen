/**
 * @file ble_dp_validate.c
 * @brief BLE datapoint payload validation helpers.
 *
 * @copyright Copyright (c) 2021-2026 Tuya Inc. All Rights Reserved.
 */

#include "ble_dp_validate.h"

#include <stddef.h>
#include <string.h>

#define BLE_DP_TYPE_RAW    0
#define BLE_DP_TYPE_BOOL   1
#define BLE_DP_TYPE_VALUE  2
#define BLE_DP_TYPE_STRING 3
#define BLE_DP_TYPE_ENUM   4
#define BLE_DP_TYPE_BITMAP 5

bool ble_dp_v4_payload_valid(const uint8_t *data, uint16_t len)
{
    return data != NULL && len >= 5;
}

bool ble_dp_klv_length_valid(uint8_t type, uint16_t len)
{
    switch (type) {
    case BLE_DP_TYPE_RAW:
    case BLE_DP_TYPE_STRING:
        return len <= 255;
    case BLE_DP_TYPE_BOOL:
        return len == 1;
    case BLE_DP_TYPE_VALUE:
        return len == 4;
    case BLE_DP_TYPE_ENUM:
    case BLE_DP_TYPE_BITMAP:
        return len == 1 || len == 2 || len == 4;
    default:
        return false;
    }
}

bool ble_dp_enum_value_valid(uint32_t value, int count, char *const *values)
{
    return count > 0 && values != NULL && value < (uint32_t)count && values[value] != NULL;
}

bool ble_dp_scalar_to_big_endian(const void *data, uint16_t len, uint8_t *output)
{
    if (data == NULL || output == NULL || (len != 1 && len != 2 && len != 4)) {
        return false;
    }

    uint32_t value = 0;
    if (len == 1) {
        value = *(const uint8_t *)data;
    } else if (len == 2) {
        uint16_t value16;
        memcpy(&value16, data, sizeof(value16));
        value = value16;
    } else {
        memcpy(&value, data, sizeof(value));
    }

    for (uint16_t i = 0; i < len; i++) {
        output[i] = (uint8_t)(value >> ((len - i - 1) * 8));
    }
    return true;
}

bool ble_dp_client_ready(bool is_activated, const void *schema)
{
    return is_activated && schema != NULL;
}

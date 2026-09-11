/**
 * @file ble_dp_validate.h
 * @brief BLE datapoint payload validation helpers.
 *
 * @copyright Copyright (c) 2021-2026 Tuya Inc. All Rights Reserved.
 */

#ifndef __BLE_DP_VALIDATE_H__
#define __BLE_DP_VALIDATE_H__

#include <stdbool.h>
#include <stdint.h>

bool ble_dp_v4_payload_valid(const uint8_t *data, uint16_t len);
bool ble_dp_klv_length_valid(uint8_t type, uint16_t len);
bool ble_dp_enum_value_valid(uint32_t value, int count, char *const *values);
bool ble_dp_scalar_to_big_endian(const void *data, uint16_t len, uint8_t *output);
bool ble_dp_client_ready(bool is_activated, const void *schema);

#endif

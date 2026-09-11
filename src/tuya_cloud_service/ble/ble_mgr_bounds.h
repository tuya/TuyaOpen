/**
 * @file ble_mgr_bounds.h
 * @brief Bounds and allocation helpers for BLE manager negotiation.
 *
 * @copyright Copyright (c) 2021-2026 Tuya Inc. All Rights Reserved.
 */

#ifndef __BLE_MGR_BOUNDS_H__
#define __BLE_MGR_BOUNDS_H__

#include <stdbool.h>
#include <stdint.h>

#define BLE_MGR_PACKET_LEN_MIN 10U
#define BLE_MGR_DEV_INFO_LEN   121U

int      ble_mgr_packet_length_parse(const uint8_t *data, uint16_t len, uint16_t *packet_len);
int      ble_mgr_subpkg_resize(uint8_t **buffer, uint32_t *capacity, uint16_t new_capacity);
bool     ble_mgr_dev_info_buffer_valid(const void *manager, const uint8_t *buffer, uint32_t capacity);
uint32_t ble_mgr_u32_from_big_endian(const uint8_t data[4]);

#endif

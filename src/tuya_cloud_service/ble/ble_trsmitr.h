/**
 * @file ble_trsmitr.h
 * @brief Header file for the BLE transmission module.
 *
 * This file defines the interfaces and data structures for the BLE transmission
 * module. It includes macros for frame version and sequence offsets, types for
 * frame total length and subpackage number, and external variable declarations
 * for module-wide use. The module facilitates the encoding and transmission of
 * BLE frames between devices.
 *
 * @copyright Copyright (c) 2021-2026 Tuya Inc. All Rights Reserved.
 *
 */

#ifndef __BLE_TRSMITR_H__
#define __BLE_TRSMITR_H__

#include "tuya_cloud_types.h"
#include "tuya_error_code.h"
#include "ble_protocol.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifdef __BLE_TRSMITR_GLOBALS
#define __BLE_TRSMITR_EXT
#else
#define __BLE_TRSMITR_EXT extern
#endif

/***********************************************************
*************************micro define***********************
***********************************************************/
#define BLE_FRAME_VERSION_OFFSET (0x0f << 4)
#define BLE_FRAME_SEQ_OFFSET     (0x0f << 0)
#define BLE_FRAME_SEQ_LMT        16
#define BLE_FRAME_HEADER_MAX_LEN 9U

// frame total len
typedef uint32_t ble_frame_total_t;
// frame subpackage num
typedef uint32_t ble_frame_subpkg_num_t;
// frame sequence
typedef uint8_t ble_frame_seq_t;
// frame subpackage len
typedef uint32_t ble_frame_subpkg_len_t;

// frame package description
typedef uint8_t ble_frame_pkg_desc_t;
#define BLE_FRAME_PKG_INIT   0 // frame package init
#define BLE_FRAME_PKG_FIRST  1 // frame package first
#define BLE_FRAME_PKG_MIDDLE 2 // frame package middle
#define BLE_FRAME_PKG_END    3 // frame package end

// frame transmitter process
typedef struct {
    ble_frame_total_t    total;             // 4 bytes, total data length, excluding header
    uint8_t              version;           // 1 byte, protocol major version number
    ble_frame_seq_t      seq;               // 1 byte,
    ble_frame_pkg_desc_t pkg_desc;          // 1 byte, current subpackage frame type
                                            // (init/first/middle/end)
    ble_frame_subpkg_num_t subpkg_num;      // 4 bytes, current subpackage number
    uint32_t               pkg_trsmitr_cnt; // package process count, number of bytes sent
    ble_frame_subpkg_len_t subpkg_len;      // 1 byte, data length in the current subpackage
    uint32_t               subpkg_capacity; // allocated size of subpkg
    uint8_t               *subpkg;
} ble_frame_trsmitr_t;

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
__BLE_TRSMITR_EXT
ble_frame_trsmitr_t *ble_frame_trsmitr_create(void);

/**
 * @brief Deletes a BLE frame transmitter.
 *
 * This function frees the memory allocated for a BLE frame transmitter and its
 * subpackage.
 *
 * @param trsmitr Pointer to the BLE frame transmitter to be deleted.
 */
__BLE_TRSMITR_EXT
void ble_frame_trsmitr_delete(ble_frame_trsmitr_t *frm_trsmitr);
__BLE_TRSMITR_EXT
void ble_frame_trsmitr_reset(ble_frame_trsmitr_t *frm_trsmitr);

/**
 * @brief Get the length of the subpacket in a BLE frame transmitter.
 *
 * This function returns the length of the subpacket in a BLE frame transmitter.
 *
 * @param trsmitr The BLE frame transmitter.
 * @return The length of the subpacket.
 */
__BLE_TRSMITR_EXT
ble_frame_subpkg_len_t ble_frame_subpacket_len_get(ble_frame_trsmitr_t *trsmitr);

/**
 * @brief Get the length of the BLE frame packet.
 *
 * This function returns the length of the BLE frame packet.
 *
 * @return The length of the BLE frame packet.
 */
uint16_t ble_frame_packet_len_get(void);

/**
 * @brief Sets the length of the BLE frame packet.
 *
 * This function sets the length of the BLE frame packet to the specified value.
 *
 * @param len The length of the BLE frame packet.
 */
void ble_frame_packet_len_set(uint16_t len);

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
__BLE_TRSMITR_EXT
uint8_t *ble_frame_subpacket_get(ble_frame_trsmitr_t *frm_trsmitr);

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
__BLE_TRSMITR_EXT
int ble_frame_trsmitr_send_pkg_encode(ble_frame_trsmitr_t *trsmitr, unsigned char version, unsigned char *buf,
                                      unsigned int len);

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
__BLE_TRSMITR_EXT
int ble_frame_trsmitr_recv_pkg_decode(ble_frame_trsmitr_t *trsmitr, unsigned char *raw_data, uint16_t raw_data_len);

#endif

#ifdef __cplusplus
}
#endif

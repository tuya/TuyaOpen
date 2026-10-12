/**
 * @file ble_protocol.h
 * @brief Bluetooth protocol definitions and configurations for Tuya BLE
 * devices.
 *
 * This file contains the definitions and macros for the Bluetooth communication
 * protocol used by Tuya BLE devices. It includes command codes, protocol
 * version information, device communication abilities, and encryption settings.
 *
 * @copyright Copyright (c) 2021-2024 Tuya Inc. All Rights Reserved.
 *
 */

#ifndef __BLE_PROTOCOL_H__
#define __BLE_PROTOCOL_H__

#define FRM_QRY_DEV_INFO_REQ              0x0000 // APP->BLE
#define FRM_PAIR_REQ                      0x0001 // APP->BLE
#define FRM_STATE_QUERY                   0x0003 // APP->BLE
#define FRM_UNBONDING_REQ                 0x0005 // APP->BLE
#define FRM_DEVICE_RESET                  0x0006 // APP->BLE
#define FRM_STAT_REPORT                   0x8001 // BLE->APP
#define FRM_DOWNLINK_TRANSPARENT_REQ      0x801B // APP->APP
#define FRM_UPLINK_TRANSPARENT_REQ        0x801C // BLE->APP
#define FRM_DOWNLINK_TRANSPARENT_SPEC_REQ 0x801E // APP->APP,
#define FRM_UPLINK_TRANSPARENT_SPEC_REQ   0x801F // BLE->APP,
#define FRM_RPT_NET_STAT_REQ              0x001E // BLE->APP

// Protocol version 4.0 and above
// DP issuance
#define FRM_DP_CMD_SEND_V4 0x0027 // APP<->BLE
// DP reporting
#define FRM_DP_STAT_REPORT_V4 0x8006 // BLE->APP
// DP reporting with timestamp
#define FRM_DP_STAT_REPORT_WITH_TIME_V4 0x8007 // BLE->APP

/* Sub-command definitions for large data channel transmission
(0x801B, 0x801C, 0x801E, 0x801F command sub-commands are uniformly planned,
 conflicts are not allowed) */
// Bluetooth timing
#define FRM_DATA_TRANS_SUBCMD_TIMER_TASK 0x0000 // APP<->BLE
// psk3.0 Bluetooth network configuration
#define FRM_DATA_TRANS_SUBCMD_BT_NETCFG 0x0001 // APP<->BLE
// psk3.0 Cloud connection activation
#define FRM_DATA_TRANS_SUBCMD_SET_WIFI 0x0002 // APP<->BLE
// Query WIFI list
#define FRM_DATA_TRANS_SUBCMD_WIFI_LST 0x0003 // APP<->BLE
// Query network configuration status
#define FRM_DATA_TRANS_SUBCMD_NETCFG_STAT 0x0004 // APP<->BLE
// Query device log
#define FRM_DATA_TRANS_SUBCMD_DEV_LOG 0x0005 // APP<->BLE
// Query AP hotspot name
#define FRM_DATA_TRANS_SUBCMD_GET_AP_NAME 0x0006 // APP<->BLE

/** @defgroup TUY_BLE_DEVICE_COMMUNICATION_ABILITY tuya ble device communication
 * ability
 * @{
 */
#define TUYA_BLE_DEVICE_COMMUNICATION_ABILITY_BLE               0x0000
#define TUYA_BLE_DEVICE_COMMUNICATION_ABILITY_REGISTER_FROM_BLE 0x0001
#define TUYA_BLE_DEVICE_COMMUNICATION_ABILITY_MESH              0x0002
#define TUYA_BLE_DEVICE_COMMUNICATION_ABILITY_WIFI_24G          0x0004
#define TUYA_BLE_DEVICE_COMMUNICATION_ABILITY_WIFI_5G           0x0008
#define TUYA_BLE_DEVICE_COMMUNICATION_ABILITY_ZIGBEE            0x0010
#define TUYA_BLE_DEVICE_COMMUNICATION_ABILITY_NB                0x0020
#define TUYA_BLE_DEVICE_COMMUNICATION_ABILITY_CELLUAR           0x0040
/** End of TUY_BLE_DEVICE_COMMUNICATION_ABILITY
 * @}
 */

#define TUYA_BLE_DEVICE_COMMUNICATION_ABILITY                                                                          \
    (TUYA_BLE_DEVICE_COMMUNICATION_ABILITY_BLE | TUYA_BLE_DEVICE_COMMUNICATION_ABILITY_WIFI_24G |                      \
     TUYA_BLE_DEVICE_COMMUNICATION_ABILITY_WIFI_5G)

/*Flag: 1Byte
bit7: 1: Bound, 0: Not bound
bit6: 1: V2 encryption protocol enabled, 0: Not enabled
bit5: 1: Supports V2 encryption protocol, 0: Does not support
bit4: 1: Bluetooth preferred, 0: WiFi preferred
bit3: Dual-mode supports plug&play, 1: Supported, 0: Not supported
bit1: Shared class flag, 1: Tuya shared class device, 0: Non-shared class device
*/
#define ADV_FLAG_BOND            (1 << 7)
#define ADV_FLAG_ENCRPT_V2_START (1 << 6)
#define ADV_FLAG_ENCRPT_V2       (1 << 5)
#define ADV_FLAG_PLUG_PLAY       (1 << 3)
#define ADV_FLAG_BLE_FIRST       (1 << 4)
#define ADV_FLAG_SHARE           (1 << 1)
#define ADV_FLAG_UUID_COMP       (1 << 0) // DEVICE UUID is compress

/** @defgroup TUYA_BLE_SECURE_CONNECTION_TYPE tuya ble secure connection type
 * @{
 */
#define TUYA_BLE_SECURE_CONNECTION_WITH_AUTH_KEY              0x00
#define TUYA_BLE_SECURE_CONNECTION_WITH_ECC                   0x01
#define TUYA_BLE_SECURE_CONNECTION_WTIH_PASSTHROUGH           0x02
#define TUYA_BLE_SECURE_CONNECTION_WITH_AUTH_KEY_DEVICE_ID_20 0x03

/*
 * MACRO for advanced encryption,if 1 will use user rand check.
 */
#define TUYA_BLE_ADVANCED_ENCRYPTION_DEVICE 0

/*
Data origin
255: The maximum length of dp is 255
3: Information related to each sub-packet's dp id, including dp_id, dp_type,
dp_len 14: Protocol overhead for the frame format, including sn, ack_sn,
function code, data length, CRC

*/
#define TUYA_BLE_AIR_FRAME_MAX 1024 // 305//256//255 bytes for dp distribution, 255+3+14+1+16+16=305

#define TUYA_BLE_TRANSMISSION_MAX_DATA_LEN (TUYA_BLE_AIR_FRAME_MAX - 29)
// Large data sub-packet size for transparent transmission command (length set
// to the total length of the underlying sub-packet assembly - header length)
#define TUYA_BLE_TRANS_DATA_SUBPACK_LEN (TUYA_BLE_AIR_FRAME_MAX - 64)
// Largest reassembled transparent-channel message the device will accept
#define TUYA_BLE_TRANS_DATA_TOTAL_LEN_MAX (TUYA_BLE_AIR_FRAME_MAX * 16)
// largest header the encoder writes: sub-packet no(4) + frame len(4) + ver/seq(1)
#define TUYA_BLE_SUBPACK_HEADER_MAX 9
// smallest sub-packet size the APP may request, header plus one payload byte
#define TUYA_BLE_SUBPACK_LEN_MIN (TUYA_BLE_SUBPACK_HEADER_MAX + 1)
// BLE communication protocol version is set here
// The APP parses the message based on the major and minor version numbers, thus
// the Bluetooth version is changed to 3.7/4.4
#define TUYA_BLE_PROTOCOL_VERSION_HIGN 0x04
#define TUYA_BLE_PROTOCOL_VERSION_LOW  0x04

#define AUTH_KEY_LEN                       32
#define LOGIN_KEY_LEN_16                   16
#define SECRET_KEY_LEN                     16
#define PAIR_RANDOM_LEN                    6
#define TUYA_BLE_PRODUCT_ID_MAX_LEN        16
#define TUYA_BLE_WIFI_DEVICE_REGISTER_MODE 1

#endif

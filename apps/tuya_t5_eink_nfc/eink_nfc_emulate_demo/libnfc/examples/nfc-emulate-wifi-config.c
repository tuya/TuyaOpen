/*-
 * Free/Libre Near Field Communication (NFC) library
 *
 * Libnfc historical contributors:
 * Copyright (C) 2009      Roel Verdult
 * Copyright (C) 2009-2013 Romuald Conty
 * Copyright (C) 2010-2012 Romain Tartière
 * Copyright (C) 2010-2013 Philippe Teuwen
 * Copyright (C) 2012-2013 Ludovic Rousseau
 * See AUTHORS file for a more comprehensive list of contributors.
 * Additional contributors of this file:
 * Copyright (C) 2025      Tuya Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *  1) Redistributions of source code must retain the above copyright notice,
 *  this list of conditions and the following disclaimer.
 *  2 )Redistributions in binary form must reproduce the above copyright
 *  notice, this list of conditions and the following disclaimer in the
 *  documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * Note that this license only applies on the examples, NFC library itself is under LGPL
 *
 */

/**
 * @file nfc-emulate-wifi-config.c
 * @brief Emulates a NFC-Forum Tag Type 2 with WiFi configuration NDEF message
 *
 * This example emulates an NFC-Forum Tag Type 2 that contains a WiFi Simple
 * Configuration (WSC) NDEF message. When a phone touches the PN532, it will
 * automatically prompt to connect to the configured WiFi network.
 *
 * WiFi NDEF Record Format (WFA WSC):
 * - MIME Type: "application/vnd.wfa.wsc"
 * - Payload: WiFi Simple Configuration credential attributes
 *
 * Hardware Requirements:
 * - PN532 NFC chip connected via UART
 * - Tuya T5AI platform (BK7258)
 *
 * @note Developed for Tuya T5AI + PN532 UART2 configuration
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif // HAVE_CONFIG_H

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>

#include <nfc/nfc.h>
#include <nfc/nfc-emulation.h>

#include "nfc-utils.h"
#include "pn53x.h"

#include "tkl_memory.h"
#include "tal_log.h"
#include "tkl_system.h"

// WiFi WSC Credential Attribute IDs (WFA spec)
#define WSC_ID_CREDENTIAL    0x100E
#define WSC_ID_NETWORK_INDEX 0x1026
#define WSC_ID_SSID          0x1045
#define WSC_ID_AUTH_TYPE     0x1003
#define WSC_ID_ENCR_TYPE     0x100F
#define WSC_ID_NETWORK_KEY   0x1027
#define WSC_ID_MAC_ADDRESS   0x1020
#define WSC_ID_VENDOR_EXT    0x1049 // WFA Vendor Extension
#define WSC_ID_VERSION2      0x00   // Version2 sub-element ID within Vendor Extension

// WFA Vendor ID (00:37:2A)
#define WFA_VENDOR_ID_0 0x00
#define WFA_VENDOR_ID_1 0x37
#define WFA_VENDOR_ID_2 0x2A

// WiFi Authentication Types
#define WSC_AUTH_OPEN              0x0001
#define WSC_AUTH_WPA_PERSONAL      0x0002
#define WSC_AUTH_SHARED            0x0004
#define WSC_AUTH_WPA_ENTERPRISE    0x0008
#define WSC_AUTH_WPA2_PERSONAL     0x0010
#define WSC_AUTH_WPA2_ENTERPRISE   0x0020
#define WSC_AUTH_WPA_WPA2_PERSONAL 0x0022

// WiFi Encryption Types
#define WSC_ENCR_NONE     0x0001
#define WSC_ENCR_WEP      0x0002
#define WSC_ENCR_TKIP     0x0004
#define WSC_ENCR_AES      0x0008
#define WSC_ENCR_AES_TKIP 0x000C

/**
 * @brief WiFi configuration structure
 */
typedef struct {
    const char *ssid;      // WiFi SSID (max 32 bytes)
    const char *password;  // WiFi password (max 64 bytes)
    uint16_t    auth_type; // Authentication type (see WSC_AUTH_*)
    uint16_t    encr_type; // Encryption type (see WSC_ENCR_*)
} wifi_config_t;

/**
 * @brief Write a 16-bit value in network byte order (big-endian)
 */
static void write_be16(uint8_t *buf, uint16_t value)
{
    buf[0] = (value >> 8) & 0xFF;
    buf[1] = value & 0xFF;
}

/**
 * @brief Write a WSC attribute (TLV format)
 *
 * @param buf Output buffer
 * @param attr_id Attribute ID (16-bit)
 * @param attr_len Attribute length (16-bit)
 * @param attr_data Attribute data
 * @return Number of bytes written
 */
static int write_wsc_attribute(uint8_t *buf, uint16_t attr_id, uint16_t attr_len, const uint8_t *attr_data)
{
    int offset = 0;

    // Attribute ID (2 bytes, big-endian)
    write_be16(buf + offset, attr_id);
    offset += 2;

    // Attribute Length (2 bytes, big-endian)
    write_be16(buf + offset, attr_len);
    offset += 2;

    // Attribute Data
    if (attr_data && attr_len > 0) {
        memcpy(buf + offset, attr_data, attr_len);
        offset += attr_len;
    }

    return offset;
}

/**
 * @brief Generate WiFi Simple Configuration (WSC) credential payload
 *
 * @param config WiFi configuration
 * @param buffer Output buffer for WSC payload
 * @param buffer_size Size of output buffer
 * @return Number of bytes written, or negative error code
 */
static int generate_wsc_credential(const wifi_config_t *config, uint8_t *buffer, size_t buffer_size)
{
    if (config == NULL || buffer == NULL) {
        return -1;
    }

    size_t ssid_len = strlen(config->ssid);
    size_t pass_len = strlen(config->password);

    if (ssid_len > 32 || pass_len > 64) {
        PR_ERR("SSID or password too long: ssid=%zu, pass=%zu", ssid_len, pass_len);
        return -2;
    }

    int     offset = 0;
    uint8_t auth_type_buf[2];
    uint8_t encr_type_buf[2];
    uint8_t network_idx_buf = 1;
    // Use 00:00:00:00:00:00 for wildcard MAC (some Android devices reject FF:FF:FF:FF:FF:FF)
    uint8_t mac_addr[6] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

    PR_INFO("Generating WSC credential: SSID='%s', Auth=0x%04X, Encr=0x%04X", config->ssid, config->auth_type,
            config->encr_type);

    // Calculate credential length first (内部属性)
    int cred_len = 0;
    cred_len += 4 + 1;        // Network Index
    cred_len += 4 + ssid_len; // SSID
    cred_len += 4 + 2;        // Auth Type
    cred_len += 4 + 2;        // Encr Type
    cred_len += 4 + pass_len; // Network Key
    cred_len += 4 + 6;        // MAC Address

    // WFA Vendor Extension length: WFA Vendor ID (3) + Version2 subelement (1+1+1)
    // Format: [Vendor ID: 3 bytes] [Subelement ID: 1 byte] [Length: 1 byte] [Value: 1 byte]
    int vendor_ext_len = 3 + 1 + 1 + 1; // 6 bytes total

    // Total WSC payload length (Credential + WFA Vendor Extension)
    int total_wsc_len = 4 + cred_len + 4 + vendor_ext_len;

    // Check buffer size (need space for Credential attribute header too)
    if (buffer_size < (size_t)(total_wsc_len)) {
        PR_ERR("Buffer too small: need %d bytes, have %zu", total_wsc_len, buffer_size);
        return -3;
    }

    // Start Credential attribute
    write_be16(buffer + offset, WSC_ID_CREDENTIAL);
    offset += 2;
    write_be16(buffer + offset, cred_len);
    offset += 2;

    // Network Index (always 1 for single credential)
    offset += write_wsc_attribute(buffer + offset, WSC_ID_NETWORK_INDEX, 1, &network_idx_buf);

    // SSID
    offset += write_wsc_attribute(buffer + offset, WSC_ID_SSID, ssid_len, (const uint8_t *)config->ssid);

    // Authentication Type
    write_be16(auth_type_buf, config->auth_type);
    offset += write_wsc_attribute(buffer + offset, WSC_ID_AUTH_TYPE, 2, auth_type_buf);

    // Encryption Type
    write_be16(encr_type_buf, config->encr_type);
    offset += write_wsc_attribute(buffer + offset, WSC_ID_ENCR_TYPE, 2, encr_type_buf);

    // Network Key (password)
    offset += write_wsc_attribute(buffer + offset, WSC_ID_NETWORK_KEY, pass_len, (const uint8_t *)config->password);

    // MAC Address (wildcard)
    offset += write_wsc_attribute(buffer + offset, WSC_ID_MAC_ADDRESS, 6, mac_addr);

    // END of Credential attribute content

    // WFA Vendor Extension attribute (REQUIRED for WPS 2.0 support)
    // Contains Version2 subelement inside WFA Vendor Extension
    // Format:
    //   Attribute ID: 0x1049 (2 bytes)
    //   Length: 6 (2 bytes)
    //   WFA Vendor ID: 00:37:2A (3 bytes)
    //   Subelement: Version2 (ID=0x00, Len=1, Value=0x20)
    write_be16(buffer + offset, WSC_ID_VENDOR_EXT);
    offset += 2;
    write_be16(buffer + offset, vendor_ext_len);
    offset += 2;
    // WFA Vendor ID
    buffer[offset++] = WFA_VENDOR_ID_0;
    buffer[offset++] = WFA_VENDOR_ID_1;
    buffer[offset++] = WFA_VENDOR_ID_2;
    // Version2 subelement (ID=0x00, Length=1, Value=0x20 for WPS 2.0)
    buffer[offset++] = WSC_ID_VERSION2; // 0x00
    buffer[offset++] = 0x01;            // Length: 1
    buffer[offset++] = 0x20;            // Value: WPS 2.0

    PR_INFO("Generated WSC payload: %d bytes (credential=%d, vendor_ext=%d)", offset, cred_len, vendor_ext_len);

    return offset;
}

/**
 * @brief Generate NFC Forum Tag Type 2 memory area with WiFi NDEF message
 *
 * @param config WiFi configuration
 * @param buffer Output buffer (must be at least 64 bytes for Tag Type 2)
 * @param buffer_size Size of output buffer
 * @return Number of bytes written, or negative error code
 *
 * @note Tag Type 2 memory layout:
 *       Block 0-1: UID and internal data (set to 0 for emulation)
 *       Block 2: Static lock bytes (0xFF, 0xFF)
 *       Block 3: Capability Container (CC)
 *       Block 4+: NDEF message
 */
int generate_tag2_ndef_wifi(const wifi_config_t *config, uint8_t *buffer, size_t buffer_size)
{
    if (config == NULL || buffer == NULL || buffer_size < 64) {
        PR_ERR("Invalid parameters: config=%p, buffer=%p, size=%zu", config, buffer, buffer_size);
        return -1;
    }

    // Generate WSC credential payload
    uint8_t wsc_payload[256];
    int     wsc_len = generate_wsc_credential(config, wsc_payload, sizeof(wsc_payload));
    if (wsc_len < 0) {
        return wsc_len;
    }

    // MIME type string: "application/vnd.wfa.wsc"
    const char *mime_type     = "application/vnd.wfa.wsc";
    size_t      mime_type_len = strlen(mime_type);

    // Calculate NDEF record size
    size_t ndef_payload_len = wsc_len;
    size_t ndef_record_len  = 3 + mime_type_len + ndef_payload_len; // Header(3) + Type + Payload
    size_t ndef_message_len = ndef_record_len;

    PR_INFO("NDEF sizes: mime_type=%zu, wsc=%d, record=%zu, message=%zu", mime_type_len, wsc_len, ndef_record_len,
            ndef_message_len);

    // Check if fits in Tag Type 2 (we have limited space)
    // Standard Tag Type 2 has 48 bytes, but we might need larger memory
    if (ndef_message_len > (buffer_size - 18)) { // Reserve space for headers
        PR_ERR("NDEF message too large: %zu bytes (max %zu)", ndef_message_len, buffer_size - 18);
        return -2;
    }

    // Clear buffer
    memset(buffer, 0, buffer_size);

    int offset = 0;

    // Blocks 0-1: UID and internal data (Tag Type 2 format)
    // Block 0: UID0, UID1, UID2, BCC0 (XOR of first 3 bytes)
    // Block 1: UID3, UID4, UID5, UID6
    // For emulation, we use a fixed UID
    buffer[0] = 0x04;               // Manufacturer: NXP
    buffer[1] = 0x01;               // UID byte 1
    buffer[2] = 0x02;               // UID byte 2
    buffer[3] = 0x04 ^ 0x01 ^ 0x02; // BCC0 = XOR of UID0-2
    buffer[4] = 0x03;               // UID byte 3
    buffer[5] = 0x04;               // UID byte 4
    buffer[6] = 0x05;               // UID byte 5
    buffer[7] = 0x06;               // UID byte 6
    offset    = 8;

    // Block 2: Static lock bytes
    buffer[offset++] = 0xFF; // Lock byte 0
    buffer[offset++] = 0xFF; // Lock byte 1
    buffer[offset++] = 0x00; // Reserved
    buffer[offset++] = 0x00; // Reserved

    // Block 3: Capability Container (CC)
    buffer[offset++] = 0xE1; // NDEF Magic Number
    buffer[offset++] = 0x10; // Version 1.0

    // Calculate memory size in 8-byte blocks (round up)
    uint8_t memory_blocks = ((ndef_message_len + 2 + 15) / 8); // +2 for TLV header, round up
    if (memory_blocks < 6)
        memory_blocks = 6; // Minimum 48 bytes
    buffer[offset++] = memory_blocks;

    buffer[offset++] = 0x00; // Read/write access

    // Block 4+: NDEF Message TLV
    buffer[offset++] = 0x03; // TLV Type: NDEF Message

    // TLV Length: use 3-byte format if > 254 bytes
    if (ndef_message_len <= 254) {
        buffer[offset++] = (uint8_t)ndef_message_len;
    } else {
        buffer[offset++] = 0xFF; // Extended length indicator
        write_be16(buffer + offset, ndef_message_len);
        offset += 2;
    }

    // NDEF Record Header (MIME type, short record)
    buffer[offset++] = 0xD2;                      // MB=1, ME=1, CF=0, SR=1, IL=0, TNF=010 (MIME)
    buffer[offset++] = (uint8_t)mime_type_len;    // Type Length
    buffer[offset++] = (uint8_t)ndef_payload_len; // Payload Length (short record, 1 byte)

    // NDEF Record Type: MIME type string
    memcpy(buffer + offset, mime_type, mime_type_len);
    offset += mime_type_len;

    // NDEF Payload: WSC credential
    memcpy(buffer + offset, wsc_payload, wsc_len);
    offset += wsc_len;

    // TLV Terminator
    buffer[offset++] = 0xFE;

    PR_INFO("Generated Tag Type 2 NDEF WiFi: total %d bytes", offset);

    // Debug output: print first 128 bytes in hex (enough to see full WSC payload)
    // PR_DEBUG("NDEF data (first 128 bytes):");
    // for (int i = 0; i < 128 && i < offset; i += 16) {
    //     char hex_str[64];
    //     int  hex_pos = 0;
    //     for (int j = 0; j < 16 && (i + j) < offset; j++) {
    //         hex_pos += sprintf(hex_str + hex_pos, "%02X ", buffer[i + j]);
    //     }
    //     PR_DEBUG("  [%02d] %s", i, hex_str);
    // }

    return offset;
}

// NFC Forum Tag Type 2 Commands
#define READ          0x30
#define WRITE         0xA2
#define SECTOR_SELECT 0xC2
#define HALT          0x50

/**
 * @brief I/O handler for NFC Forum Tag Type 2 emulation
 *
 * This function handles READ and HALT commands from the phone.
 * It's called by libnfc's emulation loop for each command received.
 */
static int nfcforum_tag2_io_wifi(struct nfc_emulator *emulator, const uint8_t *data_in, const size_t data_in_len,
                                 uint8_t *data_out, const size_t data_out_len)
{
    int res = 0;

    uint8_t *tag2_memory = (uint8_t *)(emulator->user_data);

    printf("    In: ");
    print_hex(data_in, data_in_len);

    switch (data_in[0]) {
    case READ:
        if (data_out_len >= 16) {
            // Read 16 bytes (4 blocks) starting from the requested block
            memcpy(data_out, tag2_memory + (data_in[1] * 4), 16);
            res = 16;
        } else {
            res = -ENOSPC;
        }
        break;
    case HALT:
        printf("HALT sent - phone disconnected\n");
        res = -ECONNABORTED;
        break;
    default:
        printf("Unknown command: 0x%02x\n", data_in[0]);
        res = -ENOTSUP;
    }

    if (res < 0) {
        ERR("%s (%d)", strerror(-res), -res);
    } else {
        printf("    Out: ");
        print_hex(data_out, res);
    }

    return res;
}

/**
 * @brief Main function for NFC WiFi configuration emulation
 */
int nfc_emulate_wifi_config_main(char *ssid, char *password)
{
    nfc_device  *pnd;
    nfc_context *context;

    PR_NOTICE("=== NFC WiFi Configuration Tag Emulator ===");
    PR_NOTICE("Hardware: PN532 on UART2 (/dev/ttyS2)");

    // Initialize libnfc
    nfc_init(&context);
    if (context == NULL) {
        PR_ERR("Unable to init libnfc (malloc)");
        return EXIT_FAILURE;
    }

    // Set connection string for PN532 UART
    const char *connstring = "pn532_uart:/dev/ttyS2";

    // Open NFC device
    pnd = nfc_open(context, connstring);
    if (pnd == NULL) {
        PR_ERR("Unable to open NFC device: %s", connstring);
        nfc_exit(context);
        return EXIT_FAILURE;
    }

    PR_NOTICE("NFC device: %s opened", nfc_device_get_name(pnd));

    // ========== WiFi Configuration ==========
    // TODO: Modify these values according to your WiFi network
    // Note: Use WPA/WPA2 mixed mode for better compatibility
    wifi_config_t wifi_config = {
        .ssid      = ssid,                       // Your WiFi SSID
        .password  = password,                   // Your WiFi password
        .auth_type = WSC_AUTH_WPA_WPA2_PERSONAL, // WPA/WPA2-PSK mixed mode (more compatible)
        .encr_type = WSC_ENCR_AES_TKIP,          // TKIP+AES mixed (more compatible)
    };

    PR_NOTICE("WiFi Config: SSID='%s', Auth=WPA/WPA2-PSK(0x%04X), Encr=TKIP+AES(0x%04X)", wifi_config.ssid,
              wifi_config.auth_type, wifi_config.encr_type);

    // Allocate memory for Tag Type 2 data (use larger buffer for WiFi config)
    uint8_t *tag2_memory = tkl_system_malloc(256);
    if (tag2_memory == NULL) {
        PR_ERR("Failed to allocate memory for Tag Type 2 data");
        nfc_close(pnd);
        nfc_exit(context);
        return EXIT_FAILURE;
    }

    // Generate NDEF message with WiFi configuration
    int data_len = generate_tag2_ndef_wifi(&wifi_config, tag2_memory, 256);
    if (data_len < 0) {
        PR_ERR("Failed to generate WiFi NDEF message: error %d", data_len);
        tkl_system_free(tag2_memory);
        nfc_close(pnd);
        nfc_exit(context);
        return EXIT_FAILURE;
    }

    PR_NOTICE("Generated NDEF WiFi config: %d bytes", data_len);
    PR_NOTICE("Emulating WiFi tag now, please touch it with your phone");
    PR_NOTICE("Phone should prompt to connect to WiFi: %s", wifi_config.ssid);

    // Set up emulation parameters
    nfc_target nt = {
        .nm =
            {
                .nmt = NMT_ISO14443A,
                .nbr = NBR_UNDEFINED, // Will be updated by emulation
            },
        .nti =
            {
                .nai =
                    {
                        .abtAtqa  = {0x00, 0x04},
                        .abtUid   = {0x08, 0x00, 0xb0, 0x0b}, // Fixed UID for demo
                        .btSak    = 0x00,
                        .szUidLen = 4,
                        .abtAts   = {0x00},
                        .szAtsLen = 0,
                    },
            },
    };

    // Set up state machine with I/O handler
    struct nfc_emulation_state_machine state_machine = {.io = nfcforum_tag2_io_wifi};

    // Create emulator structure
    struct nfc_emulator emulator = {
        .target        = &nt,
        .state_machine = &state_machine,
        .user_data     = tag2_memory, // Tag memory data
    };

    PR_NOTICE("Starting emulation loop (will wait for phone to touch)...");

    // Start emulation - this will loop and wait for phone touches
    // while (1) {
    //     nfc_emulate_target(pnd, &emulator, 0);
    //     tkl_system_sleep(20);
    // }
    if (nfc_emulate_target(pnd, &emulator, 0) < 0) {
        nfc_perror(pnd, "nfc_emulate_target");
        tkl_system_free(tag2_memory);
        nfc_close(pnd);
        nfc_exit(context);
        return EXIT_FAILURE;
    }

    PR_NOTICE("Tag emulation completed successfully");

    // Cleanup
    tkl_system_free(tag2_memory);
    nfc_close(pnd);
    nfc_exit(context);

    return EXIT_SUCCESS;
}

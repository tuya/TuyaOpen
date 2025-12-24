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
 * @file nfc-emulate-forum-tag2.c
 * @brief Emulates a NFC-Forum Tag Type 2 with a NDEF message
 * This example allow to emulate an NFC-Forum Tag Type 2 that contains
 * a read-only NDEF message.
 *
 * This example has been developed using PN533 USB hardware as target and
 * Google Nexus S phone as initiator.
 *
 * This is know to NOT work with Nokia 6212 Classic and could fail with
 * several NFC Forum compliant devices due to the following reasons:
 *  - The emulated target has only a 4-byte UID while most devices assume a Tag
 *  Type 2 has always a 7-byte UID (as a real Mifare Ultralight tag);
 *  - The chip is emulating an ISO/IEC 14443-3 tag, without any hardware helper.
 *  If the initiator has too strict timeouts for software-based emulation
 *  (which is usually the case), this example will fail. This is not a bug
 *  and we can't do anything using this hardware (PN531/PN533).
 */

/*
 * This implementation was written based on information provided by the
 * following documents:
 *
 * NFC Forum Type 2 Tag Operation
 *  Technical Specification
 *  NFCForum-TS-Type-2-Tag_1.0 - 2007-07-09
 *
 * ISO/IEC 14443-3
 *  First edition - 2001-02-01
 *  Identification cards — Contactless integrated circuit(s) cards — Proximity cards
 *  Part 3: Initialization and anticollision
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif // HAVE_CONFIG_H

#include "platform_compat.h"
#include <errno.h>
#include <signal.h>
#include <stdlib.h>

#include <nfc/nfc.h>
#include <nfc/nfc-emulation.h>

#include "utils/nfc-utils.h"

static nfc_device  *pnd;
static nfc_context *context;

/**
 * @brief URI Protocol prefixes for NDEF URI records
 */
typedef enum {
    URI_PREFIX_NONE        = 0x00, // No prefix
    URI_PREFIX_HTTP_WWW    = 0x01, // http://www.
    URI_PREFIX_HTTPS_WWW   = 0x02, // https://www.
    URI_PREFIX_HTTP        = 0x03, // http://
    URI_PREFIX_HTTPS       = 0x04, // https://
    URI_PREFIX_TEL         = 0x05, // tel:
    URI_PREFIX_MAILTO      = 0x06, // mailto:
    URI_PREFIX_FTP_ANON    = 0x07, // ftp://anonymous:anonymous@
    URI_PREFIX_FTP_FTP     = 0x08, // ftp://ftp.
    URI_PREFIX_FTPS        = 0x09, // ftps://
    URI_PREFIX_SFTP        = 0x0A, // sftp://
    URI_PREFIX_SMB         = 0x0B, // smb://
    URI_PREFIX_NFS         = 0x0C, // nfs://
    URI_PREFIX_FTP         = 0x0D, // ftp://
    URI_PREFIX_DAV         = 0x0E, // dav://
    URI_PREFIX_NEWS        = 0x0F, // news:
    URI_PREFIX_TELNET      = 0x10, // telnet://
    URI_PREFIX_IMAP        = 0x11, // imap:
    URI_PREFIX_RTSP        = 0x12, // rtsp://
    URI_PREFIX_URN         = 0x13, // urn:
    URI_PREFIX_POP         = 0x14, // pop:
    URI_PREFIX_SIP         = 0x15, // sip:
    URI_PREFIX_SIPS        = 0x16, // sips:
    URI_PREFIX_TFTP        = 0x17, // tftp:
    URI_PREFIX_BTSPP       = 0x18, // btspp://
    URI_PREFIX_BTL2CAP     = 0x19, // btl2cap://
    URI_PREFIX_BTGOEP      = 0x1A, // btgoep://
    URI_PREFIX_TCPOBEX     = 0x1B, // tcpobex://
    URI_PREFIX_IRDAOBEX    = 0x1C, // irdaobex://
    URI_PREFIX_FILE        = 0x1D, // file://
    URI_PREFIX_URN_EPC_ID  = 0x1E, // urn:epc:id:
    URI_PREFIX_URN_EPC_TAG = 0x1F, // urn:epc:tag:
    URI_PREFIX_URN_EPC_PAT = 0x20, // urn:epc:pat:
    URI_PREFIX_URN_EPC_RAW = 0x21, // urn:epc:raw:
    URI_PREFIX_URN_EPC     = 0x22, // urn:epc:
    URI_PREFIX_URN_NFC     = 0x23, // urn:nfc:
} uri_prefix_t;

/**
 * @brief Structure to hold URI prefix information
 */
typedef struct {
    const char *prefix_str;
    uint8_t     prefix_code;
    size_t      prefix_len;
} uri_prefix_info_t;

// URI prefix lookup table
static const uri_prefix_info_t uri_prefixes[] = {
    {"http://www.", URI_PREFIX_HTTP_WWW, 11},
    {"https://www.", URI_PREFIX_HTTPS_WWW, 12},
    {"http://", URI_PREFIX_HTTP, 7},
    {"https://", URI_PREFIX_HTTPS, 8},
    {"tel:", URI_PREFIX_TEL, 4},
    {"mailto:", URI_PREFIX_MAILTO, 7},
    {"ftp://", URI_PREFIX_FTP, 6},
    {NULL, URI_PREFIX_NONE, 0} // End marker
};

/**
 * @brief Detect URI prefix and return prefix code and remaining URI
 *
 * @param uri Full URI string
 * @param remaining_uri Output: pointer to remaining URI after prefix
 * @return uint8_t URI prefix code
 */
static uint8_t detect_uri_prefix(const char *uri, const char **remaining_uri)
{
    if (uri == NULL || remaining_uri == NULL) {
        return URI_PREFIX_NONE;
    }

    // Try to match known prefixes
    for (int i = 0; uri_prefixes[i].prefix_str != NULL; i++) {
        if (strncmp(uri, uri_prefixes[i].prefix_str, uri_prefixes[i].prefix_len) == 0) {
            *remaining_uri = uri + uri_prefixes[i].prefix_len;
            return uri_prefixes[i].prefix_code;
        }
    }

    // No prefix matched, use the whole URI
    *remaining_uri = uri;
    return URI_PREFIX_NONE;
}

/**
 * @brief Generate NFC Forum Tag Type 2 memory area with NDEF URI record + Android Application Record (AAR)
 *
 * Adding an AAR forces Android to open the specified app (or Play Store if not installed).
 * This prevents other apps like Alipay/WeChat from intercepting the NFC tag.
 *
 * @param uri URI string (e.g., "https://tuyaopen.ai")
 * @param package_name Android package name (e.g., "com.android.chrome" or NULL to skip AAR)
 * @param buffer Output buffer (must be at least 128 bytes for AAR)
 * @param buffer_size Size of output buffer
 * @return int Number of bytes written, or negative error code
 *
 * @note Common package names:
 *   - "com.android.chrome" - Chrome browser
 *   - "com.android.browser" - Default browser
 *   - "com.example.myapp" - Your custom app
 *   - NULL - No AAR, use system default handling
 */
int generate_tag2_ndef_uri_with_aar(const char *uri, const char *package_name, uint8_t *buffer, size_t buffer_size)
{
    if (uri == NULL || buffer == NULL || buffer_size < 64) {
        PR_ERR("Invalid parameters: uri=%p, buffer=%p, size=%zu", uri, buffer, buffer_size);
        return -1;
    }

    const char *remaining_uri = NULL;
    uint8_t     prefix_code   = detect_uri_prefix(uri, &remaining_uri);
    size_t      uri_len       = strlen(remaining_uri);
    size_t      pkg_len       = package_name ? strlen(package_name) : 0;
    bool        use_aar       = (package_name != NULL && pkg_len > 0);

    PR_DEBUG("Generating NDEF URI: prefix=0x%02X, remaining='%s' (len=%zu)", prefix_code, remaining_uri, uri_len);
    if (use_aar) {
        PR_DEBUG("Adding AAR for package: %s", package_name);
    }

    // Calculate sizes
    size_t uri_payload_len = 1 + uri_len;             // Prefix code + URI
    size_t uri_record_len  = 3 + 1 + uri_payload_len; // Header(3) + Type(1) + Payload

    size_t aar_record_len = 0;
    if (use_aar) {
        // AAR: Header(3) + Type(15="android.com:pkg") + Payload(package_name)
        aar_record_len = 3 + 15 + pkg_len;
    }

    size_t ndef_message_len = uri_record_len + aar_record_len;

    // Check buffer size
    size_t required_size = 16 + 2 + ndef_message_len + 1; // Headers + TLV + Message + Terminator
    if (buffer_size < required_size) {
        PR_ERR("Buffer too small: need %zu, have %zu", required_size, buffer_size);
        return -2;
    }

    // Clear buffer
    memset(buffer, 0, buffer_size);

    int offset = 0;

    // Blocks 0-1: Internal data (UID area - set to 0 for emulation)
    offset += 8;

    // Block 2: Static lock bytes
    buffer[offset++] = 0xFF;
    buffer[offset++] = 0xFF;
    buffer[offset++] = 0x00;
    buffer[offset++] = 0x00;

    // Block 3: Capability Container (CC)
    buffer[offset++] = 0xE1; // NDEF Magic Number
    buffer[offset++] = 0x10; // Version 1.0
    // Memory size: calculate based on actual data
    uint8_t memory_blocks = ((ndef_message_len + 2 + 15) / 8); // +2 TLV header, round up
    if (memory_blocks < 6)
        memory_blocks = 6;
    buffer[offset++] = memory_blocks;
    buffer[offset++] = 0x00; // Read/write access

    // NDEF Message TLV
    buffer[offset++] = 0x03; // TLV Type: NDEF Message
    if (ndef_message_len <= 254) {
        buffer[offset++] = (uint8_t)ndef_message_len;
    } else {
        buffer[offset++] = 0xFF; // Extended length
        buffer[offset++] = (uint8_t)(ndef_message_len >> 8);
        buffer[offset++] = (uint8_t)(ndef_message_len & 0xFF);
    }

    // ========== URI Record ==========
    // Header: MB=1, ME=(1 if no AAR, 0 if AAR), CF=0, SR=1, IL=0, TNF=001
    uint8_t uri_header = use_aar ? 0x91 : 0xD1; // 0x91: MB=1,ME=0  0xD1: MB=1,ME=1
    buffer[offset++]   = uri_header;
    buffer[offset++]   = 0x01;                     // Type Length = 1
    buffer[offset++]   = (uint8_t)uri_payload_len; // Payload Length
    buffer[offset++]   = 0x55;                     // Type: 'U' (URI)

    // URI Payload
    buffer[offset++] = prefix_code;
    memcpy(buffer + offset, remaining_uri, uri_len);
    offset += uri_len;

    // ========== Android Application Record (AAR) ==========
    if (use_aar) {
        // Header: MB=0, ME=1, CF=0, SR=1, IL=0, TNF=100 (External)
        buffer[offset++] = 0x54;             // 0x54 = 0101 0100 = MB=0,ME=1,SR=1,TNF=100
        buffer[offset++] = 15;               // Type Length = 15 ("android.com:pkg")
        buffer[offset++] = (uint8_t)pkg_len; // Payload Length

        // Type: "android.com:pkg"
        const char *aar_type = "android.com:pkg";
        memcpy(buffer + offset, aar_type, 15);
        offset += 15;

        // Payload: package name
        memcpy(buffer + offset, package_name, pkg_len);
        offset += pkg_len;
    }

    // NDEF Terminator TLV
    buffer[offset++] = 0xFE;

    PR_DEBUG("NDEF generated: %d bytes (URI=%zu, AAR=%zu)", offset, uri_record_len, aar_record_len);
    return offset;
}

/**
 * @brief Generate NFC Forum Tag Type 2 memory area with NDEF URI record
 *
 * @param uri URI string (e.g., "https://tuyaopen.ai" or "http://example.com")
 * @param buffer Output buffer (must be at least 64 bytes)
 * @param buffer_size Size of output buffer
 * @return int Number of bytes written, or negative error code
 *
 * @note Tag Type 2 memory layout:
 *       Block 0-1: UID and internal data (set to 0 for emulation)
 *       Block 2: Static lock bytes (0xFF, 0xFF)
 *       Block 3: Capability Container (CC)
 *       Block 4+: NDEF message
 */
int generate_tag2_ndef_uri(const char *uri, uint8_t *buffer, size_t buffer_size)
{
    if (uri == NULL || buffer == NULL || buffer_size < 64) {
        PR_ERR("Invalid parameters: uri=%p, buffer=%p, size=%zu", uri, buffer, buffer_size);
        return -1;
    }

    const char *remaining_uri = NULL;
    uint8_t     prefix_code   = detect_uri_prefix(uri, &remaining_uri);
    size_t      uri_len       = strlen(remaining_uri);

    PR_DEBUG("Generating NDEF URI: prefix=0x%02X, remaining='%s' (len=%zu)", prefix_code, remaining_uri, uri_len);

    // Check if URI fits in Tag Type 2 (48 bytes data area)
    // NDEF overhead: TLV(2) + Record Header(3) + Type(1) + Prefix(1) + Terminator(1) = 8 bytes
    // Max URI length: 48 - 8 = 40 bytes
    if (uri_len > 40) {
        PR_ERR("URI too long: %zu bytes (max 40 bytes after prefix removal)", uri_len);
        return -2;
    }

    size_t ndef_payload_len = 1 + uri_len;              // Prefix code + URI
    size_t ndef_record_len  = 3 + 1 + ndef_payload_len; // Header(3) + Type(1) + Payload
    size_t ndef_message_len = ndef_record_len;
    // size_t ndef_tlv_len     = 2 + ndef_message_len; // Type(1) + Length(1) + Message

    // Clear buffer
    memset(buffer, 0, buffer_size);

    int offset = 0;

    // Blocks 0-1: Internal data (UID area - set to 0 for emulation)
    offset += 8;

    // Block 2: Static lock bytes
    buffer[offset++] = 0xFF; // Lock byte 0
    buffer[offset++] = 0xFF; // Lock byte 1
    buffer[offset++] = 0x00; // Reserved
    buffer[offset++] = 0x00; // Reserved

    // Block 3: Capability Container (CC)
    buffer[offset++] = 0xE1; // NDEF Magic Number
    buffer[offset++] = 0x10; // Version 1.0, no read/write access control
    buffer[offset++] = 0x06; // Memory size: 6 * 8 = 48 bytes
    buffer[offset++] = 0x00; // Read/write access: 0x00 = read-write

    // Block 4+: NDEF Message TLV
    buffer[offset++] = 0x03;                      // TLV Type: NDEF Message
    buffer[offset++] = (uint8_t)ndef_message_len; // TLV Length

    // NDEF Record Header
    buffer[offset++] = 0xD1;                      // MB=1, ME=1, CF=0, SR=1, IL=0, TNF=001 (Well-known)
    buffer[offset++] = 0x01;                      // Type Length = 1
    buffer[offset++] = (uint8_t)ndef_payload_len; // Payload Length

    // NDEF Record Type: 'U' (URI)
    buffer[offset++] = 0x55; // 'U'

    // NDEF Payload: URI Identifier Code + URI
    buffer[offset++] = prefix_code;
    memcpy(buffer + offset, remaining_uri, uri_len);
    offset += uri_len;

    // NDEF Terminator TLV
    buffer[offset++] = 0xFE;

    // Pad remaining bytes with 0x00
    while (offset < 64) {
        buffer[offset++] = 0x00;
    }

    PR_DEBUG("NDEF URI generated successfully: %d bytes total", offset);
    PR_DEBUG("  - NDEF Message: %zu bytes", ndef_message_len);
    PR_DEBUG("  - URI Payload: %zu bytes (prefix=0x%02X + '%s')", ndef_payload_len, prefix_code, remaining_uri);

    return offset;
}

void stop_emulation(int sig)
{
    (void)sig;
    if (pnd != NULL) {
        nfc_abort_command(pnd);
    } else {
        nfc_exit(context);
        exit(EXIT_FAILURE);
    }
}

#define READ          0x30
#define WRITE         0xA2
#define SECTOR_SELECT 0xC2

#define HALT 0x50
static int nfcforum_tag2_io(struct nfc_emulator *emulator, const uint8_t *data_in, const size_t data_in_len,
                            uint8_t *data_out, const size_t data_out_len)
{
    int res = 0;

    uint8_t *nfcforum_tag2_memory_area = (uint8_t *)(emulator->user_data);

    printf("    In: ");
    print_hex(data_in, data_in_len);

    switch (data_in[0]) {
    case READ:
        if (data_out_len >= 16) {
            memcpy(data_out, nfcforum_tag2_memory_area + (data_in[1] * 4), 16);
            res = 16;
        } else {
            res = -ENOSPC;
        }
        break;
    case HALT:
        printf("HALT sent\n");
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

int nfc_emulate_forum_tag2_main(char *uri, char *aar_package)
{
    // ========== Configuration ==========
    // URI to be encoded in the NFC tag
    // const char *uri = "https://tuyaopen.ai";  // ⬅️ Modify this URL as needed
    // const char *uri = "tel:+8613519284253";
    // const char *uri = "mailto:maidang.xing@tuya.com";

    // Android Application Record (AAR) - prevents other apps from intercepting
    // Set to NULL to disable AAR, or specify a package name:
    // - "com.android.chrome"     : Force open in Chrome
    // - "com.android.browser"    : Force open in default browser
    // - "com.tencent.mm"         : Force open in WeChat
    // - NULL                     : No AAR, system default handling (may be intercepted by Alipay etc.)
    // const char *aar_package = "com.android.browser";  // ⬅️ Set to NULL to disable AAR
    // const char *aar_package = NULL;  // Disable AAR
    // const char *aar_package = "com.android.contacts";  // Force open in Chrome

    // Buffer size: 64 bytes for simple URI, 128+ bytes if using AAR
    size_t buffer_size = (aar_package != NULL) ? 256 : 64;

    // Allocate memory for Tag Type 2 memory area
    uint8_t *tag2_memory = (uint8_t *)tal_psram_malloc(buffer_size);
    if (tag2_memory == NULL) {
        PR_ERR("Failed to allocate memory for Tag Type 2 data");
        return EXIT_FAILURE;
    }

    PR_DEBUG("====================================");
    PR_DEBUG("Generating NDEF Tag Type 2 Data");
    PR_DEBUG("====================================");
    PR_DEBUG("URI: %s", uri);
    if (aar_package) {
        PR_DEBUG("AAR Package: %s (prevents app interception)", aar_package);
    } else {
        PR_DEBUG("AAR: Disabled (other apps may intercept)");
    }

    int result;
    if (aar_package != NULL) {
        // Generate NDEF with AAR - prevents Alipay/WeChat from intercepting
        result = generate_tag2_ndef_uri_with_aar(uri, aar_package, tag2_memory, buffer_size);
    } else {
        // Generate simple NDEF URI
        result = generate_tag2_ndef_uri(uri, tag2_memory, buffer_size);
    }

    if (result < 0) {
        PR_ERR("Failed to generate NDEF URI data: error %d", result);
        tal_psram_free(tag2_memory);
        return EXIT_FAILURE;
    }

    // PR_DEBUG("====================================");
    // PR_DEBUG("NDEF data generated successfully!");
    // PR_DEBUG("Memory dump (first %d bytes):", (result > 64) ? 64 : result);
    // int dump_size = (result > 64) ? 64 : result;
    // for (int i = 0; i < dump_size; i += 16) {
    //     int remaining = dump_size - i;
    //     if (remaining >= 16) {
    //         PR_DEBUG("  %02X: %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X", i,
    //                 tag2_memory[i + 0], tag2_memory[i + 1], tag2_memory[i + 2], tag2_memory[i + 3], tag2_memory[i +
    //                 4], tag2_memory[i + 5], tag2_memory[i + 6], tag2_memory[i + 7], tag2_memory[i + 8], tag2_memory[i
    //                 + 9], tag2_memory[i + 10], tag2_memory[i + 11], tag2_memory[i + 12], tag2_memory[i + 13],
    //                 tag2_memory[i + 14], tag2_memory[i + 15]);
    //     }
    // }
    // PR_DEBUG("====================================");

    nfc_target nt = {.nm =
                         {
                             .nmt = NMT_ISO14443A,
                             .nbr = NBR_UNDEFINED, // Will be updated by nfc_target_init()
                         },
                     .nti = {
                         .nai =
                             {
                                 .abtAtqa  = {0x00, 0x04},
                                 .abtUid   = {0x08, 0x00, 0xb0, 0x0b},
                                 .abtAts   = {0x00},
                                 .szUidLen = 4,
                                 .btSak    = 0x00,
                                 .szAtsLen = 0,
                             },
                     }};

    struct nfc_emulation_state_machine state_machine = {.io = nfcforum_tag2_io};

    struct nfc_emulator emulator = {
        .target        = &nt,
        .state_machine = &state_machine,
        .user_data     = tag2_memory, // Use dynamically generated data
    };

    PR_DEBUG("Step 1: Initializing libnfc context...");
    nfc_init(&context);
    if (context == NULL) {
        PR_ERR("Failed to init libnfc context (malloc)");
        ERR("Unable to init libnfc (malloc)");
        tal_psram_free(tag2_memory);
        exit(EXIT_FAILURE);
    }
    PR_DEBUG("Context initialized: %p", context);

    // Use hardcoded connection string for embedded system (bypasses file system)
    const char *connstring = "pn532_uart:/dev/ttyS2";
    PR_DEBUG("Step 2: Opening NFC device with connstring: %s", connstring);
    pnd = nfc_open(context, connstring);
    PR_DEBUG("nfc_open returned: %p", pnd);

    if (pnd == NULL) {
        PR_ERR("nfc_open returned NULL - device open failed!");
        nfc_exit(context);
        tal_psram_free(tag2_memory);
        exit(EXIT_FAILURE);
    }

    PR_DEBUG("Step 3: Device opened successfully!");
    PR_DEBUG("NFC device: %s opened\n", nfc_device_get_name(pnd));
    PR_DEBUG("Emulating NDEF tag with URI: %s\n", uri);
    if (aar_package) {
        PR_DEBUG("AAR enabled: %s (prevents Alipay/WeChat interception)\n", aar_package);
    }
    PR_DEBUG("Please touch it with a second NFC device\n");

    // while (1) {
    //     nfc_emulate_target(pnd, &emulator, 0);
    //     tkl_system_sleep(20);
    // }

    if (nfc_emulate_target(pnd, &emulator, 0) < 0) {
        nfc_perror(pnd, "nfc_emulate_target");
        nfc_close(pnd);
        nfc_exit(context);
        tal_psram_free(tag2_memory);
        return EXIT_FAILURE;
    }

    nfc_close(pnd);
    nfc_exit(context);
    tal_psram_free(tag2_memory);
    return EXIT_SUCCESS;
}

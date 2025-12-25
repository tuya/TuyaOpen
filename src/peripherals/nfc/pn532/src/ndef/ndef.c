/**
 * @file ndef.c
 * @brief NDEF (NFC Data Exchange Format) Encoding Library Implementation
 *
 * @author Tuya Inc.
 * @date 2025
 */

#include "ndef/ndef.h"
#include <string.h>
#include <stdio.h>

#ifdef HAVE_CONFIG_H
#include "nfc_config.h"
#endif

// Tuya platform logging (optional - falls back to printf if not available)
#ifdef TAL_LOG_H
#include "tal_log.h"
#define NDEF_LOG_ERR(...)   PR_ERR(__VA_ARGS__)
#define NDEF_LOG_DEBUG(...) PR_DEBUG(__VA_ARGS__)
#else
#define NDEF_LOG_ERR(...)                                                                                              \
    do {                                                                                                               \
        printf("[NDEF ERR] " __VA_ARGS__);                                                                             \
        printf("\n");                                                                                                  \
    } while (0)
#define NDEF_LOG_DEBUG(...)                                                                                            \
    do { /* disabled */                                                                                                \
    } while (0)
#endif

#define NDEF_OK           0  /**< Success */
#define NDEF_ERR_INVALID  -1 /**< Invalid parameter */
#define NDEF_ERR_BUFFER   -2 /**< Buffer too small */
#define NDEF_ERR_STATE    -3 /**< Invalid state */
#define NDEF_ERR_OVERFLOW -4 /**< Data overflow */
#define NDEF_ERR_FORMAT   -5 /**< Format error */

/*============================================================================
 * Internal Constants
 *============================================================================*/

// NFC Forum RTD Type Names
static const uint8_t RTD_URI[]  = {'U'};      // URI Record
static const uint8_t RTD_TEXT[] = {'T'};      // Text Record
static const uint8_t RTD_SP[]   = {'S', 'p'}; // Smart Poster
// static const uint8_t RTD_AC[]   = { 'a', 'c' };               // Alternative Carrier
// static const uint8_t RTD_HC[]   = { 'H', 'c' };               // Handover Carrier

// AAR Type Name (External Type)
static const char AAR_TYPE[] = "android.com:pkg";

// WiFi MIME Type
static const char WIFI_MIME_TYPE[] = "application/vnd.wfa.wsc";

// vCard MIME Type
static const char VCARD_MIME_TYPE[] = "text/vcard";

// WiFi WSC Attribute IDs
#define WSC_ID_CREDENTIAL    0x100E
#define WSC_ID_NETWORK_INDEX 0x1026
#define WSC_ID_SSID          0x1045
#define WSC_ID_AUTH_TYPE     0x1003
#define WSC_ID_ENCR_TYPE     0x100F
#define WSC_ID_NETWORK_KEY   0x1027
#define WSC_ID_MAC_ADDRESS   0x1020
#define WSC_ID_VENDOR_EXT    0x1049
#define WSC_ID_VERSION2      0x00

// WFA Vendor ID
#define WFA_VENDOR_ID_0 0x00
#define WFA_VENDOR_ID_1 0x37
#define WFA_VENDOR_ID_2 0x2A

// URI Prefix Table
typedef struct {
    const char     *prefix_str;
    ndef_uri_code_t prefix_code;
    size_t          prefix_len;
} uri_prefix_entry_t;

static const uri_prefix_entry_t uri_prefixes[] = {
    {"http://www.", NDEF_URI_HTTP_WWW, 11},
    {"https://www.", NDEF_URI_HTTPS_WWW, 12},
    {"http://", NDEF_URI_HTTP, 7},
    {"https://", NDEF_URI_HTTPS, 8},
    {"tel:", NDEF_URI_TEL, 4},
    {"mailto:", NDEF_URI_MAILTO, 7},
    {"ftp://", NDEF_URI_FTP, 6},
    {"ftps://", NDEF_URI_FTPS, 7},
    {"sftp://", NDEF_URI_SFTP, 7},
    {"smb://", NDEF_URI_SMB, 6},
    {"nfs://", NDEF_URI_NFS, 6},
    {"dav://", NDEF_URI_DAV, 6},
    {"telnet://", NDEF_URI_TELNET, 9},
    {"rtsp://", NDEF_URI_RTSP, 7},
    {"sip:", NDEF_URI_SIP, 4},
    {"sips:", NDEF_URI_SIPS, 5},
    {"file://", NDEF_URI_FILE, 7},
    {NULL, NDEF_URI_NONE, 0} // End marker
};

/*============================================================================
 * Internal Helper Functions
 *============================================================================*/

/**
 * @brief Write 16-bit value in big-endian format
 */
static inline void write_be16(uint8_t *buf, uint16_t value)
{
    buf[0] = (value >> 8) & 0xFF;
    buf[1] = value & 0xFF;
}

/**
 * @brief Write 32-bit value in big-endian format
 */
static inline void write_be32(uint8_t *buf, uint32_t value)
{
    buf[0] = (value >> 24) & 0xFF;
    buf[1] = (value >> 16) & 0xFF;
    buf[2] = (value >> 8) & 0xFF;
    buf[3] = value & 0xFF;
}

/**
 * @brief Detect URI prefix and return code
 */
static ndef_uri_code_t detect_uri_prefix(const char *uri, const char **remaining)
{
    if (uri == NULL || remaining == NULL) {
        return NDEF_URI_NONE;
    }

    for (int i = 0; uri_prefixes[i].prefix_str != NULL; i++) {
        if (strncmp(uri, uri_prefixes[i].prefix_str, uri_prefixes[i].prefix_len) == 0) {
            *remaining = uri + uri_prefixes[i].prefix_len;
            return uri_prefixes[i].prefix_code;
        }
    }

    *remaining = uri;
    return NDEF_URI_NONE;
}

/**
 * @brief Calculate record header size
 */
static size_t calc_record_header_size(size_t type_len, size_t payload_len, size_t id_len)
{
    size_t size = 1; // Flags byte

    // Type length (always 1 byte)
    size += 1;

    // Payload length (1 byte for SR, 4 bytes otherwise)
    size += (payload_len <= 255) ? 1 : 4;

    // ID length (1 byte if present)
    if (id_len > 0) {
        size += 1;
    }

    // Type field
    size += type_len;

    // ID field
    size += id_len;

    return size;
}

/**
 * @brief Write NDEF record header
 */
static int write_record_header(uint8_t *buf, size_t buf_size, ndef_tnf_t tnf, bool mb, bool me, const uint8_t *type,
                               size_t type_len, size_t payload_len, const uint8_t *id, size_t id_len)
{
    bool sr = (payload_len <= 255);
    bool il = (id_len > 0);

    size_t header_size = calc_record_header_size(type_len, payload_len, id_len);
    if (buf_size < header_size) {
        return NDEF_ERR_BUFFER;
    }

    int offset = 0;

    // Flags byte: MB | ME | CF | SR | IL | TNF
    uint8_t flags = (tnf & 0x07);
    if (mb)
        flags |= NDEF_FLAG_MB;
    if (me)
        flags |= NDEF_FLAG_ME;
    if (sr)
        flags |= NDEF_FLAG_SR;
    if (il)
        flags |= NDEF_FLAG_IL;
    buf[offset++] = flags;

    // Type length
    buf[offset++] = (uint8_t)type_len;

    // Payload length
    if (sr) {
        buf[offset++] = (uint8_t)payload_len;
    } else {
        write_be32(buf + offset, (uint32_t)payload_len);
        offset += 4;
    }

    // ID length (if present)
    if (il) {
        buf[offset++] = (uint8_t)id_len;
    }

    // Type field
    if (type && type_len > 0) {
        memcpy(buf + offset, type, type_len);
        offset += type_len;
    }

    // ID field
    if (id && id_len > 0) {
        memcpy(buf + offset, id, id_len);
        offset += id_len;
    }

    return offset;
}

/**
 * @brief Write WSC attribute (TLV format)
 */
static int write_wsc_attribute(uint8_t *buf, uint16_t attr_id, uint16_t attr_len, const uint8_t *data)
{
    int offset = 0;

    write_be16(buf + offset, attr_id);
    offset += 2;

    write_be16(buf + offset, attr_len);
    offset += 2;

    if (data && attr_len > 0) {
        memcpy(buf + offset, data, attr_len);
        offset += attr_len;
    }

    return offset;
}

/*============================================================================
 * NDEF Message API Implementation
 *============================================================================*/

int ndef_message_init(ndef_message_t *msg, uint8_t *buffer, size_t buffer_size)
{
    if (msg == NULL || buffer == NULL || buffer_size < 8) {
        return NDEF_ERR_INVALID;
    }

    msg->buffer             = buffer;
    msg->buffer_size        = buffer_size;
    msg->offset             = 0;
    msg->last_record_offset = 0;
    msg->record_count       = 0;
    msg->message_open       = false;

    return NDEF_OK;
}

int ndef_message_begin(ndef_message_t *msg)
{
    if (msg == NULL) {
        return NDEF_ERR_INVALID;
    }

    msg->offset             = 0;
    msg->last_record_offset = 0;
    msg->record_count       = 0;
    msg->message_open       = true;

    return NDEF_OK;
}

int ndef_message_end(ndef_message_t *msg)
{
    if (msg == NULL || !msg->message_open) {
        return NDEF_ERR_STATE;
    }

    // Update ME flag on last record if there are records
    if (msg->record_count > 0 && msg->offset > 0) {
        // Find the last record's flags byte and set ME
        // This is tricky because we'd need to track record positions
        // For simplicity, we rely on ndef_add_*_record to set ME when appropriate
    }

    msg->message_open = false;
    return (int)msg->offset;
}

size_t ndef_message_get_length(const ndef_message_t *msg)
{
    return msg ? msg->offset : 0;
}

void ndef_message_reset(ndef_message_t *msg)
{
    if (msg) {
        msg->offset             = 0;
        msg->last_record_offset = 0;
        msg->record_count       = 0;
        msg->message_open       = false;
    }
}

/*============================================================================
 * NDEF Record API Implementation
 *============================================================================*/

int ndef_add_raw_record(ndef_message_t *msg, ndef_tnf_t tnf, const uint8_t *type, size_t type_len,
                        const uint8_t *payload, size_t payload_len, const uint8_t *id, size_t id_len)
{
    if (msg == NULL || !msg->message_open) {
        return NDEF_ERR_STATE;
    }

    bool mb = (msg->record_count == 0); // First record has MB set
    bool me = true;                     // Assume this is the last record (caller can add more)

    // Calculate required space
    size_t header_size = calc_record_header_size(type_len, payload_len, id_len);
    size_t total_size  = header_size + payload_len;

    if (msg->offset + total_size > msg->buffer_size) {
        NDEF_LOG_ERR("Buffer overflow: need %zu, have %zu", total_size, msg->buffer_size - msg->offset);
        return NDEF_ERR_BUFFER;
    }

    // If this is not the first record, clear ME from previous record
    if (msg->record_count > 0 && msg->last_record_offset < msg->offset) {
        // Clear ME flag (bit 6) from previous record's flags byte
        msg->buffer[msg->last_record_offset] &= ~NDEF_FLAG_ME;
    }

    // Save current record offset for future ME flag updates
    msg->last_record_offset = msg->offset;

    // Write record header
    int header_len = write_record_header(msg->buffer + msg->offset, msg->buffer_size - msg->offset, tnf, mb, me, type,
                                         type_len, payload_len, id, id_len);

    if (header_len < 0) {
        return header_len;
    }

    msg->offset += header_len;

    // Write payload
    if (payload && payload_len > 0) {
        memcpy(msg->buffer + msg->offset, payload, payload_len);
        msg->offset += payload_len;
    }

    msg->record_count++;

    return NDEF_OK;
}

int ndef_add_uri_record(ndef_message_t *msg, const ndef_uri_config_t *config)
{
    if (msg == NULL || config == NULL || config->uri == NULL) {
        return NDEF_ERR_INVALID;
    }

    const char     *remaining   = NULL;
    ndef_uri_code_t prefix_code = detect_uri_prefix(config->uri, &remaining);
    size_t          uri_len     = strlen(remaining);

    // Payload: prefix code (1 byte) + URI string
    size_t  payload_len = 1 + uri_len;
    uint8_t payload[256];

    if (payload_len > sizeof(payload)) {
        return NDEF_ERR_OVERFLOW;
    }

    payload[0] = (uint8_t)prefix_code;
    memcpy(payload + 1, remaining, uri_len);

    return ndef_add_raw_record(msg, NDEF_TNF_WELL_KNOWN, RTD_URI, sizeof(RTD_URI), payload, payload_len, NULL, 0);
}

int ndef_add_text_record(ndef_message_t *msg, const ndef_text_config_t *config)
{
    if (msg == NULL || config == NULL || config->text == NULL) {
        return NDEF_ERR_INVALID;
    }

    const char *lang     = config->lang ? config->lang : NDEF_LANG_EN;
    size_t      lang_len = strlen(lang);
    size_t      text_len = strlen(config->text);

    if (lang_len > 63) {
        return NDEF_ERR_INVALID; // Language code too long
    }

    // Payload: status byte + language code + text
    size_t  payload_len = 1 + lang_len + text_len;
    uint8_t payload[512];

    if (payload_len > sizeof(payload)) {
        return NDEF_ERR_OVERFLOW;
    }

    // Status byte: bit 7 = encoding (0=UTF-8, 1=UTF-16), bits 5-0 = language length
    payload[0] = (config->is_utf16 ? 0x80 : 0x00) | (lang_len & 0x3F);
    memcpy(payload + 1, lang, lang_len);
    memcpy(payload + 1 + lang_len, config->text, text_len);

    return ndef_add_raw_record(msg, NDEF_TNF_WELL_KNOWN, RTD_TEXT, sizeof(RTD_TEXT), payload, payload_len, NULL, 0);
}

int ndef_add_wifi_record(ndef_message_t *msg, const ndef_wifi_config_t *config)
{
    if (msg == NULL || config == NULL || config->ssid == NULL || config->password == NULL) {
        return NDEF_ERR_INVALID;
    }

    size_t ssid_len = strlen(config->ssid);
    size_t pass_len = strlen(config->password);

    if (ssid_len > 32 || pass_len > 64) {
        NDEF_LOG_ERR("SSID or password too long: ssid=%zu, pass=%zu", ssid_len, pass_len);
        return NDEF_ERR_INVALID;
    }

    // Build WSC credential payload
    uint8_t wsc_payload[256];
    int     offset = 0;

    // Calculate credential inner length
    int cred_len = 0;
    cred_len += 4 + 1;        // Network Index
    cred_len += 4 + ssid_len; // SSID
    cred_len += 4 + 2;        // Auth Type
    cred_len += 4 + 2;        // Encr Type
    cred_len += 4 + pass_len; // Network Key
    cred_len += 4 + 6;        // MAC Address

    // WFA Vendor Extension length
    int vendor_ext_len = 3 + 1 + 1 + 1; // Vendor ID + Version2 subelement

    // Credential attribute header
    write_be16(wsc_payload + offset, WSC_ID_CREDENTIAL);
    offset += 2;
    write_be16(wsc_payload + offset, cred_len);
    offset += 2;

    // Network Index
    uint8_t net_idx = 1;
    offset += write_wsc_attribute(wsc_payload + offset, WSC_ID_NETWORK_INDEX, 1, &net_idx);

    // SSID
    offset += write_wsc_attribute(wsc_payload + offset, WSC_ID_SSID, ssid_len, (const uint8_t *)config->ssid);

    // Auth Type
    uint8_t auth_buf[2];
    write_be16(auth_buf, config->auth_type);
    offset += write_wsc_attribute(wsc_payload + offset, WSC_ID_AUTH_TYPE, 2, auth_buf);

    // Encr Type
    uint8_t encr_buf[2];
    write_be16(encr_buf, config->encr_type);
    offset += write_wsc_attribute(wsc_payload + offset, WSC_ID_ENCR_TYPE, 2, encr_buf);

    // Network Key
    offset +=
        write_wsc_attribute(wsc_payload + offset, WSC_ID_NETWORK_KEY, pass_len, (const uint8_t *)config->password);

    // MAC Address (wildcard)
    uint8_t mac_addr[6] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    offset += write_wsc_attribute(wsc_payload + offset, WSC_ID_MAC_ADDRESS, 6, mac_addr);

    // WFA Vendor Extension
    write_be16(wsc_payload + offset, WSC_ID_VENDOR_EXT);
    offset += 2;
    write_be16(wsc_payload + offset, vendor_ext_len);
    offset += 2;
    wsc_payload[offset++] = WFA_VENDOR_ID_0;
    wsc_payload[offset++] = WFA_VENDOR_ID_1;
    wsc_payload[offset++] = WFA_VENDOR_ID_2;
    wsc_payload[offset++] = WSC_ID_VERSION2; // Version2 ID
    wsc_payload[offset++] = 0x01;            // Length
    wsc_payload[offset++] = 0x20;            // Value (WPS 2.0)

    // Add as MIME record
    return ndef_add_raw_record(msg, NDEF_TNF_MIME_MEDIA, (const uint8_t *)WIFI_MIME_TYPE, strlen(WIFI_MIME_TYPE),
                               wsc_payload, offset, NULL, 0);
}

int ndef_add_aar_record(ndef_message_t *msg, const ndef_aar_config_t *config)
{
    if (msg == NULL || config == NULL || config->package_name == NULL) {
        return NDEF_ERR_INVALID;
    }

    size_t pkg_len = strlen(config->package_name);

    return ndef_add_raw_record(msg, NDEF_TNF_EXTERNAL, (const uint8_t *)AAR_TYPE, strlen(AAR_TYPE),
                               (const uint8_t *)config->package_name, pkg_len, NULL, 0);
}

int ndef_add_vcard_record(ndef_message_t *msg, const ndef_vcard_config_t *config)
{
    if (msg == NULL || config == NULL || config->name == NULL) {
        return NDEF_ERR_INVALID;
    }

    // Build vCard 3.0 payload (iOS compatible format)
    // Reference: RFC 6350 (vCard 4.0) / RFC 2426 (vCard 3.0)
    char vcard[1024];
    int  len = 0;

    len += snprintf(vcard + len, sizeof(vcard) - len, "BEGIN:VCARD\r\n");
    len += snprintf(vcard + len, sizeof(vcard) - len, "VERSION:3.0\r\n");

    // FN (Formatted Name) - required in vCard 3.0
    len += snprintf(vcard + len, sizeof(vcard) - len, "FN:%s\r\n", config->name);

    // N (Name) - structured format required for iOS
    // Format: N:LastName;FirstName;MiddleName;Prefix;Suffix
    // For simplicity, we put the full name in LastName field
    len += snprintf(vcard + len, sizeof(vcard) - len, "N:%s;;;;\r\n", config->name);

    if (config->phone) {
        // TEL with TYPE parameter for better iOS compatibility
        len += snprintf(vcard + len, sizeof(vcard) - len, "TEL;TYPE=WORK:%s\r\n", config->phone);
    }
    if (config->email) {
        // EMAIL with TYPE parameter
        len += snprintf(vcard + len, sizeof(vcard) - len, "EMAIL;TYPE=INTERNET:%s\r\n", config->email);
    }
    if (config->org) {
        len += snprintf(vcard + len, sizeof(vcard) - len, "ORG:%s\r\n", config->org);
    }
    if (config->title) {
        len += snprintf(vcard + len, sizeof(vcard) - len, "TITLE:%s\r\n", config->title);
    }
    if (config->url) {
        len += snprintf(vcard + len, sizeof(vcard) - len, "URL:%s\r\n", config->url);
    }
    if (config->address) {
        // ADR structured format: PO Box;Extended;Street;City;Region;PostalCode;Country
        // For simplicity, put address in Street field
        len += snprintf(vcard + len, sizeof(vcard) - len, "ADR;TYPE=WORK:;;%s;;;;\r\n", config->address);
    }
    if (config->note) {
        len += snprintf(vcard + len, sizeof(vcard) - len, "NOTE:%s\r\n", config->note);
    }

    len += snprintf(vcard + len, sizeof(vcard) - len, "END:VCARD");

    return ndef_add_raw_record(msg, NDEF_TNF_MIME_MEDIA, (const uint8_t *)VCARD_MIME_TYPE, strlen(VCARD_MIME_TYPE),
                               (const uint8_t *)vcard, len, NULL, 0);
}

int ndef_add_mime_record(ndef_message_t *msg, const ndef_mime_config_t *config)
{
    if (msg == NULL || config == NULL || config->mime_type == NULL) {
        return NDEF_ERR_INVALID;
    }

    return ndef_add_raw_record(msg, NDEF_TNF_MIME_MEDIA, (const uint8_t *)config->mime_type, strlen(config->mime_type),
                               config->payload, config->payload_len, NULL, 0);
}

int ndef_add_external_record(ndef_message_t *msg, const ndef_external_config_t *config)
{
    if (msg == NULL || config == NULL || config->domain == NULL || config->type == NULL) {
        return NDEF_ERR_INVALID;
    }

    // Build external type string: "domain:type"
    char type_str[128];
    int  type_len = snprintf(type_str, sizeof(type_str), "%s:%s", config->domain, config->type);

    return ndef_add_raw_record(msg, NDEF_TNF_EXTERNAL, (const uint8_t *)type_str, type_len, config->payload,
                               config->payload_len, NULL, 0);
}

int ndef_add_smart_poster_record(ndef_message_t *msg, const ndef_smart_poster_config_t *config)
{
    if (msg == NULL || config == NULL || config->uri == NULL) {
        return NDEF_ERR_INVALID;
    }

    // Build nested NDEF message for Smart Poster payload
    uint8_t        sp_payload[512];
    ndef_message_t sp_msg;

    ndef_message_init(&sp_msg, sp_payload, sizeof(sp_payload));
    ndef_message_begin(&sp_msg);

    // Add URI record (required)
    ndef_uri_config_t uri_cfg = {.uri = config->uri};
    ndef_add_uri_record(&sp_msg, &uri_cfg);

    // Add title record (optional)
    if (config->title) {
        ndef_text_config_t text_cfg = {
            .text = config->title, .lang = config->title_lang ? config->title_lang : NDEF_LANG_EN, .is_utf16 = false};
        ndef_add_text_record(&sp_msg, &text_cfg);
    }

    int sp_len = ndef_message_end(&sp_msg);
    if (sp_len < 0) {
        return sp_len;
    }

    // Add Smart Poster as well-known type with nested payload
    return ndef_add_raw_record(msg, NDEF_TNF_WELL_KNOWN, RTD_SP, sizeof(RTD_SP), sp_payload, sp_len, NULL, 0);
}

/*============================================================================
 * Tag Type 2 Implementation
 *============================================================================*/

int ndef_tag2_init(ndef_tag2_t *tag, uint8_t *buffer, size_t buffer_size)
{
    if (tag == NULL || buffer == NULL || buffer_size < 64) {
        return NDEF_ERR_INVALID;
    }

    tag->buffer      = buffer;
    tag->buffer_size = buffer_size;
    tag->data_len    = 0;

    memset(buffer, 0, buffer_size);

    return NDEF_OK;
}

int ndef_tag2_write(ndef_tag2_t *tag, const uint8_t *ndef_data, size_t ndef_len)
{
    if (tag == NULL || ndef_data == NULL) {
        return NDEF_ERR_INVALID;
    }

    // Calculate required size
    size_t tlv_header_size = (ndef_len <= 254) ? 2 : 4;
    size_t required_size   = 16 + tlv_header_size + ndef_len + 1; // Header + TLV + NDEF + Terminator

    if (required_size > tag->buffer_size) {
        NDEF_LOG_ERR("Tag2 buffer too small: need %zu, have %zu", required_size, tag->buffer_size);
        return NDEF_ERR_BUFFER;
    }

    int offset = 0;

    // Blocks 0-1: UID area (8 bytes) - set to 0 for emulation
    offset += 8;

    // Block 2: Static lock bytes
    tag->buffer[offset++] = 0xFF; // Lock byte 0
    tag->buffer[offset++] = 0xFF; // Lock byte 1
    tag->buffer[offset++] = 0x00; // Reserved
    tag->buffer[offset++] = 0x00; // Reserved

    // Block 3: Capability Container (CC)
    tag->buffer[offset++] = 0xE1; // NDEF Magic Number
    tag->buffer[offset++] = 0x10; // Version 1.0

    // Memory size in 8-byte blocks
    uint8_t memory_blocks = ((ndef_len + tlv_header_size + 1 + 7) / 8);
    if (memory_blocks < 6)
        memory_blocks = 6; // Minimum
    tag->buffer[offset++] = memory_blocks;

    tag->buffer[offset++] = 0x00; // Read/write access

    // NDEF Message TLV
    tag->buffer[offset++] = 0x03; // TLV Type: NDEF Message

    if (ndef_len <= 254) {
        tag->buffer[offset++] = (uint8_t)ndef_len;
    } else {
        tag->buffer[offset++] = 0xFF; // Extended length
        tag->buffer[offset++] = (uint8_t)(ndef_len >> 8);
        tag->buffer[offset++] = (uint8_t)(ndef_len & 0xFF);
    }

    // NDEF Message data
    memcpy(tag->buffer + offset, ndef_data, ndef_len);
    offset += ndef_len;

    // Terminator TLV
    tag->buffer[offset++] = 0xFE;

    tag->data_len = offset;

    return offset;
}

/**
 * @brief Get Tag Type 2 data pointer
 *
 * @param tag Tag Type 2 context
 * @return Pointer to tag data
 */
uint8_t *ndef_tag2_get_data(ndef_tag2_t *tag)
{
    return tag ? tag->buffer : NULL;
}

/**
 * @brief Get Tag Type 2 data length
 *
 * @param tag Tag Type 2 context
 * @return Data length in bytes
 */
size_t ndef_tag2_get_length(const ndef_tag2_t *tag)
{
    return tag ? tag->data_len : 0;
}

/*============================================================================
 * Convenience Functions Implementation
 *============================================================================*/

int ndef_create_uri_tag(ndef_tag2_t *tag, const char *uri)
{
    if (tag == NULL || uri == NULL) {
        return NDEF_ERR_INVALID;
    }

    uint8_t        ndef_buf[256];
    ndef_message_t msg;

    ndef_message_init(&msg, ndef_buf, sizeof(ndef_buf));
    ndef_message_begin(&msg);

    ndef_uri_config_t config = {.uri = uri};
    int               ret    = ndef_add_uri_record(&msg, &config);
    if (ret < 0)
        return ret;

    int ndef_len = ndef_message_end(&msg);
    if (ndef_len < 0)
        return ndef_len;

    return ndef_tag2_write(tag, ndef_buf, ndef_len);
}

int ndef_create_uri_aar_tag(ndef_tag2_t *tag, const char *uri, const char *package_name)
{
    if (tag == NULL || uri == NULL) {
        return NDEF_ERR_INVALID;
    }

    uint8_t        ndef_buf[512];
    ndef_message_t msg;

    ndef_message_init(&msg, ndef_buf, sizeof(ndef_buf));
    ndef_message_begin(&msg);

    // Add URI record
    ndef_uri_config_t uri_cfg = {.uri = uri};
    int               ret     = ndef_add_uri_record(&msg, &uri_cfg);
    if (ret < 0)
        return ret;

    // Add AAR record if package specified
    if (package_name != NULL) {
        ndef_aar_config_t aar_cfg = {.package_name = package_name};
        ret                       = ndef_add_aar_record(&msg, &aar_cfg);
        if (ret < 0)
            return ret;
    }

    int ndef_len = ndef_message_end(&msg);
    if (ndef_len < 0)
        return ndef_len;

    return ndef_tag2_write(tag, ndef_buf, ndef_len);
}

int ndef_create_wifi_tag(ndef_tag2_t *tag, const char *ssid, const char *password, ndef_wifi_auth_t auth_type,
                         ndef_wifi_encr_t encr_type)
{
    if (tag == NULL || ssid == NULL || password == NULL) {
        return NDEF_ERR_INVALID;
    }

    uint8_t        ndef_buf[512];
    ndef_message_t msg;

    ndef_message_init(&msg, ndef_buf, sizeof(ndef_buf));
    ndef_message_begin(&msg);

    ndef_wifi_config_t config = {.ssid = ssid, .password = password, .auth_type = auth_type, .encr_type = encr_type};

    int ret = ndef_add_wifi_record(&msg, &config);
    if (ret < 0)
        return ret;

    int ndef_len = ndef_message_end(&msg);
    if (ndef_len < 0)
        return ndef_len;

    return ndef_tag2_write(tag, ndef_buf, ndef_len);
}

int ndef_create_text_tag(ndef_tag2_t *tag, const char *text, const char *lang)
{
    if (tag == NULL || text == NULL) {
        return NDEF_ERR_INVALID;
    }

    uint8_t        ndef_buf[512];
    ndef_message_t msg;

    ndef_message_init(&msg, ndef_buf, sizeof(ndef_buf));
    ndef_message_begin(&msg);

    ndef_text_config_t config = {.text = text, .lang = lang ? lang : NDEF_LANG_EN, .is_utf16 = false};

    int ret = ndef_add_text_record(&msg, &config);
    if (ret < 0)
        return ret;

    int ndef_len = ndef_message_end(&msg);
    if (ndef_len < 0)
        return ndef_len;

    return ndef_tag2_write(tag, ndef_buf, ndef_len);
}

int ndef_create_vcard_tag(ndef_tag2_t *tag, const ndef_vcard_config_t *config)
{
    if (tag == NULL || config == NULL) {
        return NDEF_ERR_INVALID;
    }

    uint8_t        ndef_buf[1024];
    ndef_message_t msg;

    ndef_message_init(&msg, ndef_buf, sizeof(ndef_buf));
    ndef_message_begin(&msg);

    int ret = ndef_add_vcard_record(&msg, config);
    if (ret < 0)
        return ret;

    int ndef_len = ndef_message_end(&msg);
    if (ndef_len < 0)
        return ndef_len;

    return ndef_tag2_write(tag, ndef_buf, ndef_len);
}

int ndef_create_smart_poster_tag(ndef_tag2_t *tag, const char *uri, const char *title)
{
    if (tag == NULL || uri == NULL) {
        return NDEF_ERR_INVALID;
    }

    uint8_t        ndef_buf[512];
    ndef_message_t msg;

    ndef_message_init(&msg, ndef_buf, sizeof(ndef_buf));
    ndef_message_begin(&msg);

    ndef_smart_poster_config_t config = {.uri = uri, .title = title, .title_lang = NDEF_LANG_EN, .action = 0};

    int ret = ndef_add_smart_poster_record(&msg, &config);
    if (ret < 0)
        return ret;

    int ndef_len = ndef_message_end(&msg);
    if (ndef_len < 0)
        return ndef_len;

    return ndef_tag2_write(tag, ndef_buf, ndef_len);
}

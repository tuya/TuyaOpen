/**
 * @file ndef.h
 * @brief NDEF (NFC Data Exchange Format) Encoding Library Header
 * @version 0.1
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#ifndef _NDEF_H_
#define _NDEF_H_

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/***********************************************************
************************macro define************************
***********************************************************/
#define CMD_READ          0x30
#define CMD_WRITE         0xA2
#define CMD_SECTOR_SELECT 0xC2
#define CMD_HALT          0x50

/**
 * @brief NDEF Record Flags
 */
#define NDEF_FLAG_MB 0x80 /**< Message Begin */
#define NDEF_FLAG_ME 0x40 /**< Message End */
#define NDEF_FLAG_CF 0x20 /**< Chunk Flag */
#define NDEF_FLAG_SR 0x10 /**< Short Record (payload length fits in 1 byte) */
#define NDEF_FLAG_IL 0x08 /**< ID Length present */

/**
 * @brief Text Record Language Codes (ISO 639-1)
 */
#define NDEF_LANG_EN "en" /**< English */
#define NDEF_LANG_ZH "zh" /**< Chinese */
#define NDEF_LANG_JA "ja" /**< Japanese */
#define NDEF_LANG_KO "ko" /**< Korean */
#define NDEF_LANG_DE "de" /**< German */
#define NDEF_LANG_FR "fr" /**< French */
#define NDEF_LANG_ES "es" /**< Spanish */
/***********************************************************
***********************typedef define***********************
***********************************************************/
/**
 * @brief NDEF Record Type Name Format (TNF) values
 */
typedef enum {
    NDEF_TNF_EMPTY        = 0x00, /**< Empty record */
    NDEF_TNF_WELL_KNOWN   = 0x01, /**< NFC Forum well-known type [NFC RTD] */
    NDEF_TNF_MIME_MEDIA   = 0x02, /**< Media-type as defined in RFC 2046 */
    NDEF_TNF_ABSOLUTE_URI = 0x03, /**< Absolute URI as defined in RFC 3986 */
    NDEF_TNF_EXTERNAL     = 0x04, /**< NFC Forum external type [NFC RTD] */
    NDEF_TNF_UNKNOWN      = 0x05, /**< Unknown payload type */
    NDEF_TNF_UNCHANGED    = 0x06, /**< Unchanged (for chunked records) */
    NDEF_TNF_RESERVED     = 0x07, /**< Reserved */
} ndef_tnf_t;

/**
 * @brief URI Identifier Codes (NFC Forum URI RTD)
 */
typedef enum {
    NDEF_URI_NONE        = 0x00, /**< No prepending */
    NDEF_URI_HTTP_WWW    = 0x01, /**< http://www. */
    NDEF_URI_HTTPS_WWW   = 0x02, /**< https://www. */
    NDEF_URI_HTTP        = 0x03, /**< http:// */
    NDEF_URI_HTTPS       = 0x04, /**< https:// */
    NDEF_URI_TEL         = 0x05, /**< tel: */
    NDEF_URI_MAILTO      = 0x06, /**< mailto: */
    NDEF_URI_FTP_ANON    = 0x07, /**< ftp://anonymous:anonymous@ */
    NDEF_URI_FTP_FTP     = 0x08, /**< ftp://ftp. */
    NDEF_URI_FTPS        = 0x09, /**< ftps:// */
    NDEF_URI_SFTP        = 0x0A, /**< sftp:// */
    NDEF_URI_SMB         = 0x0B, /**< smb:// */
    NDEF_URI_NFS         = 0x0C, /**< nfs:// */
    NDEF_URI_FTP         = 0x0D, /**< ftp:// */
    NDEF_URI_DAV         = 0x0E, /**< dav:// */
    NDEF_URI_NEWS        = 0x0F, /**< news: */
    NDEF_URI_TELNET      = 0x10, /**< telnet:// */
    NDEF_URI_IMAP        = 0x11, /**< imap: */
    NDEF_URI_RTSP        = 0x12, /**< rtsp:// */
    NDEF_URI_URN         = 0x13, /**< urn: */
    NDEF_URI_POP         = 0x14, /**< pop: */
    NDEF_URI_SIP         = 0x15, /**< sip: */
    NDEF_URI_SIPS        = 0x16, /**< sips: */
    NDEF_URI_TFTP        = 0x17, /**< tftp: */
    NDEF_URI_BTSPP       = 0x18, /**< btspp:// */
    NDEF_URI_BTL2CAP     = 0x19, /**< btl2cap:// */
    NDEF_URI_BTGOEP      = 0x1A, /**< btgoep:// */
    NDEF_URI_TCPOBEX     = 0x1B, /**< tcpobex:// */
    NDEF_URI_IRDAOBEX    = 0x1C, /**< irdaobex:// */
    NDEF_URI_FILE        = 0x1D, /**< file:// */
    NDEF_URI_URN_EPC_ID  = 0x1E, /**< urn:epc:id: */
    NDEF_URI_URN_EPC_TAG = 0x1F, /**< urn:epc:tag: */
    NDEF_URI_URN_EPC_PAT = 0x20, /**< urn:epc:pat: */
    NDEF_URI_URN_EPC_RAW = 0x21, /**< urn:epc:raw: */
    NDEF_URI_URN_EPC     = 0x22, /**< urn:epc: */
    NDEF_URI_URN_NFC     = 0x23, /**< urn:nfc: */
} ndef_uri_code_t;

/**
 * @brief WiFi Authentication Types (WFA WSC)
 */
typedef enum {
    NDEF_WIFI_AUTH_OPEN              = 0x0001, /**< Open (no security) */
    NDEF_WIFI_AUTH_WPA_PERSONAL      = 0x0002, /**< WPA-PSK */
    NDEF_WIFI_AUTH_SHARED            = 0x0004, /**< Shared key (WEP) */
    NDEF_WIFI_AUTH_WPA_ENTERPRISE    = 0x0008, /**< WPA-Enterprise */
    NDEF_WIFI_AUTH_WPA2_PERSONAL     = 0x0010, /**< WPA2-PSK */
    NDEF_WIFI_AUTH_WPA2_ENTERPRISE   = 0x0020, /**< WPA2-Enterprise */
    NDEF_WIFI_AUTH_WPA_WPA2_PERSONAL = 0x0022, /**< WPA/WPA2-PSK mixed */
} ndef_wifi_auth_t;

/**
 * @brief WiFi Encryption Types (WFA WSC)
 */
typedef enum {
    NDEF_WIFI_ENCR_NONE     = 0x0001, /**< No encryption */
    NDEF_WIFI_ENCR_WEP      = 0x0002, /**< WEP */
    NDEF_WIFI_ENCR_TKIP     = 0x0004, /**< TKIP */
    NDEF_WIFI_ENCR_AES      = 0x0008, /**< AES (CCMP) */
    NDEF_WIFI_ENCR_AES_TKIP = 0x000C, /**< AES + TKIP mixed */
} ndef_wifi_encr_t;

/**
 * @brief Supported NFC tag types for this demo
 */
typedef enum {
    NFC_TAG_TYPE_URI,          /**< Simple URL/URI tag */
    NFC_TAG_TYPE_URI_AAR,      /**< URL with Android Application Record */
    NFC_TAG_TYPE_WIFI,         /**< WiFi configuration tag */
    NFC_TAG_TYPE_TEXT,         /**< Plain text tag */
    NFC_TAG_TYPE_VCARD,        /**< Contact information (vCard) */
    NFC_TAG_TYPE_SMART_POSTER, /**< Smart Poster (URL + title) */
} nfc_tag_type_t;

/**
 * @brief NDEF Message Builder Context
 */
typedef struct {
    uint8_t *buffer;             /**< Output buffer */
    size_t   buffer_size;        /**< Buffer size */
    size_t   offset;             /**< Current write offset */
    size_t   last_record_offset; /**< Offset of last record (for ME flag update) */
    int      record_count;       /**< Number of records added */
    bool     message_open;       /**< Message is open for adding records */
} ndef_message_t;

/**
 * @brief URI Record Configuration
 */
typedef struct {
    const char *uri; /**< Full URI string (prefix will be auto-detected) */
} ndef_uri_config_t;

/**
 * @brief Text Record Configuration
 */
typedef struct {
    const char *text;     /**< Text content (UTF-8) */
    const char *lang;     /**< Language code (e.g., "en", "zh") */
    bool        is_utf16; /**< Use UTF-16 encoding (default: UTF-8) */
} ndef_text_config_t;

/**
 * @brief WiFi Configuration Record
 */
typedef struct {
    const char      *ssid;      /**< WiFi SSID (max 32 bytes) */
    const char      *password;  /**< WiFi password (max 64 bytes) */
    ndef_wifi_auth_t auth_type; /**< Authentication type */
    ndef_wifi_encr_t encr_type; /**< Encryption type */
} ndef_wifi_config_t;

/**
 * @brief Android Application Record (AAR) Configuration
 */
typedef struct {
    const char *package_name; /**< Android package name (e.g., "com.android.chrome") */
} ndef_aar_config_t;

/**
 * @brief Smart Poster Record Configuration
 */
typedef struct {
    const char *uri;        /**< URI (required) */
    const char *title;      /**< Title text (optional, can be NULL) */
    const char *title_lang; /**< Title language (default: "en") */
    uint8_t     action;     /**< Recommended action (0=default, 1=save, 2=edit, 3=open) */
} ndef_smart_poster_config_t;

/**
 * @brief vCard Record Configuration
 */
typedef struct {
    const char *name;    /**< Full name (FN) */
    const char *phone;   /**< Phone number (TEL) */
    const char *email;   /**< Email address */
    const char *org;     /**< Organization */
    const char *title;   /**< Job title */
    const char *url;     /**< Website URL */
    const char *address; /**< Address (ADR) */
    const char *note;    /**< Note */
} ndef_vcard_config_t;

/**
 * @brief MIME Record Configuration
 */
typedef struct {
    const char    *mime_type;   /**< MIME type string (e.g., "text/plain") */
    const uint8_t *payload;     /**< Payload data */
    size_t         payload_len; /**< Payload length */
} ndef_mime_config_t;

/**
 * @brief External Type Record Configuration
 */
typedef struct {
    const char    *domain;      /**< Domain name (e.g., "example.com") */
    const char    *type;        /**< Type name (e.g., "mytype") */
    const uint8_t *payload;     /**< Payload data */
    size_t         payload_len; /**< Payload length */
} ndef_external_config_t;

/**
 * @brief Combined tag configuration
 */
typedef struct {
    nfc_tag_type_t type;

    union {
        // URI configuration
        struct {
            const char *uri;
            const char *aar_package; // NULL to disable AAR
        } uri;

        // WiFi configuration
        struct {
            const char      *ssid;
            const char      *password;
            ndef_wifi_auth_t auth_type;
            ndef_wifi_encr_t encr_type;
        } wifi;

        // Text configuration
        struct {
            const char *text;
            const char *lang;
        } text;

        // vCard configuration
        ndef_vcard_config_t vcard;

        // Smart Poster configuration
        struct {
            const char *uri;
            const char *title;
        } smart_poster;
    };
} nfc_tag_config_t;

/***********************************************************
********************function declaration********************
***********************************************************/

/**
 * @brief Add raw NDEF record to message
 *
 * @param msg NDEF message context
 * @param tnf Type Name Format
 * @param type Type string (NULL for TNF_EMPTY or TNF_UNKNOWN)
 * @param type_len Type length
 * @param payload Payload data
 * @param payload_len Payload length
 * @param id Record ID (can be NULL)
 * @param id_len ID length
 * @return 0 on success, negative error code on failure
 */
int ndef_add_raw_record(ndef_message_t *msg, ndef_tnf_t tnf, const uint8_t *type, size_t type_len,
                        const uint8_t *payload, size_t payload_len, const uint8_t *id, size_t id_len);

/**
 * @brief Tag Type 2 Memory Structure
 */
typedef struct {
    uint8_t *buffer;      /**< Output buffer */
    size_t   buffer_size; /**< Buffer size */
    size_t   data_len;    /**< Actual data length */
} ndef_tag2_t;

/**
 * @brief Initialize Tag Type 2 structure
 *
 * @param tag Tag Type 2 context
 * @param buffer Output buffer (minimum 64 bytes recommended)
 * @param buffer_size Buffer size
 * @return 0 on success, negative error code on failure
 */
int ndef_tag2_init(ndef_tag2_t *tag, uint8_t *buffer, size_t buffer_size);

/**
 * @brief Write NDEF message to Tag Type 2 memory layout
 *
 * This function creates a complete Tag Type 2 memory area including:
 * - Blocks 0-1: UID area (set to 0 for emulation)
 * - Block 2: Static lock bytes
 * - Block 3: Capability Container (CC)
 * - Block 4+: NDEF message TLV
 * - Terminator TLV
 *
 * @param tag Tag Type 2 context
 * @param ndef_data NDEF message data
 * @param ndef_len NDEF message length
 * @return Total bytes written on success, negative error code on failure
 */
int ndef_tag2_write(ndef_tag2_t *tag, const uint8_t *ndef_data, size_t ndef_len);

/**
 * @brief Get Tag Type 2 data pointer
 *
 * @param tag Tag Type 2 context
 * @return Pointer to tag data
 */
uint8_t *ndef_tag2_get_data(ndef_tag2_t *tag);

/**
 * @brief Get Tag Type 2 data length
 *
 * @param tag Tag Type 2 context
 * @return Data length in bytes
 */
size_t ndef_tag2_get_length(const ndef_tag2_t *tag);

/**
 * @brief Create a simple URI tag (one-shot function)
 *
 * @param tag Tag Type 2 context
 * @param uri URI string
 * @return Total bytes on success, negative error code on failure
 */
int ndef_create_uri_tag(ndef_tag2_t *tag, const char *uri);

/**
 * @brief Create a URI tag with AAR (prevents app interception)
 *
 * @param tag Tag Type 2 context
 * @param uri URI string
 * @param package_name Android package name (or NULL to skip AAR)
 * @return Total bytes on success, negative error code on failure
 */
int ndef_create_uri_aar_tag(ndef_tag2_t *tag, const char *uri, const char *package_name);

/**
 * @brief Create a WiFi configuration tag (one-shot function)
 *
 * @param tag Tag Type 2 context
 * @param ssid WiFi SSID
 * @param password WiFi password
 * @param auth_type Authentication type
 * @param encr_type Encryption type
 * @return Total bytes on success, negative error code on failure
 */
int ndef_create_wifi_tag(ndef_tag2_t *tag, const char *ssid, const char *password, ndef_wifi_auth_t auth_type,
                         ndef_wifi_encr_t encr_type);

/**
 * @brief Create a text tag (one-shot function)
 *
 * @param tag Tag Type 2 context
 * @param text Text content
 * @param lang Language code (e.g., "en", "zh")
 * @return Total bytes on success, negative error code on failure
 */
int ndef_create_text_tag(ndef_tag2_t *tag, const char *text, const char *lang);

/**
 * @brief Create a vCard tag (one-shot function)
 *
 * @param tag Tag Type 2 context
 * @param config vCard configuration
 * @return Total bytes on success, negative error code on failure
 */
int ndef_create_vcard_tag(ndef_tag2_t *tag, const ndef_vcard_config_t *config);

/**
 * @brief Create a Smart Poster tag (one-shot function)
 *
 * @param tag Tag Type 2 context
 * @param uri URI string
 * @param title Title (can be NULL)
 * @return Total bytes on success, negative error code on failure
 */
int ndef_create_smart_poster_tag(ndef_tag2_t *tag, const char *uri, const char *title);

#ifdef __cplusplus
}
#endif

#endif /* _NDEF_H_ */

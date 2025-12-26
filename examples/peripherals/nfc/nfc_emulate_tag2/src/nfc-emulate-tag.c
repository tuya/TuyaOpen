/**
 * @file nfc-emulate-tag.c
 * @brief NFC Tag Emulation Functions
 * @version 0.1
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */
// Tuya platform includes
#include "tal_api.h"

#include <stdlib.h>
#include <string.h>

#include "nfc.h"
#include "nfc-emulation.h"
#include "ndef.h"
#include "platform_compat.h"

#ifdef NFC_DEBUG
#include <stdio.h>
#include "nfc-utils.h"
#endif

/**
 * @brief I/O handler for NFC Forum Tag Type 2 emulation
 */
static int tag2_io_handler(struct nfc_emulator *emulator, const uint8_t *data_in, const size_t data_in_len,
                           uint8_t *data_out, const size_t data_out_len)
{
    int      res        = 0;
    uint8_t *tag_memory = (uint8_t *)(emulator->user_data);

    // Log incoming command
#ifdef NFC_DEBUG
    printf("  In: ");
    print_hex(data_in, data_in_len);
#endif

    switch (data_in[0]) {
    case CMD_READ:
        // READ command: return 16 bytes (4 blocks) starting from specified block
        if (data_out_len >= 16) {
            uint8_t block = data_in[1];
            memcpy(data_out, tag_memory + (block * 4), 16);
            res = 16;
            PR_DEBUG("READ block %d", block);
        } else {
            res = -ENOSPC;
        }
        break;

    case CMD_HALT:
        PR_NOTICE("HALT - Phone disconnected");
        res = -ECONNABORTED;
        break;

    case CMD_WRITE:
        // WRITE command: write 4 bytes to specified block
        // For read-only emulation, we ignore writes
        PR_DEBUG("WRITE to block %d (ignored - read-only)", data_in[1]);
        res = 0; // ACK
        break;

    default:
        PR_DEBUG("Unknown command: 0x%02X", data_in[0]);
        res = -ENOTSUP;
        break;
    }

#ifdef NFC_DEBUG
    if (res > 0) {
        printf("  Out: ");
        print_hex(data_out, res);
    }
#endif

    return res;
}

/*============================================================================
 * Tag Generation Functions
 *============================================================================*/

/**
 * @brief Generate tag memory based on configuration
 *
 * @param config Tag configuration
 * @param tag_memory Output: tag memory buffer
 * @param buffer_size Buffer size
 * @return Bytes written on success, negative error on failure
 */
static int generate_tag_memory(const nfc_tag_config_t *config, uint8_t *tag_memory, size_t buffer_size)
{
    ndef_tag2_t tag;
    int         ret;

    ret = ndef_tag2_init(&tag, tag_memory, buffer_size);
    if (ret < 0) {
        PR_ERR("Failed to init tag2: %d", ret);
        return ret;
    }

    switch (config->type) {
    case NFC_TAG_TYPE_URI:
        PR_NOTICE("Creating URI tag: %s", config->uri.uri);
        ret = ndef_create_uri_tag(&tag, config->uri.uri);
        break;

    case NFC_TAG_TYPE_URI_AAR:
        PR_NOTICE("Creating URI+AAR tag: %s (package: %s)", config->uri.uri, config->uri.aar_package);
        ret = ndef_create_uri_aar_tag(&tag, config->uri.uri, config->uri.aar_package);
        break;

    case NFC_TAG_TYPE_WIFI:
        PR_NOTICE("Creating WiFi tag: SSID=%s, Auth=0x%04X, Encr=0x%04X", config->wifi.ssid, config->wifi.auth_type,
                  config->wifi.encr_type);
        ret = ndef_create_wifi_tag(&tag, config->wifi.ssid, config->wifi.password, config->wifi.auth_type,
                                   config->wifi.encr_type);
        break;

    case NFC_TAG_TYPE_TEXT:
        PR_NOTICE("Creating Text tag: \"%s\" (lang=%s)", config->text.text, config->text.lang);
        ret = ndef_create_text_tag(&tag, config->text.text, config->text.lang);
        break;

    case NFC_TAG_TYPE_VCARD:
        PR_NOTICE("Creating vCard tag: %s", config->vcard.name);
        ret = ndef_create_vcard_tag(&tag, &config->vcard);
        break;

    case NFC_TAG_TYPE_SMART_POSTER:
        PR_NOTICE("Creating Smart Poster tag: %s (%s)", config->smart_poster.uri, config->smart_poster.title);
        ret = ndef_create_smart_poster_tag(&tag, config->smart_poster.uri, config->smart_poster.title);
        break;

    default:
        PR_ERR("Unknown tag type: %d", config->type);
        return OPRT_INVALID_PARM;
    }

    if (ret < 0) {
        PR_ERR("Failed to create tag: %d", ret);
        return ret;
    }

    return ret;
}

/*============================================================================
 * Main Emulation Function
 *============================================================================*/

/**
 * @brief Emulate an NFC tag with the given configuration
 *
 * @param config Tag configuration
 * @return OPRT_OK on success, OPRT_COM_ERROR on error
 */
OPERATE_RET nfc_emulate_tag(const nfc_tag_config_t *config)
{
    nfc_device  *pnd        = NULL;
    nfc_context *context    = NULL;
    uint8_t     *tag_memory = NULL;
    OPERATE_RET  ret        = OPRT_OK;

    // Allocate tag memory (512 bytes should be enough for most cases)
    size_t buffer_size = 512;
    tag_memory         = tal_psram_malloc(buffer_size);
    if (tag_memory == NULL) {
        PR_ERR("Failed to allocate tag memory");
        return OPRT_COM_ERROR;
    }

    // Generate tag data
    int data_len = generate_tag_memory(config, tag_memory, buffer_size);
    if (data_len < 0) {
        PR_ERR("Failed to generate tag data: %d", data_len);
        goto cleanup;
    }

    // Initialize libnfc
    nfc_init(&context);
    if (context == NULL) {
        PR_ERR("Failed to init libnfc context");
        goto cleanup;
    }

    // Open NFC device
    const char *connstring = "pn532_uart:UART2";
    pnd                    = nfc_open(context, connstring);
    if (pnd == NULL) {
        PR_ERR("Failed to open NFC device: %s", connstring);
        goto cleanup;
    }

    PR_NOTICE("NFC device: %s", nfc_device_get_name(pnd));
    PR_NOTICE("Tag data ready (%d bytes)", data_len);
    PR_NOTICE("Please touch with a phone...");

    // Set up emulation target
    nfc_target nt = {
        .nm =
            {
                .nmt = NMT_ISO14443A,
                .nbr = NBR_UNDEFINED,
            },
        .nti =
            {
                .nai =
                    {
                        .abtAtqa  = {0x00, 0x04},
                        .abtUid   = {0x08, 0x00, 0xB0, 0x0B}, // Fixed UID for demo
                        .btSak    = 0x00,
                        .szUidLen = 4,
                        .abtAts   = {0x00},
                        .szAtsLen = 0,
                    },
            },
    };

    // Set up state machine
    struct nfc_emulation_state_machine state_machine = {.io = tag2_io_handler};

    // Create emulator
    struct nfc_emulator emulator = {
        .target        = &nt,
        .state_machine = &state_machine,
        .user_data     = tag_memory,
    };

    // Run emulation
    if (nfc_emulate_target(pnd, &emulator, 0) != OPRT_OK) {
        PR_ERR("nfc emulate target failed");
        goto cleanup;
    }

    PR_NOTICE("Emulation completed successfully");
    ret = OPRT_OK;

cleanup:
    if (pnd)
        nfc_close(pnd);
    if (context)
        nfc_exit(context);
    if (tag_memory)
        tal_psram_free(tag_memory);

    return ret;
}

/*============================================================================
 * Demo Main Functions (called from nfc_emulate_main.c)
 *============================================================================*/

/**
 * @brief Demo: URI tag with optional AAR
 */
int nfc_demo_uri_tag(const char *uri, const char *aar_package)
{
    PR_NOTICE("=== URI Tag Demo ===");
    nfc_tag_config_t config = {.type = (aar_package != NULL) ? NFC_TAG_TYPE_URI_AAR : NFC_TAG_TYPE_URI,
                               .uri  = {.uri = uri, .aar_package = aar_package}};

    return nfc_emulate_tag(&config);
}

/**
 * @brief Demo: WiFi configuration tag
 */
int nfc_demo_wifi_tag(const char *ssid, const char *password)
{
    PR_NOTICE("=== WiFi Configuration Tag Demo ===");
    nfc_tag_config_t config = {.type = NFC_TAG_TYPE_WIFI,
                               .wifi = {.ssid      = ssid,
                                        .password  = password,
                                        .auth_type = NDEF_WIFI_AUTH_WPA_WPA2_PERSONAL,
                                        .encr_type = NDEF_WIFI_ENCR_AES_TKIP}};

    return nfc_emulate_tag(&config);
}

/**
 * @brief Demo: Text tag
 */
int nfc_demo_text_tag(const char *text, const char *lang)
{
    PR_NOTICE("=== Text Tag Demo ===");
    nfc_tag_config_t config = {.type = NFC_TAG_TYPE_TEXT, .text = {.text = text, .lang = lang ? lang : "en"}};

    return nfc_emulate_tag(&config);
}

/**
 * @brief Demo: vCard tag
 */
int nfc_demo_vcard_tag(void)
{
    PR_NOTICE("=== vCard Tag Demo ===");
    nfc_tag_config_t config = {.type  = NFC_TAG_TYPE_VCARD,
                               .vcard = {.name    = "Tuya",
                                         .phone   = "+86-12345678",
                                         .email   = "support@tuya.com",
                                         .org     = "Tuya Inc.",
                                         .title   = "Customer Support",
                                         .url     = "https://www.tuya.com",
                                         .address = "China",
                                         .note    = "IoT Platform"}};

    return nfc_emulate_tag(&config);
}

/**
 * @brief Demo: Smart Poster tag
 */
int nfc_demo_smart_poster_tag(const char *uri, const char *title)
{
    PR_NOTICE("=== Smart Poster Tag Demo ===");
    nfc_tag_config_t config = {.type = NFC_TAG_TYPE_SMART_POSTER, .smart_poster = {.uri = uri, .title = title}};

    return nfc_emulate_tag(&config);
}

/*============================================================================
 * Combined Demo (Interactive Menu)
 *============================================================================*/

/**
 * @brief Run all demos sequentially
 */
int nfc_demo_all(void)
{
    PR_NOTICE("========================================");
    PR_NOTICE("Starting All NFC Tag Demos");
    PR_NOTICE("========================================");

    // Demo 0: Text Tag
    PR_DEBUG("[Demo 0/7] Text Tag");
    nfc_demo_text_tag("Hello, Tuya !", "en");

    // Demo 1: Simple URI
    PR_DEBUG("[Demo 1/7] URI Tag");
    nfc_demo_uri_tag("https://tuyaopen.ai", NULL);

    // Demo 2: URI with AAR
    PR_DEBUG("[Demo 2/7] URI + AAR Tag");
    nfc_demo_uri_tag("https://tuyaopen.ai", "com.android.browser");

    // Demo 3: TEL
    PR_DEBUG("[Demo 3/7] TEL Tag");
    nfc_demo_uri_tag("tel:+86123456789", NULL);

    // Demo 4: MAIL
    PR_DEBUG("[Demo 4/7] MAIL Tag");
    nfc_demo_uri_tag("mailto:example@example.com", NULL);

    // Demo 5: WiFi Config
    PR_DEBUG("[Demo 5/7] WiFi Config Tag");
    nfc_demo_wifi_tag("TuyaOpen", "12345678");

    // Demo 6: vCard
    PR_DEBUG("[Demo 6/7] vCard Tag");
    nfc_demo_vcard_tag();

    // Demo 7: Smart Poster
    PR_DEBUG("[Demo 7/7] Smart Poster Tag");
    nfc_demo_smart_poster_tag("https://tuyaopen.ai", "TuyaOpen AI Platform");

    PR_NOTICE("========================================");
    PR_NOTICE("All demos completed!");
    PR_NOTICE("========================================");

    return OPRT_OK;
}

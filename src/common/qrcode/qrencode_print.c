/**
 * @file qrencode_print.c
 * @brief Provides functions to encode strings into QR codes and print them in
 * UTF-8 format using QR-Code-generator library.
 *
 * This file includes the implementation of functions to encode input strings
 * into QR codes using the QR-Code-generator library and print the resulting QR codes
 * to the console or a file in UTF-8 format.
 *
 * Key functionalities included:
 * - Encoding input strings into QR codes with configurable settings.
 * - Printing QR codes in UTF-8 format with customizable appearance options.
 * - Functions to invert QR code colors and adjust margins for better visibility.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 *
 */

#include "qrcodegen.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

// Configuration parameters
static const int margin = 3;
static const enum qrcodegen_Ecc level = qrcodegen_Ecc_LOW;

/*
 * Maximum QR-code module count is 177 (version 40).  With a margin of 3 on
 * each side the widest line has 183 modules.  Each UTF-8 block character is
 * 3 bytes, so the worst-case line is 183*3 + 2 (\r\n) + 1 (NUL) = 552 bytes.
 */
#define QR_LINE_BUF_SIZE 640

/**
 * @brief Append margin rows into a buffer
 */
static void buf_margin(int realwidth, const char *full, char *buf, int *pos, int buf_size) {
    for (int y = 0; y < margin / 2; y++) {
        for (int x = 0; x < realwidth && *pos < buf_size - 4; x++)
            *pos += snprintf(buf + *pos, buf_size - *pos, "%s", full);
        *pos += snprintf(buf + *pos, buf_size - *pos, "\r\n");
    }
}

/**
 * @brief Render QR code into a buffer (caller must free)
 *
 * The entire QR code is rendered into a single heap-allocated buffer so that
 * the caller can emit it with one fputs() call.  This makes the output atomic
 * and prevents interleaving with log messages from other threads.
 *
 * @return heap-allocated NUL-terminated string, or NULL on allocation failure
 */
static char *buildQrCodeBuffer(const uint8_t qrcode[], int invert) {
    int size = qrcodegen_getSize(qrcode);
    int realwidth = size + margin * 2;
    int lines = (size + 1) / 2 + margin;  /* data rows (2-per-line) + margins */
    int buf_size = lines * QR_LINE_BUF_SIZE;

    char *buf = (char *)malloc(buf_size);
    if (!buf)
        return NULL;
    int pos = 0;

    // UTF-8 block characters
    const char *empty   = " ";
    const char *lowhalf = "\342\226\204";  // ▄
    const char *uphalf  = "\342\226\200";  // ▀
    const char *full    = "\342\226\210";  // █

    if (invert) {
        const char *tmp;
        tmp = empty;   empty   = full;  full   = tmp;
        tmp = lowhalf; lowhalf = uphalf; uphalf = tmp;
    }

    // Top margin
    buf_margin(realwidth, full, buf, &pos, buf_size);

    // QR Code data — 2 module-rows per text line (half-block compaction)
    for (int y = 0; y < size; y += 2) {
        // Left margin
        for (int x = 0; x < margin && pos < buf_size - 4; x++)
            pos += snprintf(buf + pos, buf_size - pos, "%s", full);

        // Modules
        for (int x = 0; x < size && pos < buf_size - 4; x++) {
            bool m1 = qrcodegen_getModule(qrcode, x, y);
            bool m2 = (y + 1 < size) ? qrcodegen_getModule(qrcode, x, y + 1) : false;
            if (m1)
                pos += snprintf(buf + pos, buf_size - pos, "%s", m2 ? empty : lowhalf);
            else
                pos += snprintf(buf + pos, buf_size - pos, "%s", m2 ? uphalf : full);
        }

        // Right margin + newline
        for (int x = 0; x < margin && pos < buf_size - 4; x++)
            pos += snprintf(buf + pos, buf_size - pos, "%s", full);
        pos += snprintf(buf + pos, buf_size - pos, "\r\n");
    }

    // Odd height — last single row
    if (size % 2 == 1) {
        for (int x = 0; x < margin && pos < buf_size - 4; x++)
            pos += snprintf(buf + pos, buf_size - pos, "%s", full);
        for (int x = 0; x < size && pos < buf_size - 4; x++) {
            bool m = qrcodegen_getModule(qrcode, x, size - 1);
            pos += snprintf(buf + pos, buf_size - pos, "%s", m ? lowhalf : full);
        }
        for (int x = 0; x < margin && pos < buf_size - 4; x++)
            pos += snprintf(buf + pos, buf_size - pos, "%s", full);
        pos += snprintf(buf + pos, buf_size - pos, "\r\n");
    }

    // Bottom margin
    buf_margin(realwidth, full, buf, &pos, buf_size);

    buf[pos] = '\0';
    return buf;
}

/**
 * @brief Main interface function to generate and print QR code from string
 * 
 * @param string The input string to encode
 * @param fputs Function pointer for output
 * @param invert Whether to invert the QR code colors
 */
void qrcode_string_output(const char *string, void (*fputs)(const char *str), int invert)
{
    /* Use heap allocation instead of stack to avoid stack overflow on
     * memory-constrained RTOS platforms (e.g. K230 RT-Smart). */
    uint8_t *qrcode     = (uint8_t *)malloc(qrcodegen_BUFFER_LEN_MAX);
    uint8_t *tempBuffer = (uint8_t *)malloc(qrcodegen_BUFFER_LEN_MAX);

    if (!qrcode || !tempBuffer) {
        fputs("Error: Failed to allocate QR code buffers\r\n");
        free(qrcode);
        free(tempBuffer);
        return;
    }

    // Generate QR Code
    bool ok = qrcodegen_encodeText(string, tempBuffer, qrcode,
                                    level,                    // Error correction level
                                    qrcodegen_VERSION_MIN,    // Min version (1)
                                    qrcodegen_VERSION_MAX,    // Max version (40)
                                    qrcodegen_Mask_AUTO,      // Automatic mask selection
                                    true);                    // Boost ECC if possible

    if (!ok) {
        // If text encoding fails, try binary encoding
        size_t len = strlen(string);
        memcpy(tempBuffer, string, len);
        ok = qrcodegen_encodeBinary(tempBuffer, len, qrcode,
                                     level,
                                     qrcodegen_VERSION_MIN,
                                     qrcodegen_VERSION_MAX,
                                     qrcodegen_Mask_AUTO,
                                     true);
    }

    if (ok) {
        /*
         * Build the ENTIRE QR code into one heap buffer, then emit it with a
         * single fputs() call.  This makes the output effectively atomic —
         * other threads cannot interleave their log messages in the middle of
         * the QR code block.
         *
         * Peak extra heap usage: ~1.5 KB for a version-3 QR code (typical for
         * URL strings), up to ~100 KB for version 40.
         */
        char *qrbuf = buildQrCodeBuffer(qrcode, invert);
        if (qrbuf) {
            fputs(qrbuf);
            free(qrbuf);
        } else {
            fputs("Error: Failed to allocate QR code output buffer\r\n");
        }
    } else {
        fputs("Error: Failed to generate QR code - data too long\r\n");
    }

    free(qrcode);
    free(tempBuffer);
}
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

// Function pointer for output
static void (*qrcode_fputs)(const char *str);

/**
 * @brief Write margin rows for QR code
 */
static void writeUTF8_margin(int realwidth, const char *white, const char *reset, const char *full) {
    int x, y;
    
    for (y = 0; y < margin / 2; y++) {
        qrcode_fputs(white);
        for (x = 0; x < realwidth; x++)
            qrcode_fputs(full);
        qrcode_fputs(reset);
        qrcode_fputs("\r\n");
    }
}

/**
 * @brief Print QR code using UTF-8 block characters
 */
static void printQrCode(const uint8_t qrcode[], int invert) {
    int size = qrcodegen_getSize(qrcode);
    int realwidth = size + margin * 2;
    
    // UTF-8 block characters
    const char *empty = " ";
    const char *lowhalf = "\342\226\204";  // ▄
    const char *uphalf = "\342\226\200";   // ▀
    const char *full = "\342\226\210";     // █
    
    // ANSI color codes (not used in this implementation)
    const char *white = "";
    const char *reset = "";
    
    // Invert colors if requested
    if (invert) {
        const char *tmp;
        tmp = empty;
        empty = full;
        full = tmp;
        
        tmp = lowhalf;
        lowhalf = uphalf;
        uphalf = tmp;
    }
    
    // Top margin
    writeUTF8_margin(realwidth, white, reset, full);
    
    // QR Code data (process 2 rows at a time for compact output)
    for (int y = 0; y < size; y += 2) {
        qrcode_fputs(white);
        
        // Left margin
        for (int x = 0; x < margin; x++) {
            qrcode_fputs(full);
        }
        
        // QR Code modules
        for (int x = 0; x < size; x++) {
            bool module1 = qrcodegen_getModule(qrcode, x, y);
            bool module2 = (y + 1 < size) ? qrcodegen_getModule(qrcode, x, y + 1) : false;
            
            if (module1) {
                if (module2) {
                    qrcode_fputs(empty);
                } else {
                    qrcode_fputs(lowhalf);
                }
            } else {
                if (module2) {
                    qrcode_fputs(uphalf);
                } else {
                    qrcode_fputs(full);
                }
            }
        }
        
        // Right margin
        for (int x = 0; x < margin; x++) {
            qrcode_fputs(full);
        }
        
        qrcode_fputs(reset);
        qrcode_fputs("\r\n");
    }
    
    // Handle odd height (last row if size is odd)
    if (size % 2 == 1) {
        qrcode_fputs(white);
        
        // Left margin
        for (int x = 0; x < margin; x++) {
            qrcode_fputs(full);
        }
        
        // Last row modules
        for (int x = 0; x < size; x++) {
            bool module = qrcodegen_getModule(qrcode, x, size - 1);
            if (module) {
                qrcode_fputs(lowhalf);
            } else {
                qrcode_fputs(full);
            }
        }
        
        // Right margin
        for (int x = 0; x < margin; x++) {
            qrcode_fputs(full);
        }
        
        qrcode_fputs(reset);
        qrcode_fputs("\r\n");
    }
    
    // Bottom margin
    writeUTF8_margin(realwidth, white, reset, full);
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
    qrcode_fputs = fputs;
    
    // Allocate buffers
    uint8_t qrcode[qrcodegen_BUFFER_LEN_MAX];
    uint8_t tempBuffer[qrcodegen_BUFFER_LEN_MAX];
    
    // Generate QR Code
    bool ok = qrcodegen_encodeText(string, tempBuffer, qrcode,
                                    level,                    // Error correction level
                                    qrcodegen_VERSION_MIN,    // Min version (1)
                                    qrcodegen_VERSION_MAX,    // Max version (40)
                                    qrcodegen_Mask_AUTO,      // Automatic mask selection
                                    true);                    // Boost ECC if possible
    
    if (ok) {
        printQrCode(qrcode, invert);
    } else {
        // If text encoding fails, try binary encoding
        size_t len = strlen(string);
        memcpy(tempBuffer, string, len);
        
        ok = qrcodegen_encodeBinary(tempBuffer, len, qrcode,
                                     level,
                                     qrcodegen_VERSION_MIN,
                                     qrcodegen_VERSION_MAX,
                                     qrcodegen_Mask_AUTO,
                                     true);
        
        if (ok) {
            printQrCode(qrcode, invert);
        } else {
            qrcode_fputs("Error: Failed to generate QR code - data too long\r\n");
        }
    }
}
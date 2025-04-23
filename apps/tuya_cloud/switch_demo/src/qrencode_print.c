/**
 * @file qrencode_print.c
 * @brief Provides functions to encode strings into QR codes and print them in
 * UTF-8 format.
 *
 * This file includes the implementation of functions to encode input strings
 * into QR codes using the libqrencode library and print the resulting QR codes
 * to the console or a file in UTF-8 format. It supports various QR code
 * versions, error correction levels, and encoding modes. Additionally, it
 * allows for the customization of the QR code output, such as setting the
 * margin size, choosing between micro QR codes and regular QR codes, and
 * inverting the QR code colors for better visibility on different backgrounds.
 * The file also demonstrates handling of UTF-8 characters for QR code printing,
 * making it suitable for a wide range of applications including terminal-based
 * tools and utilities.
 *
 * Key functionalities included:
 * - Encoding input strings into QR codes with configurable settings.
 * - Printing QR codes in UTF-8 format with customizable appearance options.
 * - Support for both micro and regular QR codes.
 * - Functions to invert QR code colors and adjust margins for better
 * visibility.
 *
 * This implementation is designed to be used in applications that require QR
 * code generation and printing, offering a flexible and easy-to-use interface
 * for QR code encoding and visualization.
 *
 * @copyright Copyright (c) 2021-2024 Tuya Inc. All Rights Reserved.
 *
 */

#include "qrencode.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int casesensitive = 1;
static int eightbit = 0;
static int version = 0;
// static int size = 1;
static int margin = 3;
static int micro = 0;
static QRecLevel level = QR_ECLEVEL_L;
static QRencodeMode hint = QR_MODE_8;

void (*qrcode_fputs)(const char *str);

static QRcode *encode(const unsigned char *intext, int length)
{
    QRcode *code;

    if (micro) {
        if (eightbit) {
            code = QRcode_encodeDataMQR(length, intext, version, level);
        } else {
            code = QRcode_encodeStringMQR((char *)intext, version, level, hint, casesensitive);
        }
    } else if (eightbit) {
        code = QRcode_encodeData(length, intext, version, level);
    } else {
        code = QRcode_encodeString((char *)intext, version, level, hint, casesensitive);
    }

    return code;
}

static void writeUTF8_margin(int realwidth, const char *white, const char *reset, const char *full)
{
    int x, y;

    for (y = 0; y < margin / 2; y++) {
        qrcode_fputs(white);
        for (x = 0; x < realwidth; x++)
            qrcode_fputs(full);
        qrcode_fputs(reset);
        qrcode_fputs("\r\n");
    }
}

static int writeUTF8(const QRcode *qrcode, const char *outfile, int use_ansi, int invert)
{
    int x, y;
    int realwidth;
    const char *white, *reset;
    const char *empty, *lowhalf, *uphalf, *full;

    empty = " ";
    lowhalf = "\342\226\204";
    uphalf = "\342\226\200";
    full = "\342\226\210";

    if (invert) {
        const char *tmp;

        tmp = empty;
        empty = full;
        full = tmp;

        tmp = lowhalf;
        lowhalf = uphalf;
        uphalf = tmp;
    }

    if (use_ansi) {
        if (use_ansi == 2) {
            white = "\033[38;5;231m\033[48;5;16m";
        } else {
            white = "\033[40;37;1m";
        }
        reset = "\033[0m";
    } else {
        white = "";
        reset = "";
    }

    realwidth = (qrcode->width + margin * 2);

    /* top margin */
    writeUTF8_margin(realwidth, white, reset, full);

    /* data */
    for (y = 0; y < qrcode->width; y += 2) {
        unsigned char *row1, *row2;
        row1 = qrcode->data + y * qrcode->width;
        row2 = row1 + qrcode->width;

        qrcode_fputs(white);

        for (x = 0; x < margin; x++) {
            qrcode_fputs(full);
        }

        for (x = 0; x < qrcode->width; x++) {
            if (row1[x] & 1) {
                if (y < qrcode->width - 1 && row2[x] & 1) {
                    qrcode_fputs(empty);
                } else {
                    qrcode_fputs(lowhalf);
                }
            } else if (y < qrcode->width - 1 && row2[x] & 1) {
                qrcode_fputs(uphalf);
            } else {
                qrcode_fputs(full);
            }
        }

        for (x = 0; x < margin; x++)
            qrcode_fputs(full);

        qrcode_fputs(reset);
        qrcode_fputs("\r\n");
    }

    /* bottom margin */
    writeUTF8_margin(realwidth, white, reset, full);

    return 0;
}

void example_qrcode_string(const char *string, void (*fputs)(const char *str), int invert)
{
    QRcode *qrcode = encode((const unsigned char *)string, strlen(string));
    qrcode_fputs = fputs;
    writeUTF8(qrcode, NULL, 0, invert);
    QRcode_free(qrcode);
}

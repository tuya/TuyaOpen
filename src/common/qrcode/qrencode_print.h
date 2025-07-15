/**
 * @file qrencode_print.h
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
#ifndef __QRCODE_PRINT_H__
#define __QRCODE_PRINT_H__

#include "qrcodegen.h"

/**
 * @brief Main interface function to generate and print QR code from string
 * 
 * @param string The input string to encode
 * @param fputs Function pointer for output
 * @param invert Whether to invert the QR code colors
 */
void qrcode_string_output(const char *string, void (*fputs)(const char *str), int invert);

#endif
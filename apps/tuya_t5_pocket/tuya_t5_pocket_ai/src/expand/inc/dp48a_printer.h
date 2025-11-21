/**
 * @file dp48a_printer.h
 * @brief Implementation of DP48A printer functions
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 *
 */
#ifndef __DP48A_PRINTER_H__
#define __DP48A_PRINTER_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "tuya_cloud_types.h"
#include <stdint.h>
#include <stdbool.h>

// Text alignment
typedef enum {
    DP48A_ALIGN_LEFT = 0,
    DP48A_ALIGN_CENTER = 1,
    DP48A_ALIGN_RIGHT = 2
} dp48a_align_t;

// Text size
typedef enum {
    DP48A_TEXT_NORMAL = 0x00,
    DP48A_TEXT_DOUBLE_HEIGHT = 0x01,
    DP48A_TEXT_DOUBLE_WIDTH = 0x10,
    DP48A_TEXT_DOUBLE_BOTH = 0x11,
    DP48A_TEXT_TRIPLE_HEIGHT = 0x02,
    DP48A_TEXT_TRIPLE_WIDTH = 0x20,
    DP48A_TEXT_QUAD_HEIGHT = 0x03,
    DP48A_TEXT_QUAD_WIDTH = 0x30
} dp48a_text_size_t;

// Font type
typedef enum {
    DP48A_FONT_A = 0,  // 12x24
    DP48A_FONT_B = 1,  // 9x17
    DP48A_FONT_C = 2   // 9x24
} dp48a_font_t;

// Barcode type
typedef enum {
    DP48A_BARCODE_UPC_A = 65,
    DP48A_BARCODE_UPC_E = 66,
    DP48A_BARCODE_EAN13 = 67,
    DP48A_BARCODE_EAN8 = 68,
    DP48A_BARCODE_CODE39 = 69,
    DP48A_BARCODE_ITF = 70,
    DP48A_BARCODE_CODABAR = 71,
    DP48A_BARCODE_CODE93 = 72,
    DP48A_BARCODE_CODE128 = 73
} dp48a_barcode_type_t;

// Barcode HRI position
typedef enum {
    DP48A_HRI_NONE = 0,
    DP48A_HRI_ABOVE = 1,
    DP48A_HRI_BELOW = 2,
    DP48A_HRI_BOTH = 3
} dp48a_hri_pos_t;

// QR code error correction level
typedef enum {
    DP48A_QR_ERROR_L = 0x30,  // 7%
    DP48A_QR_ERROR_M = 0x31,  // 15%
    DP48A_QR_ERROR_Q = 0x32,  // 25%
    DP48A_QR_ERROR_H = 0x33   // 30%
} dp48a_qr_error_t;

// Print density
typedef enum {
    DP48A_DENSITY_LIGHT = 0,
    DP48A_DENSITY_NORMAL = 1,
    DP48A_DENSITY_DARK = 2
} dp48a_density_t;

// Print speed
typedef enum {
    DP48A_SPEED_HIGH = 0,
    DP48A_SPEED_MEDIUM = 1,
    DP48A_SPEED_LOW = 2
} dp48a_speed_t;

// Character set
typedef enum {
    DP48A_CHARSET_USA = 0,
    DP48A_CHARSET_FRANCE = 1,
    DP48A_CHARSET_GERMANY = 2,
    DP48A_CHARSET_UK = 3,
    DP48A_CHARSET_DENMARK = 4,
    DP48A_CHARSET_SWEDEN = 5,
    DP48A_CHARSET_ITALY = 6,
    DP48A_CHARSET_SPAIN = 7,
    DP48A_CHARSET_JAPAN = 8,
    DP48A_CHARSET_NORWAY = 9,
    DP48A_CHARSET_DENMARK2 = 10
} dp48a_charset_t;

// Code page
typedef enum {
    DP48A_CODEPAGE_PC437 = 0,
    DP48A_CODEPAGE_PC850 = 2,
    DP48A_CODEPAGE_PC860 = 3,
    DP48A_CODEPAGE_PC863 = 4,
    DP48A_CODEPAGE_PC865 = 5,
    DP48A_CODEPAGE_WPC1252 = 16,
    DP48A_CODEPAGE_PC866 = 17,
    DP48A_CODEPAGE_PC852 = 18,
    DP48A_CODEPAGE_PC858 = 19,
    DP48A_CODEPAGE_GB18030 = 255
} dp48a_codepage_t;

// ==================== Basic functions ====================
void dp48a_init(void);
void dp48a_reset(void);
void dp48a_print_test_page(void);

// ==================== Text printing ====================
void dp48a_print_text(const char *text);
void dp48a_print_line(const char *text);
void dp48a_print_enter(void);
void dp48a_print_text_raw(const uint8_t *data, size_t len);

// ==================== Text formatting ====================
void dp48a_set_align(dp48a_align_t align);
void dp48a_set_text_size(dp48a_text_size_t size);
void dp48a_set_font(dp48a_font_t font);
void dp48a_set_bold(bool enable);
void dp48a_set_underline(uint8_t mode);  // 0=off, 1=1dot, 2=2dot
void dp48a_set_inverse(bool enable);
void dp48a_set_rotate(bool enable);
void dp48a_set_upside_down(bool enable);

// ==================== Character set and encoding ====================
void dp48a_set_charset(dp48a_charset_t charset);
void dp48a_set_codepage(dp48a_codepage_t codepage);

// ==================== Line spacing and margins ====================
void dp48a_set_line_spacing(uint8_t n);
void dp48a_set_default_line_spacing(void);
void dp48a_set_left_margin(uint16_t dots);
void dp48a_set_print_area_width(uint16_t dots);

// ==================== Paper feed control ====================
void dp48a_feed_lines(uint8_t n);
void dp48a_feed_dots(uint8_t n);
void dp48a_feed_and_cut(uint8_t lines);
void dp48a_cut_paper(bool partial);

// ==================== Barcode printing ====================
void dp48a_set_barcode_height(uint8_t height);
void dp48a_set_barcode_width(uint8_t width);
void dp48a_set_barcode_hri(dp48a_hri_pos_t pos);
void dp48a_set_barcode_hri_font(dp48a_font_t font);
void dp48a_print_barcode(dp48a_barcode_type_t type, const char *data);
void dp48a_print_barcode_ex(dp48a_barcode_type_t type, const uint8_t *data, uint8_t len);

// ==================== QR code printing ====================
void dp48a_set_qr_size(uint8_t size);  // 1-16
void dp48a_set_qr_error_level(dp48a_qr_error_t level);
void dp48a_print_qr(const char *data);
void dp48a_print_qr_ex(const uint8_t *data, uint16_t len);

// ==================== Image printing ====================
void dp48a_print_bitmap(uint16_t width, uint16_t height, const uint8_t *data);
void dp48a_print_bitmap_raster(uint16_t width, uint16_t height, const uint8_t *data);

// ==================== Printer settings ====================
void dp48a_set_density(dp48a_density_t density);
void dp48a_set_speed(dp48a_speed_t speed);
void dp48a_set_print_mode(uint8_t mode);

// ==================== Status query ====================
void dp48a_query_status(void);
void dp48a_query_printer_status(void);
void dp48a_query_offline_status(void);
void dp48a_query_error_status(void);
void dp48a_query_paper_status(void);

// ==================== Serial port control ====================
void dp48a_open_uart(void);
void dp48a_close_uart(void);

// ==================== Advanced functions ====================
void dp48a_set_user_char(uint8_t c, const uint8_t *data);
void dp48a_cancel_user_char(uint8_t c);
void dp48a_set_print_position(uint16_t pos);
void dp48a_enable_panel_buttons(bool enable);
void dp48a_beep(uint8_t times, uint8_t duration);

// ==================== Composite functions ====================
void dp48a_print_title(const char *text);
void dp48a_print_receipt_header(const char *store_name, const char *address);
void dp48a_print_receipt_item(const char *name, const char *price);
void dp48a_print_receipt_footer(const char *total);
void dp48a_print_divider(char ch, uint8_t count);

// ==================== Debug functions ====================
void dp48a_debug_print_hex(const char *prefix, const char *text);
void dp48a_debug_print_hex_raw(const char *prefix, const uint8_t *data, size_t len);

#ifdef __cplusplus
}
#endif

#endif
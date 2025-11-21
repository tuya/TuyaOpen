/**
 * @file dp48a_printer.c
 * @brief Implementation of DP-48A thermal printer functions
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 *
 */
#include "dp48a_printer.h"
#include "utf8_to_gbk.h"

#include "tal_uart.h"
#include "tal_log.h"

#include <string.h>
#include <stdio.h>

#define GBK_SIZE 512
#define UART_PORT TUYA_UART_NUM_2

/**
 * @brief Send data to printer via UART
 * @param buf Data buffer to send
 * @param len Length of data
 */
void uart_send(const uint8_t *buf, size_t len) {
    tal_uart_write(UART_PORT, buf, len);
}

/**
 * @brief Send command to printer
 * @param cmd Command buffer
 * @param len Command length
 */
static void dp48a_send_command(const uint8_t *cmd, size_t len) {
    uart_send(cmd, len);
}

// ==================== Basic functions ====================

/**
 * @brief Initialize printer to default state
 */
void dp48a_init(void) {
    uint8_t cmd[] = {0x1B, 0x40};  // ESC @
    dp48a_send_command(cmd, sizeof(cmd));
}

/**
 * @brief Reset printer (same as init)
 */
void dp48a_reset(void) {
    dp48a_init();
}

/**
 * @brief Print printer self-test page
 */
void dp48a_print_test_page(void) {
    uint8_t cmd[] = {0x12, 0x54};  // DC2 T
    dp48a_send_command(cmd, sizeof(cmd));
}

// ==================== Text printing ====================

/**
 * @brief Print UTF-8 text (converted to GBK internally)
 * @param text UTF-8 encoded text string
 */
void dp48a_print_text(const char *text) {
    if (!text) return;
    
    size_t len = strlen(text);
    uint8_t gbk_buf[GBK_SIZE];
    
    // Convert UTF8 to GBK
    int gbk_len = utf8_to_gbk_buf((const uint8_t *)text, len, gbk_buf, sizeof(gbk_buf));

    PR_NOTICE("text: %s", text);
    PR_NOTICE("text len: %zu, GBK len: %d", len, gbk_len);
    
    // Check for conversion errors
    if (gbk_len < 0) {
        // Conversion failed, print error message or handle error
        PR_ERR("UTF8 to GBK conversion failed: %d", gbk_len);
        return;
    }
    
    // Check for buffer overflow
    if ((size_t)gbk_len > sizeof(gbk_buf)) {
        PR_ERR("GBK buffer overflow: %d > %zu", gbk_len, sizeof(gbk_buf));
        return;
    }
    
    dp48a_send_command(gbk_buf, gbk_len);
}

/**
 * @brief Print text line with line feed
 * @param text UTF-8 encoded text string
 */
void dp48a_print_line(const char *text) {
    dp48a_print_text(text);
    uint8_t lf[] = {0x0D, 0x0A};
    dp48a_send_command(lf, sizeof(lf));
}

void dp48a_print_enter(void)
{
    uint8_t lf[] = {0x0D, 0x0A};
    dp48a_send_command(lf, sizeof(lf));
}

/**
 * @brief Print raw data without conversion
 * @param data Raw data buffer (should be GBK encoded)
 * @param len Length of data
 */
void dp48a_print_text_raw(const uint8_t *data, size_t len) {
    dp48a_send_command(data, len);
}

// ==================== Text formatting ====================

/**
 * @brief Set text alignment
 * @param align Alignment mode (LEFT, CENTER, RIGHT)
 */
void dp48a_set_align(dp48a_align_t align) {
    uint8_t cmd[] = {0x1B, 0x61, align};
    dp48a_send_command(cmd, sizeof(cmd));
}

/**
 * @brief Set text size
 * @param size Text size (NORMAL, DOUBLE_HEIGHT, DOUBLE_WIDTH, DOUBLE_BOTH)
 */
void dp48a_set_text_size(dp48a_text_size_t size) {
    uint8_t cmd[] = {0x1D, 0x21, size};
    dp48a_send_command(cmd, sizeof(cmd));
}

/**
 * @brief Set font type
 * @param font Font type (A, B, C)
 */
void dp48a_set_font(dp48a_font_t font) {
    uint8_t cmd[] = {0x1B, 0x4D, font};
    dp48a_send_command(cmd, sizeof(cmd));
}

/**
 * @brief Enable or disable bold text
 * @param enable true to enable, false to disable
 */
void dp48a_set_bold(bool enable) {
    uint8_t cmd[] = {0x1B, 0x45, enable ? 1 : 0};
    dp48a_send_command(cmd, sizeof(cmd));
}

/**
 * @brief Set underline mode
 * @param mode 0=off, 1=single, 2=double
 */
void dp48a_set_underline(uint8_t mode) {
    uint8_t cmd[] = {0x1B, 0x2D, mode};
    dp48a_send_command(cmd, sizeof(cmd));
}

/**
 * @brief Enable or disable inverse text (white on black)
 * @param enable true to enable, false to disable
 */
void dp48a_set_inverse(bool enable) {
    uint8_t cmd[] = {0x1D, 0x42, enable ? 1 : 0};
    dp48a_send_command(cmd, sizeof(cmd));
}

/**
 * @brief Enable or disable 90-degree rotation
 * @param enable true to enable, false to disable
 */
void dp48a_set_rotate(bool enable) {
    uint8_t cmd[] = {0x1B, 0x56, enable ? 1 : 0};
    dp48a_send_command(cmd, sizeof(cmd));
}

/**
 * @brief Enable or disable upside-down printing
 * @param enable true to enable, false to disable
 */
void dp48a_set_upside_down(bool enable) {
    uint8_t cmd[] = {0x1B, 0x7B, enable ? 1 : 0};
    dp48a_send_command(cmd, sizeof(cmd));
}

// ==================== Character set and encoding ====================

/**
 * @brief Set character set
 * @param charset Character set code
 */
void dp48a_set_charset(dp48a_charset_t charset) {
    uint8_t cmd[] = {0x1B, 0x52, charset};
    dp48a_send_command(cmd, sizeof(cmd));
}

/**
 * @brief Set code page
 * @param codepage Code page number
 */
void dp48a_set_codepage(dp48a_codepage_t codepage) {
    uint8_t cmd[] = {0x1B, 0x74, codepage};
    dp48a_send_command(cmd, sizeof(cmd));
}

// ==================== Line spacing and margins ====================

/**
 * @brief Set line spacing in dots
 * @param n Line spacing value (0-255 dots)
 */
void dp48a_set_line_spacing(uint8_t n) {
    uint8_t cmd[] = {0x1B, 0x33, n};
    dp48a_send_command(cmd, sizeof(cmd));
}

/**
 * @brief Reset line spacing to default
 */
void dp48a_set_default_line_spacing(void) {
    uint8_t cmd[] = {0x1B, 0x32};
    dp48a_send_command(cmd, sizeof(cmd));
}

/**
 * @brief Set left margin
 * @param dots Left margin in dots
 */
void dp48a_set_left_margin(uint16_t dots) {
    uint8_t cmd[] = {0x1D, 0x4C, dots & 0xFF, (dots >> 8) & 0xFF};
    dp48a_send_command(cmd, sizeof(cmd));
}

/**
 * @brief Set printable area width
 * @param dots Width in dots
 */
void dp48a_set_print_area_width(uint16_t dots) {
    uint8_t cmd[] = {0x1D, 0x57, dots & 0xFF, (dots >> 8) & 0xFF};
    dp48a_send_command(cmd, sizeof(cmd));
}

// ==================== Paper feed control ====================

/**
 * @brief Feed paper by number of lines
 * @param n Number of lines to feed
 */
void dp48a_feed_lines(uint8_t n) {
    uint8_t cmd[] = {0x1B, 0x64, n};
    dp48a_send_command(cmd, sizeof(cmd));
}

/**
 * @brief Feed paper by number of dots
 * @param n Number of dots to feed
 */
void dp48a_feed_dots(uint8_t n) {
    uint8_t cmd[] = {0x1B, 0x4A, n};
    dp48a_send_command(cmd, sizeof(cmd));
}

/**
 * @brief Feed paper and cut
 * @param lines Number of lines to feed before cutting
 */
void dp48a_feed_and_cut(uint8_t lines) {
    dp48a_feed_lines(lines);
    dp48a_cut_paper(false);
}

/**
 * @brief Cut paper
 * @param partial true for partial cut, false for full cut
 */
void dp48a_cut_paper(bool partial) {
    uint8_t cmd[] = {0x1D, 0x56, partial ? 1 : 0};
    dp48a_send_command(cmd, sizeof(cmd));
}

// ==================== Barcode printing ====================

/**
 * @brief Set barcode height
 * @param height Height in dots (1-255)
 */
void dp48a_set_barcode_height(uint8_t height) {
    uint8_t cmd[] = {0x1D, 0x68, height};
    dp48a_send_command(cmd, sizeof(cmd));
}

/**
 * @brief Set barcode width
 * @param width Width multiplier (2-6)
 */
void dp48a_set_barcode_width(uint8_t width) {
    uint8_t cmd[] = {0x1D, 0x77, width};
    dp48a_send_command(cmd, sizeof(cmd));
}

/**
 * @brief Set barcode HRI text position
 * @param pos Position (NONE, ABOVE, BELOW, BOTH)
 */
void dp48a_set_barcode_hri(dp48a_hri_pos_t pos) {
    uint8_t cmd[] = {0x1D, 0x48, pos};
    dp48a_send_command(cmd, sizeof(cmd));
}

/**
 * @brief Set barcode HRI font
 * @param font Font type (A or B)
 */
void dp48a_set_barcode_hri_font(dp48a_font_t font) {
    uint8_t cmd[] = {0x1D, 0x66, font};
    dp48a_send_command(cmd, sizeof(cmd));
}

/**
 * @brief Print barcode with string data
 * @param type Barcode type (CODE128, EAN13, CODE39, etc.)
 * @param data Barcode data string
 */
void dp48a_print_barcode(dp48a_barcode_type_t type, const char *data) {
    uint8_t len = strlen(data);
    dp48a_print_barcode_ex(type, (const uint8_t *)data, len);
}

/**
 * @brief Print barcode with binary data
 * @param type Barcode type
 * @param data Barcode data buffer
 * @param len Data length
 */
void dp48a_print_barcode_ex(dp48a_barcode_type_t type, const uint8_t *data, uint8_t len) {
    uint8_t cmd[] = {0x1D, 0x6B, type, len};
    dp48a_send_command(cmd, sizeof(cmd));
    dp48a_send_command(data, len);
}

// ==================== QR code printing ====================

/**
 * @brief Set QR code module size
 * @param size Module size (1-16)
 */
void dp48a_set_qr_size(uint8_t size) {
    if (size < 1) size = 1;
    if (size > 16) size = 16;
    uint8_t cmd[] = {0x1D, 0x28, 0x6B, 0x03, 0x00, 0x31, 0x43, size};
    dp48a_send_command(cmd, sizeof(cmd));
}

/**
 * @brief Set QR code error correction level
 * @param level Error correction level (L=7%, M=15%, Q=25%, H=30%)
 */
void dp48a_set_qr_error_level(dp48a_qr_error_t level) {
    uint8_t cmd[] = {0x1D, 0x28, 0x6B, 0x03, 0x00, 0x31, 0x45, level};
    dp48a_send_command(cmd, sizeof(cmd));
}

/**
 * @brief Print QR code with string data
 * @param data QR code data string
 */
void dp48a_print_qr(const char *data) {
    uint16_t len = strlen(data);
    dp48a_print_qr_ex((const uint8_t *)data, len);
}

/**
 * @brief Print QR code with binary data
 * @param data QR code data buffer
 * @param len Data length
 */
void dp48a_print_qr_ex(const uint8_t *data, uint16_t len) {
    // Store QR data
    uint8_t pL = (len + 3) & 0xFF;
    uint8_t pH = ((len + 3) >> 8) & 0xFF;
    uint8_t cmd_store[] = {0x1D, 0x28, 0x6B, pL, pH, 0x31, 0x50, 0x30};
    dp48a_send_command(cmd_store, sizeof(cmd_store));
    dp48a_send_command(data, len);
    
    // Print QR code
    uint8_t cmd_print[] = {0x1D, 0x28, 0x6B, 0x03, 0x00, 0x31, 0x51, 0x30};
    dp48a_send_command(cmd_print, sizeof(cmd_print));
}

// ==================== Image printing ====================

/**
 * @brief Print monochrome bitmap image
 * @param width Image width in pixels
 * @param height Image height in pixels
 * @param data Image data (1 bit per pixel, row-major)
 */
void dp48a_print_bitmap(uint16_t width, uint16_t height, const uint8_t *data) {
    uint16_t width_bytes = (width + 7) / 8;
    uint8_t xL = width_bytes & 0xFF;
    uint8_t xH = (width_bytes >> 8) & 0xFF;
    uint8_t yL = height & 0xFF;
    uint8_t yH = (height >> 8) & 0xFF;

    uint8_t cmd[] = {0x1D, 0x76, 0x30, 0x00, xL, xH, yL, yH};
    dp48a_send_command(cmd, sizeof(cmd));
    dp48a_send_command(data, width_bytes * height);
}

/**
 * @brief Print raster bitmap image
 * @param width Image width in pixels
 * @param height Image height in pixels
 * @param data Image data (1 bit per pixel, row-major)
 */
void dp48a_print_bitmap_raster(uint16_t width, uint16_t height, const uint8_t *data) {
    uint16_t width_bytes = (width + 7) / 8;
    uint8_t xL = width_bytes & 0xFF;
    uint8_t xH = (width_bytes >> 8) & 0xFF;
    uint8_t yL = height & 0xFF;
    uint8_t yH = (height >> 8) & 0xFF;

    uint8_t cmd[] = {0x1D, 0x76, 0x30, 0x01, xL, xH, yL, yH};
    dp48a_send_command(cmd, sizeof(cmd));
    dp48a_send_command(data, width_bytes * height);
}

// ==================== Printer settings ====================

/**
 * @brief Set print density
 * @param density Density level (LIGHT, NORMAL, DARK)
 */
void dp48a_set_density(dp48a_density_t density) {
    uint8_t cmd[] = {0x1D, 0x7C, density};
    dp48a_send_command(cmd, sizeof(cmd));
}

/**
 * @brief Set print speed
 * @param speed Speed level (HIGH, MEDIUM, LOW)
 */
void dp48a_set_speed(dp48a_speed_t speed) {
    uint8_t cmd[] = {0x1D, 0x73, speed};
    dp48a_send_command(cmd, sizeof(cmd));
}

/**
 * @brief Set print mode
 * @param mode Print mode flags
 */
void dp48a_set_print_mode(uint8_t mode) {
    uint8_t cmd[] = {0x1B, 0x21, mode};
    dp48a_send_command(cmd, sizeof(cmd));
}

// ==================== Status query ====================

/**
 * @brief Query real-time printer status
 */
void dp48a_query_status(void) {
    uint8_t cmd[] = {0x10, 0x04, 0x01};
    dp48a_send_command(cmd, sizeof(cmd));
}

/**
 * @brief Query printer status
 */
void dp48a_query_printer_status(void) {
    uint8_t cmd[] = {0x10, 0x04, 0x02};
    dp48a_send_command(cmd, sizeof(cmd));
}

/**
 * @brief Query offline status
 */
void dp48a_query_offline_status(void) {
    uint8_t cmd[] = {0x10, 0x04, 0x03};
    dp48a_send_command(cmd, sizeof(cmd));
}

/**
 * @brief Query error status
 */
void dp48a_query_error_status(void) {
    uint8_t cmd[] = {0x10, 0x04, 0x04};
    dp48a_send_command(cmd, sizeof(cmd));
}

/**
 * @brief Query paper sensor status
 */
void dp48a_query_paper_status(void) {
    uint8_t cmd[] = {0x1D, 0x72, 0x01};
    dp48a_send_command(cmd, sizeof(cmd));
}

/**
 * @brief Open UART communication
 */
void dp48a_open_uart(void) {
    uint8_t cmd[] = {0x1F, 0x77, 0x00};
    dp48a_send_command(cmd, sizeof(cmd));
}

/**
 * @brief Close UART communication
 */
void dp48a_close_uart(void) {
    uint8_t cmd[] = {0x1F, 0x77, 0x01};
    dp48a_send_command(cmd, sizeof(cmd));
}

// ==================== Advanced functions ====================

/**
 * @brief Define user-defined character
 * @param c Character code to define
 * @param data Character bitmap data (24 bytes)
 */
void dp48a_set_user_char(uint8_t c, const uint8_t *data) {
    uint8_t cmd[] = {0x1B, 0x26, 0x03, c, c, 0x18};
    dp48a_send_command(cmd, sizeof(cmd));
    dp48a_send_command(data, 24);
}

/**
 * @brief Cancel user-defined character
 * @param c Character code to cancel
 */
void dp48a_cancel_user_char(uint8_t c) {
    uint8_t cmd[] = {0x1B, 0x3F, c};
    dp48a_send_command(cmd, sizeof(cmd));
}

/**
 * @brief Set absolute print position
 * @param pos Position in dots from left margin
 */
void dp48a_set_print_position(uint16_t pos) {
    uint8_t cmd[] = {0x1B, 0x24, pos & 0xFF, (pos >> 8) & 0xFF};
    dp48a_send_command(cmd, sizeof(cmd));
}

/**
 * @brief Enable or disable panel buttons
 * @param enable true to enable, false to disable
 */
void dp48a_enable_panel_buttons(bool enable) {
    uint8_t cmd[] = {0x1B, 0x63, 0x35, enable ? 1 : 0};
    dp48a_send_command(cmd, sizeof(cmd));
}

/**
 * @brief Trigger printer buzzer
 * @param times Number of beeps
 * @param duration Duration of each beep (units of 10ms)
 */
void dp48a_beep(uint8_t times, uint8_t duration) {
    uint8_t cmd[] = {0x1B, 0x42, times, duration};
    dp48a_send_command(cmd, sizeof(cmd));
}

// ==================== Composite functions ====================

/**
 * @brief Print title text (large, bold, centered)
 * @param text Title text
 */
void dp48a_print_title(const char *text) {
    dp48a_set_align(DP48A_ALIGN_CENTER);
    dp48a_set_text_size(DP48A_TEXT_DOUBLE_BOTH);
    dp48a_set_bold(true);
    dp48a_print_line(text);
    dp48a_set_bold(false);
    dp48a_set_text_size(DP48A_TEXT_NORMAL);
    dp48a_set_align(DP48A_ALIGN_LEFT);
}

/**
 * @brief Print receipt header with store info
 * @param store_name Store name
 * @param address Store address
 */
void dp48a_print_receipt_header(const char *store_name, const char *address) {
    dp48a_set_align(DP48A_ALIGN_CENTER);
    dp48a_set_text_size(DP48A_TEXT_DOUBLE_HEIGHT);
    dp48a_set_bold(true);
    dp48a_print_line(store_name);
    dp48a_set_bold(false);
    dp48a_set_text_size(DP48A_TEXT_NORMAL);
    dp48a_print_line(address);
    dp48a_print_divider('-', 32);
    dp48a_set_align(DP48A_ALIGN_LEFT);
}

/**
 * @brief Print receipt item line
 * @param name Item name
 * @param price Item price
 */
void dp48a_print_receipt_item(const char *name, const char *price) {
    char line[64];
    int name_len = strlen(name);
    int price_len = strlen(price);
    int spaces = 32 - name_len - price_len;
    if (spaces < 1) spaces = 1;
    
    snprintf(line, sizeof(line), "%s%*s%s", name, spaces, "", price);
    dp48a_print_line(line);
}

/**
 * @brief Print receipt footer with total
 * @param total Total amount
 */
void dp48a_print_receipt_footer(const char *total) {
    dp48a_print_divider('-', 32);
    dp48a_set_align(DP48A_ALIGN_RIGHT);
    dp48a_set_text_size(DP48A_TEXT_DOUBLE_WIDTH);
    dp48a_set_bold(true);
    dp48a_print_line(total);
    dp48a_set_bold(false);
    dp48a_set_text_size(DP48A_TEXT_NORMAL);
    dp48a_set_align(DP48A_ALIGN_LEFT);
}

/**
 * @brief Print divider line
 * @param ch Character to use for divider
 * @param count Number of characters
 */
void dp48a_print_divider(char ch, uint8_t count) {
    char line[64];
    if (count > sizeof(line) - 1) count = sizeof(line) - 1;
    memset(line, ch, count);
    line[count] = '\0';
    dp48a_print_line(line);
}

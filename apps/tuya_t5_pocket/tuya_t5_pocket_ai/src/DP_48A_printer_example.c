/**
 * @file DP_48A_printer_example.c
 * @brief Example code for using the DP-48A thermal printer
 */

#include "DP_48A_printer.h"
#include <stdio.h>

// ==================== Basic text printing examples ====================
void example_basic_text(void) {
    dp48a_init();
    
    // Normal text
    dp48a_print_line("Hello World!");
    dp48a_print_line("Welcome to DP-48A printer");
    
    // Different alignment modes
    dp48a_set_align(DP48A_ALIGN_LEFT);
    dp48a_print_line("Left aligned text");
    
    dp48a_set_align(DP48A_ALIGN_CENTER);
    dp48a_print_line("Center aligned text");
    
    dp48a_set_align(DP48A_ALIGN_RIGHT);
    dp48a_print_line("Right aligned text");
    
    dp48a_set_align(DP48A_ALIGN_LEFT);
    dp48a_feed_lines(3);
}

// ==================== Text formatting examples ====================
void example_text_formatting(void) {
    dp48a_init();
    
    // Different font sizes
    dp48a_set_text_size(DP48A_TEXT_NORMAL);
    dp48a_print_line("Normal size");
    
    dp48a_set_text_size(DP48A_TEXT_DOUBLE_HEIGHT);
    dp48a_print_line("Double height");
    
    dp48a_set_text_size(DP48A_TEXT_DOUBLE_WIDTH);
    dp48a_print_line("Double width");
    
    dp48a_set_text_size(DP48A_TEXT_DOUBLE_BOTH);
    dp48a_print_line("Double both");
    
    dp48a_set_text_size(DP48A_TEXT_NORMAL);
    
    // Bold
    dp48a_set_bold(true);
    dp48a_print_line("Bold text");
    dp48a_set_bold(false);
    
    // Underline
    dp48a_set_underline(1);
    dp48a_print_line("Single underline");
    dp48a_set_underline(2);
    dp48a_print_line("Double underline");
    dp48a_set_underline(0);
    
    // Inverse
    dp48a_set_inverse(true);
    dp48a_print_line("Inverse text");
    dp48a_set_inverse(false);
    
    dp48a_feed_lines(3);
}

// ==================== Barcode printing examples ====================
void example_barcode(void) {
    dp48a_init();
    
    // Set barcode parameters
    dp48a_set_barcode_height(80);
    dp48a_set_barcode_width(2);
    dp48a_set_barcode_hri(DP48A_HRI_BELOW);
    
    // Print different types of barcodes
    dp48a_set_align(DP48A_ALIGN_CENTER);
    
    dp48a_print_line("CODE128 barcode:");
    dp48a_print_barcode(DP48A_BARCODE_CODE128, "123456789");
    dp48a_feed_lines(2);
    
    dp48a_print_line("EAN13 barcode:");
    dp48a_print_barcode(DP48A_BARCODE_EAN13, "1234567890123");
    dp48a_feed_lines(2);
    
    dp48a_print_line("CODE39 barcode:");
    dp48a_print_barcode(DP48A_BARCODE_CODE39, "ABC123");
    
    dp48a_set_align(DP48A_ALIGN_LEFT);
    dp48a_feed_lines(3);
}

// ==================== QR code printing examples ====================
// ==================== QR code printing examples ====================
void example_qrcode(void) {
    dp48a_init();
    
    dp48a_set_align(DP48A_ALIGN_CENTER);
    dp48a_print_line("Scan QR code:");
    
    // Set QR code parameters
    dp48a_set_qr_size(6);
    dp48a_set_qr_error_level(DP48A_QR_ERROR_M);
    
    // Print QR code
    dp48a_print_qr("https://www.tuya.com");
    
    dp48a_feed_lines(2);
    dp48a_print_line("www.tuya.com");
    
    dp48a_set_align(DP48A_ALIGN_LEFT);
    dp48a_feed_lines(3);
}

// ==================== Receipt printing examples ====================
void example_receipt(void) {
    dp48a_init();
    
    // Print store information
    dp48a_print_receipt_header("Tuya Smart Store", "Shenzhen Nanshan Science Park");
    
    // Print time
    dp48a_print_line("Date: 2024-01-15 14:30:25");
    dp48a_print_line("Order: 20240115001");
    dp48a_print_divider('-', 32);
    
    // Print item list
    dp48a_print_line("Item                  Price");
    dp48a_print_divider('-', 32);
    dp48a_print_receipt_item("Smart Switch", "$89.00");
    dp48a_print_receipt_item("Smart Bulb", "$59.00");
    dp48a_print_receipt_item("Smart Plug", "$39.00");
    
    // Print total
    dp48a_print_receipt_footer("Total: $187.00");
    
    // Print QR code
    dp48a_feed_lines(1);
    dp48a_set_align(DP48A_ALIGN_CENTER);
    dp48a_print_line("Scan to pay:");
    dp48a_set_qr_size(5);
    dp48a_print_qr("wxp://pay123456");
    
    dp48a_feed_lines(2);
    dp48a_print_line("Thank you!");
    dp48a_feed_lines(3);
}

// ==================== Bitmap printing examples ====================
void example_bitmap(void) {
    // Example: 8x8 pixel smiley face pattern
    const uint8_t smiley_face[] = {
        0x3C, 0x42, 0xA5, 0x81,
        0xA5, 0x99, 0x42, 0x3C
    };
    
    dp48a_init();
    dp48a_set_align(DP48A_ALIGN_CENTER);
    dp48a_print_line("Bitmap printing:");
    dp48a_print_bitmap(8, 8, smiley_face);
    dp48a_feed_lines(3);
}

// ==================== Advanced features examples ====================
// ==================== Advanced features examples ====================
void example_advanced(void) {
    dp48a_init();
    
    // Set print density
    dp48a_set_density(DP48A_DENSITY_DARK);
    
    // Set print speed
    dp48a_set_speed(DP48A_SPEED_MEDIUM);
    
    // Set line spacing
    dp48a_set_line_spacing(40);
    dp48a_print_line("Line spacing 40 dots");
    dp48a_print_line("Second line");
    
    // Restore default line spacing
    dp48a_set_default_line_spacing();
    dp48a_print_line("Default line spacing");
    
    // Set left margin
    dp48a_set_left_margin(50);
    dp48a_print_line("Left margin 50 dots");
    dp48a_set_left_margin(0);
    
    // Buzzer
    dp48a_beep(2, 5);  // Beep 2 times, 50ms each
    
    dp48a_feed_lines(3);
}

// ==================== Status query examples ====================
void example_status_query(void) {
    // Query printer status
    dp48a_query_printer_status();
    
    // Query paper status
    dp48a_query_paper_status();
    
    // Query error status
    dp48a_query_error_status();
    
    // Note: Need to implement UART receive function to read status response
}

// ==================== Complete demo ====================
void example_complete_demo(void) {
    // Initialize printer
    dp48a_init();
    
    // Print title
    dp48a_print_title("DP-48A Printer Test");
    dp48a_feed_lines(1);
    
    // Print text
    dp48a_print_line("1. Text printing test");
    dp48a_set_bold(true);
    dp48a_print_line("   Bold text");
    dp48a_set_bold(false);
    dp48a_set_underline(1);
    dp48a_print_line("   Underlined text");
    dp48a_set_underline(0);
    dp48a_feed_lines(1);
    
    // Print barcode
    dp48a_print_line("2. Barcode printing test");
    dp48a_set_align(DP48A_ALIGN_CENTER);
    dp48a_set_barcode_height(60);
    dp48a_set_barcode_width(2);
    dp48a_set_barcode_hri(DP48A_HRI_BELOW);
    dp48a_print_barcode(DP48A_BARCODE_CODE128, "TEST123");
    dp48a_set_align(DP48A_ALIGN_LEFT);
    dp48a_feed_lines(2);
    
    // Print QR code
    dp48a_print_line("3. QR code printing test");
    dp48a_set_align(DP48A_ALIGN_CENTER);
    dp48a_set_qr_size(4);
    dp48a_set_qr_error_level(DP48A_QR_ERROR_M);
    dp48a_print_qr("https://www.tuya.com");
    dp48a_set_align(DP48A_ALIGN_LEFT);
    dp48a_feed_lines(2);
    
    // Print divider
    dp48a_print_divider('=', 32);
    
    // Print completion message
    dp48a_set_align(DP48A_ALIGN_CENTER);
    dp48a_print_line("Test completed!");
    dp48a_feed_lines(3);
    
    // Cut paper
    dp48a_cut_paper(false);
}

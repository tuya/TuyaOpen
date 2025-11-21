/**
 * @file dp48a_printer_examples.c
 * @brief Example code for using the DP-48A thermal printer
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 *
 */
#include "dp48a_printer.h"
#include <stdio.h>

// ==================== Basic text printing examples ====================
/**
 * @brief Demonstrates basic text printing capabilities
 * 
 * This example shows:
 * - Printing simple text lines
 * - Using different text alignment modes (left, center, right)
 * - Paper feeding after printing
 * 
 * Use case: Basic receipt or label printing with different alignments
 */
void example_basic_text(void) {
    dp48a_init();
    
    // Print normal text lines
    dp48a_print_line("Hello World!");
    dp48a_print_line("Welcome to DP-48A printer");
    
    // Demonstrate left alignment
    dp48a_set_align(DP48A_ALIGN_LEFT);
    dp48a_print_line("Left aligned text");
    
    // Demonstrate center alignment (useful for titles and headers)
    dp48a_set_align(DP48A_ALIGN_CENTER);
    dp48a_print_line("Center aligned text");
    
    // Demonstrate right alignment (useful for prices and totals)
    dp48a_set_align(DP48A_ALIGN_RIGHT);
    dp48a_print_line("Right aligned text");
    
    // Reset to left alignment for subsequent printing
    dp48a_set_align(DP48A_ALIGN_LEFT);
    // Feed 3 lines of paper to separate from next content
    dp48a_feed_lines(3);
}

// ==================== Text formatting examples ====================
/**
 * @brief Demonstrates text formatting and styling options
 * 
 * This example shows:
 * - Different text sizes (normal, double height, double width, double both)
 * - Bold text for emphasis
 * - Single and double underline
 * - Inverse text (white on black background)
 * 
 * Use case: Creating visually appealing receipts with emphasis on important information
 */
void example_text_formatting(void) {
    dp48a_init();
    
    // Print text at different sizes
    dp48a_set_text_size(DP48A_TEXT_NORMAL);
    dp48a_print_line("Normal size");
    
    dp48a_set_text_size(DP48A_TEXT_DOUBLE_HEIGHT);
    dp48a_print_line("Double height");  // Tall text for headings
    
    dp48a_set_text_size(DP48A_TEXT_DOUBLE_WIDTH);
    dp48a_print_line("Double width");   // Wide text for emphasis
    
    dp48a_set_text_size(DP48A_TEXT_DOUBLE_BOTH);
    dp48a_print_line("Double both");    // Large text for titles
    
    // Reset to normal size
    dp48a_set_text_size(DP48A_TEXT_NORMAL);
    
    // Print bold text for emphasis
    dp48a_set_bold(true);
    dp48a_print_line("Bold text");
    dp48a_set_bold(false);
    
    // Print underlined text
    dp48a_set_underline(1);
    dp48a_print_line("Single underline");  // Subtle emphasis
    dp48a_set_underline(2);
    dp48a_print_line("Double underline");  // Strong emphasis
    dp48a_set_underline(0);  // Disable underline
    
    // Print inverse text (white on black)
    dp48a_set_inverse(true);
    dp48a_print_line("Inverse text");  // High contrast for warnings or highlights
    dp48a_set_inverse(false);
    
    dp48a_feed_lines(3);
}

// ==================== Barcode printing examples ====================
/**
 * @brief Demonstrates barcode printing capabilities
 * 
 * This example shows:
 * - Setting barcode height and width
 * - Positioning barcode text (HRI - Human Readable Interpretation)
 * - Printing different barcode types (CODE128, EAN13, CODE39)
 * 
 * Use case: Product labeling, inventory management, retail receipts
 * 
 * Supported formats:
 * - CODE128: Alphanumeric, high density
 * - EAN13: 13-digit European Article Number (product barcodes)
 * - CODE39: Alphanumeric, widely used in logistics
 */
void example_barcode(void) {
    dp48a_init();
    
    // Configure barcode appearance
    dp48a_set_barcode_height(80);   // Height in dots (taller = easier to scan)
    dp48a_set_barcode_width(2);     // Width multiplier (2 = medium thickness)
    dp48a_set_barcode_hri(DP48A_HRI_BELOW);  // Print barcode text below the bars
    
    // Center align for better appearance
    dp48a_set_align(DP48A_ALIGN_CENTER);
    
    // Print CODE128 barcode (supports full ASCII character set)
    dp48a_print_line("CODE128 barcode:");
    dp48a_print_barcode(DP48A_BARCODE_CODE128, "123456789");
    dp48a_feed_lines(2);
    
    // Print EAN13 barcode (standard product barcode format)
    dp48a_print_line("EAN13 barcode:");
    dp48a_print_barcode(DP48A_BARCODE_EAN13, "1234567890123");  // Must be exactly 13 digits
    dp48a_feed_lines(2);
    
    // Print CODE39 barcode (common in shipping and logistics)
    dp48a_print_line("CODE39 barcode:");
    dp48a_print_barcode(DP48A_BARCODE_CODE39, "ABC123");
    
    dp48a_set_align(DP48A_ALIGN_LEFT);
    dp48a_feed_lines(3);
}

// ==================== QR code printing examples ====================
/**
 * @brief Demonstrates QR code printing for 2D barcodes
 * 
 * This example shows:
 * - Setting QR code size (module size 1-16)
 * - Setting error correction level (L=7%, M=15%, Q=25%, H=30%)
 * - Printing QR code with URL
 * 
 * Use case: 
 * - Payment QR codes (WeChat Pay, Alipay)
 * - URL sharing (website links, product information)
 * - Contact information (vCard)
 * - WiFi credentials
 * 
 * Error correction levels:
 * - L (Low): 7% damage recovery, more data capacity
 * - M (Medium): 15% damage recovery, balanced (recommended)
 * - Q (Quartile): 25% damage recovery
 * - H (High): 30% damage recovery, less data capacity
 */
void example_qrcode(void) {
    dp48a_init();
    
    dp48a_set_align(DP48A_ALIGN_CENTER);
    dp48a_print_line("Scan QR code:");
    
    // Configure QR code parameters
    dp48a_set_qr_size(6);  // Module size: 6 = medium (1-16, larger = easier to scan but bigger)
    dp48a_set_qr_error_level(DP48A_QR_ERROR_M);  // Medium error correction (15% recovery)
    
    // Print QR code containing URL
    dp48a_print_qr("https://www.tuya.com");
    
    dp48a_feed_lines(2);
    // Print human-readable URL below QR code
    dp48a_print_line("www.tuya.com");
    
    dp48a_set_align(DP48A_ALIGN_LEFT);
    dp48a_feed_lines(3);
}

// ==================== Receipt printing examples ====================
/**
 * @brief Demonstrates complete retail receipt printing
 * 
 * This example shows:
 * - Store header with name and address
 * - Transaction details (date, order number)
 * - Item list with names and prices
 * - Total amount calculation
 * - Payment QR code
 * - Thank you message
 * 
 * Use case: Complete point-of-sale receipt for retail stores
 * 
 * Features demonstrated:
 * - Composite functions for common receipt sections
 * - Text dividers for visual separation
 * - Mixed alignment (left for items, right for totals)
 * - Integration of QR code for mobile payment
 */
void example_receipt(void) {
    dp48a_init();
    
    // Print store header (centered, bold, large text)
    dp48a_print_receipt_header("Tuya Smart Store", "Shenzhen Nanshan Science Park");
    
    // Print transaction metadata
    dp48a_print_line("Date: 2024-01-15 14:30:25");
    dp48a_print_line("Order: 20240115001");
    dp48a_print_divider('-', 32);  // Visual separator
    
    // Print item table header
    dp48a_print_line("Item                  Price");
    dp48a_print_divider('-', 32);
    
    // Print purchased items (name left-aligned, price right-aligned)
    dp48a_print_receipt_item("Smart Switch", "$89.00");
    dp48a_print_receipt_item("Smart Bulb", "$59.00");
    dp48a_print_receipt_item("Smart Plug", "$39.00");
    
    // Print total with emphasis (right-aligned, large, bold)
    dp48a_print_receipt_footer("Total: $187.00");
    
    // Add payment QR code section
    dp48a_feed_lines(1);
    dp48a_set_align(DP48A_ALIGN_CENTER);
    dp48a_print_line("Scan to pay:");
    dp48a_set_qr_size(5);  // Slightly smaller for receipt
    dp48a_print_qr("wxp://pay123456");  // WeChat Pay example
    
    // Print closing message
    dp48a_feed_lines(2);
    dp48a_print_line("Thank you!");
    dp48a_feed_lines(3);
}

// ==================== Bitmap printing examples ====================
/**
 * @brief Demonstrates bitmap/image printing
 * 
 * This example shows:
 * - Printing monochrome bitmap images
 * - Image data format (1 bit per pixel, row-major order)
 * 
 * Use case: 
 * - Logo printing on receipts
 * - Custom graphics and icons
 * - Product images (converted to monochrome)
 * 
 * Image format requirements:
 * - Monochrome (1-bit): each bit represents one pixel (1=black, 0=white)
 * - Row-major order: pixels organized left-to-right, top-to-bottom
 * - Width must be divisible by 8 (padded if necessary)
 * - Data size = (width/8) * height bytes
 * 
 * Example: 8x8 smiley face
 * - Each row = 1 byte (8 pixels)
 * - Total = 8 bytes
 * 
 * https://www.zhetao.com/fontarray.html
 * 
 */
void example_bitmap(void)
{
    // Example: 8x8 pixel smiley face pattern
    // Binary representation:
    //   00111100 = 0x3C (  ****  )
    //   01000010 = 0x42 ( *    * )
    //   10100101 = 0xA5 (*  * * *)
    //   10000001 = 0x81 (*      *)
    //   10100101 = 0xA5 (*  * * *)
    //   10011001 = 0x99 (*  **  *)
    //   01000010 = 0x42 ( *    * )
    //   00111100 = 0x3C (  ****  )
    const uint8_t smiley_face[] = {
        0x3C, 0x42, 0xA5, 0x81,
        0xA5, 0x99, 0x42, 0x3C
    };

    dp48a_init();
    dp48a_set_align(DP48A_ALIGN_CENTER);
    dp48a_print_line("Bitmap printing:");
    // Print bitmap: 8 pixels wide, 8 pixels tall
    dp48a_print_bitmap(1*8, 8, smiley_face);
    dp48a_feed_lines(3);
}

// ==================== Advanced features examples ====================
/**
 * @brief Demonstrates advanced printer settings and features
 * 
 * This example shows:
 * - Print density control (light, normal, dark)
 * - Print speed adjustment (high, medium, low) 
 * - Custom line spacing
 * - Left margin adjustment
 * - Buzzer/beeper control
 * 
 * Use case:
 * - Adjusting print quality for different paper types
 * - Optimizing speed vs quality tradeoff
 * - Custom layout formatting
 * - Audio feedback for user interaction
 * 
 * Density levels:
 * - LIGHT: Saves toner/ink, faster, may be less readable
 * - NORMAL: Balanced quality and speed (default)
 * - DARK: High quality, slower, better for poor lighting
 * 
 * Speed levels:
 * - HIGH: Fastest printing, may reduce quality
 * - MEDIUM: Balanced (recommended)
 * - LOW: Highest quality, slower
 */
void example_advanced(void) {
    dp48a_init();
    
    // Set print density (affects darkness of printed text)
    dp48a_set_density(DP48A_DENSITY_DARK);  // Darker print for better visibility
    
    // Set print speed (affects print quality vs speed tradeoff)
    dp48a_set_speed(DP48A_SPEED_MEDIUM);    // Balanced speed and quality
    
    // Set custom line spacing (distance between lines)
    dp48a_set_line_spacing(40);  // 40 dots spacing (larger = more space between lines)
    dp48a_print_line("Line spacing 40 dots");
    dp48a_print_line("Second line");
    
    // Restore default line spacing (typically 30 dots)
    dp48a_set_default_line_spacing();
    dp48a_print_line("Default line spacing");
    
    // Set left margin (indent text from left edge)
    dp48a_set_left_margin(50);   // 50 dots from left edge
    dp48a_print_line("Left margin 50 dots");
    dp48a_set_left_margin(0);    // Reset to no margin
    
    // Trigger buzzer for audio feedback
    // Parameters: number of beeps, duration (units of 10ms)
    dp48a_beep(2, 5);  // Beep 2 times, 50ms each (5 * 10ms)
    
    dp48a_feed_lines(3);
}

// ==================== Status query examples ====================
/**
 * @brief Demonstrates printer status checking
 * 
 * This example shows:
 * - Querying printer operational status
 * - Checking paper status (present/absent, near end)
 * - Detecting error conditions
 * 
 * Use case:
 * - Pre-print validation (ensure printer is ready)
 * - Error handling and user notification
 * - Low paper warnings
 * - Printer health monitoring
 * 
 * Status types:
 * - Printer status: Online/offline, ready/busy
 * - Paper status: Paper present, paper near end, paper out
 * - Error status: Paper jam, cutter error, temperature error, etc.
 * 
 * Note: This sends query commands to the printer. To receive responses,
 * you need to implement a UART receive handler that processes the
 * printer's status response bytes according to the ESC/POS protocol.
 * 
 * Response handling (not shown here):
 * - Set up UART RX interrupt or polling
 * - Parse response bytes according to printer documentation
 * - Update application state based on status
 */
void example_status_query(void) {
    // Query overall printer status (online/offline, errors)
    dp48a_query_printer_status();
    
    // Query paper sensor status (paper present/absent, near end)
    dp48a_query_paper_status();
    
    // Query error status (paper jam, temperature, etc.)
    dp48a_query_error_status();
    
    // Important: These functions send query commands but don't return values
    // You need to implement UART receive function to read status response bytes
    // Typical response: 1 byte containing status bits
    // Example: 0x12 = 0b00010010 (bit 1=paper near end, bit 4=offline)
}

// ==================== Complete demo ====================
/**
 * @brief Comprehensive demonstration of all major printer features
 * 
 * This example combines multiple features in a single print job:
 * 1. Title printing with large, bold, centered text
 * 2. Text formatting (normal, bold, underline)
 * 3. Barcode printing with centered alignment
 * 4. QR code printing with centered alignment
 * 5. Visual separator line
 * 6. Completion message
 * 7. Paper cutting
 * 
 * Use case: 
 * - Printer capability testing
 * - Demonstration print for new users
 * - Quality assurance testing
 * - Feature showcase for customers
 * 
 * This demonstrates a complete print workflow from initialization
 * to paper cutting, suitable as a template for custom applications.
 */
void example_complete_demo(void) {
    // Initialize printer and reset to default settings
    dp48a_init();
    
    // Section 1: Print title (large, bold, centered)
    dp48a_print_title("DP-48A Printer Test");
    dp48a_feed_lines(1);
    
    // Section 2: Text formatting demonstration
    dp48a_print_line("1. Text printing test");
    dp48a_set_bold(true);
    dp48a_print_line("   Bold text");        // Indented bold text
    dp48a_set_bold(false);
    dp48a_set_underline(1);
    dp48a_print_line("   Underlined text");  // Indented underlined text
    dp48a_set_underline(0);
    dp48a_feed_lines(1);  // Space before next section
    
    // Section 3: Barcode printing demonstration
    dp48a_print_line("2. Barcode printing test");
    dp48a_set_align(DP48A_ALIGN_CENTER);
    dp48a_set_barcode_height(60);  // Medium height barcode
    dp48a_set_barcode_width(2);    // Medium thickness
    dp48a_set_barcode_hri(DP48A_HRI_BELOW);  // Show human-readable text below
    dp48a_print_barcode(DP48A_BARCODE_CODE128, "TEST123");
    dp48a_set_align(DP48A_ALIGN_LEFT);  // Reset alignment
    dp48a_feed_lines(2);  // Space before next section
    
    // Section 4: QR code printing demonstration
    dp48a_print_line("3. QR code printing test");
    dp48a_set_align(DP48A_ALIGN_CENTER);
    dp48a_set_qr_size(4);  // Medium-small QR code
    dp48a_set_qr_error_level(DP48A_QR_ERROR_M);  // Medium error correction
    dp48a_print_qr("https://www.tuya.com");
    dp48a_set_align(DP48A_ALIGN_LEFT);  // Reset alignment
    dp48a_feed_lines(2);  // Space before divider
    
    // Section 5: Print visual separator
    dp48a_print_divider('=', 32);  // 32 equal signs as separator
    
    // Section 6: Print completion message
    dp48a_set_align(DP48A_ALIGN_CENTER);
    dp48a_print_line("Test completed!");
    dp48a_feed_lines(3);  // Feed extra paper before cutting
    
    // Section 7: Cut paper (partial cut - leaves small connection)
    dp48a_cut_paper(false);  // false = partial cut, true = full cut
}

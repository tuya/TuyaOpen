/**
 * @file ui_http_client_post.h
 * @brief UI interface for HTTP client POST example
 *
 * This file provides the interface for creating and managing the HTTP client POST UI,
 * including button creation, response display, and Wi-Fi status indicator.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#ifndef UI_HTTP_CLIENT_POST_H_
#define UI_HTTP_CLIENT_POST_H_

#include "tuya_cloud_types.h"

#ifdef __cplusplus
extern "C" {
#endif

#if defined(ENABLE_LIBLVGL) && (ENABLE_LIBLVGL == 1)

/**
 * @brief Callback function type for button click event
 * 
 * This callback will be called when the send button is clicked.
 * The callback should handle sending the HTTP POST request.
 */
typedef void (*ui_button_click_cb_t)(void);

/**
 * @brief Initialize the HTTP client POST UI
 * 
 * This function creates the UI elements including:
 * - Wi-Fi status indicator (green/red dot in top-right)
 * - "Receive" label
 * - Response text box
 * - Send button at bottom
 * 
 * @param button_cb Callback function to be called when button is clicked
 */
void ui_http_client_post_init(ui_button_click_cb_t button_cb);

/**
 * @brief Update Wi-Fi status indicator
 * 
 * @param connected true if Wi-Fi is connected, false otherwise
 */
void ui_update_wifi_status(bool connected);

/**
 * @brief Update response text display
 * 
 * @param text Response text to display (NULL to show default text)
 * @param is_error true if this is an error message (red color), false for normal text
 */
void ui_update_response_text(const char *text, bool is_error);

/**
 * @brief Update response text display with sending status
 * 
 * Shows "Sending..." message in blue color
 */
void ui_update_response_sending(void);

#endif // ENABLE_LIBLVGL

#ifdef __cplusplus
}
#endif

#endif /* UI_HTTP_CLIENT_POST_H_ */


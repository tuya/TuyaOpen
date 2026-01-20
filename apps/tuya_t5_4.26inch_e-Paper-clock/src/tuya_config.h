/**
 * @file tuya_config.h
 * @brief Tuya Cloud configuration for the e-Paper clock example.
 *
 * Notes:
 * - If your board has OTP-burned UUID/AUTHKEY, you usually only need to set
 *   `TUYA_PRODUCT_ID` correctly.
 * - If OTP read fails, the code will fall back to the UUID/AUTHKEY below.
 *
 * @copyright Copyright (c) 2021-2026 Tuya Inc. All Rights Reserved.
 *
 */
#ifndef __TUYA_CONFIG_H__
#define __TUYA_CONFIG_H__

// clang-format off

// Timezone offset (seconds). Default: China Standard Time (UTC+8).
#ifndef EPD_CLOCK_TZ_SECONDS
#define EPD_CLOCK_TZ_SECONDS (8 * 60 * 60)
#endif

// Product ID (PID) created on Tuya IoT platform (usually 16 chars).
// If your module is pre-burned, ask the vendor for the matching PID.
#define TUYA_PRODUCT_ID         "8nrgjfcvbtjx25er"

// Fallback only (used when OTP/KV license read fails).
#define TUYA_OPENSDK_UUID       "uuidxxxxxxxxxxxxxxxx"                    // Please change the correct uuid
#define TUYA_OPENSDK_AUTHKEY    "keyxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"        // Please change the correct authkey

// ----------------------------------------------------------------------------
// WiFi (temporary development shortcut)
// ----------------------------------------------------------------------------
// Set to 1 to connect to WiFi directly (skip Tuya APP netcfg for now).
// Set to 0 to use Tuya APP netcfg (BLE/AP).
#ifndef EPD_CLOCK_USE_STATIC_WIFI
#define EPD_CLOCK_USE_STATIC_WIFI 0
#endif

// Your 2.4G WiFi SSID / password
#if defined(EPD_CLOCK_USE_STATIC_WIFI) && (EPD_CLOCK_USE_STATIC_WIFI == 1)
#ifndef EPD_CLOCK_WIFI_SSID
// #define EPD_CLOCK_WIFI_SSID     "your_ssid"
#error "EPD_CLOCK_USE_STATIC_WIFI=1 requires defining EPD_CLOCK_WIFI_SSID"
#endif
#ifndef EPD_CLOCK_WIFI_PASSWORD
// #define EPD_CLOCK_WIFI_PASSWORD "your_password"
#error "EPD_CLOCK_USE_STATIC_WIFI=1 requires defining EPD_CLOCK_WIFI_PASSWORD"
#endif
#endif

// ----------------------------------------------------------------------------
// Time sync (optional): NTP fallback if Tuya cloud timestamp is not received
// ----------------------------------------------------------------------------
// If you haven't bound/activated the device yet, Tuya cloud may not push
// `TUYA_EVENT_TIMESTAMP_SYNC`. Enabling NTP fallback lets the clock get time
// after WiFi is up (UDP/123 must be reachable).
#define EPD_CLOCK_ENABLE_NTP_FALLBACK 1
// Common choices: "ntp.aliyun.com", "pool.ntp.org", or a fixed IP like "129.6.15.28"
#define EPD_CLOCK_NTP_SERVER "ntp.aliyun.com"

// ----------------------------------------------------------------------------
// Clock UI layout tuning (optional)
// ----------------------------------------------------------------------------
// If the time partial-refresh area still looks "down shifted", try reducing
// `EPD_CLOCK_TIME_Y` (move up) or increasing `EPD_CLOCK_TIME_INNER_TOP` (move down).
// All x/width values should keep 8-pixel alignment for partial refresh.

// #define EPD_CLOCK_TIME_Y 80
// #define EPD_CLOCK_TIME_INNER_TOP 0

// ----------------------------------------------------------------------------
// Key button: press to (re)enter pairing mode
// ----------------------------------------------------------------------------
// Default pin matches `boards/T5AI/TUYA_T5AI_BOARD` KEY (GPIO12, active-low).
#ifndef EPD_CLOCK_NETCFG_KEY_ENABLE
#define EPD_CLOCK_NETCFG_KEY_ENABLE 1
#endif

#ifndef EPD_CLOCK_NETCFG_KEY_PIN
#define EPD_CLOCK_NETCFG_KEY_PIN TUYA_GPIO_NUM_12
#endif

#ifndef EPD_CLOCK_NETCFG_KEY_ACTIVE_LEVEL
#define EPD_CLOCK_NETCFG_KEY_ACTIVE_LEVEL TUYA_GPIO_LEVEL_LOW
#endif

#ifndef EPD_CLOCK_NETCFG_KEY_LONGPRESS_MS
#define EPD_CLOCK_NETCFG_KEY_LONGPRESS_MS 3000
#endif

// clang-format on

#endif /* __TUYA_CONFIG_H__ */

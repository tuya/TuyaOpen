#ifndef __ECHO_CONFIG_H__
#define __ECHO_CONFIG_H__

/* Echo bot 应用：仅保留 WiFi 等应用侧配置，IM 相关见 im/im_config.h */
#if __has_include("echo_secrets.h")
#include "echo_secrets.h"
#endif

#ifndef ECHO_SECRET_WIFI_SSID
#define ECHO_SECRET_WIFI_SSID "Pico"
#endif
#ifndef ECHO_SECRET_WIFI_PASS
#define ECHO_SECRET_WIFI_PASS "Pico123456"
#endif

#define ECHO_WIFI_MAX_RETRY     10
#define ECHO_WIFI_RETRY_BASE_MS 1000
#define ECHO_WIFI_RETRY_MAX_MS  30000

#define ECHO_NVS_WIFI "wifi_config"
#define ECHO_NVS_KEY_SSID "ssid"
#define ECHO_NVS_KEY_PASS "password"

#endif /* __ECHO_CONFIG_H__ */

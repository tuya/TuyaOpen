#ifndef __ECHO_CONFIG_H__
#define __ECHO_CONFIG_H__

/* Echo bot app-level config. For IM settings see IM/im_config.h. */
#if __has_include("echo_secrets.h")
#include "echo_secrets.h"
#endif

#ifndef ECHO_SECRET_WIFI_SSID
#define ECHO_SECRET_WIFI_SSID "your_wifi_ssid"
#endif
#ifndef ECHO_SECRET_WIFI_PASS
#define ECHO_SECRET_WIFI_PASS "your_wifi_password"
#endif

#define ECHO_WIFI_MAX_RETRY     10
#define ECHO_WIFI_RETRY_BASE_MS 1000
#define ECHO_WIFI_RETRY_MAX_MS  30000

#define ECHO_NVS_WIFI "wifi_config"
#define ECHO_NVS_KEY_SSID "ssid"
#define ECHO_NVS_KEY_PASS "password"

#endif /* __ECHO_CONFIG_H__ */

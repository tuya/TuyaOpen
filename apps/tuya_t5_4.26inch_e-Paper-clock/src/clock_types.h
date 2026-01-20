/**
 * @file clock_types.h
 * @brief Shared types for the e-Paper clock example.
 *
 * This file defines data structures shared across clock modules, including
 * time source/status, network info, UI state, and user settings.
 *
 * @copyright Copyright (c) 2021-2026 Tuya Inc. All Rights Reserved.
 *
 */
#ifndef __CLOCK_TYPES_H__
#define __CLOCK_TYPES_H__

#include "netmgr.h"
#include "tal_time_service.h"

#if defined(ENABLE_WIFI) && (ENABLE_WIFI == 1)
#include "netconn_wifi.h"
#endif

typedef enum {
    CLOCK_TIME_SRC_DEFAULT = 0,
    CLOCK_TIME_SRC_NTP = 1,
    CLOCK_TIME_SRC_CLOUD = 2,
} clock_time_src_t;

typedef enum {
    CLOCK_TIME_MODE_24H = 0,
    CLOCK_TIME_MODE_12H = 1,
} clock_time_mode_t;

typedef enum {
    CLOCK_THEME_DARK = 0,  // black background, white text
    CLOCK_THEME_LIGHT = 1, // white background, black text
} clock_theme_t;

typedef struct {
    netmgr_status_e link;
#if defined(ENABLE_WIFI) && (ENABLE_WIFI == 1)
    char ssid[WIFI_SSID_LEN + 1];
#else
    char ssid[32 + 1];
#endif
    char ip[40];
} clock_net_info_t;

typedef struct {
    POSIX_TM_S local;
    BOOL_T time_synced;
    clock_time_src_t time_src;

    clock_time_mode_t time_mode;
    clock_theme_t theme;

    clock_net_info_t net;
    BOOL_T cloud_connected;
} clock_ui_state_t;

#endif /* __CLOCK_TYPES_H__ */

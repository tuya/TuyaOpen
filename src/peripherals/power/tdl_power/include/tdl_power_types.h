/**
 * @file tdl_power_types.h
 * @brief Shared power contract types (chip-agnostic), used by both manage & driver layers.
 *
 * @copyright Copyright (c) 2021-2026 Tuya Inc. All Rights Reserved.
 */

#ifndef __TDL_POWER_TYPES_H__
#define __TDL_POWER_TYPES_H__

#include "tuya_cloud_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Handle to the (single) power device on a board. */
typedef void *TDL_POWER_HANDLE;

/**
 * @brief Power-domain semantic roles (bitfield, OR-able into a mask).
 * Roles are cross-board; boards map physical pin/channel to a role at registration.
 * uint32_t caps at 32 domains; append at the tail, no board-private range.
 */
typedef enum {
    TDL_PWR_DOMAIN_DISPLAY     = 1u << 0, // display (EINK_3V3 / EPD_3V3)
    TDL_PWR_DOMAIN_SD          = 1u << 1, // SD card
    TDL_PWR_DOMAIN_AUDIO       = 1u << 2, // audio / speaker (PA)
    TDL_PWR_DOMAIN_CAMERA      = 1u << 3, // camera main
    TDL_PWR_DOMAIN_CAMERA_AVDD = 1u << 4,
    TDL_PWR_DOMAIN_CAMERA_DVDD = 1u << 5,
    TDL_PWR_DOMAIN_RTC         = 1u << 6,
    TDL_PWR_DOMAIN_JOYSTICK    = 1u << 7,
    TDL_PWR_DOMAIN_CELLULAR    = 1u << 8, // 4G/cellular module supply
} TDL_POWER_DOMAIN_E;

/** Charge state (3-state contract; 2-state boards never report FULL). */
typedef enum {
    TDL_CHG_DISCHARGE = 0,
    TDL_CHG_CHARGING,
    TDL_CHG_FULL,
} TDL_CHG_STATE_E;

/** Charge state-change callback. */
typedef void (*TDL_CHG_EVENT_CB)(TDL_CHG_STATE_E state, void *arg);

/** One point on a battery discharge (OCV->SOC) curve. */
typedef struct {
    uint16_t mv;  // pack terminal voltage
    uint8_t  pct; // 0..100
} TDL_POWER_OCV_PT_T;

/** Board-declared battery landmarks (facts/recommendations, not policy). */
typedef struct {
    uint16_t                  v_full_mv;     // full (100%)
    uint16_t                  v_empty_mv;    // empty (0%); v_full/v_empty also = linear endpoints
    uint16_t                  v_low_mv;      // recommended low-battery threshold (0 = unspecified)
    uint16_t                  v_critical_mv; // recommended shutdown threshold (0 = unspecified)
    const TDL_POWER_OCV_PT_T *curve;         // optional mV->% table (ascending mv); NULL = linear
    uint8_t                   curve_cnt;
} TDL_POWER_BATTERY_INFO_T;

/** Device-level static facts, passed at register and read via tdl_power_get_info(). */
typedef struct {
    TDL_POWER_BATTERY_INFO_T battery;
} TDL_POWER_INFO_T;

#ifdef __cplusplus
}
#endif

#endif /* __TDL_POWER_TYPES_H__ */

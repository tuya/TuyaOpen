/**
 * @file otto_robot_dp_profile.h
 * @brief Otto robot cloud DP ID definitions per product profile
 * @version 1.0
 * @date 2025-05-22
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */
#ifndef __OTTO_ROBOT_DP_PROFILE_H__
#define __OTTO_ROBOT_DP_PROFILE_H__

#ifdef __cplusplus
extern "C" {
#endif

/* ---------------------------------------------------------------------------
 * DP IDs
 * --------------------------------------------------------------------------- */
#if defined(OTTO_PRODUCT_ST7789_V1) && (OTTO_PRODUCT_ST7789_V1 == 1)

#define DPID_OTTO_PTZ_CONTROL  5
#define DPID_OTTO_SPEED        102
#define DPID_OTTO_STEP         103
#define DPID_OTTO_AUDIO        104

#define OTTO_STEP_MIN          1
#define OTTO_STEP_MAX          30

#else

#define DPID_OTTO_STEP           11
#define DPID_OTTO_SPEED          5
#define DPID_OTTO_AUDIO          9
#define DPID_OTTO_ACTION         10
#define DPID_OTTO_WALK_DIRECTION 4

#define DPID_OTTO_LEFT_LEG_TRIM    101
#define DPID_OTTO_RIGHT_LEG_TRIM   102
#define DPID_OTTO_LEFT_FOOT_TRIM   103
#define DPID_OTTO_RIGHT_FOOT_TRIM  104
#define DPID_OTTO_LEFT_HAND_TRIM   105
#define DPID_OTTO_RIGHT_HAND_TRIM  106

#endif

#ifdef __cplusplus
}
#endif

#endif /* __OTTO_ROBOT_DP_PROFILE_H__ */

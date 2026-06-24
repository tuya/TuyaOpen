/**
 * @file speaker_config.h
 * @copyright Copyright (c) 2026 Tuya Inc. All Rights Reserved.
 * @brief DP ID constants and product configuration for smart_speaker.
 *
 * DP IDs are configurable via Kconfig (CONFIG_SPEAKER_DP_*).
 * Fallback values match default product DP IDs.
 *
 * NOTE: Kconfig generates SPEAKER_DP_* and SPEAKER_DEFAULT_VOLUME
 * directly into tuya_kconfig.h. This header provides fallbacks only
 * when those are not set (i.e. before Kconfig processing or on host).
 */

#ifndef __SPEAKER_CONFIG_H__
#define __SPEAKER_CONFIG_H__

#include "tuya_cloud_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ---- DP IDs ---- */
#ifndef SPEAKER_DP_VOLUME
#define SPEAKER_DP_VOLUME 203
#endif

#ifndef SPEAKER_DP_MIC
#define SPEAKER_DP_MIC 204
#endif

#ifndef SPEAKER_DP_PLAY_SWITCH
#define SPEAKER_DP_PLAY_SWITCH 205
#endif

#ifndef SPEAKER_DP_CTRL_GROUP
#define SPEAKER_DP_CTRL_GROUP 208
#endif

#ifndef SPEAKER_DP_RING
#define SPEAKER_DP_RING 207
#endif

#ifndef SPEAKER_DP_QUIT
#define SPEAKER_DP_QUIT 4
#endif

#ifndef SPEAKER_DP_BT
#define SPEAKER_DP_BT 206
#endif

#ifndef SPEAKER_DP_BT_SWITCH
#define SPEAKER_DP_BT_SWITCH 5
#endif

#ifndef SPEAKER_DP_BT_VISIBLE
#define SPEAKER_DP_BT_VISIBLE 6
#endif

/* ---- CTRL_GROUP JSON keys (from tuya_voice_app_dp_process.c) ---- */
#define SPEAKER_CTRL_REPLY   "reply"
#define SPEAKER_CTRL_CTALK   "CTalk"
#define SPEAKER_CTRL_TMODE   "Tmode"
#define SPEAKER_CTRL_DND     "DND"
#define SPEAKER_CTRL_DISTURB "disturb"
#define SPEAKER_CTRL_START   "startTime"
#define SPEAKER_CTRL_END     "endTime"

#define SPEAKER_TALK_MODE_SINGLE "single"
#define SPEAKER_TALK_MODE_MULTI  "multi"
#define SPEAKER_TALK_MODE_FREE   "free"

/* ---- Default volume ---- */
#ifndef SPEAKER_DEFAULT_VOLUME
#define SPEAKER_DEFAULT_VOLUME 70
#endif

/* ---- On-device audio assets (VOICE_APP_RES_PATH) ---- */
#ifndef SPEAKER_AUDIO_RES_PREFIX
#define SPEAKER_AUDIO_RES_PREFIX "/data/audio"
#endif

#ifdef __cplusplus
}
#endif

#endif /* __SPEAKER_CONFIG_H__ */
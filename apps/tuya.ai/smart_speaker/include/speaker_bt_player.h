/**
 * @file speaker_bt_player.h
 * @copyright Copyright (c) 2026 Tuya Inc. All Rights Reserved.
 * @brief Classic Bluetooth music player scaffold (DP206/5/6 → compat BG).
 *
 * Full port requires: tuya_comm_classic_bt + Opus decoder + A2DP PCM feed into
 * player_compat BG (see tuya_comm_bt_player.c).
 */

#ifndef __SPEAKER_BT_PLAYER_H__
#define __SPEAKER_BT_PLAYER_H__

#include "tuya_cloud_types.h"

#ifdef __cplusplus
extern "C" {
#endif

int  speaker_bt_player_init(void);
void speaker_bt_player_deinit(void);

/** DP206: master BT enable (discoverable + connectable when enabled). */
int speaker_bt_dp_handle_enable(BOOL_T enable);

/** DP5: BT switch (connectable without discoverable when on). */
int speaker_bt_dp_handle_switch(BOOL_T enable);

/** DP6: BT visible / discoverable. */
int speaker_bt_dp_handle_visible(BOOL_T visible);

/**
 * Future hook: decoded PCM from A2DP/Opus path → compat BG player.
 * @return OPRT_OK if fed, OPRT_NOT_SUPPORTED when stack not linked.
 */
int speaker_bt_player_feed_pcm(const int16_t *pcm, uint32_t samples);

#ifdef __cplusplus
}
#endif

#endif /* __SPEAKER_BT_PLAYER_H__ */

/**
 * @file speaker_bt_player.c
 * @brief BT classic music scaffold for smart_speaker (P2-1).
 * @copyright Copyright (c) 2026 Tuya Inc. All Rights Reserved.
 *
 * TuyaOpen smart_speaker has no tuya_comm_classic_bt / A2DP stack wired yet.
 * This module:
 *   - Registers DP intent for legacy-aligned behavior when CONFIG_SPEAKER_BT_CLASSIC=y
 *   - Exposes speaker_bt_player_feed_pcm() for future Opus→PCM→player_compat_feed BG path
 *
 * Full port checklist (reference: .../tuya_comm_bt_player.c):
 *   1. Init Opus decoder + ring buffer + decode thread (16 kHz mono).
 *   2. Register TUYA_AUDIO_PLAYER_TYPE_MUSIC device or call player_compat_play(FALSE,…)
 *      + player_compat_feed(FALSE, pcm, …) from __simple_write_data.
 *   3. Link tuya_comm_classic_bt / A2DP stack and route protocol packets to Opus ring.
 *   4. Pause/resume cloud BG via voice_app_music_pause on BT play (restore_action).
 *   5. DP report helpers (206/5/6) when stack state changes.
 */

#include "speaker_bt_player.h"

#include "speaker_config.h"

#include "tal_log.h"

#include <string.h>

#if defined(CONFIG_SPEAKER_BT_CLASSIC) && CONFIG_SPEAKER_BT_CLASSIC
#define SPEAKER_BT_STACK_ENABLED 1
#else
#define SPEAKER_BT_STACK_ENABLED 0
#endif

typedef struct {
    BOOL_T inited;
    BOOL_T bt_enabled;
    BOOL_T bt_switch;
    BOOL_T bt_visible;
} speaker_bt_ctx_t;

static speaker_bt_ctx_t sg_bt;

int speaker_bt_player_init(void)
{
    if (sg_bt.inited) {
        return OPRT_OK;
    }
    memset(&sg_bt, 0, sizeof(sg_bt));
    sg_bt.inited = TRUE;
    PR_NOTICE("speaker_bt_player: scaffold init (stack=%d)", SPEAKER_BT_STACK_ENABLED);
    return OPRT_OK;
}

void speaker_bt_player_deinit(void)
{
    memset(&sg_bt, 0, sizeof(sg_bt));
}

static void _bt_scaffold_notice(const char *what)
{
#if SPEAKER_BT_STACK_ENABLED
    PR_WARN("speaker_bt_player: %s — CONFIG_SPEAKER_BT_CLASSIC set but A2DP/Opus stack not ported", what);
#else
    PR_WARN("speaker_bt_player: %s — BT not enabled (set CONFIG_SPEAKER_BT_CLASSIC=y after stack port)", what);
#endif
}

int speaker_bt_dp_handle_enable(BOOL_T enable)
{
    if (!sg_bt.inited) {
        return OPRT_COM_ERROR;
    }
    sg_bt.bt_enabled = enable;
    if (enable) {
        sg_bt.bt_switch  = TRUE;
        sg_bt.bt_visible = TRUE;
    } else {
        sg_bt.bt_switch  = FALSE;
        sg_bt.bt_visible = FALSE;
    }
    PR_NOTICE("speaker_bt_player: DP206 enable=%d (stored, stack not active)", enable);
    _bt_scaffold_notice("DP206 enable");
    return OPRT_OK;
}

int speaker_bt_dp_handle_switch(BOOL_T enable)
{
    if (!sg_bt.inited) {
        return OPRT_COM_ERROR;
    }
    sg_bt.bt_switch = enable;
    if (!enable) {
        sg_bt.bt_visible = FALSE;
    }
    PR_NOTICE("speaker_bt_player: DP5 switch=%d (stored)", enable);
    _bt_scaffold_notice("DP5 switch");
    return OPRT_OK;
}

int speaker_bt_dp_handle_visible(BOOL_T visible)
{
    if (!sg_bt.inited) {
        return OPRT_COM_ERROR;
    }
    sg_bt.bt_visible = visible;
    PR_NOTICE("speaker_bt_player: DP6 visible=%d (stored)", visible);
    _bt_scaffold_notice("DP6 visible");
    return OPRT_OK;
}

int speaker_bt_player_feed_pcm(const int16_t *pcm, uint32_t samples)
{
    (void)pcm;
    (void)samples;
    _bt_scaffold_notice("feed_pcm");
    return OPRT_NOT_SUPPORTED;
}

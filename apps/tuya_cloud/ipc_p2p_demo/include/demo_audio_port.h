/**
 * @file demo_audio_port.h
 * @brief The five things the demo needs from a sound device
 * @version 1.0
 * @date 2026-09-02
 * @copyright Copyright (c) Tuya Inc.
 *
 * Everything above this line - G.711, the uplink ring, the duplex ducking, the
 * p2p callbacks - is one implementation shared by every board. Only the calls
 * that touch a driver differ: both T5AI and Linux open tdl_audio (board AFE
 * at 16 kHz, converted to 8 kHz here). So those calls, and nothing else, live
 * behind this header.
 *
 * PCM is 16-bit mono at DEMO_AUDIO_PORT_RATE in both directions. G.711 is an
 * 8 kHz codec and the demo encodes to it, so there is nothing to gain from
 * carrying a wider band through the app.
 */
#ifndef __DEMO_AUDIO_PORT_H__
#define __DEMO_AUDIO_PORT_H__

#include "tuya_cloud_types.h"

#ifdef __cplusplus
extern "C" {
#endif

#define DEMO_AUDIO_PORT_RATE 8000

/**
 * @brief Handed one captured frame, 16-bit mono
 * @param[in] pcm samples, valid only for the duration of the call
 * @param[in] samples how many
 * @note Called from whatever thread the driver captures on, once per frame.
 *       The frame length is the driver's, not the caller's: T5AI delivers 20 ms
 *       and ALSA delivers a period.
 */
typedef void (*DEMO_AUDIO_PORT_MIC_CB)(const int16_t *pcm, uint32_t samples);

/**
 * @brief Bring up capture and playback together
 * @param[in] cb where captured frames go
 * @return OPRT_OK when both directions are running
 * @note Both directions come up at once because full duplex is the only mode
 *       the demo uses, and on T5AI the canceller needs them opened as a pair.
 */
OPERATE_RET demo_audio_port_open(DEMO_AUDIO_PORT_MIC_CB cb);

/**
 * @brief Stop capture and playback
 */
void demo_audio_port_close(void);

/**
 * @brief Queue one frame for the speaker
 * @param[in] pcm 16-bit mono at DEMO_AUDIO_PORT_RATE
 * @param[in] samples how many
 * @return OPRT_OK when the driver took it
 */
OPERATE_RET demo_audio_port_play(const int16_t *pcm, uint32_t samples);

/**
 * @brief Set playback gain
 * @param[in] vol 0-100
 */
OPERATE_RET demo_audio_port_volume(uint8_t vol);

#ifdef __cplusplus
}
#endif

#endif /* __DEMO_AUDIO_PORT_H__ */

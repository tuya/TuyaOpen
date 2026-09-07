/**
 * @file demo_media_event.h
 * @brief Demo MEDIA_STREAM event callback registration
 * @version 1.0
 * @date 2026-08-06
 * @copyright Copyright (c) Tuya Inc.
 */
#ifndef __DEMO_MEDIA_EVENT_H__
#define __DEMO_MEDIA_EVENT_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "tuya_cloud_types.h"

/**
 * @brief Register demo media stream event callback (playback query/ctrl)
 * @return none
 * @note Call after TUYA_APP_Start()/p2p_init()
 */
void demo_media_event_register(void);

/** @brief Stop the playback send thread if it is running. */
void demo_media_pb_stop(void);

#ifdef __cplusplus
}
#endif

#endif /* __DEMO_MEDIA_EVENT_H__ */

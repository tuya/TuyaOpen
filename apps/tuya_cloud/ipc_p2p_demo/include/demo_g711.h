/**
 * @file demo_g711.h
 * @brief G.711 u-law, the ITU reference algorithm
 * @version 1.0
 * @date 2026-09-02
 * @copyright Copyright (c) Tuya Inc.
 */
#ifndef __DEMO_G711_H__
#define __DEMO_G711_H__

#include "tuya_cloud_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief One 16-bit sample to one u-law byte
 */
uint8_t demo_g711u_encode_sample(int pcm_val);

/**
 * @brief One u-law byte back to a finished 16-bit sample
 * @note The result is already full scale; scaling it again clips.
 */
int demo_g711u_decode_sample(uint8_t u_val);

#ifdef __cplusplus
}
#endif

#endif /* __DEMO_G711_H__ */

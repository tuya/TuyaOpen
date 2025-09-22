/**
 * @file tkl_vad.h
 * @version 0.1
 * @date 2025-04-15
 */

#ifndef __TKL_VAD_H__
#define __TKL_VAD_H__

#include "tuya_cloud_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/***********************************************************
************************macro define************************
***********************************************************/


/***********************************************************
***********************typedef define***********************
***********************************************************/
typedef struct {
    uint32_t sample_rate;
    uint8_t  channel_num;
    int      speech_min_ms;
    int      noise_min_ms;
    int      frame_duration_ms;
    float    scale;
}TKL_VAD_CONFIG_T;

typedef uint8_t TKL_VAD_STATUS_T;
#define TKL_VAD_STATUS_NONE 0
#define TKL_VAD_STATUS_SPEECH 1

/***********************************************************
********************function declaration********************
***********************************************************/

OPERATE_RET tkl_vad_init(TKL_VAD_CONFIG_T *config);

OPERATE_RET tkl_vad_feed(uint8_t *data, uint32_t len);

TKL_VAD_STATUS_T tkl_vad_get_status(void);

OPERATE_RET tkl_vad_start(void);

OPERATE_RET tkl_vad_stop(void);

OPERATE_RET tkl_vad_deinit(void);

#ifdef __cplusplus
}
#endif

#endif /* __TKL_VAD_H__ */

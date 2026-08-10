/**
 * @file demo_audio_aec.c
 * @brief Speex AEC + RNN VAD (align OS wukong_audio_aec_vad.c)
 * @version 1.1
 * @date 2026-08-06
 * @copyright Copyright (c) Tuya Inc.
 *
 * Vendor AFE (tkl_ai enable=1) supplies mic+ref; registering this callback
 * replaces built-in aec_proc with Speex AES + RNN VAD (same path as TuyaOS).
 */
#include "demo_audio_aec.h"
#include "speexdsp_aes.h"
#include "audio_subsys_rnn_vad.h"
#include "tal_log.h"
#include "tal_memory.h"
#include <string.h>

/* ---------------------------------------------------------------------------
 * Macros
 * --------------------------------------------------------------------------- */
#define DEMO_AEC_SPEEX_LEVEL 5
#define DEMO_AEC_NS_LEVEL1   8
#define DEMO_AEC_NS_LEVEL2   10
#define DEMO_AEC_VAD_THR_MID (-50)

#if defined(ENABLE_EXT_RAM) && (ENABLE_EXT_RAM == 1)
#define DEMO_AEC_MALLOC(s) tal_psram_malloc(s)
#define DEMO_AEC_FREE(p)   tal_psram_free(p)
#else
#define DEMO_AEC_MALLOC(s) tal_malloc(s)
#define DEMO_AEC_FREE(p)   tal_free(p)
#endif

/* ---------------------------------------------------------------------------
 * File scope variables
 * --------------------------------------------------------------------------- */
STATIC VOID_T *s_speex = NULL;
STATIC VOID_T *s_rnn_vad = NULL;
STATIC UINT16_T *s_linearaec = NULL;
STATIC UINT32_T s_frame_bytes = 0;
STATIC DEMO_AUDIO_VAD_FLAG_E s_vad_flag = DEMO_AUDIO_VAD_STOP;
STATIC BOOL_T s_ready = FALSE;

/* ---------------------------------------------------------------------------
 * Weak fallback: some libaudio_subsys builds omit speex_ns_set_param
 * --------------------------------------------------------------------------- */
/**
 * @brief Weak NS param stub when symbol missing from libaudio_subsys
 * @param[in] obj Speex handle
 * @param[in] level1 NS level1
 * @param[in] level2 NS level2
 * @return 0
 */
__attribute__((weak)) int speex_ns_set_param(void *obj, int level1, int level2)
{
    (VOID_T)obj;
    (VOID_T)level1;
    (VOID_T)level2;
    return 0;
}

/* ---------------------------------------------------------------------------
 * Function implementations
 * --------------------------------------------------------------------------- */
/**
 * @brief Init Speex AES + RNN VAD
 * @param[in] min_speech_len_ms min speech length
 * @param[in] max_speech_interval_ms max speech interval
 * @param[in] frame_bytes frame size in bytes
 * @return OPRT_OK on success
 */
OPERATE_RET demo_audio_aec_init(UINT32_T min_speech_len_ms, UINT32_T max_speech_interval_ms, UINT32_T frame_bytes)
{
    rnn_vad_param_in vad_param;

    if (s_ready) {
        return OPRT_OK;
    }
    if (frame_bytes == 0U || (frame_bytes % 2U) != 0U) {
        PR_ERR("invalid frame_bytes=%u", (UINT_T)frame_bytes);
        return OPRT_INVALID_PARM;
    }

    /* OS: speex_aes_create(frame_size / 2) — frame_size is bytes */
    s_speex = speex_aes_create((INT_T)(frame_bytes / 2U));
    if (s_speex == NULL) {
        PR_ERR("speex_aes_create failed");
        return OPRT_COM_ERROR;
    }
    (VOID_T) speex_aes_set_param(s_speex, DEMO_AEC_SPEEX_LEVEL);
    (VOID_T) speex_ns_set_param(s_speex, DEMO_AEC_NS_LEVEL1, DEMO_AEC_NS_LEVEL2);

    s_rnn_vad = rnn_vad_create();
    if (s_rnn_vad == NULL) {
        PR_ERR("rnn_vad_create failed");
        demo_audio_aec_deinit();
        return OPRT_COM_ERROR;
    }
    memset(&vad_param, 0, sizeof(vad_param));
    vad_param.min_speech_len = (float)min_speech_len_ms;
    vad_param.max_speech_interval = (float)max_speech_interval_ms;
    (VOID_T) rnn_vad_init(&vad_param, s_rnn_vad);
    rnn_vad_set_callback(s_rnn_vad, (float)DEMO_AEC_VAD_THR_MID);

    /* OS: linearaec = malloc(frame_size * 2) */
    s_linearaec = (UINT16_T *)DEMO_AEC_MALLOC(frame_bytes * 2U);
    if (s_linearaec == NULL) {
        PR_ERR("linearaec alloc failed size=%u", (UINT_T)(frame_bytes * 2U));
        demo_audio_aec_deinit();
        return OPRT_MALLOC_FAILED;
    }
    memset(s_linearaec, 0, frame_bytes * 2U);

    s_frame_bytes = frame_bytes;
    s_vad_flag = DEMO_AUDIO_VAD_STOP;
    s_ready = TRUE;
    PR_NOTICE("demo Speex+RNN ready frame_bytes=%u speech=%u/%u thr=%d",
              (UINT_T)frame_bytes, (UINT_T)min_speech_len_ms, (UINT_T)max_speech_interval_ms,
              DEMO_AEC_VAD_THR_MID);
    return OPRT_OK;
}

/**
 * @brief Destroy Speex/RNN and free linearaec
 * @return none
 */
VOID_T demo_audio_aec_deinit(VOID_T)
{
    if (s_linearaec != NULL) {
        DEMO_AEC_FREE(s_linearaec);
        s_linearaec = NULL;
    }
    if (s_rnn_vad != NULL) {
        rnn_vad_destroy(s_rnn_vad);
        s_rnn_vad = NULL;
    }
    if (s_speex != NULL) {
        speex_aes_destory(s_speex);
        s_speex = NULL;
    }
    s_frame_bytes = 0;
    s_vad_flag = DEMO_AUDIO_VAD_STOP;
    s_ready = FALSE;
}

/**
 * @brief Soft AEC + RNN VAD process for tkl_ai_set_vad_aec_algorithm
 * @param[in] mic_data near-end
 * @param[in] ref_data far-end reference
 * @param[out] out_data cancelled output
 * @return 0 on success, -1 on error
 */
INT_T demo_audio_aec_process(SHORT_T *mic_data, SHORT_T *ref_data, SHORT_T *out_data)
{
    STATIC UINT_T s_cnt = 0;
    BOOL_T has_vad;
    UINT32_T samples;

    if (mic_data == NULL || ref_data == NULL || out_data == NULL) {
        return -1;
    }
    samples = (s_frame_bytes > 0U) ? (s_frame_bytes / 2U) : 320U;
    if (!s_ready || s_speex == NULL) {
        memcpy(out_data, mic_data, samples * sizeof(SHORT_T));
        return -1;
    }

    (VOID_T) speex_aes_process(s_speex, (short *)mic_data, (short *)ref_data, (short *)out_data);

    if (s_rnn_vad != NULL) {
        has_vad = (BOOL_T)rnn_vad_process(s_rnn_vad, (short *)out_data);
        if (has_vad && s_vad_flag != DEMO_AUDIO_VAD_START) {
            PR_NOTICE("################ [vad start] ################");
            s_vad_flag = DEMO_AUDIO_VAD_START;
        } else if (!has_vad && s_vad_flag != DEMO_AUDIO_VAD_STOP) {
            PR_NOTICE("################ [vad stop] ################");
            s_vad_flag = DEMO_AUDIO_VAD_STOP;
        }
    }

    if (s_linearaec != NULL) {
        (VOID_T) speex_get_param(s_speex, NULL, (short *)s_linearaec);
    }

    if ((s_cnt++ % 500U) == 0U) {
        PR_DEBUG("speex+rnn n=%u vad=%d", (UINT_T)s_cnt, (INT_T)s_vad_flag);
    }
    return 0;
}

/**
 * @brief Start RNN VAD
 * @return OPRT_OK on success
 */
OPERATE_RET demo_audio_aec_vad_start(VOID_T)
{
    s_vad_flag = DEMO_AUDIO_VAD_STOP;
    if (s_rnn_vad != NULL) {
        rnn_vad_start(s_rnn_vad);
    }
    return OPRT_OK;
}

/**
 * @brief Stop RNN VAD
 * @return OPRT_OK on success
 */
OPERATE_RET demo_audio_aec_vad_stop(VOID_T)
{
    s_vad_flag = DEMO_AUDIO_VAD_STOP;
    if (s_rnn_vad != NULL) {
        rnn_vad_stop(s_rnn_vad);
    }
    return OPRT_OK;
}

/**
 * @brief Get current VAD flag
 * @return DEMO_AUDIO_VAD_START or DEMO_AUDIO_VAD_STOP
 */
INT_T demo_audio_aec_vad_get_flag(VOID_T)
{
    return (INT_T)s_vad_flag;
}

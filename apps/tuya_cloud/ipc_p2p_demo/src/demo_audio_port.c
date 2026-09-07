/**
 * @file demo_audio_port.c
 * @brief demo_audio_port on board tdl_audio (16 kHz AFE, 8 kHz to the app)
 * @version 1.0
 * @date 2026-09-03
 * @copyright Copyright (c) Tuya Inc.
 */
#include "tuya_cloud_types.h"

#if OPERATING_SYSTEM != SYSTEM_LINUX

#include "demo_audio_port.h"
#include "tal_api.h"
#include "tkl_audio.h"
#include "tkl_gpio.h"
#include <string.h>

#if defined(ENABLE_AUDIO_CODECS) && (ENABLE_AUDIO_CODECS == 1)
#include "tdl_audio_manage.h"
#define PORT_HAS_TDL_AUDIO 1
#else
#define PORT_HAS_TDL_AUDIO 0
#endif

/* Exported by the T5AI adapter but absent from its headers. */
extern void tkl_ai_disable_vendor_vad(void);

#ifndef AUDIO_CODEC_NAME
#define AUDIO_CODEC_NAME "audio_codec"
#endif

/* Align boards/T5AI/TUYA_T5AI_BOARD: BOARD_SPEAKER_EN_PIN=GPIO28, high-enable */
#define PORT_SPK_GPIO          TUYA_GPIO_NUM_28
#define PORT_SPK_GPIO_POLARITY 0
#define PORT_SPK_VOLUME        70
#define PORT_MIC_VOLUME        50

#if PORT_HAS_TDL_AUDIO

static DEMO_AUDIO_PORT_MIC_CB s_mic_cb = NULL;
static TDL_AUDIO_HANDLE_T     s_handle = NULL;
static volatile BOOL_T        s_running = FALSE;
static uint32_t               s_dev_rate = DEMO_AUDIO_PORT_RATE;
static uint32_t               s_decim = 1;

#define PORT_CONV_MAX 4096
#define PORT_FIR_TAPS 15

static const int32_t c_fir_q15[PORT_FIR_TAPS] = {
    -30,    197,   310,  -781, -1885,
    1587,  9777, 14418,  9777,  1587,
    -1885,  -781,   310,   197,   -30};
static int16_t s_fir_hist[PORT_FIR_TAPS - 1];

static uint32_t __port_downsample(const int16_t *in, uint32_t n, int16_t *out)
{
    static int16_t work[PORT_CONV_MAX + PORT_FIR_TAPS];
    uint32_t       i, k = 0;

    if (n > PORT_CONV_MAX) {
        n = PORT_CONV_MAX;
    }
    memcpy(work, s_fir_hist, sizeof(s_fir_hist));
    memcpy(work + (PORT_FIR_TAPS - 1), in, n * sizeof(int16_t));

    for (i = 0; i + PORT_FIR_TAPS <= n + (PORT_FIR_TAPS - 1); i += 2) {
        int64_t  acc = 0;
        uint32_t t;

        for (t = 0; t < PORT_FIR_TAPS; t++) {
            acc += (int64_t)c_fir_q15[t] * work[i + t];
        }
        acc >>= 15;
        if (acc > 32767) {
            acc = 32767;
        } else if (acc < -32768) {
            acc = -32768;
        }
        out[k++] = (int16_t)acc;
    }
    if (n >= (PORT_FIR_TAPS - 1)) {
        memcpy(s_fir_hist, in + n - (PORT_FIR_TAPS - 1), sizeof(s_fir_hist));
    }
    return k;
}

static void __port_mic_cb(TDL_AUDIO_FRAME_FORMAT_E type, TDL_AUDIO_STATUS_E status, uint8_t *data, uint32_t len)
{
    DEMO_AUDIO_PORT_MIC_CB cb = s_mic_cb;

    (void)status;
    if (!s_running || cb == NULL || data == NULL || len < sizeof(int16_t)) {
        return;
    }
    if (type != TDL_AUDIO_FRAME_FORMAT_PCM) {
        return;
    }
    {
        uint32_t samples = len / (uint32_t)sizeof(int16_t);

        if (s_decim == 1) {
            cb((const int16_t *)data, samples);
        } else {
            static int16_t down[PORT_CONV_MAX / 2 + 8];

            cb(down, __port_downsample((const int16_t *)data, samples, down));
        }
    }
}

static OPERATE_RET __port_enable_pa(void)
{
    OPERATE_RET rt;

    rt = tkl_gpio_write(PORT_SPK_GPIO, (PORT_SPK_GPIO_POLARITY == 0) ? TUYA_GPIO_LEVEL_HIGH : TUYA_GPIO_LEVEL_LOW);
    if (rt != OPRT_OK) {
        PR_ERR("speaker amplifier enable failed: %d", rt);
    }
    return rt;
}

OPERATE_RET demo_audio_port_open(DEMO_AUDIO_PORT_MIC_CB cb)
{
    TDL_AUDIO_INFO_T info;
    OPERATE_RET      rt;

    s_mic_cb = cb;

    rt = tdl_audio_find(AUDIO_CODEC_NAME, &s_handle);
    if (rt != OPRT_OK || s_handle == NULL) {
        PR_ERR("audio port: '%s' not registered, rt %d", AUDIO_CODEC_NAME, rt);
        return (rt != OPRT_OK) ? rt : OPRT_NOT_FOUND;
    }

    tkl_ai_disable_vendor_vad();

    s_running = TRUE;
    rt = tdl_audio_open(s_handle, __port_mic_cb);
    if (rt != OPRT_OK) {
        s_running = FALSE;
        s_handle = NULL;
        PR_ERR("tdl_audio_open failed: %d", rt);
        return rt;
    }

    rt = tkl_ai_set_vol(TKL_AUDIO_TYPE_BOARD, TKL_AI_0, PORT_MIC_VOLUME);
    if (rt != OPRT_OK) {
        PR_ERR("tkl_ai_set_vol(%d) failed: %d", PORT_MIC_VOLUME, rt);
    }

    (void)__port_enable_pa();
    (void)demo_audio_port_volume(PORT_SPK_VOLUME);

    memset(&info, 0, sizeof(info));
    if (tdl_audio_get_info(s_handle, &info) == OPRT_OK) {
        s_dev_rate = (info.sample_rate != 0) ? (uint32_t)info.sample_rate : DEMO_AUDIO_PORT_RATE;
        PR_NOTICE("audio port: tdl %uHz/%ubit/%uch frame %u", (uint32_t)info.sample_rate, (uint32_t)info.sample_bits,
                  (uint32_t)info.sample_ch_num, (uint32_t)info.frame_size);
    } else {
        s_dev_rate = TKL_AUDIO_SAMPLE_16K;
        PR_NOTICE("audio port: tdl_audio_get_info failed, assuming %uHz", s_dev_rate);
    }
    if (s_dev_rate == DEMO_AUDIO_PORT_RATE) {
        s_decim = 1;
    } else if (s_dev_rate == DEMO_AUDIO_PORT_RATE * 2) {
        s_decim = 2;
        memset(s_fir_hist, 0, sizeof(s_fir_hist));
        PR_NOTICE("audio port: converting %uHz <-> %u, mic vol %d", s_dev_rate, (uint32_t)DEMO_AUDIO_PORT_RATE,
                  PORT_MIC_VOLUME);
    } else {
        PR_ERR("audio port: board registered %uHz, only %u and %u are handled", s_dev_rate,
               (uint32_t)DEMO_AUDIO_PORT_RATE, (uint32_t)DEMO_AUDIO_PORT_RATE * 2);
        demo_audio_port_close();
        return OPRT_NOT_SUPPORTED;
    }
    return OPRT_OK;
}

void demo_audio_port_close(void)
{
    if (!s_running) {
        return;
    }
    s_running = FALSE;
    if (s_handle != NULL) {
        (void)tdl_audio_close(s_handle);
        s_handle = NULL;
    }
    PR_NOTICE("audio port closed");
}

OPERATE_RET demo_audio_port_play(const int16_t *pcm, uint32_t samples)
{
    if (s_handle == NULL || pcm == NULL || samples == 0) {
        return OPRT_INVALID_PARM;
    }
    if (s_decim == 1) {
        return tdl_audio_play(s_handle, (uint8_t *)pcm, samples * (uint32_t)sizeof(int16_t));
    }
    {
        static int16_t up[PORT_CONV_MAX];
        uint32_t       i, k = 0;

        if (samples > PORT_CONV_MAX / 2) {
            samples = PORT_CONV_MAX / 2;
        }
        for (i = 0; i < samples; i++) {
            int32_t a = pcm[i];
            int32_t b = (i + 1 < samples) ? pcm[i + 1] : a;

            up[k++] = (int16_t)a;
            up[k++] = (int16_t)((a + b) / 2);
        }
        return tdl_audio_play(s_handle, (uint8_t *)up, k * (uint32_t)sizeof(int16_t));
    }
}

OPERATE_RET demo_audio_port_volume(uint8_t vol)
{
    if (s_handle == NULL) {
        return OPRT_RESOURCE_NOT_READY;
    }
    return tdl_audio_volume_set(s_handle, vol);
}

#else /* !PORT_HAS_TDL_AUDIO */

OPERATE_RET demo_audio_port_open(DEMO_AUDIO_PORT_MIC_CB cb)
{
    (void)cb;
    PR_WARN("audio port: built without ENABLE_AUDIO_CODECS");
    return OPRT_NOT_SUPPORTED;
}

void demo_audio_port_close(void)
{
}

OPERATE_RET demo_audio_port_play(const int16_t *pcm, uint32_t samples)
{
    (void)pcm;
    (void)samples;
    return OPRT_NOT_SUPPORTED;
}

OPERATE_RET demo_audio_port_volume(uint8_t vol)
{
    (void)vol;
    return OPRT_NOT_SUPPORTED;
}

#endif /* PORT_HAS_TDL_AUDIO */

#endif /* OPERATING_SYSTEM != SYSTEM_LINUX */

/**
 * @file example_recorder.c
 * @brief Audio recorder example for SDK.
 *
 * This file provides an example implementation of an audio recorder using the Tuya SDK.
 * It demonstrates the configuration and usage of audio recording and playback functionalities.
 * The example covers initializing the audio system, configuring audio parameters (sample rate, sample bits, channel),
 * recording audio from a microphone, and playing back the recorded audio. It also includes handling file operations
 * for storing and reading audio data.
 *
 * The audio recorder example aims to help developers understand how to integrate audio recording and playback features
 * in their Tuya IoT projects for applications requiring audio interaction. It includes detailed examples of setting up
 * audio configurations, handling audio data, and integrating these functionalities within a multitasking environment.
 *
 * @note This example is designed to be adaptable to various Tuya IoT devices and platforms, showcasing fundamental
 * audio operations critical for IoT device development.
 *
 * @copyright Copyright (c) 2021-2024 Tuya Inc. All Rights Reserved.
 *
 */
#include "tuya_cloud_types.h"

#include "tuya_ringbuf.h"

#include "tal_api.h"

#include "tkl_output.h"
#include "tkl_fs.h"
#include "tkl_memory.h"
#include "tkl_audio.h"
#include "tkl_gpio.h"

#include "mp3dec.h"

#include "wav_encode.h"

/***********************************************************
************************macro define************************
***********************************************************/
#define USE_RING_BUFFER      0
#define USE_INTERNAL_FLASH   1 // Store recording file in internal flash
#define USE_SD_CARD          2
#define RECORDER_FILE_SOURCE USE_RING_BUFFER

#if (RECORDER_FILE_SOURCE == USE_INTERNAL_FLASH)
#define RECORDER_FILE_DIR      "/"
#define RECORDER_FILE_PATH     "/tuya_recorder.pcm"
#define RECORDER_WAV_FILE_PATH "/tuya_recorder.wav"
#elif (RECORDER_FILE_SOURCE == USE_SD_CARD)
#define RECORDER_FILE_DIR      "/sdcard"
#define RECORDER_FILE_PATH     "/sdcard/tuya_recorder.pcm"
#define RECORDER_WAV_FILE_PATH "/sdcard/tuya_recorder.wav"
#endif

#define SPEAKER_ENABLE_PIN EXAMPLE_AUDIO_SPEAKER_PIN
#define AUDIO_TRIGGER_PIN  EXAMPLE_AUDIO_TRIGGER_PIN

// MIC sample rate
#define MIC_SAMPLE_RATE TKL_AUDIO_SAMPLE_16K
// MIC sample bits
#define MIC_SAMPLE_BITS TKL_AUDIO_DATABITS_16
// MIC channel
#define MIC_CHANNEL TKL_AUDIO_CHANNEL_MONO
// Maximum recordable duration, unit ms
#define MIC_RECORD_DURATION_MS (3 * 1000)
// RINGBUF size
#define PCM_BUF_SIZE (MIC_RECORD_DURATION_MS * MIC_SAMPLE_RATE * MIC_SAMPLE_BITS * MIC_CHANNEL / 8 / 1000)
// 10ms PCM data size
#define PCM_FRAME_SIZE (10 * MIC_SAMPLE_RATE * MIC_SAMPLE_BITS * MIC_CHANNEL / 8 / 1000)

/***********************************************************
***********************typedef define***********************
***********************************************************/
struct recorder_ctx {
    TUYA_RINGBUFF_T pcm_buf;

    // Recording status
    BOOL_T recording;

    // Playing status
    BOOL_T playing;

    // Recording file handle
    TUYA_FILE file_hdl;
};

#include <modules/mp3dec.h>

extern const char media_src_hello_tuya_16k[9009];

#define PCM_SIZE_MAX (MAX_NSAMP * MAX_NCHAN * MAX_NGRAN)
#define MP3_FILE_ARRAY          media_src_hello_tuya_16k



struct speaker_mp3_ctx {
    HMP3Decoder decode_hdl;
    MP3FrameInfo frame_info;
    unsigned char *read_buf;
    uint32_t read_size; // valid data size in read_buf

    uint32_t mp3_offset; // current mp3 read position

    short *pcm_buf;
};

static struct speaker_mp3_ctx sg_mp3_ctx = {
    .decode_hdl = NULL,
    .read_buf = NULL,
    .read_size = 0,
    .mp3_offset = 0,
    .pcm_buf = NULL,
};


/***********************************************************
********************function declaration********************
***********************************************************/

/***********************************************************
***********************variable define**********************
***********************************************************/
static THREAD_HANDLE recorder_hdl = NULL;

struct recorder_ctx sg_recorder_ctx = {
    .pcm_buf = NULL,
    .recording = FALSE,
    .playing = FALSE,
    .file_hdl = NULL,
};

/***********************************************************
***********************function define**********************
***********************************************************/
static void app_audio_trigger_pin_init(void)
{
    OPERATE_RET rt = OPRT_OK;

    TUYA_GPIO_BASE_CFG_T pin_cfg = {
        .mode = TUYA_GPIO_PULLUP,
        .direct = TUYA_GPIO_INPUT,
        .level = TUYA_GPIO_LEVEL_HIGH,
    };
    TUYA_CALL_ERR_LOG(tkl_gpio_init(AUDIO_TRIGGER_PIN, &pin_cfg));

    return;
}

static BOOL_T audio_trigger_pin_is_pressed(void)
{
    TUYA_GPIO_LEVEL_E level = TUYA_GPIO_LEVEL_HIGH;
    tkl_gpio_read(AUDIO_TRIGGER_PIN, &level);
    return (level == TUYA_GPIO_LEVEL_LOW) ? TRUE : FALSE;
}

static int _audio_frame_put(TKL_AUDIO_FRAME_INFO_T *pframe)
{
    if (NULL == sg_recorder_ctx.pcm_buf) {
        return pframe->used_size;
    }

    if (sg_recorder_ctx.recording) {
        tuya_ring_buff_write(sg_recorder_ctx.pcm_buf, pframe->pbuf, pframe->used_size);
    }

    return pframe->used_size;
}

static void app_audio_init(void)
{
    int ret = 0;
    TKL_AUDIO_CONFIG_T config ={0};

    config.enable = false;
    config.card = TKL_AUDIO_TYPE_BOARD;
    config.ai_chn = TKL_AI_0;
    config.spk_gpio_polarity= 0;     // sample
    config.sample = MIC_SAMPLE_RATE;     // sample
    config.datebits = MIC_SAMPLE_BITS;   // datebit
    config.channel = TKL_AUDIO_CHANNEL_MONO;                         // channel num
    config.codectype = TKL_CODEC_AUDIO_PCM;     // codec type

    config.spk_gpio = SPEAKER_ENABLE_PIN;
    config.put_cb = _audio_frame_put;

    PR_DEBUG("TODO ... %s %d", __func__, __LINE__);

    ret = tkl_ai_init(&config, 1);

    ret |= tkl_ai_set_vol(TKL_AUDIO_TYPE_BOARD, 0, 80);

    ret |= tkl_ai_start(TKL_AUDIO_TYPE_BOARD,0);

    tkl_ao_set_vol(TKL_AUDIO_TYPE_BOARD, 0, NULL, 60);

    return;
}

static void app_mp3_decode_init(void)
{
    sg_mp3_ctx.read_buf = tkl_system_psram_malloc(MAINBUF_SIZE);
    if (sg_mp3_ctx.read_buf == NULL) {
        PR_ERR("mp3 read buf malloc failed!");
        return;
    }

    sg_mp3_ctx.pcm_buf = tkl_system_psram_malloc(PCM_SIZE_MAX * 2);
    if (sg_mp3_ctx.pcm_buf == NULL) {
        PR_ERR("pcm_buf malloc failed!");
        return;
    }

    sg_mp3_ctx.decode_hdl = MP3InitDecoder();
    if (sg_mp3_ctx.decode_hdl == NULL) {
        tkl_system_psram_free(sg_mp3_ctx.read_buf);
        sg_mp3_ctx.read_buf = NULL;
        tkl_system_psram_free(sg_mp3_ctx.pcm_buf);
        sg_mp3_ctx.pcm_buf = NULL;
        PR_ERR("MP3Decoder init failed!");
        return;
    }

    return;
}

static void app_speaker_play(void)
{
    int rt = 0;
    uint32_t head_offset = 0;
    unsigned char *mp3_frame_head = NULL;
    uint32_t decode_size_remain = 0;
    uint32_t read_size_remain = 0;

    if (sg_mp3_ctx.decode_hdl == NULL || sg_mp3_ctx.read_buf == NULL || sg_mp3_ctx.pcm_buf == NULL) {
        PR_ERR("MP3Decoder init fail!");
        return;
    }

    memset(sg_mp3_ctx.read_buf, 0, MAINBUF_SIZE);
    memset(sg_mp3_ctx.pcm_buf, 0, PCM_SIZE_MAX * 2);
    sg_mp3_ctx.read_size = 0;
    sg_mp3_ctx.mp3_offset = 0;

    do {
        // 1. read mp3 data
        // Audio file frequency should match the configured spk_sample
        // You can use https://convertio.co/zh/ website for online audio format and frequency conversion
        if (mp3_frame_head != NULL && decode_size_remain > 0) {
            memmove(sg_mp3_ctx.read_buf, mp3_frame_head, decode_size_remain);
            sg_mp3_ctx.read_size = decode_size_remain;
        }

        if (sg_mp3_ctx.mp3_offset >= sizeof(MP3_FILE_ARRAY)) { // mp3 file reading completed
            if (decode_size_remain == 0) {                     // last frame data decoding and playback completed
                PR_NOTICE("mp3 play finish!");
                break;
            } else {
                goto __MP3_DECODE;
            }
        }

        read_size_remain = MAINBUF_SIZE - sg_mp3_ctx.read_size;
        if (read_size_remain > sizeof(MP3_FILE_ARRAY) - sg_mp3_ctx.mp3_offset) {
            read_size_remain =
                sizeof(MP3_FILE_ARRAY) - sg_mp3_ctx.mp3_offset; // remaining data is less than read_buf size
        }
        if (read_size_remain > 0) {
            memcpy(sg_mp3_ctx.read_buf + sg_mp3_ctx.read_size, MP3_FILE_ARRAY + sg_mp3_ctx.mp3_offset,
                   read_size_remain);
            sg_mp3_ctx.read_size += read_size_remain;
            sg_mp3_ctx.mp3_offset += read_size_remain;
        }

    __MP3_DECODE:
        // 2. decode mp3 data
        head_offset = MP3FindSyncWord(sg_mp3_ctx.read_buf, sg_mp3_ctx.read_size);
        if (head_offset < 0) {
            PR_ERR("MP3FindSyncWord not find!");
            break;
        }

        mp3_frame_head = sg_mp3_ctx.read_buf + head_offset;
        decode_size_remain = sg_mp3_ctx.read_size - head_offset;
        rt = MP3Decode(sg_mp3_ctx.decode_hdl, &mp3_frame_head, (int *)&decode_size_remain, sg_mp3_ctx.pcm_buf, 0);
        if (rt != ERR_MP3_NONE) {
            PR_ERR("MP3Decode failed, code is %d", rt);
            break;
        }

        memset(&sg_mp3_ctx.frame_info, 0, sizeof(MP3FrameInfo));
        MP3GetLastFrameInfo(sg_mp3_ctx.decode_hdl, &sg_mp3_ctx.frame_info);

        // 3. play pcm data
        TKL_AUDIO_FRAME_INFO_T frame;
        frame.pbuf = (char *)sg_mp3_ctx.pcm_buf;
        frame.used_size = sg_mp3_ctx.frame_info.outputSamps * 2;
        tkl_ao_put_frame(0, 0, NULL, &frame);
    } while (1);

    return;
}


static void app_mic_record(void)
{
#if RECORDER_FILE_SOURCE == USE_RING_BUFFER
    // Nothing to do
    // Recording through ring buffer will only record the first MIC_RECORD_DURATION_MS of data
    // It will not record subsequent data to avoid overwriting data in the ring buffer, which would corrupt the PCM
    // format and cause playback anomalies
#elif (RECORDER_FILE_SOURCE == USE_INTERNAL_FLASH) || (RECORDER_FILE_SOURCE == USE_SD_CARD)
    if (NULL == sg_recorder_ctx.file_hdl) {
        return;
    }

    uint32_t data_len = 0;
    data_len = tuya_ring_buff_used_size_get(sg_recorder_ctx.pcm_buf);
    if (data_len == 0) {
        return;
    }

    char *read_buf = tkl_system_psram_malloc(data_len);
    if (NULL == read_buf) {
        PR_ERR("tkl_system_psram_malloc failed");
        return;
    }

    // Write to file
    tuya_ring_buff_read(sg_recorder_ctx.pcm_buf, read_buf, data_len);
    int rt_len = tkl_fwrite(read_buf, data_len, sg_recorder_ctx.file_hdl);
    if (rt_len != data_len) {
        PR_ERR("write file failed, maybe disk full");
        PR_ERR("write len %d, data len %d", rt_len, data_len);
    }

    if (read_buf) {
        tkl_system_psram_free(read_buf);
        read_buf = NULL;
    }
#endif
    return;
}

#if RECORDER_FILE_SOURCE == USE_RING_BUFFER
static void app_recode_play_from_ringbuf(void)
{
    if (NULL == sg_recorder_ctx.pcm_buf) {
        return;
    }

    uint32_t data_len = tuya_ring_buff_used_size_get(sg_recorder_ctx.pcm_buf);
    if (data_len == 0) {
        return;
    }

    uint32_t out_len = 0;
    char *frame_buf = tkl_system_psram_malloc(PCM_FRAME_SIZE);
    if (NULL == frame_buf) {
        PR_ERR("tkl_system_psram_malloc failed");
        return;
    }

    do {
        memset(frame_buf, 0, PCM_FRAME_SIZE);
        out_len = 0;

        data_len = tuya_ring_buff_used_size_get(sg_recorder_ctx.pcm_buf);
        if (data_len == 0) {
            break;
        }

        if (data_len > PCM_FRAME_SIZE) {
            tuya_ring_buff_read(sg_recorder_ctx.pcm_buf, frame_buf, PCM_FRAME_SIZE);
            out_len = PCM_FRAME_SIZE;
        } else {
            tuya_ring_buff_read(sg_recorder_ctx.pcm_buf, frame_buf, data_len);
            out_len = data_len;
        }

        // PR_NOTICE("data_len %d out_len %d", data_len, out_len);

        TKL_AUDIO_FRAME_INFO_T frame_info;
        frame_info.pbuf = frame_buf;
        frame_info.used_size = out_len;
        tkl_ao_put_frame(0, 0, NULL, &frame_info);
    } while (1);

    if (frame_buf) {
        tkl_system_psram_free(frame_buf);
        frame_buf = NULL;
    }
}
#endif

#if (RECORDER_FILE_SOURCE == USE_INTERNAL_FLASH) || (RECORDER_FILE_SOURCE == USE_SD_CARD)
static void app_recode_play_from_flash(void)
{
    // Read file
    TUYA_FILE file_hdl = tkl_fopen(RECORDER_FILE_PATH, "r");
    if (NULL == file_hdl) {
        PR_ERR("open file failed");
        return;
    }

    uint32_t data_len = 0;
    char *read_buf = tkl_system_psram_malloc(PCM_FRAME_SIZE);
    if (NULL == read_buf) {
        PR_ERR("tkl_system_psram_malloc failed");
        return;
    }

    do {
        memset(read_buf, 0, PCM_FRAME_SIZE);
        data_len = tkl_fread(read_buf, PCM_FRAME_SIZE, file_hdl);
        if (data_len == 0) {
            break;
        }

        TKL_AUDIO_FRAME_INFO_T frame_info;
        frame_info.pbuf = read_buf;
        frame_info.used_size = data_len;
        tkl_ao_put_frame(0, 0, NULL, &frame_info);
    } while (1);

    if (read_buf) {
        tkl_system_psram_free(read_buf);
        read_buf = NULL;
    }

    if (file_hdl) {
        tkl_fclose(file_hdl);
        file_hdl = NULL;
    }
}
#endif

static void app_recode_play(void)
{
#if RECORDER_FILE_SOURCE == USE_RING_BUFFER
    app_recode_play_from_ringbuf();
#elif (RECORDER_FILE_SOURCE == USE_INTERNAL_FLASH) || (RECORDER_FILE_SOURCE == USE_SD_CARD)
    app_recode_play_from_flash();
#endif
    return;
}

#if (RECORDER_FILE_SOURCE == USE_SD_CARD)
static OPERATE_RET app_pcm_to_wav(char *pcm_file)
{
    OPERATE_RET rt = OPRT_OK;

    uint8_t head[WAV_HEAD_LEN] = {0};
    uint32_t pcm_len = 0;
    uint32_t sample_rate = MIC_SAMPLE_RATE;
    uint16_t bit_depth = MIC_SAMPLE_BITS;
    uint16_t channel = MIC_CHANNEL;

    // Get pcm file length
    TUYA_FILE pcm_hdl = tkl_fopen(pcm_file, "r");
    if (NULL == pcm_hdl) {
        PR_ERR("open file failed");
        return OPRT_FILE_OPEN_FAILED;
    }
    tkl_fseek(pcm_hdl, 0, 2);
    pcm_len = tkl_ftell(pcm_hdl);

    tkl_fclose(pcm_hdl);
    pcm_hdl = NULL;

    PR_DEBUG("pcm file len %d", pcm_len);
    if (pcm_len == 0) {
        PR_ERR("pcm file is empty");
        return OPRT_COM_ERROR;
    }

    // Get wav head
    rt = app_get_wav_head(pcm_len, 1, sample_rate, bit_depth, channel, head);
    if (OPRT_OK != rt) {
        PR_ERR("app_get_wav_head failed, rt = %d", rt);
        return rt;
    }

    TAL_PR_HEXDUMP_DEBUG("wav head", head, WAV_HEAD_LEN);

    // Create wav file
    TUYA_FILE wav_hdl = tkl_fopen(RECORDER_WAV_FILE_PATH, "w");
    if (NULL == wav_hdl) {
        PR_ERR("open file: %s failed", RECORDER_WAV_FILE_PATH);
        rt = OPRT_FILE_OPEN_FAILED;
        goto __EXIT;
    }

    // Write wav head
    tkl_fwrite(head, WAV_HEAD_LEN, wav_hdl);

    // Read pcm file
    char *read_buf = tkl_system_psram_malloc(PCM_FRAME_SIZE);
    if (NULL == read_buf) {
        PR_ERR("tkl_system_psram_malloc failed");
        // return OPRT_COM_ERROR;
        rt = OPRT_MALLOC_FAILED;
        goto __EXIT;
    }

    PR_DEBUG("pcm file len %d", pcm_len);
    pcm_hdl = tkl_fopen(pcm_file, "r");
    if (NULL == pcm_hdl) {
        PR_ERR("open file failed");
        rt = OPRT_FILE_OPEN_FAILED;
        goto __EXIT;
    }

    tkl_fseek(pcm_hdl, WAV_HEAD_LEN, 0);

    // Write wav data
    do {
        memset(read_buf, 0, PCM_FRAME_SIZE);
        uint32_t read_len = tkl_fread(read_buf, PCM_FRAME_SIZE, pcm_hdl);
        if (read_len == 0) {
            break;
        }

        tkl_fwrite(read_buf, read_len, wav_hdl);
    } while (1);

__EXIT:
    if (pcm_hdl) {
        tkl_fclose(pcm_hdl);
        pcm_hdl = NULL;
    }

    if (wav_hdl) {
        tkl_fclose(wav_hdl);
        wav_hdl = NULL;
    }

    if (read_buf) {
        tkl_system_psram_free(read_buf);
        read_buf = NULL;
    }

    return rt;
}
#endif

static void app_recorder_thread(void *arg)
{
    app_audio_trigger_pin_init();
    app_mp3_decode_init();
    app_audio_init();

    app_speaker_play();

    for (;;) {
        app_mic_record();

        if (FALSE == audio_trigger_pin_is_pressed()) {
            tal_system_sleep(100);

            // End recording
            if (TRUE == sg_recorder_ctx.recording) {
                sg_recorder_ctx.recording = FALSE;
                sg_recorder_ctx.playing = TRUE;
#if (RECORDER_FILE_SOURCE == USE_INTERNAL_FLASH) || (RECORDER_FILE_SOURCE == USE_SD_CARD)
                if (sg_recorder_ctx.file_hdl) {
                    tkl_fclose(sg_recorder_ctx.file_hdl);
                    sg_recorder_ctx.file_hdl = NULL;
                }
                // Convert pcm to wav
#if (RECORDER_FILE_SOURCE == USE_SD_CARD)
                rt = app_pcm_to_wav(RECORDER_FILE_PATH);
                if (OPRT_OK != rt) {
                    PR_ERR("app_pcm_to_wav failed, rt = %d", rt);
                }
#endif
#endif
                PR_DEBUG("stop recording");
            }

            // Start playing
            if (TRUE == sg_recorder_ctx.playing) {
                PR_DEBUG("start playing");
                sg_recorder_ctx.playing = FALSE;
                app_recode_play();
                PR_DEBUG("stop playing");
            }

            continue;
        }

        // Start recording
        if (FALSE == sg_recorder_ctx.recording) {
#if (RECORDER_FILE_SOURCE == USE_INTERNAL_FLASH) || (RECORDER_FILE_SOURCE == USE_SD_CARD)
            // If recording file exists, delete it
            BOOL_T is_exist = FALSE;
            tkl_fs_is_exist(RECORDER_FILE_PATH, &is_exist);
            if (is_exist == TRUE) {
                tkl_fs_remove(RECORDER_FILE_PATH);
                PR_DEBUG("remove file %s", RECORDER_FILE_PATH);
            }

            is_exist = FALSE;
            tkl_fs_is_exist(RECORDER_WAV_FILE_PATH, &is_exist);
            if (is_exist == TRUE) {
                tkl_fs_remove(RECORDER_WAV_FILE_PATH);
                PR_DEBUG("remove file %s", RECORDER_WAV_FILE_PATH);
            }

            // Create recording file
            sg_recorder_ctx.file_hdl = tkl_fopen(RECORDER_FILE_PATH, "w");
            if (NULL == sg_recorder_ctx.file_hdl) {
                PR_ERR("open file failed");
                continue;
            }
            PR_DEBUG("open file %s success", RECORDER_FILE_PATH);
#endif
            tuya_ring_buff_reset(sg_recorder_ctx.pcm_buf);
            sg_recorder_ctx.recording = TRUE;
            sg_recorder_ctx.playing = FALSE;
            PR_DEBUG("start recording");
        }

        tal_system_sleep(10);
    }
}

static OPERATE_RET app_fs_init(void)
{
    OPERATE_RET rt = OPRT_OK;

#if RECORDER_FILE_SOURCE == USE_INTERNAL_FLASH
    rt = tkl_fs_mount("/", DEV_INNER_FLASH);
    if (rt != OPRT_OK) {
        PR_ERR("mount fs failed ");
        return rt;
    }
    PR_DEBUG("mount inner flash success ");
#elif RECORDER_FILE_SOURCE == USE_SD_CARD
    rt = tkl_fs_mount("/sdcard", DEV_SDCARD);
    if (rt != OPRT_OK) {
        PR_ERR("mount sd card failed, please retry after format ");
        return rt;
    }
    PR_DEBUG("mount sd card success ");
#else
    return rt;
#endif

    return rt;
}

void user_main(void)
{
    OPERATE_RET rt = OPRT_OK;

    tal_log_init(TAL_LOG_LEVEL_DEBUG, 1024, (TAL_LOG_OUTPUT_CB)tkl_log_output);

    PR_NOTICE("Application information:");
    PR_NOTICE("Project name:        %s", PROJECT_NAME);
    PR_NOTICE("App version:         %s", PROJECT_VERSION);
    PR_NOTICE("Compile time:        %s", __DATE__);
    PR_NOTICE("TuyaOpen version:    %s", OPEN_VERSION);
    PR_NOTICE("TuyaOpen commit-id:  %s", OPEN_COMMIT);
    PR_NOTICE("Platform chip:       %s", PLATFORM_CHIP);
    PR_NOTICE("Platform board:      %s", PLATFORM_BOARD);
    PR_NOTICE("Platform commit-id:  %s", PLATFORM_COMMIT);

    // Initialize file system
    rt = app_fs_init();
    if (OPRT_OK != rt) {
        PR_ERR("app_fs_init failed, rt = %d", rt);
        return;
    }

    // Create PCM buffer
    if (NULL == sg_recorder_ctx.pcm_buf) {
        PR_DEBUG("create pcm buffer size %d", PCM_BUF_SIZE);
        rt = tuya_ring_buff_create(PCM_BUF_SIZE, OVERFLOW_PSRAM_STOP_TYPE, &sg_recorder_ctx.pcm_buf);
        if (OPRT_OK != rt) {
            PR_ERR("tuya_ring_buff_create failed, rt = %d", rt);
            return;
        }
    }

    THREAD_CFG_T thrd_param = {1024 * 6, THREAD_PRIO_3, "recorder task"};
    tal_thread_create_and_start(&recorder_hdl, NULL, NULL, app_recorder_thread, NULL, &thrd_param);
    return;
}

#if OPERATING_SYSTEM == SYSTEM_LINUX

/**
 * @brief main
 *
 * @param argc
 * @param argv
 * @return void
 */
void main(int argc, char *argv[])
{
    user_main();
}
#else

/* Tuya thread handle */
static THREAD_HANDLE ty_app_thread = NULL;

/**
 * @brief  task thread
 *
 * @param[in] arg:Parameters when creating a task
 * @return none
 */
static void tuya_app_thread(void *arg)
{
    user_main();

    tal_thread_delete(ty_app_thread);
    ty_app_thread = NULL;
}

void tuya_app_main(void)
{
    THREAD_CFG_T thrd_param = {4096, 4, "tuya_app_main"};
    tal_thread_create_and_start(&ty_app_thread, NULL, NULL, tuya_app_thread, NULL, &thrd_param);
}
#endif
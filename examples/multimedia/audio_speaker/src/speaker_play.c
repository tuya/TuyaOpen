/**
 * @file speaker_play.c
 * @brief Audio speaker playback example for MP3 audio playback
 *
 * This file demonstrates MP3 audio playback functionality using the Tuya SDK.
 * It includes MP3 decoding, audio output configuration, and playback control.
 * The example supports multiple audio sources including embedded C arrays,
 * internal flash storage, and SD card files.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#include "speaker_play.h"

#include "tal_api.h"

#include "tkl_output.h"
#include "tkl_fs.h"
#include "tkl_memory.h"
#include "tkl_audio.h"

#define MINIMP3_IMPLEMENTATION
#include "minimp3_ex.h"

#include "app_media.h"

/***********************************************************
************************macro define************************
***********************************************************/
// MP3 file source: internal flash, C array, SD card
#define USE_INTERNAL_FLASH 0
#define USE_C_ARRAY        1
#define USE_SD_CARD        2
#define MP3_FILE_SOURCE    USE_C_ARRAY

#define AUDIO_INPUT_CH    TKL_AI_1
#define AUDIO_CH_NUM      TKL_AUDIO_CHANNEL_MONO
#define AUDIO_TYPE        TKL_AUDIO_TYPE_BOARD
#define AUDIO_CODEC_TYPE  TKL_CODEC_AUDIO_PCM
#define AUDIO_SAMPLE_RATE TKL_AUDIO_SAMPLE_16K
#define AUDIO_SAMPLE_BITS 16

#define MP3_DATA_BUF_SIZE 1940

#define MAX_NGRAN 2   /* max granules */
#define MAX_NCHAN 2   /* max channels */
#define MAX_NSAMP 576 /* max samples per channel, per granule */

#define PCM_SIZE_MAX (MAX_NSAMP * MAX_NCHAN * MAX_NGRAN)

#define SPEAKER_ENABLE_PIN EXAMPLE_AUDIO_SPEAKER_PIN

#define MP3_FILE_ARRAY          media_src_hello_tuya_16k
#define MP3_FILE_INTERNAL_FLASH "/media/hello_tuya.mp3"
#define MP3_FILE_SD_CARD        "/sdcard/hello_tuya.mp3"

/***********************************************************
***********************typedef define***********************
***********************************************************/
struct speaker_mp3_ctx {
    mp3dec_t *mp3_dec;
    mp3dec_frame_info_t mp3_frame_info;

    unsigned char *read_buf;
    uint32_t read_size; // valid data size in read_buf

    uint32_t mp3_offset; // current mp3 read position

    short *pcm_buf;
};

/***********************************************************
********************function declaration********************
***********************************************************/

/***********************************************************
***********************variable define**********************
***********************************************************/
static THREAD_HANDLE speaker_hdl = NULL;

static struct speaker_mp3_ctx sg_mp3_ctx = {
    .mp3_dec = NULL,
    .read_buf = NULL,
    .read_size = 0,
    .mp3_offset = 0,
    .pcm_buf = NULL,
};

/***********************************************************
***********************function define**********************
***********************************************************/
static void app_fs_init(void)
{

#if MP3_FILE_SOURCE == USE_INTERNAL_FLASH
    OPERATE_RET rt = OPRT_OK;
    rt = tkl_fs_mount("/", DEV_INNER_FLASH);
    if (rt != OPRT_OK) {
        PR_ERR("mount fs failed ");
        return;
    }
#elif MP3_FILE_SOURCE == USE_SD_CARD
    OPERATE_RET rt = OPRT_OK;
    rt = tkl_fs_mount("/sdcard", DEV_SDCARD);
    if (rt != OPRT_OK) {
        PR_ERR("mount fs failed ");
        return;
    }
#endif

    PR_DEBUG("mount inner flash success ");

    return;
}

static void app_mp3_decode_init(void)
{
    sg_mp3_ctx.read_buf = tkl_system_psram_malloc(MP3_DATA_BUF_SIZE);
    if (sg_mp3_ctx.read_buf == NULL) {
        PR_ERR("mp3 read buf malloc failed!");
        return;
    }

    sg_mp3_ctx.pcm_buf = tkl_system_psram_malloc(PCM_SIZE_MAX * 2);
    if (sg_mp3_ctx.pcm_buf == NULL) {
        PR_ERR("pcm_buf malloc failed!");
        return;
    }

    sg_mp3_ctx.mp3_dec = (mp3dec_t *)tkl_system_psram_malloc(sizeof(mp3dec_t));
    if (NULL == sg_mp3_ctx.mp3_dec) {
        PR_ERR("malloc mp3dec_t failed");
        return;
    }
    mp3dec_init(sg_mp3_ctx.mp3_dec);

    return;
}

static int _audio_frame_put(TKL_AUDIO_FRAME_INFO_T *pframe)
{
    return 0;
}

static OPERATE_RET app_speaker_init(void)
{
    OPERATE_RET rt = OPRT_OK;
    TKL_AUDIO_CONFIG_T config = {0};

    config.enable = true;
    config.card = AUDIO_TYPE;
    config.ai_chn = AUDIO_INPUT_CH;
    config.sample = AUDIO_SAMPLE_RATE;
    config.datebits = AUDIO_SAMPLE_BITS;
    config.channel = AUDIO_CH_NUM;
    config.codectype = AUDIO_CODEC_TYPE;
    config.put_cb = _audio_frame_put;

    config.fps = 25; // frame per second，suggest 25
    config.mic_volume = 0x2d;
    config.spk_volume = 0x2d;

    config.spk_gpio_polarity = 0;
    config.spk_sample = AUDIO_SAMPLE_RATE;
    config.spk_gpio = SPEAKER_ENABLE_PIN;

    TUYA_CALL_ERR_RETURN(tkl_ai_init(&config, 1));

    TUYA_CALL_ERR_RETURN(tkl_ai_start(AUDIO_TYPE, AUDIO_INPUT_CH));

    TUYA_CALL_ERR_RETURN(tkl_ai_set_vol(AUDIO_TYPE, AUDIO_INPUT_CH, 80));

    TUYA_CALL_ERR_RETURN(tkl_ao_set_vol(AUDIO_TYPE, AUDIO_INPUT_CH, NULL, 60));

    return rt;
}

static void app_speaker_play(void)
{
    unsigned char *mp3_frame_head = NULL;
    uint32_t decode_size_remain = 0;
    uint32_t read_size_remain = 0;

    if (sg_mp3_ctx.mp3_dec == NULL || sg_mp3_ctx.read_buf == NULL || sg_mp3_ctx.pcm_buf == NULL) {
        PR_ERR("MP3Decoder init fail!");
        return;
    }

    memset(sg_mp3_ctx.read_buf, 0, MP3_DATA_BUF_SIZE);
    memset(sg_mp3_ctx.pcm_buf, 0, PCM_SIZE_MAX * 2);
    sg_mp3_ctx.read_size = 0;
    sg_mp3_ctx.mp3_offset = 0;

#if (MP3_FILE_SOURCE == USE_INTERNAL_FLASH) || (MP3_FILE_SOURCE == USE_SD_CARD)
    char *mp3_file_path = NULL;
    if (MP3_FILE_SOURCE == USE_INTERNAL_FLASH) {
        mp3_file_path = MP3_FILE_INTERNAL_FLASH;
    } else if (MP3_FILE_SOURCE == USE_SD_CARD) {
        mp3_file_path = MP3_FILE_SD_CARD;
    } else {
        PR_ERR("mp3 file source error!");
        return;
    }

    BOOL_T is_exist = FALSE;
    tkl_fs_is_exist(mp3_file_path, &is_exist);
    if (is_exist == FALSE) {
        PR_ERR("mp3 file not exist!");
        return;
    }

    TUYA_FILE mp3_file = tkl_fopen(mp3_file_path, "r");
    if (NULL == mp3_file) {
        PR_ERR("open mp3 file %s failed!", mp3_file_path);
        return;
    }
#endif

    do {
        // 1. read mp3 data
        // Audio file frequency should match the configured spk_sample
        // You can use https://convertio.co/zh/ website for online audio format and frequency conversion
        if (mp3_frame_head != NULL && decode_size_remain > 0) {
            memmove(sg_mp3_ctx.read_buf, mp3_frame_head, decode_size_remain);
            sg_mp3_ctx.read_size = decode_size_remain;
        }

#if MP3_FILE_SOURCE == USE_C_ARRAY
        if (sg_mp3_ctx.mp3_offset >= sizeof(MP3_FILE_ARRAY)) { // mp3 file reading completed
            if (decode_size_remain == 0) {                     // last frame data decoding and playback completed
                PR_NOTICE("mp3 play finish!");
                break;
            } else {
                goto __MP3_DECODE;
            }
        }

        read_size_remain = MP3_DATA_BUF_SIZE - sg_mp3_ctx.read_size;
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
#elif (MP3_FILE_SOURCE == USE_INTERNAL_FLASH) || (MP3_FILE_SOURCE == USE_SD_CARD)
        read_size_remain = MP3_DATA_BUF_SIZE - sg_mp3_ctx.read_size;
        int fs_read_len = tkl_fread(sg_mp3_ctx.read_buf + sg_mp3_ctx.read_size, read_size_remain, mp3_file);
        if (fs_read_len <= 0) {
            if (decode_size_remain == 0) { // last frame data decoding and playback completed
                PR_NOTICE("mp3 play finish!");
                break;
            } else {
                goto __MP3_DECODE;
            }
        } else {
            sg_mp3_ctx.read_size += fs_read_len;
        }
#endif

    __MP3_DECODE:
        // 2. decode mp3 data
        mp3_frame_head = sg_mp3_ctx.read_buf;
        int samples = mp3dec_decode_frame(sg_mp3_ctx.mp3_dec, mp3_frame_head, sg_mp3_ctx.read_size, sg_mp3_ctx.pcm_buf,
                                          &sg_mp3_ctx.mp3_frame_info);

        if (samples == 0) {
            decode_size_remain += 64;
            PR_ERR("mp3dec_decode_frame failed!");
            break;
        }
        mp3_frame_head += sg_mp3_ctx.mp3_frame_info.frame_bytes;
        decode_size_remain = sg_mp3_ctx.read_size - sg_mp3_ctx.mp3_frame_info.frame_bytes;

        // 3. play pcm data
        TKL_AUDIO_FRAME_INFO_T frame;
        frame.pbuf = (char *)sg_mp3_ctx.pcm_buf;
        frame.used_size = samples * 2;
        tkl_ao_put_frame(0, 0, NULL, &frame);
    } while (1);

#if (MP3_FILE_SOURCE == USE_INTERNAL_FLASH) || (MP3_FILE_SOURCE == USE_SD_CARD)
    if (mp3_file != NULL) {
        tkl_fclose(mp3_file);
        mp3_file = NULL;
    }
#endif

    return;
}

static void app_speaker_thread(void *arg)
{
    app_fs_init();
    app_mp3_decode_init();
    app_speaker_init();

    for (;;) {
        app_speaker_play();
        tal_system_sleep(3 * 1000);
    }
}

void user_main(void)
{
    THREAD_CFG_T thrd_param = {1024 * 6, THREAD_PRIO_3, "speaker task"};
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

    tal_thread_create_and_start(&speaker_hdl, NULL, NULL, app_speaker_thread, NULL, &thrd_param);
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
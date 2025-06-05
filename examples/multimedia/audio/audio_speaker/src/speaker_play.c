/**
 * @file speaker_play.c
 * @brief speaker_play module is used to
 * @version 0.1
 * @date 2025-02-17
 */

#include "speaker_play.h"

#include "tal_api.h"

#include "tkl_output.h"
#include "tkl_fs.h"
#include "tkl_memory.h"
#include "tkl_audio.h"

#include <modules/mp3dec.h>

#include "app_media.h"

/***********************************************************
************************macro define************************
***********************************************************/
// MP3 文件来源，内部flash， C array， SD卡
#define USE_INTERNAL_FLASH 0
#define USE_C_ARRAY        1
#define USE_SD_CARD        2
#define MP3_FILE_SOURCE    USE_C_ARRAY

#define MP3_DATA_BUF_SIZE 1940

#define PCM_SIZE_MAX (MAX_NSAMP * MAX_NCHAN * MAX_NGRAN)

#define SPEAKER_ENABLE_PIN TUYA_GPIO_NUM_28

#define MP3_FILE_ARRAY          media_src_hello_tuya_16k
#define MP3_FILE_INTERNAL_FLASH "/media/hello_tuya.mp3"
#define MP3_FILE_SD_CARD        "/sdcard/hello_tuya.mp3"

/***********************************************************
***********************typedef define***********************
***********************************************************/
struct speaker_mp3_ctx {
    HMP3Decoder decode_hdl;
    MP3FrameInfo frame_info;
    unsigned char *read_buf;
    uint32_t read_size; // read_buf 中有效数据大小

    uint32_t mp3_offset; // 当前 mp3 读取位置

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
    .decode_hdl = NULL,
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
    OPERATE_RET rt = OPRT_OK;

#if MP3_FILE_SOURCE == USE_INTERNAL_FLASH
    rt = tkl_fs_mount("/", DEV_INNER_FLASH);
    if (rt != OPRT_OK) {
        PR_ERR("mount fs failed ");
        return;
    }
#elif MP3_FILE_SOURCE == USE_SD_CARD
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

static int _audio_frame_put(TKL_AUDIO_FRAME_INFO_T *pframe)
{
    return pframe->buf_size;
}

static void app_speaker_init(void)
{
    TKL_AUDIO_CONFIG_T config;

    config.enable = 0;
    config.ai_chn = 0;
    config.sample = 16000;
    config.spk_sample = 16000;
    config.datebits = 16;
    config.channel = 1;
    config.codectype = TKL_CODEC_AUDIO_PCM;
    config.card = TKL_AUDIO_TYPE_BOARD;
    config.put_cb = _audio_frame_put;
    config.spk_gpio = SPEAKER_ENABLE_PIN;
    config.spk_gpio_polarity = 0;

    tkl_ai_init(&config, 0);

    tkl_ai_start(0, 0);

    tkl_ai_set_vol(0, 0, 80);

    tkl_ao_set_vol(TKL_AUDIO_TYPE_BOARD, 0, NULL, 30);

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
        // 音频文件的频率应和设置的 spk_sample 一致
        // 可以使用 https://convertio.co/zh/ 网站在线进行音频格式和频率转换
        if (mp3_frame_head != NULL && decode_size_remain > 0) {
            memmove(sg_mp3_ctx.read_buf, mp3_frame_head, decode_size_remain);
            sg_mp3_ctx.read_size = decode_size_remain;
        }

#if MP3_FILE_SOURCE == USE_C_ARRAY
        if (sg_mp3_ctx.mp3_offset >= sizeof(MP3_FILE_ARRAY)) { // mp3 文件读取完毕
            if (decode_size_remain == 0) {                     // 最后一帧数据解码播放完毕
                PR_NOTICE("mp3 play finish!");
                break;
            } else {
                goto __MP3_DECODE;
            }
        }

        read_size_remain = MAINBUF_SIZE - sg_mp3_ctx.read_size;
        if (read_size_remain > sizeof(MP3_FILE_ARRAY) - sg_mp3_ctx.mp3_offset) {
            read_size_remain = sizeof(MP3_FILE_ARRAY) - sg_mp3_ctx.mp3_offset; // 剩余数据小于 read_buf 大小
        }
        if (read_size_remain > 0) {
            memcpy(sg_mp3_ctx.read_buf + sg_mp3_ctx.read_size, MP3_FILE_ARRAY + sg_mp3_ctx.mp3_offset,
                   read_size_remain);
            sg_mp3_ctx.read_size += read_size_remain;
            sg_mp3_ctx.mp3_offset += read_size_remain;
        }
#elif (MP3_FILE_SOURCE == USE_INTERNAL_FLASH) || (MP3_FILE_SOURCE == USE_SD_CARD)
        read_size_remain = MAINBUF_SIZE - sg_mp3_ctx.read_size;
        int fs_read_len = tkl_fread(sg_mp3_ctx.read_buf + sg_mp3_ctx.read_size, read_size_remain, mp3_file);
        if (fs_read_len <= 0) {
            if (decode_size_remain == 0) { // 最后一帧数据解码播放完毕
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
        head_offset = MP3FindSyncWord(sg_mp3_ctx.read_buf, sg_mp3_ctx.read_size);
        if (head_offset < 0) {
            PR_ERR("MP3FindSyncWord not find!");
            break;
        }

        mp3_frame_head = sg_mp3_ctx.read_buf + head_offset;
        decode_size_remain = sg_mp3_ctx.read_size - head_offset;
        rt = MP3Decode(sg_mp3_ctx.decode_hdl, &mp3_frame_head, &decode_size_remain, sg_mp3_ctx.pcm_buf, 0);
        if (rt != ERR_MP3_NONE) {
            PR_ERR("MP3Decode failed, code is %d", rt);
            break;
        }

        memset(&sg_mp3_ctx.frame_info, 0, sizeof(MP3FrameInfo));
        MP3GetLastFrameInfo(sg_mp3_ctx.decode_hdl, &sg_mp3_ctx.frame_info);

        // 3. play pcm data
        TKL_AUDIO_FRAME_INFO_T frame;
        frame.pbuf = sg_mp3_ctx.pcm_buf;
        frame.used_size = sg_mp3_ctx.frame_info.outputSamps * 2;
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
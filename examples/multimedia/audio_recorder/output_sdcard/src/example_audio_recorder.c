/**
 * @file example_audio_recorder.c
 * @brief example_audio_recorder module is used to 
 * @version 0.1
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#include "tuya_cloud_types.h"
#include "tal_api.h"

#include "tuya_ringbuf.h"
#include "tkl_output.h"
#include "tkl_memory.h"
#include "tkl_fs.h"

#include "board_com_api.h"
#include "tdl_audio_manage.h"

#include "wav_encode.h"

#if defined(ENABLE_BUTTON) && (ENABLE_BUTTON == 1)
#include "tdl_button_manage.h"
#endif

/***********************************************************
************************macro define************************
***********************************************************/
// Maximum recordable duration, unit ms
#define EXAMPLE_RECORD_DURATION_MS (3 * 1000)

#define EXAMPLE_RECORDER_FILE_DIR      "/sdcard"
#define EXAMPLE_RECORDER_FILE_PATH     "/sdcard/tuya_recorder.pcm"
#define EXAMPLE_RECORDER_WAV_FILE_PATH "/sdcard/tuya_recorder.wav"

/***********************************************************
***********************typedef define***********************
***********************************************************/
typedef enum {
    RECORDER_STATUS_IDLE = 0,
    RECORDER_STATUS_START,
    RECORDER_STATUS_RECORDING,
    RECORDER_STATUS_END,
} RECORDER_STATUS_E;

/***********************************************************
***********************variable define**********************
***********************************************************/
static RECORDER_STATUS_E sg_recorder_status = RECORDER_STATUS_IDLE;
static TDL_AUDIO_HANDLE_T sg_audio_hdl = NULL;
static TDL_AUDIO_INFO_T sg_audio_info = {0};
static TUYA_RINGBUFF_T sg_recorder_pcm_rb = NULL;
static TUYA_FILE sg_recorder_file_hdl = NULL;

/***********************************************************
***********************function define**********************
***********************************************************/
#if defined(ENABLE_BUTTON) && (ENABLE_BUTTON == 1)
static void __button_function_cb(char *name, TDL_BUTTON_TOUCH_EVENT_E event, void *argc)
{
    switch (event) {
    case TDL_BUTTON_PRESS_DOWN: {
        PR_NOTICE("%s: single click", name);
        sg_recorder_status = RECORDER_STATUS_START;
    } break;

    case TDL_BUTTON_PRESS_UP: {
        PR_NOTICE("%s: release", name);
        sg_recorder_status = RECORDER_STATUS_END;
    } break;    

    default:
        break;
    }
}
#endif
static OPERATE_RET __example_save_wav_from_pcm_file(char *pcm_file)
{
    OPERATE_RET rt = OPRT_OK;
    uint8_t head[WAV_HEAD_LEN] = {0};
    uint32_t pcm_len = 0;
    char *read_buf = NULL;

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
    rt = app_get_wav_head(pcm_len, 1, sg_audio_info.sample_rate, \
                          sg_audio_info.sample_bits, sg_audio_info.sample_ch_num, head);
    if (OPRT_OK != rt) {
        PR_ERR("app_get_wav_head failed, rt = %d", rt);
        return rt;
    }

    PR_HEXDUMP_DEBUG("wav head", head, WAV_HEAD_LEN);

    // Create wav file
    TUYA_FILE wav_hdl = tkl_fopen(EXAMPLE_RECORDER_WAV_FILE_PATH, "w");
    if (NULL == wav_hdl) {
        PR_ERR("open file: %s failed", EXAMPLE_RECORDER_WAV_FILE_PATH);
        rt = OPRT_FILE_OPEN_FAILED;
        goto __EXIT;
    }

    // Write wav head
    tkl_fwrite(head, WAV_HEAD_LEN, wav_hdl);

    // Read pcm file
    read_buf = tkl_system_psram_malloc(sg_audio_info.frame_size);
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
        memset(read_buf, 0, sg_audio_info.frame_size);
        uint32_t read_len = tkl_fread(read_buf, sg_audio_info.frame_size, pcm_hdl);
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

static OPERATE_RET __example_open_file(void)
{
    BOOL_T is_exist = TRUE;

    tkl_fs_is_exist(EXAMPLE_RECORDER_FILE_PATH, &is_exist);
    if (is_exist == TRUE) {
        tkl_fs_remove(EXAMPLE_RECORDER_FILE_PATH);
        PR_DEBUG("remove file %s", EXAMPLE_RECORDER_FILE_PATH);
    }

    is_exist = FALSE;
    tkl_fs_is_exist(EXAMPLE_RECORDER_WAV_FILE_PATH, &is_exist);
    if (is_exist == TRUE) {
        tkl_fs_remove(EXAMPLE_RECORDER_WAV_FILE_PATH);
        PR_DEBUG("remove file %s", EXAMPLE_RECORDER_WAV_FILE_PATH);
    }

    // Create recording file
    sg_recorder_file_hdl = tkl_fopen(EXAMPLE_RECORDER_FILE_PATH, "w");
    if (NULL == sg_recorder_file_hdl) {
        PR_ERR("open file failed");
        return OPRT_FILE_OPEN_FAILED;
    }
    PR_DEBUG("open file %s success", EXAMPLE_RECORDER_FILE_PATH);   

    return OPRT_OK;
}

static void __example_save_pcm_from_recorder_rb(void)
{
    if (NULL == sg_recorder_file_hdl) {
        return;
    }

    uint32_t data_len = 0;
    data_len = tuya_ring_buff_used_size_get(sg_recorder_pcm_rb);
    if (data_len == 0) {
        return;
    }

    char *read_buf = tkl_system_psram_malloc(data_len);
    if (NULL == read_buf) {
        PR_ERR("tkl_system_psram_malloc failed");
        return;
    }

    // Write to file
    tuya_ring_buff_read(sg_recorder_pcm_rb, read_buf, data_len);
    int rt_len = tkl_fwrite(read_buf, data_len, sg_recorder_file_hdl);
    if (rt_len != data_len) {
        PR_ERR("write file failed, maybe disk full");
        PR_ERR("write len %d, data len %d", rt_len, data_len);
    }

    if (read_buf) {
        tkl_system_psram_free(read_buf);
        read_buf = NULL;
    }
}
static OPERATE_RET __example_close_file(void)
{
    if (sg_recorder_file_hdl) {
        tkl_fclose(sg_recorder_file_hdl);
        sg_recorder_file_hdl = NULL;
    }

    return OPRT_OK;
}

static OPERATE_RET __example_fs_init(void)
{
    OPERATE_RET rt = OPRT_OK;

    rt = tkl_fs_mount("/sdcard", DEV_SDCARD);
    if (rt != OPRT_OK) {
        PR_ERR("mount sd card failed, please retry after format ");
        return rt;
    }
    PR_DEBUG("mount sd card success ");

    return rt;
}


static void __example_get_audio_frame(TDL_AUDIO_FRAME_FORMAT_E type, TDL_AUDIO_STATUS_E status,\
                                      uint8_t *data, uint32_t len)
{
    if (sg_recorder_pcm_rb) {
        tuya_ring_buff_write(sg_recorder_pcm_rb, data, len);
    }

    return;
}

static OPERATE_RET __example_audio_open(void)
{
    OPERATE_RET rt = OPRT_OK;
    uint32_t buf_len = 0;

    TUYA_CALL_ERR_RETURN(tdl_audio_find(AUDIO_CODEC_NAME, &sg_audio_hdl));
    TUYA_CALL_ERR_RETURN(tdl_audio_open(sg_audio_hdl, __example_get_audio_frame));
    TUYA_CALL_ERR_RETURN(tdl_audio_get_info(sg_audio_hdl, &sg_audio_info));
    if(0 == sg_audio_info.frame_size || 0 == sg_audio_info.sample_tm_ms) {
        PR_ERR("get audio info err");
        return OPRT_INVALID_PARM;
    }

    buf_len = (EXAMPLE_RECORD_DURATION_MS / sg_audio_info.sample_tm_ms) * sg_audio_info.frame_size;
    TUYA_CALL_ERR_RETURN(tuya_ring_buff_create(buf_len, \
                                               OVERFLOW_PSRAM_STOP_TYPE, &sg_recorder_pcm_rb));

    tdl_audio_volume_set(sg_audio_hdl, 60);

    PR_NOTICE("__example_audio_open success");

    return OPRT_OK;
}


/**
 * @brief user_main
 *
 * @return int
 */
int user_main()
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

    /*hardware register*/
    TUYA_CALL_ERR_LOG(board_register_hardware());

    TUYA_CALL_ERR_LOG(__example_fs_init());

    TUYA_CALL_ERR_LOG(__example_audio_open());

#if defined(ENABLE_BUTTON) && (ENABLE_BUTTON == 1)
    // button create
    TDL_BUTTON_CFG_T button_cfg = {.long_start_valid_time = 3000,
                                   .long_keep_timer = 1000,
                                   .button_debounce_time = 50,
                                   .button_repeat_valid_count = 0,
                                   .button_repeat_valid_time = 500};
    TDL_BUTTON_HANDLE button_hdl = NULL;

    TUYA_CALL_ERR_LOG(tdl_button_create(BUTTON_NAME, &button_cfg, &button_hdl));

    tdl_button_event_register(button_hdl, TDL_BUTTON_PRESS_DOWN, __button_function_cb);
    tdl_button_event_register(button_hdl, TDL_BUTTON_PRESS_UP, __button_function_cb);
#endif

    while(1) {
        switch(sg_recorder_status) {
            case RECORDER_STATUS_START:
                PR_NOTICE("Start recording");
                __example_open_file();
                sg_recorder_status = RECORDER_STATUS_RECORDING;
                break;

            case RECORDER_STATUS_RECORDING:
                __example_save_pcm_from_recorder_rb();
                break;

            case RECORDER_STATUS_END:
                PR_NOTICE("End recording");
                __example_close_file();
                __example_save_wav_from_pcm_file(EXAMPLE_RECORDER_FILE_PATH);
                sg_recorder_status = RECORDER_STATUS_IDLE;
                break;
                
            case RECORDER_STATUS_IDLE:
                tuya_ring_buff_reset(sg_recorder_pcm_rb);
            default:
                break;
        }

        tal_system_sleep(10);
    }

}

/**
 * @brief main
 *
 * @param argc
 * @param argv
 * @return void
 */
#if OPERATING_SYSTEM == SYSTEM_LINUX
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
    THREAD_CFG_T thrd_param = {0};
    thrd_param.stackDepth = 1024 * 4;
    thrd_param.priority = THREAD_PRIO_1;
    thrd_param.thrdname = "tuya_app_main";
    tal_thread_create_and_start(&ty_app_thread, NULL, NULL, tuya_app_thread, NULL, &thrd_param);
}
#endif
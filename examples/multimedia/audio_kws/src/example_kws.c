/**
 * @file example_vad.c
 * @version 0.1
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#include "tuya_cloud_types.h"
#include "tuya_ringbuf.h"
#include "tal_api.h"

#include "tkl_output.h"
#include "tkl_kws.h"
#include "tdl_audio_manage.h"
#include "board_com_api.h"
/***********************************************************
************************macro define************************
***********************************************************/
#define KWS_PROCE_UNIT_NUM    30

/***********************************************************
***********************typedef define***********************
***********************************************************/


/***********************************************************
***********************variable define**********************
***********************************************************/
const static TKL_KWS_WAKEUP_WORD_E cWAKEUP_KEYWORD_LIST[] = {
    TKL_KWS_WAKEUP_NIHAO_TUYA,
};

static TUYA_RINGBUFF_T sg_feed_ringbuff;
static uint32_t sg_feed_buff_len = 0;
/***********************************************************
***********************function define**********************
***********************************************************/
static OPERATE_RET __example_kws_init(void)
{
    OPERATE_RET rt = OPRT_OK;

    TUYA_CALL_ERR_GOTO(tkl_kws_init(), __KWS_INIT_ERR);

    TUYA_CALL_ERR_GOTO(tkl_kws_wakeup_word_config((TKL_KWS_WAKEUP_WORD_E *)cWAKEUP_KEYWORD_LIST,\
                      CNTSOF(cWAKEUP_KEYWORD_LIST)), __KWS_INIT_ERR);

    sg_feed_buff_len = tkl_kws_get_process_uint_size() * KWS_PROCE_UNIT_NUM;
    PR_DEBUG("sg_feed_buff_len:%d", sg_feed_buff_len);
    TUYA_CALL_ERR_GOTO(tuya_ring_buff_create(sg_feed_buff_len + tkl_kws_get_process_uint_size(),
                                             OVERFLOW_PSRAM_STOP_TYPE, &sg_feed_ringbuff), __KWS_INIT_ERR);

    return OPRT_OK;

__KWS_INIT_ERR:
    tkl_kws_deinit();

    if (sg_feed_ringbuff) {
        tuya_ring_buff_free(sg_feed_ringbuff);
        sg_feed_ringbuff = NULL;
    }

    return rt;
}

static void __example_get_audio_frame(TDL_AUDIO_FRAME_FORMAT_E type, TDL_AUDIO_STATUS_E status,\
                                      uint8_t *data, uint32_t len)
{
    tuya_ring_buff_write(sg_feed_ringbuff, data, len);
}

static OPERATE_RET __example_audio_open(void)
{
    OPERATE_RET rt = OPRT_OK;

    TDL_AUDIO_HANDLE_T audio_hdl = NULL;

    TUYA_CALL_ERR_RETURN(tdl_audio_find(AUDIO_CODEC_NAME, &audio_hdl));
    TUYA_CALL_ERR_RETURN(tdl_audio_open(audio_hdl, __example_get_audio_frame));

    PR_NOTICE("__example_audio_open success");

    return OPRT_OK;
}

static TKL_KWS_WAKEUP_WORD_E __kws_recognize_wakeup_keyword(void)
{
    uint32_t i = 0, fc = 0;
    TKL_KWS_WAKEUP_WORD_E wakeup_word = TKL_KWS_WAKEUP_WORD_UNKNOWN;
    uint32_t uint_size = 0, feed_size = 0;

    uint_size = tkl_kws_get_process_uint_size();
    feed_size = tuya_ring_buff_used_size_get(sg_feed_ringbuff);
    if (feed_size < uint_size) {
        return TKL_KWS_WAKEUP_WORD_UNKNOWN;
    }

    uint8_t *p_buf = tal_malloc(uint_size);
    if (NULL == p_buf) {
        PR_ERR("malloc fail");
        return TKL_KWS_WAKEUP_WORD_UNKNOWN;
    }

    fc = feed_size / uint_size;
    for (i = 0; i < fc; i++) {
        tuya_ring_buff_read(sg_feed_ringbuff, p_buf, uint_size);

        wakeup_word = tkl_kws_recognize_wakeup_word(p_buf, uint_size);
        if (wakeup_word != TKL_KWS_WAKEUP_WORD_UNKNOWN) {
            break;
        }
    }

    tal_free(p_buf);

    return wakeup_word;
}

void user_main(void)
{
    TKL_KWS_WAKEUP_WORD_E wakeup_word = TKL_KWS_WAKEUP_WORD_UNKNOWN;

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
    board_register_hardware();

    __example_audio_open();

    __example_kws_init();

    while(1) {
        wakeup_word = __kws_recognize_wakeup_keyword();
        if(wakeup_word != TKL_KWS_WAKEUP_WORD_UNKNOWN) {
            PR_NOTICE("kws wakeup key: %d", wakeup_word);
        } 

        tal_system_sleep(10);
    }


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
    THREAD_CFG_T thrd_param = {0};
    thrd_param.stackDepth = 1024 * 4;
    thrd_param.priority = THREAD_PRIO_1;
    thrd_param.thrdname = "tuya_app_main";
    tal_thread_create_and_start(&ty_app_thread, NULL, NULL, tuya_app_thread, NULL, &thrd_param);
}
#endif

/**
 * @file ai_ui_stream_text.c
 * @brief ai_ui_stream_text module is used to 
 * @version 0.1
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */
#include "tal_api.h"
#include "tuya_ringbuf.h"

#include "ai_ui_stream_text.h"
/***********************************************************
************************macro define************************
***********************************************************/
#define STREAM_BUFF_MAX_LEN          1024
#define STREAM_TEXT_SHOW_WORD_NUM    10
#define ONE_WORD_MAX_LEN             4

#define STREAM_READ_TEXT_BUF_LEN    (STREAM_TEXT_SHOW_WORD_NUM * ONE_WORD_MAX_LEN + 1)

/***********************************************************
***********************typedef define***********************
***********************************************************/
typedef struct {
    bool                is_init;
    bool                is_start;
    MUTEX_HANDLE        rb_mutex;
    TUYA_RINGBUFF_T     text_ringbuff;
    TIMER_ID            timer;
}AI_TEXT_STREAM_T;

/***********************************************************
***********************variable define**********************
***********************************************************/
static AI_TEXT_STREAM_T sg_text_stream;
static AI_UI_STREAM_TEXT_DISP_CB sg_disp_cb = NULL;
static char sg_read_text_buf[STREAM_READ_TEXT_BUF_LEN];

/***********************************************************
***********************function define**********************
***********************************************************/
static uint8_t __get_one_word_from_stream_ringbuff(AI_TEXT_STREAM_T *stream, char *result)
{
    uint32_t rb_used_size = 0, read_len = 0;
    uint8_t word_len = 0;
    char tmp = 0;

    tal_mutex_lock(stream->rb_mutex);
    rb_used_size = tuya_ring_buff_used_size_get(stream->text_ringbuff);
    tal_mutex_unlock(stream->rb_mutex);
    if (0 == rb_used_size) {
        return 0;
    }

    // get word len
    do {
        tal_mutex_lock(stream->rb_mutex);
        read_len = tuya_ring_buff_read(stream->text_ringbuff, &tmp, 1);
        tal_mutex_unlock(stream->rb_mutex);

        if ((tmp & 0xC0) != 0x80) {
            if ((tmp & 0xE0) == 0xC0) {
                word_len = 2;
            } else if ((tmp & 0xF0) == 0xE0) {
                word_len = 3;
            } else if ((tmp & 0xF8) == 0xF0) {
                word_len = 4;
            } else {
                word_len = 1;
            }
            break;
        }

        tmp = 0;
    } while (read_len);

    if (0 == word_len) {
        return 0;
    }

    // get word
    result[0] = tmp;

    if (word_len - 1) {
        tal_mutex_lock(stream->rb_mutex);
        tuya_ring_buff_read(stream->text_ringbuff, &result[1], word_len - 1);
        tal_mutex_unlock(stream->rb_mutex);
    }

    return word_len;
}

static uint8_t __get_words_from_stream_ringbuff(AI_TEXT_STREAM_T *stream, uint8_t word_num, char *result)
{
    uint8_t word_len = 0, i = 0, get_num = 0;
    uint32_t result_len = 0;
    

    for (i = 0; i < word_num; i++) {
        word_len = __get_one_word_from_stream_ringbuff(stream, &result[result_len]);
        if (0 == word_len) {
            break;
        }
        result_len += word_len;
        get_num++;
    }

    result[result_len] = '\0';

    return get_num;
}

static void __ui_stream_text_timer_cb(TIMER_ID timer_id, void *arg)
{
    AI_TEXT_STREAM_T *stream = (AI_TEXT_STREAM_T*)arg;
    uint8_t word_num = 0;

    memset(sg_read_text_buf, 0x00, STREAM_READ_TEXT_BUF_LEN);
    word_num = __get_words_from_stream_ringbuff(stream, STREAM_TEXT_SHOW_WORD_NUM, sg_read_text_buf);
    if (0 == word_num) {
        if(false == stream->is_start) {
            tal_sw_timer_stop(stream->timer);
            if(sg_disp_cb) {
                sg_disp_cb(NULL);
            }
        }
    }else {
        if(sg_disp_cb) {
            sg_disp_cb(sg_read_text_buf);
        }
    }    

    return;
}

OPERATE_RET ai_ui_stream_text_init(AI_UI_STREAM_TEXT_DISP_CB disp_cb)
{
    OPERATE_RET rt = OPRT_OK;

    if(sg_text_stream.is_init) {
        PR_DEBUG("ai ui stream text has been init");
        return OPRT_OK;
    }

    if (NULL == sg_text_stream.text_ringbuff) {
        TUYA_CALL_ERR_RETURN(tuya_ring_buff_create(STREAM_BUFF_MAX_LEN, \
                                                   OVERFLOW_PSRAM_STOP_TYPE, \
                                                   &sg_text_stream.text_ringbuff));
    }

    tuya_ring_buff_reset(sg_text_stream.text_ringbuff);

    if (sg_text_stream.rb_mutex == NULL) {
        TUYA_CALL_ERR_RETURN(tal_mutex_create_init(&sg_text_stream.rb_mutex));
    }

    if(sg_text_stream.timer == NULL) {
        TUYA_CALL_ERR_RETURN(tal_sw_timer_create(__ui_stream_text_timer_cb, \
                                                 &sg_text_stream, \
                                                 &sg_text_stream.timer));
    }

    sg_disp_cb = disp_cb;
    sg_text_stream.is_init = true;

    return OPRT_OK;
}

void ai_ui_stream_text_start(void)
{
    if(false == sg_text_stream.is_init) {
        return;
    }

    tal_sw_timer_start(sg_text_stream.timer, 1000, TAL_TIMER_CYCLE);

    sg_text_stream.is_start = true;
}

void ai_ui_stream_text_write(const char *text)
{
    if(false == sg_text_stream.is_init || false == sg_text_stream.is_start) {
        return;
    }

    if(text == NULL) {
        return;
    }

    tal_mutex_lock(sg_text_stream.rb_mutex);
    tuya_ring_buff_write(sg_text_stream.text_ringbuff, text, strlen(text));
    tal_mutex_unlock(sg_text_stream.rb_mutex);
}
void ai_ui_stream_text_end(void)
{
    if(false == sg_text_stream.is_init || false == sg_text_stream.is_start) {
        return;
    }
    
    sg_text_stream.is_start = false;
}


void ai_ui_stream_text_reset(void)
{
    if(false == sg_text_stream.is_init) {
        return;
    }

    if(sg_text_stream.text_ringbuff) {
        tal_mutex_lock(sg_text_stream.rb_mutex);
        tuya_ring_buff_reset(sg_text_stream.text_ringbuff);
        tal_mutex_unlock(sg_text_stream.rb_mutex);
    }
} 
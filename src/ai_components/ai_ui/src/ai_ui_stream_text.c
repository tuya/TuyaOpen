
/**
 * @file ai_ui_stream_text.c
 * @brief Stream text display implementation.
 *
 * This file provides functions for streaming text display, supporting incremental
 * text updates for real-time AI responses using ring buffer and timer-based display.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 *
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

#define STREAM_READ_TEXT_CHUNK_LEN  (STREAM_TEXT_SHOW_WORD_NUM * ONE_WORD_MAX_LEN)
#define STREAM_READ_TEXT_BUF_LEN    (STREAM_READ_TEXT_CHUNK_LEN + 1)

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
/**
 * @brief Get multiple UTF-8 words from the stream ring buffer.
 *
 * Read a bounded batch into SRAM first, then parse it locally and discard only
 * the bytes that form complete words. This keeps PSRAM access to one bulk peek
 * per timer tick and leaves an incomplete trailing UTF-8 sequence in the ring.
 *
 * @param stream Pointer to the text stream structure.
 * @param word_num Number of words to read.
 * @param result Buffer to store the words.
 * @return Number of words actually read.
 */
static uint8_t __get_words_from_stream_ringbuff(AI_TEXT_STREAM_T *stream, uint8_t word_num, char *result)
{
    uint32_t read_len = 0;
    uint32_t consumed = 0;
    uint32_t result_len = 0;
    uint8_t word_len = 0;
    uint8_t get_num = 0;

    tal_mutex_lock(stream->rb_mutex);
    read_len = tuya_ring_buff_peek(stream->text_ringbuff, result, STREAM_READ_TEXT_CHUNK_LEN);
    while ((consumed < read_len) && (get_num < word_num)) {
        uint8_t first = (uint8_t)result[consumed];

        /* Skip a stray UTF-8 continuation byte from a damaged stream. */
        if ((first & 0xC0) == 0x80) {
            consumed++;
            continue;
        }

        if ((first & 0xE0) == 0xC0) {
            word_len = 2;
        } else if ((first & 0xF0) == 0xE0) {
            word_len = 3;
        } else if ((first & 0xF8) == 0xF0) {
            word_len = 4;
        } else {
            word_len = 1;
        }

        /* Keep the incomplete tail in the ring until the next batch arrives. */
        if (word_len > (read_len - consumed)) {
            break;
        }

        /* Pack accepted words in place; destination never overtakes source. */
        for (uint8_t i = 0; i < word_len; i++) {
            result[result_len + i] = result[consumed + i];
        }
        consumed += word_len;
        result_len += word_len;
        get_num++;
    }

    if (consumed > 0) {
        tuya_ring_buff_discard(stream->text_ringbuff, consumed);
    }
    tal_mutex_unlock(stream->rb_mutex);

    result[result_len] = '\0';
    return get_num;
}

/**
 * @brief Timer callback for streaming text display.
 *
 * @param timer_id Timer ID.
 * @param arg Pointer to the text stream structure.
 */
static void __ui_stream_text_timer_cb(TIMER_ID timer_id, void *arg)
{
    AI_TEXT_STREAM_T *stream = (AI_TEXT_STREAM_T*)arg;
    uint8_t word_num = 0;

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

/**
 * @brief Initialize stream text display module.
 *
 * @param disp_cb Callback function for displaying text.
 * @return OPERATE_RET Operation result code.
 */
OPERATE_RET ai_ui_stream_text_init(AI_UI_STREAM_TEXT_DISP_CB disp_cb)
{
    OPERATE_RET rt = OPRT_OK;

    if(sg_text_stream.is_init) {
        PR_DEBUG("ai ui stream text has been init");
        return OPRT_OK;
    }

    if (NULL == sg_text_stream.text_ringbuff) {
#if TUYA_RINGBUF_PSRAM_ENABLED
        TUYA_CALL_ERR_RETURN(tuya_ring_buff_create(STREAM_BUFF_MAX_LEN, \
                                                   OVERFLOW_PSRAM_STOP_TYPE, \
                                                   &sg_text_stream.text_ringbuff));
#else
        TUYA_CALL_ERR_RETURN(tuya_ring_buff_create(STREAM_BUFF_MAX_LEN, \
                                                   OVERFLOW_STOP_TYPE, \
                                                   &sg_text_stream.text_ringbuff));
#endif
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

/**
 * @brief Start streaming text display.
 */
void ai_ui_stream_text_start(void)
{
    if(false == sg_text_stream.is_init) {
        return;
    }

    tal_sw_timer_start(sg_text_stream.timer, 1000, TAL_TIMER_CYCLE);

    sg_text_stream.is_start = true;
}

/**
 * @brief Write text data to stream display.
 *
 * @param text Pointer to the text string to display.
 */
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
/**
 * @brief End streaming text display.
 */
void ai_ui_stream_text_end(void)
{
    if(false == sg_text_stream.is_init || false == sg_text_stream.is_start) {
        return;
    }
    
    sg_text_stream.is_start = false;
}


/**
 * @brief Reset stream text display state.
 */
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

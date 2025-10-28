#ifndef __TUYA_AI_PROC_H__
#define __TUYA_AI_PROC_H__

#include "tuya_cloud_types.h"
#include "tal_mutex.h"
#include "tal_thread.h"
#include "tal_sw_timer.h"
#include "tal_queue.h"
#include "tuya_ringbuf.h"


#define ENABLE_VIDEO_ONE_FRAME           1

typedef enum {
    AI_PROC_AUDIO    = 1 << 8,
    AI_PROC_VIDEO    = 1 << 9,
    AI_PROC_TEXT     = 1 << 10,
    AI_PROC_FILE     = 1 << 11,
    AI_PROC_SKILL    = 1 << 12,
} ai_proc_type_t;

typedef enum {
    AI_PROC_INTERRUPT_EVENT,
    //! llm asr process
    AI_PROC_ASR_EVENT,
    AI_PROC_ASR_TIMEOUT_EVENT,
    //! audio process
    AI_PROC_AUDIO_EVENT     = AI_PROC_AUDIO,
    AI_PROC_VIDEO_EVENT     = AI_PROC_VIDEO,
    AI_PROC_TEXT_EVENT      = AI_PROC_TEXT,
    AI_PROC_SKILL_EVENT     = AI_PROC_SKILL,
    AI_PROC_FINSH_EVENT,
} ai_proc_event_in_t;


typedef enum {
    //! llm asr
    AI_PROC_ASR_OK,
    AI_PROC_ASR_EMPTY,
    AI_PROC_ASR_TIMEOUT,

    //! 上传完成
    AI_PROC_UPLOAD_DONE,
    AI_PROC_UPLOAD_FAIL,

    //! llm tts
    AI_PROC_TTS_START,
    AI_PROC_TTS_DATA,
    AI_PROC_TTS_STOP,
    AI_PROC_TTS_ABORT,
    AI_PROC_TTS_TIMEOUT,

    //! llm emoji
    AI_PROC_ASR_EMOJI,
    AI_PROC_TTS_EMOJI,

    //! llm text
    AI_PROC_TEXT_START,
    AI_PROC_TEXT_DATA,
    AI_PROC_TEXT_STOP,

    //! llm player control
    AI_PROC_PLAY_CTL_PLAY,
    AI_PROC_PLAY_CTL_PAUSE,
    AI_PROC_PLAY_CTL_REPLAY,
    AI_PROC_PLAY_CTL_RESUME,
    AI_PROC_PLAY_CTL_NEXT,

    //! chat break
    AI_PROC_CHAT_BREAK,

    //! ai skill
    AI_PROC_SKILL_TRIGGER,
} ai_proc_event_out_t;

typedef struct {
    int                 event;
    uint8_t            *data;
    uint16_t            datalen;
} ai_proc_msg_t;

typedef int (*ai_proc_output_cb_t)(ai_proc_msg_t *msg,  void *user_data);

typedef struct {
    ai_proc_output_cb_t  output_cb;
    void                *user_data;
} ty_ai_proc_cfg_t;


typedef struct {
    int                 status;
    TIMER_ID            asr_timer;
    TIMER_ID            tts_timer;
    
    QUEUE_HANDLE        event;

    ai_proc_output_cb_t  output_cb;
    void               *user_data;

    UCHAR_T            *video_buf;
    TUYA_RINGBUFF_T     video_ringbuf;
    MUTEX_HANDLE        video_mutex;

    THREAD_HANDLE       task;
} ty_ai_proc_t;

ty_ai_proc_t *ty_ai_proc_create(ty_ai_proc_cfg_t *cfg);
int ty_ai_proc_destroy(ty_ai_proc_t *llm);
int ty_ai_proc_start(ty_ai_proc_t *llm);
int ty_ai_proc_event_send(ty_ai_proc_t *llm, int event, uint8_t *data, uint16_t datalen);


#endif

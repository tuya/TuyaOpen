#include "tuya_ipc_demo.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/time.h>
#include "tuya_cloud_types.h"

char path[512] = {0};
unsigned char *video_buf = NULL;
int file_size = 0;
bool is_last_frame = FALSE;
unsigned int frame_len = 0, frame_start = 0;
unsigned int next_frame_len = 0, next_frame_start = 0;
unsigned int offset = 0;
unsigned int is_key_frame = 0;
FILE *fp = NULL;

void *video_thread(void *arg);
int __read_one_frame_from_demo_video_file(unsigned char *pVideoBuf, unsigned int offset, unsigned int BufSize,
                                          unsigned int *IskeyFrame, unsigned int *FramLen, unsigned int *Frame_start);

void tuya_ipc_demo_start()
{
    // int err = 0;
    // pthread_t ntid = 0;;
    // err = pthread_create(&ntid, NULL, video_thread, NULL);
    // pthread_detach(ntid);
    video_thread(NULL);
    return;
}

void tuya_ipc_demo_end()
{
    free(video_buf);
    video_buf = NULL;
    fclose(fp);
    fp = NULL;

    file_size = 0;
    is_last_frame = FALSE;
    frame_len = 0;
    frame_start = 0;
    next_frame_len = 0;
    next_frame_start = 0;
    offset = 0;
    is_key_frame = 0;

    return;
}

void *video_thread(void *arg)
{
    getcwd(path, sizeof(path));
    strcat(path, "/demo_video.264");
    fp = fopen(path, "rb");
    if (fp == NULL) {
        printf("==============cant not read aov file %s\n", path);
        pthread_exit(0);
    }
    fseek(fp, 0, SEEK_END);
    file_size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    video_buf = (unsigned char *)malloc(file_size);
    if (video_buf == NULL) {
        printf("malloc video buf failed\n");
        fclose(fp);
        pthread_exit(0);
    }

    fread(video_buf, 1, file_size, fp);

    // while (1)
    // {
    //     offset = frame_start + frame_len;
    //     if (offset >= file_size) {
    //         break;
    //     }

    //     int ret = __read_one_frame_from_demo_video_file(video_buf + offset, file_size - offset, offset,
    //     &is_key_frame, &frame_len, &frame_start); if (ret) {
    //         break;
    //     }

    //     // Determine whether it is the last frame
    //     if (ret) {
    //         is_last_frame = TRUE;           // important
    //     } else {
    //         is_last_frame = FALSE;
    //     }

    //     if (is_key_frame) {
    //         printf("E_VIDEO_I_FRAME\n");
    //     } else {
    //         printf("E_VIDEO_PB_FRAME\n");
    //     }

    //     usleep(10);
    // }

    return NULL;
}

/* This is for demo only. Should be replace with real H264 encoder output */
int __read_one_frame_from_demo_video_file(unsigned char *pVideoBuf, unsigned int offset, unsigned int BufSize,
                                          unsigned int *IskeyFrame, unsigned int *FramLen, unsigned int *Frame_start)
{
    unsigned int pos = 0;
    int bNeedCal = 0;
    unsigned char NalType = 0;
    int idx = 0;
    if (BufSize <= 5) {
        printf("bufSize is too small\n");
        return -1;
    }
    for (pos = 0; pos <= BufSize - 5; pos++) {
        if (pVideoBuf[pos] == 0x00 && pVideoBuf[pos + 1] == 0x00 && pVideoBuf[pos + 2] == 0x00 &&
            pVideoBuf[pos + 3] == 0x01)
        // if (pVideoBuf[pos] == 0x00 && pVideoBuf[pos + 1] == 0x00 && pVideoBuf[pos + 2] == 0x01)
        {
            NalType = pVideoBuf[pos + 4] & 0x1f;
            if (NalType == 0x7) {
                if (bNeedCal == 1) {
                    *FramLen = pos - idx;
                    return 0;
                }
                *IskeyFrame = 1;
                *Frame_start = offset + pos;
                bNeedCal = 1;
                idx = pos;
            } else if (NalType == 0x1) {
                if (bNeedCal) {
                    *FramLen = pos - idx;
                    return 0;
                }
                *Frame_start = offset + pos;
                *IskeyFrame = 0;
                idx = pos;
                bNeedCal = 1;
            }
        }
    } // for (pos = 0; pos <= BufSize - 5; pos++)

    *FramLen = BufSize;

    return 0;
}

// Get time in milliseconds
static unsigned long __get_time_ms()
{
    struct timeval tv;
    if (gettimeofday(&tv, NULL) < 0) {
        return 0;
    }
    return tv.tv_sec * 1000 + tv.tv_usec / 1000;
}

INT_T Demo_OnSignalDisconnectCallback()
{
    tuya_ipc_demo_end();
    return 0;
}

INT_T Demo_OnGetVideoFrameCallback(MEDIA_FRAME *pMediaFrame)
{
    // memcpy(pMediaFrame->data, pTalVideoFrame->pbuf, pTalVideoFrame->used_size);
    // pMediaFrame->size = pTalVideoFrame->used_size;
    // pMediaFrame->pts = pTalVideoFrame->pts;
    // pMediaFrame->timestamp = pTalVideoFrame->timestamp;
    // pMediaFrame->type = (MEDIA_FRAME_TYPE)pTalVideoFrame->frametype;

    offset = frame_start + frame_len;
    if (offset >= file_size) {
        is_last_frame = FALSE;
        frame_len = 0;
        frame_start = 0;
        next_frame_len = 0;
        next_frame_start = 0;
        offset = 0;
        is_key_frame = 0;
        return -1;
    }
    int ret = __read_one_frame_from_demo_video_file(video_buf + offset, offset, file_size - offset, &is_key_frame,
                                                    &frame_len, &frame_start);
    if (ret) {
        return -1;
    }
    memcpy(pMediaFrame->data, video_buf + offset, frame_len);
    pMediaFrame->size = frame_len;
    pMediaFrame->pts = __get_time_ms();
    pMediaFrame->timestamp = __get_time_ms();
    if (is_key_frame) {
        // printf("E_VIDEO_I_FRAME\n");
        pMediaFrame->type = eVideoIFrame;
    } else {
        // printf("E_VIDEO_PB_FRAME\n");
        pMediaFrame->type = eVideoPBFrame;
    }

    usleep(66 * 1000); // Sleep according to actual frame rate, needs to be modified

    return 0;
}

INT_T Demo_OnGetAudioFrameCallback(MEDIA_FRAME *pMediaFrame)
{
    // TAL_AUDIO_FRAME_INFO_T *pTalAudioFrame = &sg_p2p_session->tal_audio_frame;
    // if (tal_ai_get_frame(0, 0, pTalAudioFrame) != 0)
    // {
    //     return -1;
    // }
    // memcpy(pMediaFrame->data, pTalAudioFrame->pbuf, pTalAudioFrame->used_size);
    // pMediaFrame->size = pTalAudioFrame->used_size;
    // pMediaFrame->pts = pTalAudioFrame->pts;
    // pMediaFrame->timestamp = pTalAudioFrame->timestamp;
    // pMediaFrame->type = (MEDIA_FRAME_TYPE)pTalAudioFrame->type;
    return 0;
}

/**
 * @file tuya_ipc_demo.c
 * @brief Implements Tuya IPC demo functionality for video streaming
 *
 * This source file provides the implementation of the Tuya IPC demo functionality
 * required for video streaming applications. It includes functionality for managing
 * demo video files, handling video frame processing, and providing callback functions
 * for media streaming. The implementation supports integration with the Tuya IoT
 * platform and ensures proper handling of video streaming operations. This file is
 * essential for developers working on IoT camera applications that require robust
 * video streaming mechanisms.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#include "tuya_ipc_demo.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/time.h>
#include "tuya_cloud_types.h"
#include "tal_log.h"

#define TKL_VENC_MAIN_FPS      30

/* Global variable declarations */
static char g_demo_path[512] = {0};
static unsigned char *g_video_buf = NULL;
static int g_file_size = 0;
static bool g_is_last_frame = FALSE;
static unsigned int g_frame_len = 0, g_frame_start = 0;
static unsigned int g_next_frame_len = 0, g_next_frame_start = 0;
static unsigned int g_offset = 0;
static unsigned int g_is_key_frame = 0;
static FILE *g_fp = NULL;

/**
 * @brief Initialize demo video file
 */
void tuya_ipc_demo_start(void)
{
    getcwd(g_demo_path, sizeof(g_demo_path));
    strcat(g_demo_path, "/demo_video.264");
    g_fp = fopen(g_demo_path, "rb");
    if (g_fp == NULL) {
        PR_ERR("cannot read demo video file %s\n", g_demo_path);
        pthread_exit(0);
    }

    fseek(g_fp, 0, SEEK_END);
    g_file_size = ftell(g_fp);
    fseek(g_fp, 0, SEEK_SET);

    g_video_buf = (unsigned char *)malloc(g_file_size);
    if (g_video_buf == NULL) {
        PR_DEBUG("malloc video buffer failed\n");
        fclose(g_fp);
        pthread_exit(0);
    }

    fread(g_video_buf, 1, g_file_size, g_fp);

    return;
}

/**
 * @brief Clean up demo resources
 */
void tuya_ipc_demo_end(void)
{
    if (g_video_buf) {
        free(g_video_buf);
        g_video_buf = NULL;
    }
    
    if (g_fp) {
        fclose(g_fp);
        g_fp = NULL;
    }

    g_file_size = 0;
    g_is_last_frame = FALSE;
    g_frame_len = 0;
    g_frame_start = 0;
    g_next_frame_len = 0;
    g_next_frame_start = 0;
    g_offset = 0;
    g_is_key_frame = 0;

    return;
}

/**
 * @brief Read one frame from demo video file
 * @param video_buf Video buffer
 * @param offset Buffer offset
 * @param buf_size Buffer size
 * @param is_key_frame Pointer to key frame flag
 * @param frame_len Pointer to frame length
 * @param frame_start Pointer to frame start position
 * @return 0 on success, -1 on failure
 */
static int read_one_frame_from_demo_video_file(unsigned char *video_buf, 
                                              unsigned int offset, 
                                              unsigned int buf_size,
                                              unsigned int *is_key_frame, 
                                              unsigned int *frame_len, 
                                              unsigned int *frame_start)
{
    unsigned int pos = 0;
    int need_calc = 0;
    unsigned char nal_type = 0;
    int idx = 0;
    
    if (buf_size <= 5) {
        PR_DEBUG("buffer size is too small\n");
        return -1;
    }
    
    for (pos = 0; pos <= buf_size - 5; pos++) {
        if (video_buf[pos] == 0x00 && video_buf[pos + 1] == 0x00 && 
            video_buf[pos + 2] == 0x00 && video_buf[pos + 3] == 0x01) {
            
            nal_type = video_buf[pos + 4] & 0x1f;
            if (nal_type == 0x7) {
                if (need_calc == 1) {
                    *frame_len = pos - idx;
                    return 0;
                }
                *is_key_frame = 1;
                *frame_start = offset + pos;
                need_calc = 1;
                idx = pos;
            } else if (nal_type == 0x1) {
                if (need_calc) {
                    *frame_len = pos - idx;
                    return 0;
                }
                *frame_start = offset + pos;
                *is_key_frame = 0;
                idx = pos;
                need_calc = 1;
            }
        }
    }

    *frame_len = buf_size;
    return 0;
}

/**
 * @brief Get current time in milliseconds
 * @return Current timestamp in milliseconds
 */
static unsigned long get_time_ms(void)
{
    struct timeval tv;
    if (gettimeofday(&tv, NULL) < 0) {
        return 0;
    }
    return tv.tv_sec * 1000 + tv.tv_usec / 1000;
}

/**
 * @brief Signal disconnect callback function
 * @return 0 on success
 */
int demo_on_signal_disconnect_callback(void)
{
    tuya_ipc_demo_end();
    return 0;
}

/**
 * @brief Get video frame callback function
 * @param media_frame Media frame structure
 * @return 0 on success, -1 on failure
 */
int demo_on_get_video_frame_callback(MEDIA_FRAME *media_frame)
{
    g_offset = g_frame_start + g_frame_len;
    if (g_offset >= g_file_size) {
        g_is_last_frame = FALSE;
        g_frame_len = 0;
        g_frame_start = 0;
        g_next_frame_len = 0;
        g_next_frame_start = 0;
        g_offset = 0;
        g_is_key_frame = 0;
        return -1;
    }
    
    int ret = read_one_frame_from_demo_video_file(g_video_buf + g_offset, 
                                                 g_offset, 
                                                 g_file_size - g_offset, 
                                                 &g_is_key_frame,
                                                 &g_frame_len, 
                                                 &g_frame_start);
    if (ret) {
        return -1;
    }
    
    memcpy(media_frame->data, g_video_buf + g_offset, g_frame_len);
    media_frame->size = g_frame_len;
    media_frame->pts = get_time_ms();
    media_frame->timestamp = get_time_ms();
    
    if (g_is_key_frame) {
        media_frame->type = eVideoIFrame;
    } else {
        media_frame->type = eVideoPBFrame;
    }

    /* Calculate sleep time based on frame rate */
    unsigned int sleep_time = 1000 * 1000 / TKL_VENC_MAIN_FPS;
    usleep(sleep_time);
    
    return 0;
}

/**
 * @brief Get audio frame callback function
 * @param media_frame Media frame structure
 * @return 0 on success
 */
int demo_on_get_audio_frame_callback(MEDIA_FRAME *media_frame)
{
    /* TODO: Implement audio frame processing */
    return 0;
}

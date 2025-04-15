/**
 * @file speaker_upload.c
 * @brief Defines the speaker audio upload interface for Tuya's audio service.
 *
 * This source file provides the implementation of speaker audio upload
 * functionality within the Tuya audio service framework. It includes the
 * initialization, configuration, and processing functions for audio upload,
 * which handles the capture and transmission of speaker audio data to the
 * cloud platform. The implementation supports various upload parameters and
 * protocols to ensure reliable audio data transmission. This file is essential
 * for developers working on IoT applications that require remote audio
 * monitoring and speaker feedback analysis capabilities.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 *
 */

#include <stdio.h>
#include <string.h>

#include "speaker_encode.h"
#include "speaker_upload.h"

#include "tal_memory.h"
#include "tal_mutex.h"
#include "tal_time_service.h"

#include "tal_log.h"
#include "tuya_cloud_types.h"

#ifndef SYS_TIME
#define SYS_TIME                               tal_time_get_posix
#define SPEAKER_TIME_UP(start_tm, interval)    ((SYS_TIME() > start_tm) && (SYS_TIME() - start_tm > interval))
#define SYS_TIME_MS                            tal_time_get_posix_ms
#define SPEAKER_TIME_MS_UP(start_tm, interval) ((SYS_TIME_MS() > start_tm) && (SYS_TIME_MS() - start_tm > interval))
#endif

#define tal_mutex_create_init(handle) tal_mutex_create_init(handle)
#define tal_mutex_lock(handle)        tal_mutex_lock(handle)
#define tal_mutex_unlock(handle)      tal_mutex_unlock(handle)
#define tal_mutex_release(handle)     tal_mutex_release(handle)

static MEDIA_UPLOAD_MGR_S upload_mgr = {0x0};

static inline void __report_upload_status(SPEAKER_UPLOAD_STAT_E status)
{
    if (upload_mgr.config.report_stat_cb)
        upload_mgr.config.report_stat_cb(status, upload_mgr.config.userdata);
}

/**
 * @brief Start a new media upload session
 *
 * This function initializes and starts a new media upload session with the specified
 * session ID. It sets up the encoder, initializes the upload context, and begins
 * the upload process. If there's an existing upload session, it will be forcefully
 * stopped before starting the new one.
 *
 * @param[in] session_id Unique identifier for the upload session
 *
 * @return @ref OPERATE_RET
 * @retval OPRT_OK Upload session started successfully
 * @retval OPRT_INVALID_PARM Invalid session ID (NULL)
 * @retval OPRT_COM_ERROR Encoder start or upload initialization failed
 */
OPERATE_RET speaker_intf_upload_media_start(const char *session_id)
{
    OPERATE_RET ret = OPRT_COM_ERROR;

    if (NULL == session_id) {
        PR_ERR("invalid params");
        return OPRT_INVALID_PARM;
    }

    MEDIA_UPLOAD_TASK_S *p_task = &upload_mgr.task;

    if (p_task->ctx && (upload_mgr.is_encoding || upload_mgr.is_uploading)) {
        PR_WARN("context %p, encoding -> %s, uploading -> %s, force stop, will do restart", p_task->ctx,
                upload_mgr.is_encoding ? "yes" : "no", upload_mgr.is_uploading ? "yes" : "no");
        speaker_intf_upload_media_stop(TRUE);
    }

    tal_mutex_lock(upload_mgr.mutex);
    memset(p_task, 0x0, sizeof(MEDIA_UPLOAD_TASK_S));
    SPEAKER_ENCODE_INFO_S param = {0};
    param.encode_type = upload_mgr.config.params.encode_type,
    memcpy(&param.info, &upload_mgr.config.params.info, sizeof(param.info));

    if (OPRT_OK != (ret = speaker_encode_start(p_task, &param))) {
        PR_ERR("speaker_encode_start error:%d", ret);
        tal_mutex_unlock(upload_mgr.mutex);
        return ret;
    }
    upload_mgr.is_encoding = TRUE;

    ret = tuya_voice_upload_start(&p_task->ctx, param.encode_type, TUYA_VOICE_UPLOAD_TARGET_SPEECH, (char *)session_id,
                                  p_task->encoder.p_start_data, p_task->encoder.start_data_len);
    p_task->stat_manage.start_tm = SYS_TIME();
    p_task->stat_manage.last_upload_tm = SYS_TIME();
    p_task->upload_stat = UPLOAD_TASK_START;
    if (OPRT_OK != ret) {
        PR_ERR("tuya_voice_upload_start error:%d", ret);
        // p_task->upload_stat = (OPRT_GW_MQ_OFFLILNE == ret) ? UPLOAD_TASK_NET_ERR : UPLOAD_TASK_ERR;
        speaker_encode_free(p_task);
    }
    upload_mgr.is_uploading = TRUE;
    PR_DEBUG("context %p", p_task->ctx);
    tal_mutex_unlock(upload_mgr.mutex);

    return ret;
}

/**
 * @brief Send audio data for upload
 *
 * This function processes and sends audio data to the upload stream. It handles
 * the encoding of raw audio data and manages the upload timing statistics.
 *
 * @param[in] p_buf Pointer to the audio data buffer
 * @param[in] buf_len Length of the audio data in bytes
 *
 * @return @ref OPERATE_RET
 * @retval OPRT_OK Data sent successfully
 * @retval Other Error codes from encoding or upload process
 */
OPERATE_RET speaker_intf_upload_media_send(const uint8_t *p_buf, uint32_t buf_len)
{
    OPERATE_RET ret = OPRT_OK;

    tal_mutex_lock(upload_mgr.mutex);
    MEDIA_UPLOAD_TASK_S *p_task = &upload_mgr.task;
    ret = speaker_encode(p_task, (uint8_t *)p_buf, buf_len);
    p_task->stat_manage.last_upload_tm = SYS_TIME();
    if (OPRT_OK != ret) {
        // p_task->upload_stat = (OPRT_GW_MQ_OFFLILNE == ret) ? UPLOAD_TASK_NET_ERR : UPLOAD_TASK_ERR;
    }
    tal_mutex_unlock(upload_mgr.mutex);

    return ret;
}

/**
 * @brief Stop the current media upload session
 *
 * This function stops the current media upload session. It can either perform
 * a graceful shutdown or force stop the upload process. It cleans up encoder
 * resources and upload context.
 *
 * @param[in] is_force_stop TRUE for immediate force stop, FALSE for graceful shutdown
 *
 * @return @ref OPERATE_RET
 * @retval OPRT_OK Upload stopped successfully
 * @retval Other Error codes from stopping upload process
 */
OPERATE_RET speaker_intf_upload_media_stop(BOOL_T is_force_stop)
{
    OPERATE_RET ret = OPRT_OK;

    tal_mutex_lock(upload_mgr.mutex);
    MEDIA_UPLOAD_TASK_S *p_task = &upload_mgr.task;
    PR_DEBUG("context %p, force_stop:%d count:%d", p_task->ctx, is_force_stop, p_task->encoder.count);
    if (upload_mgr.is_encoding) {
        speaker_encode_free(p_task);
    }
    if (upload_mgr.is_uploading && p_task->ctx) {
        ret = tuya_voice_upload_stop(p_task->ctx, is_force_stop);
    }
    p_task->upload_stat = UPLOAD_TASK_END;
    if (OPRT_OK != ret) {
        // p_task->upload_stat = (OPRT_GW_MQ_OFFLILNE == ret) ? UPLOAD_TASK_NET_ERR : UPLOAD_TASK_ERR;
    }
    p_task->ctx = NULL; ///< XXX: maybe do nothing!
    upload_mgr.is_encoding = FALSE;
    upload_mgr.is_uploading = FALSE;
    tal_mutex_unlock(upload_mgr.mutex);

    return ret;
}

/**
 * @brief Retrieve the message ID for the current upload session
 *
 * This function gets the unique message identifier associated with the current
 * upload session. The message ID is copied into the provided buffer.
 *
 * @param[out] buffer Buffer to store the message ID
 * @param[in] len Maximum length of the buffer
 *
 * @return @ref OPERATE_RET
 * @retval OPRT_OK Message ID retrieved successfully
 * @retval Other Error codes from message ID retrieval process
 */
OPERATE_RET speaker_intf_upload_media_get_message_id(char *buffer, int len)
{
    OPERATE_RET ret = OPRT_OK;
    MEDIA_UPLOAD_TASK_S *p_task = &upload_mgr.task;
    tal_mutex_lock(upload_mgr.mutex);
    ret = tuya_voice_upload_get_message_id(p_task->ctx, buffer, len);
    tal_mutex_unlock(upload_mgr.mutex);

    return ret;
}

static void speaker_upload_check_task_stat(TIMER_ID timer_id, void *arg)
{
    static TIME_T last_check_task_tm = 0;
    SPEAKER_UPLOAD_STAT_E stat = SPEAKER_UP_STAT_ERR;

    if (SPEAKER_TIME_UP(last_check_task_tm, TY_UPLOAD_CHECK_TASK_INTR)) {
        last_check_task_tm = SYS_TIME();
        // check upload task
        MEDIA_UPLOAD_TASK_S *p_upload = &upload_mgr.task;

        if (!p_upload->stat_manage.net_alarm_flag &&
            (UPLOAD_TASK_ERR == p_upload->upload_stat || UPLOAD_TASK_NET_ERR == p_upload->upload_stat ||
             (UPLOAD_TASK_START == p_upload->upload_stat &&
              SPEAKER_TIME_UP(p_upload->stat_manage.last_upload_tm, TY_UPLOAD_TASK_TIMEOUT)))) {
            if (UPLOAD_TASK_NET_ERR == p_upload->upload_stat) {
                stat = SPEAKER_UP_STAT_NET_ERR;
            }

            PR_DEBUG("upload task may error, upload_stat: %d", p_upload->upload_stat);
            p_upload->stat_manage.net_alarm_flag = TRUE;

            __report_upload_status(stat);
        }
    }
}

/**
 * @brief Initialize the speaker upload interface
 *
 * This function initializes the speaker upload system with the provided configuration.
 * It sets up the upload manager, creates necessary mutexes and timers, and
 * initializes the encoder subsystem.
 *
 * @param[in] config Pointer to the upload configuration structure
 *
 * @return @ref OPERATE_RET
 * @retval OPRT_OK Initialization successful
 * @retval OPRT_INVALID_PARM Invalid configuration pointer
 * @retval OPRT_COM_ERROR Mutex or timer creation failed
 */
OPERATE_RET speaker_intf_upload_init(SPEAKER_UPLOAD_CONFIG_S *config)
{
    OPERATE_RET ret = OPRT_COM_ERROR;

    if (NULL == config) {
        PR_ERR("invalid params");
        return OPRT_INVALID_PARM;
    }

    memset(&upload_mgr, 0, sizeof(MEDIA_UPLOAD_MGR_S));
    memcpy(&upload_mgr.config, config, sizeof(SPEAKER_UPLOAD_CONFIG_S));

    if (OPRT_OK != (ret = tal_mutex_create_init(&upload_mgr.mutex))) {
        PR_ERR("Create mutex err:%d", ret);
        return ret;
    }

    if (OPRT_OK != (ret = tal_sw_timer_create(speaker_upload_check_task_stat, NULL, &upload_mgr.mgr_tm))) {
        PR_ERR("tal_sw_timer_create mgr_tm error: %d", ret);
        return ret;
    }
    tal_sw_timer_start(upload_mgr.mgr_tm, TY_SPEAKER_UP_MGR_TIMER_INTR, TAL_TIMER_CYCLE);

    speaker_encode_init();

    PR_DEBUG("start speaker upload ok");

    return OPRT_OK;
}

/**
 * @brief Register a new media encoder
 *
 * This function registers a new media encoder with the speaker upload system.
 * The encoder will be available for use in subsequent upload sessions.
 *
 * @param[in] encoder Pointer to the encoder structure to register
 *
 * @return @ref OPERATE_RET
 * @retval OPRT_OK Registration successful
 * @retval Other Error codes from encoder registration process
 */
OPERATE_RET speaker_intf_encode_register(SPEAKER_MEDIA_ENCODER_S *encoder)
{
    return speaker_encode_register_cb(encoder);
}

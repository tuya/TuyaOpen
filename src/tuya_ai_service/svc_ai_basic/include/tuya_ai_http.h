/**
 * @file tuya_ai_http.h
 * @author tuya
 * @brief ai http
 * @version 0.1
 * @date 2025-05-18
 *
 * @copyright Copyright (c) 2025 Tuya Inc. All Rights Reserved.
 *
 * Permission is hereby granted, to any person obtaining a copy of this software and
 * associated documentation files (the "Software"), Under the premise of complying
 * with the license of the third-party open source software contained in the software,
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software.
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 */

#ifndef __TUYA_AI_HTTP_H__
#define __TUYA_AI_HTTP_H__

#include "tuya_ai_types.h"

#include "tuya_cloud_types.h"
#include "tuya_ai_client.h"
#include "tuya_ai_biz.h"

/**
 * @brief http download ai audio
 *
 * @param[in] url media url
 * @param[in] cb callback function to receive the audio data
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tuya_ai_http_dld_audio(char *url, AI_BIZ_RECV_CB cb);

/**
 * @brief http download ai video
 *
 * @param[in] url media url
 * @param[in] cb callback function to receive the video data
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tuya_ai_http_dld_video(char *url, AI_BIZ_RECV_CB cb);

/**
 * @brief http download ai image
 *
 * @param[in] url media url
 * @param[in] cb callback function to receive the image data
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tuya_ai_http_dld_image(char *url, AI_BIZ_RECV_CB cb);

/**
 * @brief http download ai file
 *
 * @param[in] url media url
 * @param[in] cb callback function to receive the file data
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tuya_ai_http_dld_file(char *url, AI_BIZ_RECV_CB cb);

/**
 * @brief http download ai text
 *
 * @param[in] url media url
 * @param[in] cb callback function to receive the text data
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tuya_ai_http_dld_text(char *url, AI_BIZ_RECV_CB cb);
#endif // __TUYA_AI_HTTP_H__

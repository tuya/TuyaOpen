/**
 * @file app_ai.h
 * @brief app_ai module is used to
 * @version 0.1
 * @date 2025-03-26
 */

#ifndef __APP_AI_H__
#define __APP_AI_H__

#include "tuya_cloud_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/***********************************************************
************************macro define************************
***********************************************************/
typedef uint8_t APP_AI_MSG_TYPE_T;
#define APP_AI_MSG_TYPE_TEXT_ASR    0x01
#define APP_AI_MSG_TYPE_TEXT_NLG    0x02
#define APP_AI_MSG_TYPE_AUDIO_START 0x03
#define APP_AI_MSG_TYPE_AUDIO_DATA  0x04
#define APP_AI_MSG_TYPE_AUDIO_STOP  0x05
#define APP_AI_MSG_TYPE_EMOTION     0x06

typedef struct {
    APP_AI_MSG_TYPE_T type;
    uint32_t data_len;
    uint8_t *data;
} APP_AI_MSG_T;

/***********************************************************
***********************typedef define***********************
***********************************************************/
typedef void (*APP_AI_MSG_CB)(APP_AI_MSG_T *msg);

/***********************************************************
********************function declaration********************
***********************************************************/

OPERATE_RET app_ai_init(APP_AI_MSG_CB msg_cb);

OPERATE_RET app_ai_upload_start(uint8_t int_enable);

OPERATE_RET app_ai_upload_data(uint8_t is_first, uint8_t *data, uint32_t len);

OPERATE_RET app_ai_upload_stop(void);

#ifdef __cplusplus
}
#endif

#endif /* __APP_AI_H__ */

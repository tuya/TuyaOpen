/**
* @file tkl_asr.h
* @brief Common process - Automatic Speech Recognition
* @version 0.1
* @date 2021-08-18
*
* @copyright Copyright 2021-2030 Tuya Inc. All Rights Reserved.
*
*/
#ifndef __TKL_ASR_H__
#define __TKL_ASR_H__


#include "tuya_cloud_types.h"


#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    TKL_ASR_WAKEUP_WORD_UNKNOWN,
    TKL_ASR_WAKEUP_NIHAO_TUYA,
    TKL_ASR_WAKEUP_NIHAO_XIAOZHI,
    TKL_ASR_WAKEUP_XIAOZHI_TONGXUE,
    TKL_ASR_WAKEUP_XIAOZHI_GUANJIA,
    TKL_ASR_WAKEUP_XIAOAI_XIAOAI,
    TKL_ASR_WAKEUP_XIAODU_XIAODU,
    TKL_ASR_WAKEUP_WORD_MAX,
} TKL_ASR_WAKEUP_WORD_E;

OPERATE_RET tkl_asr_init(void);

OPERATE_RET tkl_asr_wakeup_word_config(TKL_ASR_WAKEUP_WORD_E *wakeup_word_arr, uint8_t arr_cnt);

uint32_t tkl_asr_get_process_uint_size(void);

TKL_ASR_WAKEUP_WORD_E tkl_asr_recognize_wakeup_word(uint8_t *data, uint32_t len);

OPERATE_RET tkl_asr_deinit(void);


#ifdef __cplusplus
}
#endif

#endif

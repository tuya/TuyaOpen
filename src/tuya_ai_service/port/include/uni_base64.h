/**
 * @file uni_base64.h
 * @brief uni_base64 module is used to
 * @version 0.1
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#ifndef __UNI_BASE64_H__
#define __UNI_BASE64_H__

#include "tuya_cloud_types.h"

#include "mix_method.h"

#ifdef __cplusplus
extern "C" {
#endif

/***********************************************************
************************macro define************************
***********************************************************/

/***********************************************************
***********************typedef define***********************
***********************************************************/

/***********************************************************
********************function declaration********************
***********************************************************/
/**
 * @brief calculate the required buffer size for base64 encoding
 * @param[in] slen  source data length
 */
#define TY_BASE64_BUF_LEN_CALC(slen) (((slen) / 3 + ((slen) % 3 != 0)) * 4 + 1) // 1 for '\0'

#ifdef __cplusplus
}
#endif

#endif /* __UNI_BASE64_H__ */

/**
 * @file uni_log.h
 * @brief uni_log module is used to
 * @version 0.1
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#ifndef __UNI_LOG_H__
#define __UNI_LOG_H__

#include "tuya_cloud_types.h"
#include "tal_log.h"

#ifdef __cplusplus
extern "C" {
#endif

/***********************************************************
************************macro define************************
***********************************************************/

#ifndef TAL_PR_ERR
#define TAL_PR_DEBUG PR_DEBUG
#endif
#ifndef TAL_PR_INFO
#define TAL_PR_INFO  PR_INFO
#endif
#ifndef TAL_PR_WARN
#define TAL_PR_WARN  PR_WARN
#endif
#ifndef TAL_PR_ERR
#define TAL_PR_ERR   PR_ERR
#endif

/***********************************************************
***********************typedef define***********************
***********************************************************/

/***********************************************************
********************function declaration********************
***********************************************************/

#ifdef __cplusplus
}
#endif

#endif /* __UNI_LOG_H__ */

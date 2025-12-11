/**
 * @file httpc.h
 * @brief httpc module is used to 
 * @version 0.1
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#ifndef __HTTPC_H__
#define __HTTPC_H__

#include "tuya_cloud_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/***********************************************************
************************macro define************************
***********************************************************/
typedef void * http_session_t;

/***********************************************************
***********************typedef define***********************
***********************************************************/


/***********************************************************
********************function declaration********************
***********************************************************/

int http_read_content(http_session_t handle, void *buf, unsigned int max_len);

#ifdef __cplusplus
}
#endif

#endif /* __HTTPC_H__ */

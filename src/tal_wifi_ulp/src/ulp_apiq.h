/**
 * @file ulp_apiq.h
 * @brief net manager - message queue driven API scheduling for ULP
 * @version 0.1
 * @date 2024-06-06
 *
 * @copyright Copyright (c) 2023 Tuya Inc. All Rights Reserved.
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
#ifndef _TUYA_ULP_APIQ_H
#define _TUYA_ULP_APIQ_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/***********************************************************
*************************micro define***********************
***********************************************************/
typedef void (*netmgr_api_callback_fn)(void* arg);

typedef enum {
    NETMGR_API_TYPE,
    NETMGR_NTFY_TYPE,
} NETMGR_CMD_TYPE;

typedef struct {
    NETMGR_CMD_TYPE type;
    union {
        struct {
            netmgr_api_callback_fn function;
            void* arg;
        } api_msg;
    } msg;
} netmgr_msg;

/********************************************************************************
 ***********************************ulp_apiq.h***********************************
 ********************************************************************************/

/**
 * @brief nlmgr init, called by lpmgr_init.
 *
 * @param[in] no.
 * @return 0: success, other: fail.
 */
int mgr_init(void);

/**
 * @brief Transmit event to netmgr_thread for api
 *
 * @param[in] msg     msg passed to netmgr_thread
 * @retval 0          success
 * @retval Other      fail
 */
int netmgr_api_call(netmgr_msg* msg);

/**
 * @brief timer refresh.
 *
 * @param[in/out] no args.
 * @return  0: success  Other: fail
 */
int netmgr_timer_refresh(void);

#ifdef __cplusplus
}
#endif

#endif /* _TUYA_ULP_APIQ_H */

/**
 * @file at_module_ml307r.h
 * @brief at_module_ml307r module is used to
 * @version 0.1
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#ifndef __AT_MODULE_ML307R_H__
#define __AT_MODULE_ML307R_H__

#include "tuya_cloud_types.h"

#include "tal_at_modem.h"

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

OPERATE_RET at_module_ml307r_init(AT_MODULE_OPS_T *ops, AT_MODEM_CB urc_cb);

#ifdef __cplusplus
}
#endif

#endif /* __AT_MODULE_ML307R_H__ */

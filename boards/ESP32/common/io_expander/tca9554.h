/**
 * @file tca9554.h
 * @brief tca9554 module is used to
 * @version 0.1
 * @date 2025-05-13
 */

#ifndef __TCA9554_H__
#define __TCA9554_H__

#include "tuya_cloud_types.h"

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

int tca9554_init(void);

int tca9554_set_dir(uint32_t pin_num_mask, int is_input);

int tca9554_set_level(uint32_t pin_num_mask, int level);

#ifdef __cplusplus
}
#endif

#endif /* __TCA9554_H__ */

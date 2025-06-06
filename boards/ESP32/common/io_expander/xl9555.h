/**
 * @file xl9555.h
 * @brief xl9555 module is used to
 * @version 0.1
 * @date 2025-06-05
 */

#ifndef __XL9555_H__
#define __XL9555_H__

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

int xl9555_init(void);

int xl9555_set_dir(uint32_t pin_num_mask, int is_input);

int xl9555_set_level(uint32_t pin_num_mask, uint32_t level);

int xl9555_get_level(uint32_t pin_num_mask, uint32_t *level);

#ifdef __cplusplus
}
#endif

#endif /* __XL9555_H__ */

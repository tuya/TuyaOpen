/**
 * @file tdd_button_io_expander.h
 * @brief TDD button driver for buttons connected via IO expander (e.g. xl9555, tca9554).
 *
 * Uses function pointers for the expander ops so any compatible expander can be used.
 * Only BUTTON_TIMER_SCAN_MODE is supported — IO expanders are polled, not IRQ-driven.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#ifndef __TDD_BUTTON_IO_EXPANDER_H__
#define __TDD_BUTTON_IO_EXPANDER_H__

#include "tuya_cloud_types.h"
#include "tdl_button_driver.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief IO expander operation callbacks.
 *
 * Wire these to the concrete expander (xl9555, tca9554, …) at registration time.
 * init may be NULL if the expander is already initialised before button registration.
 */
typedef int (*io_exp_init_fn)(void);
typedef int (*io_exp_set_dir_fn)(uint32_t pin_mask, int is_input);
typedef int (*io_exp_get_level_fn)(uint32_t pin_mask, uint32_t *level);

typedef struct {
    uint32_t             pin_mask;     /* bit-mask of the expander pin(s) used for this button */
    TUYA_GPIO_LEVEL_E    active_level; /* TUYA_GPIO_LEVEL_LOW or TUYA_GPIO_LEVEL_HIGH */
    io_exp_init_fn       init;         /* expander init — may be NULL */
    io_exp_set_dir_fn    set_dir;      /* set pin direction (required) */
    io_exp_get_level_fn  get_level;    /* read pin level (required) */
} TDD_BUTTON_IO_EXP_CFG_T;

/**
 * @brief Register a button that is connected to an IO expander.
 *
 * @param[in] name   unique button name (same as used with tdl_button_find)
 * @param[in] cfg    hardware configuration, must not be NULL
 * @return OPRT_OK on success, other values on failure
 */
OPERATE_RET tdd_io_expander_button_register(char *name, TDD_BUTTON_IO_EXP_CFG_T *cfg);

#ifdef __cplusplus
}
#endif

#endif /* __TDD_BUTTON_IO_EXPANDER_H__ */

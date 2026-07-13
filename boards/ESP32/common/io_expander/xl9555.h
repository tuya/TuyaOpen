/**
 * @file xl9555.h
 * @brief XL9555 16-bit I2C IO expander helper (ESP-IDF / TuyaOpen integration).
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#ifndef __XL9555_H__
#define __XL9555_H__

#include "tuya_cloud_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ---------------------------------------------------------------------------
 * Type definitions
 * --------------------------------------------------------------------------- */
typedef struct {
    int      i2c_port;
    int      scl_io;
    int      sda_io;
    uint16_t dev_addr;   /* XL9555 7-bit I2C address (commonly 0x20) */
} XL9555_HW_CFG_T;

/* ---------------------------------------------------------------------------
 * Function declarations
 * --------------------------------------------------------------------------- */
/**
 * @brief Initialise the XL9555 expander on the given I2C bus.
 * @param[in] hw Hardware configuration; must be non-NULL on the first call.
 *               Subsequent calls return 0 immediately and ignore @p hw.
 * @return 0 on success, -1 on failure.
 */
int xl9555_init(XL9555_HW_CFG_T *hw);

/**
 * @brief Configure the direction of one or more pins.
 * @param[in] pin_num_mask Bit mask (port0 in low byte, port1 in high byte).
 * @param[in] is_input     Non-zero for input, zero for output.
 * @return 0 on success, -1 on failure.
 */
int xl9555_set_dir(uint32_t pin_num_mask, int is_input);

/**
 * @brief Drive output pins to the given level.
 * @param[in] pin_num_mask Bit mask of pins to drive.
 * @param[in] level        0 or non-zero.
 * @return 0 on success, -1 on failure.
 */
int xl9555_set_level(uint32_t pin_num_mask, uint32_t level);

/**
 * @brief Read the current input level of one or more pins.
 * @param[in]  pin_num_mask Bit mask of pins to read.
 * @param[out] level        Output: per-pin level bits.
 * @return 0 on success, -1 on failure.
 */
int xl9555_get_level(uint32_t pin_num_mask, uint32_t *level);

#ifdef __cplusplus
}
#endif

#endif /* __XL9555_H__ */

/**
 * @file xteink_x4_spi.h
 * @brief Shared SPI bus helper for Xteink X4 peripherals.
 * @version 0.1
 * @date 2026-04-30
 * @copyright Copyright (c) 2026 Tuya Inc. All Rights Reserved.
 */
#ifndef __XTEINK_X4_SPI_H__
#define __XTEINK_X4_SPI_H__

#include "tuya_cloud_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize the shared X4 SPI bus for EPD and microSD.
 * @param[in] max_transfer_sz maximum transfer size required by the caller.
 * @return OPRT_OK on success, error code on failure.
 */
OPERATE_RET xteink_x4_spi_bus_init(uint32_t max_transfer_sz);

#ifdef __cplusplus
}
#endif
#endif /* __XTEINK_X4_SPI_H__ */

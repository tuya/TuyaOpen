/**
 * @file example_drv_ir.h
 * @brief Infrared communication example driver interface for Tuya IoT devices.
 *
 * This header file provides the interface definitions for demonstrating
 * infrared communication capabilities using Tuya's IR peripheral drivers.
 * It serves as an educational and testing framework for developers to
 * understand and implement IR functionality in their Tuya IoT applications.
 *
 * The example demonstrates:
 * - Hardware registration and configuration for IR devices
 * - Driver initialization and setup procedures
 * - Basic IR transmission and reception operations
 * - Integration with Tuya's TDD (Tuya Device Driver) and TDL (Tuya Device Layer) systems
 *
 * This interface abstracts the complexity of IR hardware management and
 * provides simple functions for registering IR hardware, opening drivers,
 * and running IR communication examples. It supports multiple hardware
 * platforms and can be configured for different IR protocols including
 * NEC protocol and raw timecode transmission.
 *
 * The example is designed to work with various Tuya-supported microcontroller
 * platforms and provides a foundation for developing custom IR-enabled
 * IoT applications.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 *
 */

#ifndef __EXAMPLE_IR_H__
#define __EXAMPLE_IR_H__

#include "tuya_cloud_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/***********************************************************
*********************** macro define ***********************
***********************************************************/


/***********************************************************
********************** typedef define **********************
***********************************************************/


/***********************************************************
******************* function declaration *******************
***********************************************************/
/**
 * @brief    register hardware 
 *
 * @param[in] : the name of the driver
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET reg_ir_hardware(char *device_name);

/**
 * @brief    open driver
 *
 * @param[in] : the name of the driver
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET open_ir_driver(char *device_name);

#ifdef __cplusplus
}
#endif

#endif /* __EXAMPLE_IR_H__ */

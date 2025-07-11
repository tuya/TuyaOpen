/**
 * @file tdl_display_driver.h
 * @brief TDL display driver interface definitions
 *
 * This file defines the high-level display driver interface for the TDL (Tuya Display Layer)
 * system. It provides abstraction layer functions and data structures for managing different
 * types of display controllers including SPI, QSPI, RGB, and MCU 8080 interfaces, enabling
 * unified display operations across various hardware configurations.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 *
 */

#ifndef __TDL_DISPLAY_DRIVER_H__
#define __TDL_DISPLAY_DRIVER_H__

#include "tuya_cloud_types.h"
#include "tdl_display_manage.h"

#ifdef __cplusplus
extern "C" {
#endif

/***********************************************************
************************macro define************************
***********************************************************/
#define DISPLAY_DEV_NAME_MAX_LEN 32

/***********************************************************
***********************typedef define***********************
***********************************************************/
typedef struct {
    TUYA_GPIO_NUM_E   pin;
    TUYA_GPIO_LEVEL_E active_level;
} TUYA_DISPLAY_IO_CTRL_T;

typedef struct {
    TUYA_PWM_NUM_E       id;
    TUYA_PWM_BASE_CFG_T  cfg;
} TUYA_DISPLAY_PWM_CTRL_T;

typedef enum  {
    TUYA_DISP_BL_TP_NONE,
    TUYA_DISP_BL_TP_GPIO,
    TUYA_DISP_BL_TP_PWM,
}TUYA_DISPLAY_BL_TYPE_E;

typedef struct {
    TUYA_DISPLAY_BL_TYPE_E    type;
    union {
        TUYA_DISPLAY_IO_CTRL_T   gpio;
        TUYA_DISPLAY_PWM_CTRL_T  pwm;
    };
} TUYA_DISPLAY_BL_CTRL_T;

typedef void*  TDD_DISP_DEV_HANDLE_T;

typedef OPERATE_RET (*TDD_DISPLAY_SEQ_INIT_CB)(void);

typedef struct {
    TUYA_DISPLAY_TYPE_E type;
    uint16_t width;
    uint16_t height;
    TUYA_DISPLAY_PIXEL_FMT_E fmt;
    TUYA_DISPLAY_ROTATION_E rotation;
    TUYA_DISPLAY_BL_CTRL_T bl;
    TUYA_DISPLAY_IO_CTRL_T power;
} TDD_DISP_DEV_INFO_T;

#if defined(ENABLE_RGB) && (ENABLE_RGB==1)
typedef struct {
    TUYA_RGB_BASE_CFG_T         cfg;
    TUYA_DISPLAY_BL_CTRL_T      bl;
    TUYA_DISPLAY_IO_CTRL_T      power;
    TDD_DISPLAY_SEQ_INIT_CB     init_cb; 
    TUYA_DISPLAY_ROTATION_E     rotation;
}TDD_DISP_RGB_CFG_T;
#endif

#if defined(ENABLE_SPI) && (ENABLE_SPI==1)
typedef struct {
    uint16_t width;
    uint16_t height;
    TUYA_DISPLAY_PIXEL_FMT_E pixel_fmt;
    TUYA_GPIO_NUM_E cs_pin;
    TUYA_GPIO_NUM_E dc_pin;
    TUYA_GPIO_NUM_E rst_pin;
    TUYA_SPI_NUM_E port;
    uint32_t spi_clk;
    uint8_t cmd_caset;
    uint8_t cmd_raset;
    uint8_t cmd_ramwr;
} DISP_SPI_BASE_CFG_T;

typedef void (*TDD_DISP_SPI_SET_WINDOW_CB)(DISP_SPI_BASE_CFG_T *p_cfg, uint16_t x_start, uint16_t y_start,\
                                           uint16_t x_end, uint16_t y_end);

typedef struct { 
    DISP_SPI_BASE_CFG_T         cfg;
    TUYA_DISPLAY_BL_CTRL_T      bl;
    TUYA_DISPLAY_IO_CTRL_T      power;
    TUYA_DISPLAY_ROTATION_E     rotation;
    const uint8_t              *init_seq;      // Initialization commands for the display
    TDD_DISP_SPI_SET_WINDOW_CB  set_window_cb; // Callback to set the display window
}TDD_DISP_SPI_CFG_T;
#endif

#if defined(ENABLE_QSPI) && (ENABLE_QSPI==1)
typedef struct {
    uint16_t                    width;
    uint16_t                    height;
    TUYA_DISPLAY_PIXEL_FMT_E    pixel_fmt;
    TUYA_GPIO_NUM_E             cs_pin;
    TUYA_GPIO_NUM_E             dc_pin;
    TUYA_GPIO_NUM_E             rst_pin;
    TUYA_QSPI_NUM_E             port;
    uint32_t                    spi_clk;
    uint8_t                     cmd_caset;
    uint8_t                     cmd_raset;
    uint8_t                     cmd_ramwr;
}DISP_QSPI_BASE_CFG_T;

typedef struct { 
    DISP_QSPI_BASE_CFG_T        cfg;
    TUYA_DISPLAY_BL_CTRL_T      bl;
    TUYA_DISPLAY_IO_CTRL_T      power;
    TUYA_DISPLAY_ROTATION_E     rotation;
    const uint8_t              *init_seq; // Initialization commands for the display
}TDD_DISP_QSPI_CFG_T;
#endif

#if defined(ENABLE_MCU8080) && (ENABLE_MCU8080==1)
typedef struct { 
    TUYA_8080_BASE_CFG_T        cfg;
    TUYA_DISPLAY_BL_CTRL_T      bl;
    TUYA_DISPLAY_IO_CTRL_T      power;
    TUYA_DISPLAY_ROTATION_E     rotation;
    TUYA_GPIO_NUM_E             te_pin;
    TUYA_GPIO_IRQ_E             te_mode;
    uint8_t                     cmd_caset;
    uint8_t                     cmd_raset;
    uint8_t                     cmd_ramwr;
    uint8_t                     cmd_ramwrc;
    const uint32_t             *init_seq; // Initialization commands for the display
}TDD_DISP_MCU8080_CFG_T;
#endif

typedef struct {
    OPERATE_RET (*open)(TDD_DISP_DEV_HANDLE_T device);
    OPERATE_RET (*flush)(TDD_DISP_DEV_HANDLE_T device, TDL_DISP_FRAME_BUFF_T *frame_buff);
    OPERATE_RET (*close)(TDD_DISP_DEV_HANDLE_T device);
} TDD_DISP_INTFS_T;

/***********************************************************
********************function declaration********************
***********************************************************/
/**
 * @brief Registers a display device with the display management system.
 *
 * This function creates and initializes a new display device entry in the internal 
 * device list, binding it with the provided name, hardware interfaces, callbacks, 
 * and device information.
 *
 * @param name Name of the display device (used for identification).
 * @param tdd_hdl Handle to the low-level display driver instance.
 * @param intfs Pointer to the display interface functions (open, flush, close, etc.).
 * @param dev_info Pointer to the display device information structure.
 *
 * @return Returns OPRT_OK on success, or an appropriate error code if registration fails.
 */
OPERATE_RET tdl_disp_device_register(char *name, TDD_DISP_DEV_HANDLE_T tdd_hdl, \
                                     TDD_DISP_INTFS_T *intfs, TDD_DISP_DEV_INFO_T *dev_info);

#if defined(ENABLE_RGB) && (ENABLE_RGB==1) 
/**
 * @brief Registers an RGB display device with the display management system.
 *
 * This function creates and initializes a new RGB display device instance, 
 * configures its interface functions, and registers it under the specified name.
 *
 * @param name Name of the display device (used for identification).
 * @param rgb Pointer to the RGB display device configuration structure.
 *
 * @return Returns OPRT_OK on success, or an appropriate error code if registration fails.
 */                                   
OPERATE_RET tdl_disp_rgb_device_register(char *name, TDD_DISP_RGB_CFG_T *rgb);
#endif

#if defined(ENABLE_SPI) && (ENABLE_SPI==1) 
/**
 * @brief Registers an RGB display device with the display management system.
 *
 * This function creates and initializes a new RGB display device instance, 
 * configures its interface functions, and registers it under the specified name.
 *
 * @param name Name of the display device (used for identification).
 * @param rgb Pointer to the RGB display device configuration structure.
 *
 * @return Returns OPRT_OK on success, or an appropriate error code if registration fails.
 */  
OPERATE_RET tdl_disp_spi_device_register(char *name, TDD_DISP_SPI_CFG_T *spi);

/**
 * @brief Initializes the SPI interface for display communication.
 *
 * This function sets up the SPI port and its associated semaphore for synchronization, 
 * and initializes the required GPIO pins for SPI-based display operations.
 *
 * @param p_cfg Pointer to the SPI configuration structure containing port and clock settings.
 *
 * @return Returns OPRT_OK on success, or an appropriate error code if initialization fails.
 */
OPERATE_RET tdl_disp_spi_init(DISP_SPI_BASE_CFG_T *p_cfg);

/**
 * @brief Sends a command over the SPI interface to the display device.
 *
 * This function pulls the chip select (CS) and data/command (DC) pins low to indicate 
 * command transmission, then sends the specified command byte via SPI.
 *
 * @param p_cfg Pointer to the SPI configuration structure containing pin and port settings.
 * @param cmd The command byte to be sent to the display.
 *
 * @return Returns OPRT_OK on success, or an appropriate error code if sending the command fails.
 */
OPERATE_RET tdl_disp_spi_send_cmd(DISP_SPI_BASE_CFG_T *p_cfg, uint8_t cmd);

/**
 * @brief Sends data over the SPI interface to the display device.
 *
 * This function pulls the chip select (CS) pin low and sets the data/command (DC) pin high 
 * to indicate data transmission, then sends the specified data buffer via SPI.
 *
 * @param p_cfg Pointer to the SPI configuration structure containing pin and port settings.
 * @param data Pointer to the data buffer to be sent.
 * @param data_len Length of the data buffer in bytes.
 *
 * @return Returns OPRT_OK on success, or an appropriate error code if sending the data fails.
 */
OPERATE_RET tdl_disp_spi_send_data(DISP_SPI_BASE_CFG_T *p_cfg, uint8_t *data, uint32_t data_len);

/**
 * @brief Executes the display initialization sequence over SPI.
 *
 * This function processes a command-based initialization sequence, sending commands 
 * and associated data to the display device to configure it during initialization.
 *
 * @param p_cfg Pointer to the SPI configuration structure containing pin and port settings.
 * @param init_seq Pointer to the initialization sequence array (command/data format).
 *
 * @return None.
 */
void tdl_disp_spi_init_seq(DISP_SPI_BASE_CFG_T *p_cfg, const uint8_t *init_seq);

/**
 * @brief Modifies a parameter in the display initialization sequence for a specific command.
 *
 * This function searches for the specified command in the initialization sequence and 
 * updates the parameter at the given index. If the index is out of bounds, an error is logged.
 *
 * @param init_seq Pointer to the initialization sequence array.
 * @param init_cmd The command whose parameter needs to be modified.
 * @param param The new parameter value to set.
 * @param idx The index of the parameter to modify within the command's data block.
 *
 * @return None.
 */
void tdl_disp_modify_init_seq_param(uint8_t *init_seq, uint8_t init_cmd, uint8_t param, uint8_t idx);
#endif

#if defined(ENABLE_QSPI) && (ENABLE_QSPI==1)   
/**
 * @brief Registers a QSPI display device with the display management system.
 *
 * This function creates and initializes a new QSPI display device instance, 
 * configures its interface functions, and registers it under the specified name.
 *
 * @param name Name of the display device (used for identification).
 * @param spi Pointer to the QSPI display device configuration structure.
 *
 * @return Returns OPRT_OK on success, or an appropriate error code if registration fails.
 */
OPERATE_RET tdl_disp_qspi_device_register(char *name, TDD_DISP_QSPI_CFG_T *qspi);
#endif

#if defined(ENABLE_MCU8080) && (ENABLE_MCU8080==1)
/**
 * @brief Registers an MCU8080 display device with the display management system.
 *
 * This function creates and initializes a new MCU8080 display device instance, 
 * configures its interface functions, and registers it under the specified name.
 *
 * @param name Name of the display device (used for identification).
 * @param mcu8080 Pointer to the MCU8080 display device configuration structure.
 *
 * @return Returns OPRT_OK on success, or an appropriate error code if registration fails.
 */
OPERATE_RET tdl_disp_mcu8080_device_register(char *name, TDD_DISP_MCU8080_CFG_T *mcu8080);
#endif

#ifdef __cplusplus
}
#endif

#endif /* __TDL_DISPLAY_DRIVER_H__ */

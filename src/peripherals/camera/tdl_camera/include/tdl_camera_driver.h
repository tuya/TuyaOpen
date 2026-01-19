/**
 * @file tdl_camera_driver.h
 * @brief Camera driver interface header
 *
 * This header file defines the camera driver interface, including device
 * registration, frame creation and management, and driver callback functions.
 * It provides structures for camera device information, configuration, and
 * frame handling for camera driver implementations.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 *
 */

#ifndef __TDL_CAMERA_DRIVER_H__
#define __TDL_CAMERA_DRIVER_H__

#include "tuya_cloud_types.h"
#include "tdl_camera_manage.h"

#ifdef __cplusplus
extern "C" {
#endif

/***********************************************************
************************macro define************************
***********************************************************/
#define CAMERA_DEV_NAME_MAX_LEN 32

/***********************************************************
***********************typedef define***********************
***********************************************************/
typedef enum {
	TUYA_CAMERA_PPI_DEFAULT     = 0,
	TUYA_CAMERA_PPI_128X128     = (128 << 16) | 128,
	TUYA_CAMERA_PPI_170X320     = (170 << 16) | 320,
	TUYA_CAMERA_PPI_240X240     = (240 << 16) | 240,
	TUYA_CAMERA_PPI_320X240     = (320 << 16) | 240,
	TUYA_CAMERA_PPI_320X480     = (320 << 16) | 480,
	TUYA_CAMERA_PPI_360X480     = (360 << 16) | 480,
	TUYA_CAMERA_PPI_400X160     = (400 << 16) | 160,
	TUYA_CAMERA_PPI_400X400     = (400 << 16) | 400,
	TUYA_CAMERA_PPI_412X412     = (412 << 16) | 412,
	TUYA_CAMERA_PPI_454X454     = (454 << 16) | 454,
	TUYA_CAMERA_PPI_480X272     = (480 << 16) | 272,
	TUYA_CAMERA_PPI_480X320     = (480 << 16) | 320,
	TUYA_CAMERA_PPI_480X480     = (480 << 16) | 480,
	TUYA_CAMERA_PPI_640X480     = (640 << 16) | 480,
	TUYA_CAMERA_PPI_480X800     = (480 << 16) | 800,
	TUYA_CAMERA_PPI_480X854     = (480 << 16) | 854,
	TUYA_CAMERA_PPI_480X864     = (480 << 16) | 864,
	TUYA_CAMERA_PPI_720X288     = (720 << 16) | 288,
	TUYA_CAMERA_PPI_720X576     = (720 << 16) | 576,
	TUYA_CAMERA_PPI_720X1280    = (720 << 16) | 1280,
	TUYA_CAMERA_PPI_854X480     = (854 << 16) | 480,
	TUYA_CAMERA_PPI_800X480     = (800 << 16) | 480,
	TUYA_CAMERA_PPI_864X480     = (864 << 16) | 480,
	TUYA_CAMERA_PPI_960X480     = (960 << 16) | 480,
	TUYA_CAMERA_PPI_800X600     = (800 << 16) | 600,
	TUYA_CAMERA_PPI_1024X600    = (1024 << 16) | 600,
	TUYA_CAMERA_PPI_1280X720    = (1280 << 16) | 720,
	TUYA_CAMERA_PPI_1600X1200   = (1600 << 16) | 1200,
	TUYA_CAMERA_PPI_1920X1080   = (1920 << 16) | 1080,
	TUYA_CAMERA_PPI_2304X1296   = (2304 << 16) | 1296,
	TUYA_CAMERA_PPI_7680X4320   = (7680 << 16) | 4320,
}TUYA_CAMERA_PPI_E;

typedef struct {
    TUYA_GPIO_NUM_E   pin;
    TUYA_GPIO_LEVEL_E active_level;
} TUYA_CAMERA_IO_CTRL_T;

typedef void* TDD_CAMERA_DEV_HANDLE_T;

typedef struct {
    TDL_CAMERA_TYPE_E         type;
    uint16_t                  max_fps;
    uint32_t                  max_width;
    uint32_t                  max_height;
    TUYA_FRAME_FMT_E          fmt;
} TDD_CAMERA_DEV_INFO_T;

typedef struct {
    uint16_t                  fps;
    uint16_t                  width;
    uint16_t                  height;
    TDL_CAMERA_FMT_E          out_fmt;
    TUYA_DVP_ENCODED_QUALITY  encoded_quality;
} TDD_CAMERA_OPEN_CFG_T;

typedef struct {
    OPERATE_RET (*open )(TDD_CAMERA_DEV_HANDLE_T device, TDD_CAMERA_OPEN_CFG_T *cfg);
    OPERATE_RET (*close)(TDD_CAMERA_DEV_HANDLE_T device);
} TDD_CAMERA_INTFS_T;

typedef struct {
    void              *sys_param;            // System use, user should not care
    TDL_CAMERA_FRAME_T frame;
    uint8_t            rsv[128];
} TDD_CAMERA_FRAME_T;

/***********************************************************
********************function declaration********************
***********************************************************/
/**
 * @brief Register a camera device
 * @param name Camera device name
 * @param tdd_hdl TDD camera device handle
 * @param intfs Pointer to camera interface functions structure
 * @param dev_info Pointer to camera device information structure
 * @return OPRT_OK on success, OPRT_INVALID_PARM if parameters are invalid,
 *         OPRT_MALLOC_FAILED on memory allocation failure
 */
OPERATE_RET tdl_camera_device_register(char *name, TDD_CAMERA_DEV_HANDLE_T tdd_hdl, \
                                       TDD_CAMERA_INTFS_T *intfs, TDD_CAMERA_DEV_INFO_T *dev_info);
                                    
/**
 * @brief Create a TDD frame from frame node pool
 * @param tdd_hdl TDD camera device handle
 * @param fmt Frame format enumeration
 * @return Pointer to TDD frame structure, or NULL if no frame available or device not found
 */
TDD_CAMERA_FRAME_T *tdl_camera_create_tdd_frame(TDD_CAMERA_DEV_HANDLE_T tdd_hdl, TUYA_FRAME_FMT_E fmt);

/**
 * @brief Release TDD frame back to frame node pool
 * @param tdd_hdl TDD camera device handle
 * @param frame Pointer to TDD frame structure to release
 */
void tdl_camera_release_tdd_frame(TDD_CAMERA_DEV_HANDLE_T tdd_hdl, TDD_CAMERA_FRAME_T *frame);

/**
 * @brief Post TDD frame to processing queue
 * @param tdd_hdl TDD camera device handle
 * @param frame Pointer to TDD frame structure to post
 * @return OPRT_OK on success, OPRT_INVALID_PARM if parameters are invalid,
 *         OPRT_COM_ERROR if queue or device not found
 */
OPERATE_RET tdl_camera_post_tdd_frame(TDD_CAMERA_DEV_HANDLE_T tdd_hdl, TDD_CAMERA_FRAME_T *frame);   






#ifdef __cplusplus
}
#endif

#endif /* __TDL_CAMERA_DRIVER_H__ */

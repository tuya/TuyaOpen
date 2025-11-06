/**
 * @file tdl_pixel_dev_manage.c
 * @brief TDL layer device management implementation for LED pixel devices
 *
 * This source file implements the TDL (Tuya Device Layer) device management functionality
 * for LED pixel devices. It provides device discovery, initialization, configuration,
 * and control operations. The module manages device lifecycle, handles device registration,
 * and provides high-level abstractions for LED strip device operations across different
 * pixel controller types.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 *
 */
#include "tal_log.h"
#include "tal_memory.h"
#include "tal_system.h"

/***********************************************************
*************************private include********************
***********************************************************/
#include "tdl_pixel_struct.h"
#include "tuya_error_code.h"

/***********************************************************
***********************typedef define***********************
***********************************************************/
#define GET_BIT(value, bit) (value & (1 << bit))

/***********************************************************
***********************variable define**********************
***********************************************************/
static PIXEL_DEV_LIST_T g_pixel_dev_list_head = {
    .next = NULL,
};

/***********************************************************
***********************function define**********************
***********************************************************/
static uint8_t __tdl_pixel_type_get(uint8_t pixel_color)
{
    uint8_t cnt = 0;

    for (uint32_t i = 0; i < 8; i++) {
        cnt += GET_BIT(pixel_color, i) == 0 ? 0 : 1;
        ;
    }

    return cnt;
}

static PIXEL_DEV_NODE_T *__tdl_pixel_dev_node_find(PIXEL_DEV_LIST_T *list, char *dev_name)
{
    PIXEL_DEV_NODE_T *node = NULL;

    if (NULL == list || NULL == dev_name) {
        return NULL;
    }

    node = list->next;
    while (node != NULL) {
        if (0 == strcmp(dev_name, node->name)) {
            return node;
        }

        node = node->next;
    }

    return NULL;
}

static int __tdl_pixel_dev_register(IN char *driver_name, IN PIXEL_DRIVER_INTFS_T *intfs, PIXEL_ATTR_T *arrt,
                                    void *param)
{
    int op_ret = 0;
    PIXEL_DEV_NODE_T *device = NULL, *last_node = NULL;

    if (NULL != __tdl_pixel_dev_node_find(&g_pixel_dev_list_head, driver_name)) {
        PR_ERR("the dev:%s is already exist", driver_name);
        return OPRT_COM_ERROR;
    }

    // new device
    device = (PIXEL_DEV_NODE_T *)tal_malloc(sizeof(PIXEL_DEV_NODE_T));
    if (NULL == device) {
        PR_ERR("malloc failed");
        return OPRT_MALLOC_FAILED;
    }
    memset(device, 0x00, sizeof(PIXEL_DEV_NODE_T));

    strncpy(device->name, driver_name, PIXEL_DEV_NAME_MAX_LEN);
    device->color_num = __tdl_pixel_type_get(arrt->color_tp);
    device->pixel_color = arrt->color_tp;
    device->color_maximum = arrt->color_maximum;
    device->white_color_control = arrt->white_color_control;

    device->intfs = (PIXEL_DRIVER_INTFS_T *)tal_malloc(sizeof(PIXEL_DRIVER_INTFS_T));
    if (NULL == device->intfs) {
        PR_ERR("malloc intfs failed");
        tal_free(device);
        return OPRT_MALLOC_FAILED;
    }
    memcpy(device->intfs, intfs, sizeof(PIXEL_DRIVER_INTFS_T));

    // mutex
    op_ret = tal_mutex_create_init(&device->mutex);
    if (op_ret != OPRT_OK) {
        tal_free(device->intfs);
        tal_free(device);
        return op_ret;
    }

    // semphore
    op_ret = tal_semaphore_create_init(&device->send_sem, 0, 1);
    if (op_ret != OPRT_OK) {
        PR_ERR("tal_semaphore_create_init err !!!");
        tal_mutex_release(device->mutex);
        tal_free(device->intfs);
        tal_free(device);
        return op_ret;
    }

    // find the last node
    last_node = &g_pixel_dev_list_head;
    while (last_node->next) {
        last_node = last_node->next;
    }

    // add node
    last_node->next = device;

    return OPRT_OK;
}

static int __tdl_pixel_dev_open(PIXEL_DEV_NODE_T *device, PIXEL_DEV_CONFIG_T *config)
{
    int op_ret = 0;

    if (device->flag.is_start == 1) {
        PR_DEBUG("pixel dev init already !");
        return OPRT_OK;
    }

    // get config
    device->pixel_num = config->pixel_num;
    if (config->pixel_resolution != 0) {
        device->pixel_resolution = config->pixel_resolution;
    } else {
        device->pixel_resolution = 1000;
    }

    // open device
    if (device->intfs->open != NULL) {
        op_ret = device->intfs->open(
            &device->drv_handle,
            device->pixel_num); // Original: &device->pixel_drv_handle changed to &device->drv_handle
        if (op_ret != 0) {
            PR_ERR("device:%s open failed :%d", device->name, op_ret);
            return OPRT_COM_ERROR;
        }
    }

    // prepare pixel buffer
    device->pixel_buffer = (uint16_t *)tal_malloc(device->color_num * device->pixel_num * sizeof(uint16_t));
    device->pixel_buffer_len = device->color_num * device->pixel_num;
    if (NULL == device->pixel_buffer) {
        PR_ERR("tx_buffer malloc err 1!!!");
        return OPRT_COM_ERROR;
    }
    memset(device->pixel_buffer, 0, device->color_num * device->pixel_num * sizeof(uint16_t));

    device->flag.is_start = 1;

    PR_DEBUG("pixel dev open succ");

    return OPRT_OK;
}

static int __tdl_pixel_refresh(PIXEL_DEV_NODE_T *device)
{
    int op_ret =OPRT_OK;

    if(device->intfs->output != NULL){
        op_ret = device->intfs->output(device->drv_handle, device->pixel_buffer, device->pixel_buffer_len);    
        if(op_ret != 0) {
            PR_ERR("device:%s output is fail:%d!", device->name, op_ret);
        }
    }

    /* Prevent two frames from being sent too close together, causing continuous packets to be 
        recognized by hardware as one frame, add delay processing. WS2812 requires frame interval 
        >50us. To ensure portability of delay code, system interface is called here. Due to BK 
        platform system heartbeat being 2ms, 1ms setting is invalid, so 4ms delay is set here. 
        2ms cannot solve the problem due to task scheduling and other reasons.
    */
    tal_system_sleep(4);

    return op_ret;
}

static int __tdl_pixel_dev_close(PIXEL_DEV_NODE_T *device)
{
    int op_ret = 0;

    if (0 == device->flag.is_start) {
        PR_ERR("device is not open");
        return OPRT_COM_ERROR;
    }

    op_ret =
        device->intfs->close(&device->drv_handle); // Original: &device->pixel_drv_handle changed to &device->drv_handle
    if (op_ret != 0) {
        return op_ret;
    }

    // clear flag
    device->flag.is_start = 0;

    // clear pixel num
    device->pixel_num = 0;

    // clear buf
    if (device->pixel_buffer != NULL) {
        tal_free(device->pixel_buffer);
        device->pixel_buffer = NULL;
    }
    device->pixel_buffer_len = 0;

    return OPRT_OK;
}

/**
 * @brief       Find the LED strip device
 *
 * @param[in]   name                  LED strip name
 * @param[out]  handle                Device handle
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
int tdl_pixel_dev_find(char *name, OUT PIXEL_HANDLE_T *handle)
{
    PIXEL_DEV_NODE_T *device = NULL;

    device = __tdl_pixel_dev_node_find(&g_pixel_dev_list_head, name);
    if (NULL == device) {
        return OPRT_COM_ERROR;
    }

    *handle = (PIXEL_HANDLE_T *)device;

    return OPRT_OK;
}

/**
 * @brief         Start device
 *
 * @param[in]   handle                Device handle
 * @param[in]   config                Configuration parameters
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
int tdl_pixel_dev_open(PIXEL_HANDLE_T handle, PIXEL_DEV_CONFIG_T *config)
{
    PIXEL_DEV_NODE_T *device = NULL;
    int op_ret = 0;

    if(NULL == handle || NULL == config) {
        return OPRT_INVALID_PARM;
    }

    device = (PIXEL_DEV_NODE_T *)handle;

    tal_mutex_lock(device->mutex);
    op_ret = __tdl_pixel_dev_open(device, config);
    tal_mutex_unlock(device->mutex);

    return op_ret;
}

/**
 * @brief        Refresh the data of all pixel display memory to the driver for display
 *
 * @param[in]    handle           Device handle
 * @param[in]    timeout_ms            Task timeout time
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
int tdl_pixel_dev_refresh(PIXEL_HANDLE_T handle)
{
    OPERATE_RET op_ret = OPRT_OK;
    PIXEL_DEV_NODE_T *device = (PIXEL_DEV_NODE_T *)handle;

    if (NULL == device) {
        return OPRT_INVALID_PARM;
    }

    if (0 == device->flag.is_start) {
        return OPRT_COM_ERROR;
    }

    tal_mutex_lock(device->mutex);

    op_ret = __tdl_pixel_refresh(device);

    tal_mutex_unlock(device->mutex);

    return op_ret;
}

static OPERATE_RET __tdl_pixel_dev_num_set(PIXEL_HANDLE_T *handle, uint16_t num)
{
    PIXEL_DEV_NODE_T *device = (PIXEL_DEV_NODE_T *)(handle);
    OPERATE_RET ret = OPRT_OK;

    if ((NULL == handle) || (0 == num)) {
        PR_ERR("handle is null/num is :%d. set pixel num failed!", num);
        return OPRT_INVALID_PARM;
    }

    if (device->pixel_num == num) {
        PR_NOTICE("dev pixel num:%d is same", num);
        return OPRT_OK;
    }
    device->pixel_num = num;

    /* tdd resource re-apply */
    ret = device->intfs->close(&device->drv_handle);
    if (ret != 0) {
        PR_ERR("device:%s close failed :%d", device->name, ret);
        return OPRT_COM_ERROR;
    }
    ret = device->intfs->open(&device->drv_handle, num);
    if (ret != 0) {
        PR_ERR("device:%s open failed :%d", device->name, ret);
        return OPRT_COM_ERROR;
    }

    /* tdl resource re-apply */
    if (device->pixel_buffer != NULL) {
        tal_free(device->pixel_buffer);
        device->pixel_buffer = NULL;
    }

    uint16_t *new_buffer = (uint16_t *)tal_malloc((device->color_num) * device->pixel_num * sizeof(uint16_t));
    if (NULL == new_buffer) {
        PR_ERR("tx_buffer malloc err 2!!!");
        return OPRT_MALLOC_FAILED;
    }
    device->pixel_buffer = new_buffer;
    device->pixel_buffer_len = (device->color_num) * device->pixel_num;
    memset(device->pixel_buffer, 0, ((device->color_num) * device->pixel_num * sizeof(uint16_t))); // Clear data

    return OPRT_OK;
}

/**
 * @brief        Configure pixel device
 *
 * @param[in]    handle           Device handle
 * @param[in]    cmd              Control command
 * @param[in]    arg              Control parameters
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
int tdl_pixel_dev_config(PIXEL_HANDLE_T handle, PIXEL_DEV_CFG_CMD_E cmd, void *arg)
{
    OPERATE_RET op_ret = OPRT_OK;
    PIXEL_DEV_NODE_T *device = (PIXEL_DEV_NODE_T *)handle;

    if (NULL == handle) {
        return OPRT_INVALID_PARM;
    }

    switch (cmd) {
    case PIXEL_DEV_CMD_GET_RESOLUTION: {
        uint32_t *max = 0;
        if (NULL == arg) {
            return OPRT_INVALID_PARM;
        }

        max = (uint32_t *)arg;
        *max = device->pixel_resolution;
    } break;
    case PIXEL_DEV_CMD_SET_PIXEL_NUM: {
        uint32_t *pixel_num = NULL;
        if (NULL == arg) {
            return OPRT_INVALID_PARM;
        }
        pixel_num = (uint32_t *)arg;
        tal_mutex_lock(device->mutex);
        __tdl_pixel_dev_num_set(handle, *pixel_num);
        tal_mutex_unlock(device->mutex);
    } break;
    case PIXEL_DEV_CMD_SET_TX_CB:
        break;
    case PIXEL_DEV_CMD_GET_PIXEL_NUM: {
        uint32_t *pixel_num = NULL;
        if (NULL == arg) {
            return OPRT_INVALID_PARM;
        }

        pixel_num = (uint32_t *)arg;
        *pixel_num = device->pixel_num;
    } break;
    case PIXEL_DEV_CMD_GET_DRV_COLOR_CH: {
        if (NULL == arg) {
            return OPRT_INVALID_PARM;
        }
        memcpy(arg, &device->pixel_color, sizeof(device->pixel_color));
    } break;
    case PIXEL_DEV_CMD_GET_WHITE_COLOR_CTRL: {
        if (NULL == arg) {
            return OPRT_INVALID_PARM;
        }
        memcpy(arg, &device->white_color_control, sizeof(device->white_color_control));
    } break;
    case PIXEL_DEV_CMD_SET_WHITE_COLOR_CTRL: {
        if (NULL == arg) {
            return OPRT_INVALID_PARM;
        }
        memcpy(&device->white_color_control, arg, sizeof(device->white_color_control));
    } break;
    case PIXEL_DEV_CMD_GET_PWM_HARDWARE_CFG: {
        if (NULL == arg) {
            return OPRT_INVALID_PARM;
        }

        if (NULL == device->intfs->config) {
            return OPRT_NOT_SUPPORTED;
        }

        op_ret = device->intfs->config(device->drv_handle, DRV_CMD_GET_PWM_HARDWARE_CFG, arg);
    } break;
    case PIXEL_DEV_CMD_SET_RGB_LINE_SEQUENCE: {
        if (NULL == arg) {
            return OPRT_INVALID_PARM;
        }

        if (NULL == device->intfs->config) {
            return OPRT_NOT_SUPPORTED;
        }

        op_ret = device->intfs->config(device->drv_handle, DRV_CMD_SET_RGB_ORDER_CFG, arg);
    } break;
    default:
        break;
    }

    return op_ret;
}

/**
 * @brief         Stop device
 *
 * @param[in]    handle                Device handle
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
int tdl_pixel_dev_close(PIXEL_HANDLE_T handle)
{
    PIXEL_DEV_NODE_T *device = (PIXEL_DEV_NODE_T *)(handle);
    int op_ret = 0;

    if (NULL == device) {
        return OPRT_INVALID_PARM;
    }

    tal_mutex_lock(device->mutex);
    op_ret = __tdl_pixel_dev_close(device);
    tal_mutex_unlock(device->mutex);

    return op_ret;
}

/**
 * @brief         Register driver
 *
 * @param[in]    driver_name                Device name
 * @param[in]    intfs                      Operation interface
 * @param[in]    arrt                       Device attributes
 * @param[in]    param                      Parameters
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
int tdl_pixel_driver_register(char *driver_name, PIXEL_DRIVER_INTFS_T *intfs, PIXEL_ATTR_T *arrt, void *param)

{
    int op_ret = 0;

    if (NULL == driver_name || NULL == intfs) {
        return OPRT_INVALID_PARM;
    }

    if (NULL == intfs->open || NULL == intfs->close || NULL == intfs->output) {
        return OPRT_INVALID_PARM;
    }

    if (arrt->color_tp > PIXEL_COLOR_TP_RGBCW || arrt->color_tp < PIXEL_COLOR_TP_RGB) {
        return OPRT_INVALID_PARM;
    }

    op_ret = __tdl_pixel_dev_register(driver_name, intfs, arrt, param);

    return op_ret;
}

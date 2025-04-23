/**
* @file tdl_pixel_dev_manage.c
* @author www.tuya.com
* @brief tdl_drv_sensor_hub module is used to manage leds pixel device
* @version 0.1
* @date 2021-12-09
*
* @copyright Copyright (c) tuya.inc 2021
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
#define GET_BIT(value,bit) (value&(1<<bit))

/***********************************************************
***********************variable define**********************
***********************************************************/
STATIC PIXEL_DEV_LIST_T g_pixel_dev_list_head = {
    .next = NULL,   
};

/***********************************************************
***********************function define**********************
***********************************************************/
STATIC UCHAR_T __tdl_pixel_type_get(UCHAR_T pixel_color) 
{
    UCHAR_T cnt = 0;

    for(UINT_T i=0; i < 8;i++){
        cnt += GET_BIT(pixel_color,i) == 0 ? 0 : 1;;
    }
    
    return cnt;
}

STATIC PIXEL_DEV_NODE_T *__tdl_pixel_dev_node_find(PIXEL_DEV_LIST_T *list, CHAR_T *dev_name)
{
    PIXEL_DEV_NODE_T *node = NULL;

    if(NULL == list || NULL == dev_name) {
        return NULL;
    }

    node = list->next;
    while(node != NULL) {
        if(0 == strcmp(dev_name, node->name)) {
            return node;
        }

        node = node->next;
    }

    return NULL;
}

STATIC int __tdl_pixel_dev_register(IN char *driver_name, IN PIXEL_DRIVER_INTFS_T *intfs, \
                                    PIXEL_ATTR_T *arrt, VOID *param)
{
    int op_ret = 0;
    PIXEL_DEV_NODE_T *device = NULL, *last_node = NULL;

    if(NULL != __tdl_pixel_dev_node_find(&g_pixel_dev_list_head, driver_name)) {
        TAL_PR_ERR("the dev:%s is already exist", driver_name);
        return OPRT_COM_ERROR;
    }

    //new device
    device = (PIXEL_DEV_NODE_T *)tal_malloc(sizeof(PIXEL_DEV_NODE_T));
    if(NULL == device) {
        TAL_PR_ERR("malloc failed");
        return OPRT_MALLOC_FAILED;
    }
    memset(device, 0x00, sizeof(PIXEL_DEV_NODE_T));

    strncpy(device->name, driver_name, PIXEL_DEV_NAME_MAX_LEN);
	device->color_num     = __tdl_pixel_type_get(arrt->color_tp); 
    device->pixel_color   = arrt->color_tp;
    device->color_maximum = arrt->color_maximum;
    device->white_color_control = arrt->white_color_control;

    device->intfs = (PIXEL_DRIVER_INTFS_T *)tal_malloc(sizeof(PIXEL_DRIVER_INTFS_T));
    memcpy(device->intfs, intfs, sizeof(PIXEL_DRIVER_INTFS_T));

    //mutex
    op_ret = tal_mutex_create_init(&device->mutex);
    if(op_ret != OPRT_OK) {
        tal_free(device);
        return op_ret;
    }

    //semphore
    op_ret = tal_semaphore_create_init(&device->send_sem, 0, 1);
    if(op_ret != OPRT_OK) {
        TAL_PR_ERR("tal_semaphore_create_init err !!!");
        return op_ret;
    }

    //find the last node
    last_node = &g_pixel_dev_list_head;
    while(last_node->next) {
        last_node = last_node->next;
    }

    //add node
    last_node->next = device;

    return OPRT_OK;
}  

STATIC int __tdl_pixel_dev_open(PIXEL_DEV_NODE_T *device, PIXEL_DEV_CONFIG_T* config)
{
    int op_ret = 0;

    if(device->flag.is_start == 1){
        TAL_PR_DEBUG("pixel dev init already !");
        return OPRT_OK;
    }

    //get config
    device->pixel_num    = config->pixel_num;
    if(config->pixel_resolution !=0 ){
        device->pixel_resolution = config ->pixel_resolution;
    }else{
        device->pixel_resolution = 1000;
    }
    
    //open device
    if(device->intfs->open != NULL){
        op_ret = device->intfs->open(&device->drv_handle, device->pixel_num); // 原型为 &device->pixel_drv_handle 修改为 &device->drv_handle
        if(op_ret != 0) {
            TAL_PR_ERR("device:%s open failed :%d", device->name,  op_ret);
            return OPRT_COM_ERROR;
        }
    } 

    //prepare pixel buffer
    device->pixel_buffer     = (USHORT_T *)tal_malloc(device->color_num * device->pixel_num * sizeof(USHORT_T));
    device->pixel_buffer_len = device->color_num * device->pixel_num;
    if(NULL == device->pixel_buffer) {  
        TAL_PR_ERR("tx_buffer malloc err 1!!!");
        return OPRT_COM_ERROR;
    }
    memset(device->pixel_buffer, 0, device->color_num * device->pixel_num * sizeof(USHORT_T)); 

    device->flag.is_start = 1;    

    TAL_PR_DEBUG("pixel dev open succ");

    return OPRT_OK;
}

STATIC int __tdl_pixel_refresh(PIXEL_DEV_NODE_T *device) 
{
    int op_ret =OPRT_OK;

    if(device->intfs->output != NULL){
        op_ret = device->intfs->output(device->drv_handle, device->pixel_buffer, device->pixel_buffer_len);    
        if(op_ret != 0) {
            TAL_PR_ERR("device:%s output is fail:%d!", device->name, op_ret);
        }
    }

    /* 防止两帧发送时间过近，造成连包被硬件识别为1帧，增加延时处理
        ws2812帧间间隔要求>50us,为保证延时代码的可移植性，此处调用系统接口设置,
        由于bk平台系统心跳为2ms，1ms设置无效，此处暂设置为4ms延时,2ms由于任务调度
        等原因，不能解决问题。
    */
    tal_system_sleep(4);

    return op_ret;
}

STATIC int __tdl_pixel_dev_close(PIXEL_DEV_NODE_T *device)
{
    int op_ret = 0;

    if(0 == device->flag.is_start) {
        TAL_PR_ERR("device is not open");
        return OPRT_COM_ERROR;
    }

    op_ret = device->intfs->close(&device->drv_handle);  // 原型为 &device->pixel_drv_handle 修改为 &device->drv_handle
    if(op_ret != 0) {
        return op_ret;
    }
 
    //clear flag
    device->flag.is_start     = 0;

    //clear pixel num
	device ->pixel_num = 0;

    //clear buf
 	if(device->pixel_buffer != NULL) {
		tal_free(device->pixel_buffer);
		device->pixel_buffer = NULL;  
    }
    device->pixel_buffer_len = 0;

    return OPRT_OK;
}

/**
* @brief      查找幻彩灯带设备
*
* @param[in]   name                 灯带名字
* @param[out]  handle               设备句柄
*
* @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
*/
int tdl_pixel_dev_find(char *name, OUT PIXEL_HANDLE_T *handle)
{
    PIXEL_DEV_NODE_T *device = NULL;

    device = __tdl_pixel_dev_node_find(&g_pixel_dev_list_head, name);
    if(NULL == device) {
        return OPRT_COM_ERROR;
    }

    *handle = (PIXEL_HANDLE_T *)device;

	return OPRT_OK;
}

/**
* @brief        启动设备
*
* @param[in]   handle               设备句柄
* @param[in]   config               配置参数
*
* @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
*/
int tdl_pixel_dev_open(PIXEL_HANDLE_T handle, PIXEL_DEV_CONFIG_T* config)
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
* @brief        刷新所有像素显存的数据到驱动端显示
*
* @param[in]    handle               设备句柄
* @param[in]    timeout_ms           任务超时时间
*
* @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
*/
int tdl_pixel_dev_refresh(PIXEL_HANDLE_T handle)
{
    OPERATE_RET op_ret = OPRT_OK;
	PIXEL_DEV_NODE_T *device = (PIXEL_DEV_NODE_T*)handle;

    if(NULL == device) {
        return OPRT_INVALID_PARM;
    }

    if(0 == device->flag.is_start) {
        return OPRT_COM_ERROR;
    }

    tal_mutex_lock(device->mutex);

    op_ret = __tdl_pixel_refresh(device);

    tal_mutex_unlock(device->mutex);

    return op_ret;
}


STATIC OPERATE_RET __tdl_pixel_dev_num_set(PIXEL_HANDLE_T *handle, uint16_t num)
{
    PIXEL_DEV_NODE_T *device = (PIXEL_DEV_NODE_T *)(handle);
    OPERATE_RET ret = OPRT_OK;

    if ((NULL == handle) || (0 == num)) {
        TAL_PR_ERR("handle is null/num is :%d. set pixel num failed!", num);
        return OPRT_INVALID_PARM;
    }

    if (device->pixel_num == num) {
        TAL_PR_NOTICE("dev pixel num:%d is same", num);
        return OPRT_OK;
    }
    device->pixel_num = num;

    /* tdd资源重新申请 */
    ret = device->intfs->close(&device->drv_handle); 
    if(ret != 0) {
        TAL_PR_ERR("device:%s close failed :%d", device->name,  ret);
        return OPRT_COM_ERROR;
    }
    ret = device->intfs->open(&device->drv_handle, num); 
    if(ret != 0) {
        TAL_PR_ERR("device:%s open failed :%d", device->name,  ret);
        return OPRT_COM_ERROR;
    }

    /* tdl 资源重新申请 */
    if (device->pixel_buffer != NULL) {
        tal_free(device->pixel_buffer);
        device->pixel_buffer = NULL;
    }

    device->pixel_buffer = (USHORT_T *)tal_malloc((device->color_num) * device->pixel_num * sizeof(USHORT_T));
    device->pixel_buffer_len = (device->color_num) * device->pixel_num;
    memset(device->pixel_buffer, 0, ((device->color_num) * device->pixel_num * sizeof(USHORT_T))); //清空数据

    return OPRT_OK;
}


/**
* @brief        配置设备参数
*
* @param[in]    handle               设备句柄
* @param[in]    cmd                  命令字
*
* @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
*/
int tdl_pixel_dev_config(PIXEL_HANDLE_T handle, PIXEL_DEV_CFG_CMD_E cmd, void *arg)
{
    OPERATE_RET op_ret = OPRT_OK;
	PIXEL_DEV_NODE_T *device = (PIXEL_DEV_NODE_T *)handle;

    if(NULL == handle) {
        return OPRT_INVALID_PARM;
    }    

    switch(cmd) {
        case PIXEL_DEV_CMD_GET_RESOLUTION: {
            UINT_T *max = 0;
            if(NULL == arg) {
                return OPRT_INVALID_PARM;
            }

            max = (UINT_T *)arg;
           *max = device->pixel_resolution;
        }
        break;
        case PIXEL_DEV_CMD_SET_PIXEL_NUM:{
            UINT_T *pixel_num = NULL;
            if(NULL == arg) {
                return OPRT_INVALID_PARM;
            }
            pixel_num = (UINT_T *)arg;
            tal_mutex_lock(device->mutex);
            __tdl_pixel_dev_num_set(handle, *pixel_num);
            tal_mutex_unlock(device->mutex);
        }
        break;
        case PIXEL_DEV_CMD_SET_TX_CB:
        break;
        case PIXEL_DEV_CMD_GET_PIXEL_NUM: {
            UINT_T *pixel_num = NULL;
            if(NULL == arg) {
                return OPRT_INVALID_PARM;
            }

            pixel_num = (UINT_T *)arg;
           *pixel_num = device->pixel_num;
        }break;
        case PIXEL_DEV_CMD_GET_DRV_COLOR_CH: {
            if(NULL == arg) {
                return OPRT_INVALID_PARM;
            }
            memcpy(arg, &device->pixel_color, sizeof(device->pixel_color));
        }break;
        case PIXEL_DEV_CMD_GET_WHITE_COLOR_CTRL: {
            if(NULL == arg) {
                return OPRT_INVALID_PARM;
            }      
            memcpy(arg, &device->white_color_control, sizeof(device->white_color_control));      
        }break;
        case PIXEL_DEV_CMD_SET_WHITE_COLOR_CTRL: {
            if(NULL == arg) {
                return OPRT_INVALID_PARM;
            }      
            memcpy(&device->white_color_control, arg, sizeof(device->white_color_control));      
        }break;
        case PIXEL_DEV_CMD_GET_PWM_HARDWARE_CFG: {
            if(NULL == arg) {
                return OPRT_INVALID_PARM;
            } 

            if (NULL == device->intfs->config) {
                return OPRT_NOT_SUPPORTED;
            }
            
            op_ret = device->intfs->config(device->drv_handle, DRV_CMD_GET_PWM_HARDWARE_CFG, arg);
        }break;
        case PIXEL_DEV_CMD_SET_RGB_LINE_SEQUENCE: {
            if(NULL == arg) {
                return OPRT_INVALID_PARM;
            }

            if (NULL == device->intfs->config) {
                return OPRT_NOT_SUPPORTED;
            }

            op_ret = device->intfs->config(device->drv_handle, DRV_CMD_SET_RGB_ORDER_CFG, arg);
        }break;
        default:
        break;
    }

    return op_ret;
}

/**
* @brief        停止设备
*
* @param[in]    handle               设备句柄
*
* @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
*/
int tdl_pixel_dev_close(PIXEL_HANDLE_T handle)
{
    PIXEL_DEV_NODE_T *device = (PIXEL_DEV_NODE_T *)(handle);
    int op_ret = 0;

    if(NULL == device ) {
        return OPRT_INVALID_PARM;
    }

    tal_mutex_lock(device->mutex);
    op_ret = __tdl_pixel_dev_close(device);
    tal_mutex_unlock(device->mutex);

    return op_ret;
}

/**
* @brief        注册驱动
*
* @param[in]    driver_name               设备名字
* @param[in]    intfs                     操作接口
* @param[in]    arrt                      设备属性
* @param[in]    param                     参数
*
* @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
*/
int tdl_pixel_driver_register(char *driver_name, PIXEL_DRIVER_INTFS_T *intfs, \
                              PIXEL_ATTR_T *arrt, void *param)

{
    int op_ret=0;

    if(NULL == driver_name || NULL == intfs ){
        return OPRT_INVALID_PARM;
    }
    
    if(NULL == intfs->open || NULL == intfs->close || NULL == intfs->output) {
        return OPRT_INVALID_PARM;
    }

    if(arrt->color_tp > PIXEL_COLOR_TP_RGBCW || arrt->color_tp < PIXEL_COLOR_TP_RGB) {
        return OPRT_INVALID_PARM;
    }
	
    op_ret = __tdl_pixel_dev_register(driver_name, intfs, arrt, param);

    return op_ret;
}

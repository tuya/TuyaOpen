/***********************************************************
*  File: tuya_led.c
*  Author: nzy
*  Date: 20171117
***********************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "tal_sw_timer.h"
#include "tal_mutex.h"
#include "uni_log.h"
#include "tal_memory.h"
#include "tuya_cloud_types.h"
#include "tuya_led.h"
#include "tal_gpio.h"

/***********************************************************
*************************micro define***********************
***********************************************************/
typedef struct led_cntl {
    struct led_cntl *next;
    UINT_T  pinname;
    LED_LT_E type;
    USHORT_T flash_cycle; // ms
    USHORT_T expire_time;
    USHORT_T flh_sum_time; // if(flh_sum_time == 0xffff) then flash forever;
}LED_CNTL_S;

#define LED_TIMER_VAL_MS 100

/***********************************************************
*************************variable define********************
***********************************************************/
STATIC LED_CNTL_S *led_cntl_list = NULL;
STATIC TIMER_ID led_timer = NULL;
STATIC MUTEX_HANDLE mutex = NULL;

/***********************************************************
*************************function define********************
***********************************************************/
STATIC VOID __led_timer_cb(TIMER_ID timerID,PVOID_T pTimerArg);

/***********************************************************
*  Function: tuya_create_led_handle
*  Input: pinname
*         high->default output pinname high/low
*  Output: handle
*  Return: OPERATE_RET
***********************************************************/
OPERATE_RET tuya_create_led_handle(IN CONST UINT_T pinname,IN CONST BOOL_T high,OUT LED_HANDLE *handle)
{
    if(NULL == handle) {
        return OPRT_INVALID_PARM;
    }
    OPERATE_RET op_ret = OPRT_OK;
    // create mutex
    if(NULL == mutex) {
        op_ret = tal_mutex_create_init(&mutex);
        if(OPRT_OK != op_ret) {
            PR_ERR("init mutex err:%d",op_ret);
            return op_ret;
        }
    }

    // create flash timer
    if(led_timer == NULL) {
        op_ret = tal_sw_timer_create(__led_timer_cb,NULL,&led_timer);
        if(OPRT_OK != op_ret) {
            return op_ret;
        }else {
            op_ret = tal_sw_timer_start(led_timer,LED_TIMER_VAL_MS,TAL_TIMER_CYCLE);
            if(op_ret != OPRT_OK)
            {
                PR_ERR("Start timer err:%d", op_ret);
                return op_ret;
            }
        }
    }

    TUYA_GPIO_BASE_CFG_T gpio_cfg;
    gpio_cfg.mode = TUYA_GPIO_PUSH_PULL;
    gpio_cfg.direct = TUYA_GPIO_OUTPUT;
    gpio_cfg.level = high ? TUYA_GPIO_LEVEL_HIGH : TUYA_GPIO_LEVEL_LOW;
    tal_gpio_init(pinname, &gpio_cfg);

    LED_CNTL_S *len_cntl = (LED_CNTL_S *)Malloc(SIZEOF(LED_CNTL_S));
    if(NULL == len_cntl) {
        return OPRT_MALLOC_FAILED;
    }
    memset(len_cntl,0,sizeof(LED_CNTL_S));
    len_cntl->pinname = pinname;

    *handle = (LED_HANDLE)len_cntl;
    tal_mutex_lock(mutex);
    if(led_cntl_list) {
        len_cntl->next = led_cntl_list;
    }
    led_cntl_list = len_cntl;
    tal_mutex_unlock(mutex);

    return OPRT_OK;
}

/***********************************************************
*  Function: tuya_create_led_handle_select
*  Input: port
*         high->default output port high/low
*  Output: handle
*  Return: OPERATE_RET
***********************************************************/
#if 0
OPERATE_RET tuya_create_led_handle_select(IN CONST UINT_T pinname,IN CONST BOOL_T high,OUT LED_HANDLE *handle)
{
    if(NULL == handle) {
        return OPRT_INVALID_PARM;
    }
    OPERATE_RET op_ret = OPRT_OK;
    // create mutex
    if(NULL == mutex) {
        op_ret = tal_mutex_create_init(&mutex);
        if(OPRT_OK != op_ret) {
            PR_ERR("init mutex err:%d",op_ret);
            return op_ret;
        }
    }

    // create flash timer
    if(led_timer == NULL) {
        op_ret = tal_sw_timer_create(__led_timer_cb,NULL,&led_timer);
        if(OPRT_OK != op_ret) {
            return op_ret;
        }else {
            op_ret = tal_sw_timer_start(led_timer,LED_TIMER_VAL_MS,TAL_TIMER_CYCLE);
            if(op_ret != OPRT_OK)
            {
                PR_ERR("Start timer err:%d", op_ret);
                return op_ret;
            }
        }
    }
    
    op_ret = tuya_gpio_inout_set_select((TUYA_GPIO_NUM_E)pinname, FALSE, high);
    if(OPRT_OK != op_ret) {
        return op_ret;
    }

    LED_CNTL_S *len_cntl = (LED_CNTL_S *)Malloc(SIZEOF(LED_CNTL_S));
    if(NULL == len_cntl) {
        return OPRT_MALLOC_FAILED;
    }
    memset(len_cntl,0,sizeof(LED_CNTL_S));
    len_cntl->pinname = pinname;

    *handle = (LED_HANDLE)len_cntl;
    tal_mutex_lock(mutex);
    if(led_cntl_list) {
        len_cntl->next = led_cntl_list;
    }
    led_cntl_list = len_cntl;
    tal_mutex_unlock(mutex);

    return OPRT_OK;
}
#endif
STATIC VOID __set_led_light_type(IN CONST LED_HANDLE handle,IN CONST LED_LT_E type,
                                           IN CONST USHORT_T flh_mstime,IN CONST USHORT_T flh_ms_sumtime)
{
    LED_CNTL_S *led_cntl = (LED_CNTL_S *)handle;
    PR_DEBUG("pinname:%d",led_cntl->pinname);
    led_cntl->type = type;
    if(OL_LOW == type) {
        tal_gpio_write(led_cntl->pinname, TUYA_GPIO_LEVEL_LOW);
    }else if(OL_HIGH == type) {
        if(tal_gpio_write(led_cntl->pinname, TUYA_GPIO_LEVEL_HIGH) != OPRT_OK){
            PR_ERR("gpio_write err");
        }
    }else {
        led_cntl->flh_sum_time = flh_ms_sumtime;
        led_cntl->flash_cycle = flh_mstime;
        led_cntl->expire_time = 0;
    }
}

/***********************************************************
*  Function: tuya_set_led_light_type
*  Input: handle type flh_mstime flh_ms_sumtime
*  Output: none
*  Return: OPERATE_RET
*  note: if(OL_FLASH == type) then flh_mstime and flh_ms_sumtime is valid
***********************************************************/
VOID tuya_set_led_light_type(IN CONST LED_HANDLE handle,IN CONST LED_LT_E type,
                                        IN CONST USHORT_T flh_mstime,IN CONST USHORT_T flh_ms_sumtime)
{
    tal_mutex_lock(mutex);
    __set_led_light_type(handle,type,flh_mstime,flh_ms_sumtime);
    tal_mutex_unlock(mutex);
}

STATIC VOID __led_timer_cb(TIMER_ID timerID,PVOID_T pTimerArg)
{
    tal_mutex_lock(mutex);
    LED_CNTL_S *tmp_led_cntl_list = led_cntl_list;
    if(NULL == tmp_led_cntl_list) {
        tal_mutex_unlock(mutex);
        return;
    }

    while(tmp_led_cntl_list) {
        if(tmp_led_cntl_list->type >= OL_FLASH_LOW) {
            tmp_led_cntl_list->expire_time += LED_TIMER_VAL_MS;
            if(tmp_led_cntl_list->expire_time >= tmp_led_cntl_list->flash_cycle) {
                tmp_led_cntl_list->expire_time = 0;
                if(tmp_led_cntl_list->type == OL_FLASH_LOW) {
                    tmp_led_cntl_list->type = OL_FLASH_HIGH;
                    tal_gpio_write(tmp_led_cntl_list->pinname,TUYA_GPIO_LEVEL_LOW);
                }else {
                    tmp_led_cntl_list->type = OL_FLASH_LOW;
                    tal_gpio_write(tmp_led_cntl_list->pinname,TUYA_GPIO_LEVEL_HIGH);
                }
            }

            // the led flash and set deadline time
            if(tmp_led_cntl_list->flh_sum_time != 0xffff) {
                if(tmp_led_cntl_list->flh_sum_time >= LED_TIMER_VAL_MS) {
                    tmp_led_cntl_list->flh_sum_time -= LED_TIMER_VAL_MS;
                }else {
                    tmp_led_cntl_list->flh_sum_time = 0;
                }
            
                if(0 == tmp_led_cntl_list->flh_sum_time) {
                    if(tmp_led_cntl_list->type == OL_FLASH_LOW) {
                        __set_led_light_type(tmp_led_cntl_list,OL_LOW,0,0);
                    }else {
                        __set_led_light_type(tmp_led_cntl_list,OL_HIGH,0,0);
                    }
                }
            }
        }
        
        tmp_led_cntl_list = tmp_led_cntl_list->next;
    }
    tal_mutex_unlock(mutex);
}


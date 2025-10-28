/***********************************************************
*  File: tuya_key.c
*  Author: nzy
*  Date: 20171117
***********************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "tal_sw_timer.h"
#include "tal_mutex.h"
#include "tal_system.h"
#include "tal_sleep.h"
#include "tal_watchdog.h"
#include "tal_memory.h"
#include "uni_log.h"
#include "tal_thread.h"
#include "tal_semaphore.h"
#include "tuya_cloud_types.h"
#include "tuya_key.h"
#include "tuya_devos_utils.h"
#include "tal_gpio.h"

/***********************************************************
*************************micro define***********************
***********************************************************/

typedef enum {
    KEY_DOWN = 0,
    KEY_DOWN_CONFIRM,
    KEY_DOWNNING, 
    KEY_UP_CONFIRM,
    KEY_UPING,
    KEY_FINISH,
}KEY_STAT_E;


typedef struct {
    // user define
    KEY_USER_DEF_S kud;
    // run variable
    KEY_STAT_E status;
    BOOL_T long_key_press;
    //BOOL_T key_val_last; //press ? TRUE : FALSE and init it with TRUE
    INT_T down_time; // ms
    INT_T up_time;
    INT_T seq_key_cnt;
}KEY_ENTITY_S;

typedef struct ken_en_lst{
    struct ken_en_lst *nxt;
    KEY_ENTITY_S key_ent;
}KEY_EN_LST_S;

typedef struct {
    KEY_ENTITY_S *p_tbl;

    //TIMER_ID kd_timer; // key detect timer
    TIMER_ID tickless_trig_timer; // tickless trig timer
    INT_T tbl_cnt;
    INT_T timer_space; // default 20ms

    KEY_EN_LST_S *lst;
    MUTEX_HANDLE mutex;

     SEM_HANDLE semHandle;
}KEY_MANAGE_S;

#define TIMER_SPACE_MAX 100 // ms
/***********************************************************
*************************variable define********************
***********************************************************/


STATIC KEY_MANAGE_S *key_mag = NULL;
STATIC PVOID_T key_handle = NULL;

STATIC VOID key_handle_thrd(PVOID_T arg);

STATIC BOOL_T has_enter_irq = FALSE;
STATIC BOOL_T tickless_endisable = FALSE;

STATIC UINT_T keep_time_val = (10*1000 + 500);//10s

/***********************************************************
*************************function define********************
***********************************************************/
STATIC VOID __key_tickless_trig_timer_cb(TIMER_ID timerID,PVOID_T pTimerArg);
STATIC VOID __key_handle(VOID);
STATIC VOID __key_ent_proc(INOUT KEY_ENTITY_S *key_ent);
VOID __gpio_for_key_irq(VOID *args);


/***********************************************************
*  Function: key_init
*  Input: p_tbl cnt
*         timer_space->if timer (space == 0) then use default value(20ms)
*  Output: 
*  Return: OPERATE_RET
***********************************************************/
OPERATE_RET key_init(IN CONST KEY_USER_DEF_S *p_tbl,IN CONST INT_T cnt,\
                     IN CONST INT_T timer_space)
{
    if(key_mag) {
        return OPRT_OK;
    }

    if(timer_space > TIMER_SPACE_MAX) {
        PR_ERR("para err");
        return OPRT_INVALID_PARM;
    }

    key_mag = tal_malloc(SIZEOF(KEY_MANAGE_S));
    if(NULL == key_mag) {
        PR_ERR("malloc err");
        return OPRT_MALLOC_FAILED;
    }
    memset(key_mag,0,sizeof(KEY_MANAGE_S));

    OPERATE_RET op_ret = OPRT_OK;
    if(cnt != 0) {
        key_mag->p_tbl = tal_malloc(SIZEOF(KEY_ENTITY_S) * cnt);
        if(NULL == key_mag->p_tbl) {
            PR_ERR("malloc err");
            op_ret = OPRT_MALLOC_FAILED;
            goto ERR_EXIT;
        }
        memset(key_mag->p_tbl,0,SIZEOF(KEY_ENTITY_S) * cnt);
    }
    else{
        key_mag->p_tbl = NULL;
    }
    key_mag->tbl_cnt = cnt;
    key_mag->timer_space = timer_space;
    if(0 == key_mag->timer_space) {
        key_mag->timer_space = 20;
    }
    key_mag->lst = NULL;
    // init
    INT_T i = 0;
    for(i = 0;i < cnt;i++) {
        memcpy(&(key_mag->p_tbl[i].kud),&p_tbl[i],SIZEOF(KEY_USER_DEF_S));
        TUYA_GPIO_IRQ_T irq_cfg;
        irq_cfg.mode = key_mag->p_tbl[i].kud.low_level_detect ? TUYA_GPIO_IRQ_FALL : TUYA_GPIO_IRQ_RISE;
        irq_cfg.cb = __gpio_for_key_irq;
        irq_cfg.arg = (void *)(intptr_t)key_mag->p_tbl[i].kud.port;
        op_ret = tal_gpio_irq_init(key_mag->p_tbl[i].kud.port, &irq_cfg);
        if(OPRT_OK != op_ret) {
            PR_ERR("gpio set err:%d",op_ret);
            goto ERR_EXIT;
        }
    }

    op_ret = tal_mutex_create_init(&key_mag->mutex);
    if(OPRT_OK != op_ret) {
        goto ERR_EXIT;
    }

#if 1
    op_ret = tal_sw_timer_create(__key_tickless_trig_timer_cb,NULL,&key_mag->tickless_trig_timer);
    if(op_ret != OPRT_OK) {
        goto ERR_EXIT;
    }
    
    op_ret = tal_semaphore_create_init(&(key_mag->semHandle), 0,10);
    if(OPRT_OK != op_ret)
    {
        PR_ERR("create sem err:%d", op_ret);
        return op_ret;
    }

    THREAD_CFG_T thrd_param;
    thrd_param.priority = THREAD_PRIO_0;
    thrd_param.stackDepth = 1024+1024+1024+1024;
    thrd_param.thrdname ="key_handle";
    OPERATE_RET opRet = tal_thread_create_and_start(&key_handle,NULL,NULL,key_handle_thrd,\
                           NULL,&thrd_param);
    if(OPRT_OK != opRet)
    {
        key_handle = NULL;
        PR_ERR("create thrd err:%d", opRet);
        goto ERR_EXIT;
    }
#endif
    return OPRT_OK;

ERR_EXIT:
    if(key_mag) {
        tal_free(key_mag->p_tbl);
        tal_free(key_mag);
        key_mag = NULL;
    }

    return op_ret;
}

/***********************************************************
*  Function: reg_proc_key
*  Input: 
*  Output: 
*  Return: OPERATE_RET
***********************************************************/
OPERATE_RET reg_proc_key(IN CONST KEY_USER_DEF_S *key_ud)
{
    if(NULL == key_ud) {
        return OPRT_INVALID_PARM;
    }
    if(NULL == key_ud->call_back) {
        return OPRT_INVALID_PARM;
    }
    // is registered?
    KEY_EN_LST_S *tmp_key_ent_lst = key_mag->lst;
    OPERATE_RET op_ret = OPRT_OK;
    TUYA_GPIO_IRQ_T irq_cfg;
    tal_mutex_lock(key_mag->mutex);
    while(tmp_key_ent_lst) {
        if(tmp_key_ent_lst->key_ent.kud.port == key_ud->port) {
            irq_cfg.mode = key_ud->low_level_detect ? TUYA_GPIO_IRQ_FALL : TUYA_GPIO_IRQ_RISE;
            irq_cfg.cb = __gpio_for_key_irq;
            irq_cfg.arg = (void *)(intptr_t)key_ud->port;
            op_ret = tal_gpio_irq_init(key_ud->port, &irq_cfg);
            if(OPRT_OK != op_ret) {
                tal_mutex_unlock(key_mag->mutex);
                return op_ret;
            }
            memcpy(&(tmp_key_ent_lst->key_ent.kud),key_ud,SIZEOF(KEY_USER_DEF_S));
            tal_mutex_unlock(key_mag->mutex);
            return OPRT_OK;
        }
        tmp_key_ent_lst = tmp_key_ent_lst->nxt;
    }
    tal_mutex_unlock(key_mag->mutex);
    KEY_EN_LST_S *key_ent_lst = tal_malloc(sizeof(KEY_EN_LST_S));
    if(NULL == key_ent_lst) {
        return OPRT_MALLOC_FAILED;
    }
    memset(key_ent_lst,0,sizeof(KEY_EN_LST_S));
    memcpy(&(key_ent_lst->key_ent.kud),key_ud,SIZEOF(KEY_USER_DEF_S));

    irq_cfg.mode = key_ud->low_level_detect ? TUYA_GPIO_IRQ_FALL : TUYA_GPIO_IRQ_RISE;
    irq_cfg.cb = __gpio_for_key_irq;
    irq_cfg.arg = (void *)(intptr_t)key_ud->port;    
    op_ret = tal_gpio_irq_init(key_ud->port, &irq_cfg);

    if(OPRT_OK != op_ret) {
        return op_ret;
    }

    tal_mutex_lock(key_mag->mutex);
    key_ent_lst->nxt = key_mag->lst;
    key_mag->lst = key_ent_lst;
    tal_mutex_unlock(key_mag->mutex);

    return OPRT_OK;
}

STATIC VOID __key_handle(VOID)
{
    INT_T i = 0;

    for(i = 0;i < key_mag->tbl_cnt;i++) {
        __key_ent_proc(&key_mag->p_tbl[i]);
    }
    //PR_NOTICE("__key_handle");
    tal_mutex_lock(key_mag->mutex);
    KEY_EN_LST_S *key_ent_lst = key_mag->lst;
    while(key_ent_lst) {
        __key_ent_proc(&key_ent_lst->key_ent);
        key_ent_lst = key_ent_lst->nxt;
    }
    tal_mutex_unlock(key_mag->mutex);
}

STATIC BOOL_T __tuya_key_down_verify(IN CONST KEY_ENTITY_S *key_ent)
{
    TUYA_GPIO_LEVEL_E gpio_stat = 0;
    tal_gpio_read(key_ent->kud.port, &gpio_stat);
    if(FALSE == key_ent->kud.low_level_detect) {
        return (gpio_stat) ? TRUE:FALSE;
    }else {
        return (gpio_stat) ? FALSE:TRUE;
    }
}

STATIC VOID __key_ent_proc(INOUT KEY_ENTITY_S *key_ent)
{
    switch(key_ent->status) {
        case KEY_DOWN: {
            if(TRUE == __tuya_key_down_verify(key_ent)) {
                key_ent->status = KEY_DOWN_CONFIRM;
            }
            
            key_ent->down_time = 0;
            key_ent->up_time = 0;
            key_ent->seq_key_cnt = 0;
        }
        break;
        
        case KEY_DOWN_CONFIRM: {
            if(TRUE == __tuya_key_down_verify(key_ent)) {
                key_ent->status = KEY_DOWNNING;
                if((FALLING_EDGE_TRIG == key_ent->kud.lp_tp) || (FALLING_LONG_TRIG == key_ent->kud.lp_tp)) {
                    key_ent->kud.call_back(key_ent->kud.port,NORMAL_KEY,0);
                }
            }else {
                key_ent->status = KEY_DOWN;
                key_ent->down_time = 0;
            }
        }
        break;
        
        case KEY_DOWNNING: {
            #define KEY_DOWN_CONT_TRIG_TIME_MS 300
            if(TRUE == __tuya_key_down_verify(key_ent)) {
                key_ent->down_time += (key_mag->timer_space);
                
                if(((LP_ONCE_TRIG == key_ent->kud.lp_tp) || (FALLING_LONG_TRIG == key_ent->kud.lp_tp)) && \
                   key_ent->down_time >= key_ent->kud.long_key_time && \
                   FALSE == key_ent->long_key_press) {
                   key_ent->kud.call_back(key_ent->kud.port,LONG_KEY,0);
                   key_ent->long_key_press = TRUE;

                } else if(LP_MORE_NORMAL_TRIG == key_ent->kud.lp_tp && \
                         key_ent->down_time >= KEY_DOWN_CONT_TRIG_TIME_MS) {
                    key_ent->kud.call_back(key_ent->kud.port,NORMAL_KEY,0);
                    key_ent->down_time = 0;
                }
            }else {
                key_ent->status = KEY_UP_CONFIRM;
                key_ent->up_time = 0;
            }
        }
        break;
        
        case KEY_UP_CONFIRM: {
            if(FALSE == __tuya_key_down_verify(key_ent)) {
                key_ent->status = KEY_UPING;
            }else {
                key_ent->down_time += key_mag->timer_space;
                key_ent->status = KEY_DOWNNING;
            }
        }
        break;

        case KEY_UPING: {
            if(FALSE == __tuya_key_down_verify(key_ent)) {
                if(0 == key_ent->up_time) {
                    key_ent->up_time = (key_mag->timer_space)*2;
                }else {
                    key_ent->up_time += key_mag->timer_space;
                }
                
                if(key_ent->up_time >= key_ent->kud.seq_key_detect_time) {
                    key_ent->status = KEY_FINISH;
                }
            }else { // is seq key?
                if(key_ent->up_time >= key_ent->kud.seq_key_detect_time) {
                    key_ent->status = KEY_FINISH;
                }else {
                    key_ent->status = KEY_DOWN_CONFIRM;
                    key_ent->up_time = 0;
                    key_ent->seq_key_cnt++;
                }
            }

            if((KEY_FINISH == key_ent->status)) {
                if(key_ent->seq_key_cnt) {
                    key_ent->seq_key_cnt++;
                }

                if(key_ent->seq_key_cnt > 1) {
                    key_ent->kud.call_back(key_ent->kud.port,SEQ_KEY,key_ent->seq_key_cnt);

                } else {
                    if((FALLING_EDGE_TRIG != key_ent->kud.lp_tp) && (FALLING_LONG_TRIG != key_ent->kud.lp_tp)) {
						if(!((LP_ONCE_TRIG == key_ent->kud.lp_tp)&&(key_ent->long_key_press))) {
							key_ent->kud.call_back(key_ent->kud.port,NORMAL_KEY,0);
						}
                    }
                }
                
                if(LP_ONCE_TRIG == key_ent->kud.lp_tp){
                    key_ent->long_key_press = FALSE;
                }
            }
        }
        break;

        case KEY_FINISH: {
            key_ent->kud.call_back(key_ent->kud.port,RELEASE_KEY,0);
            key_ent->status = KEY_DOWN;
        }
        break;

        default:
            break;
    }
}

STATIC VOID __key_tickless_trig_timer_cb(TIMER_ID timerID,PVOID_T pTimerArg)
{
    //打开tickless
    if(tickless_endisable) {
        PR_NOTICE("%s enter",__func__);
        tal_cpu_lp_enable();
        tickless_endisable = FALSE;
    }
}

STATIC VOID __restart_tickless_trig_timer(VOID)
{
    OPERATE_RET op_ret = OPRT_OK;
    op_ret = tal_sw_timer_stop(key_mag->tickless_trig_timer);
    if(op_ret != OPRT_OK) {
        PR_ERR("stop timer err");
        return;
    }

    op_ret = tal_sw_timer_start(key_mag->tickless_trig_timer, keep_time_val,TAL_TIMER_ONCE);
    if(op_ret != OPRT_OK) {
        PR_ERR("start timer err");
        return;
    }
}

STATIC VOID key_handle_thrd(PVOID_T arg)
{
    while(1) {
        tal_semaphore_wait(key_mag->semHandle, SEM_WAIT_FOREVER);
        PR_NOTICE("get key interrupt");

        //打开tickless
        if(!tickless_endisable) {
            tal_cpu_lp_enable();
            tickless_endisable = TRUE;
        }

        has_enter_irq = FALSE;
        __restart_tickless_trig_timer();
        
        while(tickless_endisable) {
            __key_handle();
            tal_system_sleep(key_mag->timer_space);//default 20ms
            if(has_enter_irq) {
                has_enter_irq = FALSE;
               __restart_tickless_trig_timer();
            }
        }

    }
    
}


VOID __gpio_for_key_irq(VOID *args)
{
    if(!tickless_endisable) {
        tal_semaphore_post(key_mag->semHandle);
    }

    has_enter_irq = TRUE;
}


VOID key_set_keep_time(IN CONST UINT_T val)
{
	keep_time_val = val;
}

UINT_T key_get_keep_time(VOID)
{
	return keep_time_val;
}

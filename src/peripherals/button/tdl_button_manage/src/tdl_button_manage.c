
/**
 * @file tdl_button_manage.c
 * @author franky.lin@tuya.com
 * @brief tdl_button_manage, base timer、semaphore、task
 * @version 1.0
 * @date 2022-03-20
 * @copyright Copyright (c) tuya.inc 2022
 * button trigger management component
 */

// sdk
#include "string.h"
#include "stdint.h"

#include "tal_semaphore.h"
#include "tal_mutex.h"
#include "tal_system.h"

#include "tal_memory.h"
#include "tal_log.h"
#include "tal_thread.h"
#include "tuya_list.h"

#include "tdl_button_driver.h"
#include "tdl_button_manage.h"

/***********************************************************
*************************micro define***********************
***********************************************************/
#define COMBINE_BUTTON_ENABLE 0

#define BUTTON_SCAN_TASK 0x01
#define BUTTON_IRQ_TASK  0x02

#define TDL_BUTTON_NAME_LEN        32    // button name max len 32byte
#define TDL_LONG_START_VAILD_TIMER 1500  // ms
#define TDL_LONG_KEEP_TIMER        100   // ms
#define TDL_BUTTON_DEBOUNCE_TIME   60    // ms
#define TDL_BUTTON_IRQ_SCAN_TIME   10000 // ms
#define TDL_BUTTON_SCAN_TIME       10    // 10ms
#define TDL_BUTTON_IRQ_SCAN_CNT    (TDL_BUTTON_IRQ_SCAN_TIME / tdl_button_scan_time)
#define TOUCH_DELAY                500 // 间隔时间500ms  用于单双击识别区分
#define PUT_EVENT_CB(btn, name, ev, arg)                                                                               \
    do {                                                                                                               \
        if (btn.list_cb[ev])                                                                                           \
            btn.list_cb[ev](name, ev, arg);                                                                            \
    } while (0)
#define TDL_BUTTON_TASK_STACK_SIZE (2048)

/***********************************************************
***********************typedef define***********************
***********************************************************/
typedef struct {
    LIST_HEAD hdr; /* list head */
} TDL_BUTTON_LIST_HEAD_T;

typedef struct {
    TDL_BUTTON_MODE_E button_mode; // 按键驱动模式：扫描 中断
} TDL_BUTTON_HARDWARE_CFG_T;

typedef struct {
    uint8_t pre_event : 4; // 上一次的事件
    uint8_t now_event : 4; // 当前生成的事件
    uint8_t flag : 3;      // 按键处理流程状态
    uint8_t debounce_cnt;  // 消抖时间转化的次数
    uint16_t ticks;        // 按下保持计数
    uint8_t status;        // 按键实际状态
    uint8_t repeat;        // 重复按下计数
    uint8_t ready;         // 标识按键上电是否ready
    uint8_t init_flag;     // 按键初始化成功

    TDL_BUTTON_CTRL_INFO ctrl_info;    // 驱动挂载的信息
    DEVICE_BUTTON_HANDLE dev_handle;   // 驱动句柄
    TDL_BUTTON_HARDWARE_CFG_T dev_cfg; // 硬件配置
} BUTTON_DRIVER_DATA_T;

typedef struct {
    TDL_BUTTON_CFG_T button_cfg;                       /*button data*/
    TDL_BUTTON_EVENT_CB list_cb[TDL_BUTTON_PRESS_MAX]; /*button cb*/
} BUTTON_USER_DATA_T;                                  // user data

typedef struct {
    LIST_HEAD hdr; /* list node */
    char *name;    /* node name */
    MUTEX_HANDLE button_mutex;
    BUTTON_USER_DATA_T user_data;     /* user data */
    BUTTON_DRIVER_DATA_T device_data; /* driver data */
} TDL_BUTTON_LIST_NODE_T;             // 单个按键节点

#if (COMBINE_BUTTON_ENABLE == 1)
typedef struct {
    LIST_HEAD hdr;                 /* list node */
    char *name;                    /* node name */
    TDL_BUTTON_FUNC_CB combine_cb; /*combine button cb*/
} TDL_BUTTON_COMBINE_LIST_NODE_T;  // 组合按键节点
#endif

typedef struct {
    uint8_t scan_task_flag;   /*扫描线程标志*/
    uint8_t irq_task_flag;    /*中断线程标志*/
    uint8_t task_mode;        /*线程类型*/
    SEM_HANDLE irq_semaphore; /*中断信号量*/
    uint32_t irq_scan_cnt;    /*中断线程断开计数*/
    MUTEX_HANDLE mutex;       /*锁*/
} TDL_BUTTON_LOCAL_T;         // TDL本地参数

/***********************************************************
***********************variable define**********************
***********************************************************/
TDL_BUTTON_LOCAL_T tdl_button_local = {.irq_task_flag = FALSE,
                                       .scan_task_flag = FALSE,
                                       .task_mode = FALSE,
                                       .irq_semaphore = NULL,
                                       .irq_scan_cnt = TDL_BUTTON_IRQ_SCAN_TIME / TDL_BUTTON_SCAN_TIME,
                                       .mutex = NULL};

THREAD_HANDLE scan_thread_handle = NULL; // 扫描线程句柄
THREAD_HANDLE irq_thread_handle = NULL;  // 中断扫描线程句柄

TDL_BUTTON_LIST_HEAD_T *p_button_list = NULL; // 单个按键链表头
// TDL_BUTTON_LIST_HEAD_T *p_combine_button_list = NULL;//组合按键链表头

static uint8_t g_tdl_button_list_exist = FALSE; // 单个按键链表头初始化标志
// static uint8_t g_tdl_combine_button_list_exist = FALSE;//组合按键链表头初始化标志
static uint8_t g_tdl_button_scan_mode_exist = 0xFF;
static uint32_t sg_bt_task_stack_size = TDL_BUTTON_TASK_STACK_SIZE;
static uint8_t tdl_button_scan_time = TDL_BUTTON_SCAN_TIME;

/***********************************************************
***********************function define**********************
***********************************************************/
static OPERATE_RET __tdl_get_operate_info(TDL_BUTTON_LIST_NODE_T *p_node, TDL_BUTTON_OPRT_INFO *oprt_info);
static OPERATE_RET __tdl_button_scan_task(uint8_t enable);
static OPERATE_RET __tdl_button_irq_task(uint8_t enable);

// 单个按键链表头生成
static OPERATE_RET __tdl_button_list_init(void)
{
    if (g_tdl_button_list_exist == FALSE) {
        p_button_list = (TDL_BUTTON_LIST_HEAD_T *)tal_malloc(sizeof(TDL_BUTTON_LIST_HEAD_T));
        if (NULL == p_button_list) {
            return OPRT_MALLOC_FAILED;
        }

        if (tal_semaphore_create_init(&tdl_button_local.irq_semaphore, 0, 1) != 0) {
            PR_ERR("tdl_semaphore_init err");
            return OPRT_COM_ERROR;
        }

        if (tal_mutex_create_init(&tdl_button_local.mutex) != 0) {
            PR_ERR("tdl_mutex_init err");
            return OPRT_COM_ERROR;
        }

        INIT_LIST_HEAD(&p_button_list->hdr);
        g_tdl_button_list_exist = TRUE;
    }

    return OPRT_OK;
}

#if (COMBINE_BUTTON_ENABLE == 1)
// 组合键链表头生成
static OPERATE_RET __tdl_button_combine_list_init(void)
{
    if (g_tdl_combine_button_list_exist == FALSE) {
        p_combine_button_list = (TDL_BUTTON_LIST_HEAD_T *)tal_malloc(sizeof(TDL_BUTTON_LIST_HEAD_T));
        if (NULL == p_combine_button_list) {
            return OPRT_MALLOC_FAILED;
        }

        INIT_LIST_HEAD(&p_combine_button_list->hdr);
        g_tdl_combine_button_list_exist = TRUE;
    }

    return OPRT_OK;
}
#endif

// 根据句柄查找按键节点
static TDL_BUTTON_LIST_NODE_T *__tdl_button_find_node(TDL_BUTTON_HANDLE handle)
{
    TDL_BUTTON_LIST_HEAD_T *p_head = p_button_list;
    TDL_BUTTON_LIST_NODE_T *p_node = NULL;
    LIST_HEAD *pos = NULL;

    if (NULL == p_head) {
        PR_ERR("__tdl_button_find_node err");
        return NULL;
    }
    tuya_list_for_each(pos, &p_head->hdr)
    {
        p_node = tuya_list_entry(pos, TDL_BUTTON_LIST_NODE_T, hdr);
        if (p_node == handle) {
            // 地址比对成功
            return p_node;
        }
    }
    return NULL;
}

#if (COMBINE_BUTTON_ENABLE == 1)
// 根据句柄查找组合键节点
static TDL_BUTTON_COMBINE_LIST_NODE_T *__tdl_button_find_combine_node(TDL_BUTTON_HANDLE handle)
{
    TDL_BUTTON_LIST_HEAD_T *p_head = p_combine_button_list;
    TDL_BUTTON_COMBINE_LIST_NODE_T *p_node = NULL;
    LIST_HEAD *pos = NULL;

    if (NULL == p_head) {
        PR_ERR("__tdl_button_find_combine_node err");
        return NULL;
    }
    tuya_list_for_each(pos, &p_head->hdr)
    {
        p_node = tuya_list_entry(pos, TDL_BUTTON_COMBINE_LIST_NODE_T, hdr);
        if (p_node == handle) {
            // 地址比对成功
            return p_node;
        }
    }
    return NULL;
}
#endif

// 根据名字查找按键节点
static TDL_BUTTON_LIST_NODE_T *__tdl_button_find_node_name(char *name)
{
    TDL_BUTTON_LIST_HEAD_T *p_head = p_button_list;
    TDL_BUTTON_LIST_NODE_T *p_node = NULL;
    LIST_HEAD *pos = NULL;

    if (NULL == p_head) {
        PR_ERR("__tdl_button_find_node_name err");
        return NULL;
    }
    tuya_list_for_each(pos, &p_head->hdr)
    {
        p_node = tuya_list_entry(pos, TDL_BUTTON_LIST_NODE_T, hdr);
        if (strcmp(name, p_node->name) == 0) {
            // 名称比对成功
            return p_node;
        }
    }
    return NULL;
}

#if (COMBINE_BUTTON_ENABLE == 1)
// 根据名字查找组合键节点
static TDL_BUTTON_COMBINE_LIST_NODE_T *__tdl_button_find_node_combine_name(char *name)
{
    TDL_BUTTON_LIST_HEAD_T *p_head = p_combine_button_list;
    TDL_BUTTON_COMBINE_LIST_NODE_T *p_node = NULL;
    LIST_HEAD *pos = NULL;

    if (NULL == p_head) {
        PR_ERR("__tdl_button_find_node_combine_name err");
        return NULL;
    }
    tuya_list_for_each(pos, &p_head->hdr)
    {
        p_node = tuya_list_entry(pos, TDL_BUTTON_COMBINE_LIST_NODE_T, hdr);
        if (strcmp(name, p_node->name) == 0) {
            // 名称比对成功
            return p_node;
        }
    }
    return NULL;
}
#endif

// 添加新节点：创建节点，并存储驱动控制信息
static TDL_BUTTON_LIST_NODE_T *__tdl_button_add_node(char *name, TDL_BUTTON_CTRL_INFO *info,
                                                     TDL_BUTTON_DEVICE_INFO_T *cfg)
{
    TDL_BUTTON_LIST_HEAD_T *p_head = p_button_list;
    TDL_BUTTON_LIST_NODE_T *p_node = NULL;
    uint8_t name_len = 0;

    if (NULL == p_head) {
        PR_ERR("__tdl_button_add_node err");
        return NULL;
    }

    if (NULL == info) {
        return NULL; // return OPRT_INVALID_PARM;
    }

    if (NULL == cfg) {
        return NULL; // return OPRT_INVALID_PARM;
    }

    // 比对名称是否存在
    if (__tdl_button_find_node_name(name) != NULL) {
        PR_NOTICE("button name existence");
        return NULL; // return OPRT_COM_ERROR;
    }

    // 创建新节点
    p_node = (TDL_BUTTON_LIST_NODE_T *)tal_malloc(sizeof(TDL_BUTTON_LIST_NODE_T));
    if (NULL == p_node) {
        return NULL; // return OPRT_MALLOC_FAILED;
    }
    memset(p_node, 0, sizeof(TDL_BUTTON_LIST_NODE_T));

    // 创建新名称
    p_node->name = (char *)tal_malloc(TDL_BUTTON_NAME_LEN);
    if (NULL == p_node->name) {
        tal_free(p_node);
        p_node = NULL;
        return NULL; // return OPRT_MALLOC_FAILED;
    }
    memset(p_node->name, 0, TDL_BUTTON_NAME_LEN);

    // 存入名称
    name_len = strlen(name);
    if (name_len >= TDL_BUTTON_NAME_LEN) {
        name_len = TDL_BUTTON_NAME_LEN;
    }

    memcpy(p_node->name, name, name_len);
    memcpy(&(p_node->device_data.ctrl_info), info, sizeof(TDL_BUTTON_CTRL_INFO));
    p_node->device_data.dev_cfg.button_mode = cfg->mode;
    p_node->device_data.dev_handle = cfg->dev_handle;

    // 添加新节点
    tal_mutex_lock(tdl_button_local.mutex);
    tuya_list_add(&p_node->hdr, &p_head->hdr);
    tal_mutex_unlock(tdl_button_local.mutex);

    return p_node;
}

// 更新节点用户数据：数据内容固定为用户数据
static TDL_BUTTON_LIST_NODE_T *__button_updata_userdata(char *name, TDL_BUTTON_CFG_T *button_cfg)
{
    TDL_BUTTON_LIST_NODE_T *p_node = NULL;

    // 比对名称是否存在
    p_node = __tdl_button_find_node_name(name);
    if (NULL == p_node) {
        PR_NOTICE("button no existence");
        return NULL;
    }

    if (NULL == button_cfg) {
        PR_NOTICE("user button_cfg NULL");
        p_node->user_data.button_cfg.long_start_valid_time = TDL_LONG_START_VAILD_TIMER;
        p_node->user_data.button_cfg.long_keep_timer = TDL_LONG_KEEP_TIMER;
        p_node->user_data.button_cfg.button_debounce_time = TDL_BUTTON_DEBOUNCE_TIME;
    } else {
        p_node->user_data.button_cfg.long_start_valid_time = button_cfg->long_start_valid_time;
        p_node->user_data.button_cfg.long_keep_timer = button_cfg->long_keep_timer;
        p_node->user_data.button_cfg.button_debounce_time = button_cfg->button_debounce_time;
        p_node->user_data.button_cfg.button_repeat_valid_time = button_cfg->button_repeat_valid_time;
        p_node->user_data.button_cfg.button_repeat_valid_count = button_cfg->button_repeat_valid_count;
    }

    p_node->device_data.pre_event = TDL_BUTTON_PRESS_NONE;
    p_node->device_data.now_event = TDL_BUTTON_PRESS_NONE;

    return p_node;
}

// 按键扫描状态机：生成按键触发事件
static void __tdl_button_state_handle(TDL_BUTTON_LIST_NODE_T *p_node)
{
    uint16_t hold_tick = 0;

    if (NULL == p_node) {
        return;
    }

    switch (p_node->device_data.flag) {
    case 0: {
        // PR_NOTICE("case0:tick=%d",p_node->device_data.ticks);
        if (p_node->device_data.status != 0) {
            if (p_node->device_data.dev_cfg.button_mode == BUTTON_IRQ_MODE) {
                tdl_button_local.irq_scan_cnt = 0;
            }
            /*触发按下事件*/
            p_node->device_data.ticks = 0;
            p_node->device_data.repeat = 1;
            p_node->device_data.flag = 1;
            p_node->device_data.pre_event = p_node->device_data.now_event;
            p_node->device_data.now_event = TDL_BUTTON_PRESS_DOWN;
            PUT_EVENT_CB(p_node->user_data, p_node->name, TDL_BUTTON_PRESS_DOWN,
                         (void *)((uint32_t)p_node->device_data.repeat));

        } else {
            p_node->device_data.pre_event = p_node->device_data.now_event;
            p_node->device_data.now_event = TDL_BUTTON_PRESS_NONE; // 默认状态无,不用执行回调
        }
    } break;

    case 1: {
        // PR_NOTICE("case1:tick=%d",p_node->device_data.ticks);
        if (p_node->device_data.status != 0) {
            if (p_node->device_data.dev_cfg.button_mode == BUTTON_IRQ_MODE) {
                tdl_button_local.irq_scan_cnt = 0;
            }
            if (p_node->user_data.button_cfg.long_start_valid_time == 0) {
                // 长按有效时间0,不执行长按
                p_node->device_data.pre_event = p_node->device_data.now_event;
            } else if (p_node->device_data.ticks >
                       (p_node->user_data.button_cfg.long_start_valid_time / tdl_button_scan_time)) {
                /*触发开始长按事件*/
                // PR_NOTICE("long tick =%d",p_node->device_data.ticks);
                p_node->device_data.pre_event = p_node->device_data.now_event;
                p_node->device_data.now_event = TDL_BUTTON_LONG_PRESS_START;
                PUT_EVENT_CB(p_node->user_data, p_node->name, TDL_BUTTON_LONG_PRESS_START,
                             (void *)((uint32_t)p_node->device_data.ticks * tdl_button_scan_time));
                p_node->device_data.flag = 5;
            } else {
                // 第一次按下，持续按着，未达到开始长按的事件，及时更新前后状态
                p_node->device_data.pre_event = p_node->device_data.now_event;
            }
        } else {
            /*触发弹起事件*/
            p_node->device_data.pre_event = p_node->device_data.now_event;
            p_node->device_data.now_event = TDL_BUTTON_PRESS_UP;
            PUT_EVENT_CB(p_node->user_data, p_node->name, TDL_BUTTON_PRESS_UP,
                         (void *)((uint32_t)p_node->device_data.repeat));
            p_node->device_data.flag = 2;
            p_node->device_data.ticks = 0;
        }
    } break;

    case 2: {
        // PR_NOTICE("case2");
        if (p_node->device_data.status != 0) {
            /*press again*/
            if (p_node->device_data.dev_cfg.button_mode == BUTTON_IRQ_MODE) {
                tdl_button_local.irq_scan_cnt = 0;
            }
            p_node->device_data.repeat++;
            p_node->device_data.pre_event = p_node->device_data.now_event;
            p_node->device_data.now_event = TDL_BUTTON_PRESS_DOWN;
            PUT_EVENT_CB(p_node->user_data, p_node->name, TDL_BUTTON_PRESS_DOWN,
                         (void *)((uint32_t)p_node->device_data.repeat));
            p_node->device_data.flag = 3;
        } else {
            /*release timeout*/
            if (p_node->device_data.ticks >=
                (p_node->user_data.button_cfg.button_repeat_valid_time / tdl_button_scan_time)) {
                /*释放超时触发单击*/
                if (p_node->device_data.repeat == 1) {
                    p_node->device_data.pre_event = p_node->device_data.now_event;
                    p_node->device_data.now_event = TDL_BUTTON_PRESS_SINGLE_CLICK;
                    PUT_EVENT_CB(p_node->user_data, p_node->name, TDL_BUTTON_PRESS_SINGLE_CLICK,
                                 (void *)((uint32_t)p_node->device_data.repeat));
                } else if (p_node->device_data.repeat == 2) {
                    /*释放触发双击事件*/
                    p_node->device_data.pre_event = p_node->device_data.now_event;
                    p_node->device_data.now_event = TDL_BUTTON_PRESS_DOUBLE_CLICK;
                    PUT_EVENT_CB(p_node->user_data, p_node->name, TDL_BUTTON_PRESS_DOUBLE_CLICK,
                                 (void *)((uint32_t)p_node->device_data.repeat));
                } else if (p_node->device_data.repeat == p_node->user_data.button_cfg.button_repeat_valid_count) {
                    if (p_node->user_data.button_cfg.button_repeat_valid_count > 2) {
                        p_node->device_data.pre_event = p_node->device_data.now_event;
                        p_node->device_data.now_event = TDL_BUTTON_PRESS_REPEAT;
                        PUT_EVENT_CB(p_node->user_data, p_node->name, TDL_BUTTON_PRESS_REPEAT,
                                     (void *)((uint32_t)p_node->device_data.repeat));
                    }
                }
                p_node->device_data.flag = 0;
            } else {
                // 释放后未超时，及时更新前后状态
                p_node->device_data.pre_event = p_node->device_data.now_event;
            }
        }
    } break;

    case 3: {
        uint16_t repeat_tick = 0;
        // PR_NOTICE("case3:tick=%d",p_node->device_data.ticks);
        /*repeat up*/
        // 大于一次按下之后的释放
        if (p_node->device_data.status == 0) {
            p_node->device_data.pre_event = p_node->device_data.now_event;
            p_node->device_data.now_event = TDL_BUTTON_PRESS_UP;
            PUT_EVENT_CB(p_node->user_data, p_node->name, TDL_BUTTON_PRESS_UP,
                         (void *)((uint32_t)p_node->device_data.repeat));
            repeat_tick = p_node->user_data.button_cfg.button_repeat_valid_time / tdl_button_scan_time;
            if (p_node->device_data.ticks >= repeat_tick) {
                // 释放后超时,双击按默认间隔时间,多击使用用户配置的间隔时间
                // PR_NOTICE("3: tick=%d",p_node->device_data.ticks);
                // PR_NOTICE("%d",repeat_tick);
                p_node->device_data.flag = 0;
            } else {
                p_node->device_data.flag = 2;
                p_node->device_data.ticks = 0;
            }
        } else {
            // 大于一次按下，持续按着，及时更新前后状态
            p_node->device_data.pre_event = p_node->device_data.now_event;
        }

    } break;

    case 5: {
        if (p_node->device_data.status != 0) {
            /*触发长按保持事件*/
            if (p_node->device_data.dev_cfg.button_mode == BUTTON_IRQ_MODE) {
                tdl_button_local.irq_scan_cnt = 0;
            }
            hold_tick = p_node->user_data.button_cfg.long_keep_timer / tdl_button_scan_time;
            if (hold_tick == 0) {
                hold_tick = 1;
            }
            if (p_node->device_data.ticks >= hold_tick) {
                // 大于hold计数立即刷新状态
                p_node->device_data.pre_event = p_node->device_data.now_event;
                p_node->device_data.now_event = TDL_BUTTON_LONG_PRESS_HOLD;
                if (p_node->device_data.ticks % hold_tick == 0) {
                    // 确认达到hold整数倍才执行
                    // PR_NOTICE("hold,tick=%d",hold_tick);
                    PUT_EVENT_CB(p_node->user_data, p_node->name, TDL_BUTTON_LONG_PRESS_HOLD,
                                 (void *)((uint32_t)p_node->device_data.ticks * tdl_button_scan_time));
                }
            }
        } else {
            /*hold release*/
            p_node->device_data.pre_event = p_node->device_data.now_event;
            p_node->device_data.now_event = TDL_BUTTON_PRESS_UP;
            PUT_EVENT_CB(p_node->user_data, p_node->name, TDL_BUTTON_PRESS_UP,
                         (void *)((uint32_t)p_node->device_data.ticks * tdl_button_scan_time));
            p_node->device_data.ticks = 0;
            p_node->device_data.flag = 0;
        }
    } break;
    case 6: {
        /*If the power is continuously maintained at an effective level and triggered after recovery*/
        PUT_EVENT_CB(p_node->user_data, p_node->name, TDL_BUTTON_RECOVER_PRESS_UP, NULL);
        p_node->device_data.ticks = 0;
        p_node->device_data.flag = 0;
    } break;

    default:
        break;
    }
    return;
}

// 按键中断回调函数
static void __tdl_button_irq_cb(void *arg)
{
    if (tdl_button_local.irq_scan_cnt >= TDL_BUTTON_IRQ_SCAN_CNT) {
        tal_semaphore_post(tdl_button_local.irq_semaphore);
    }
    return;
}

// 获取传给TDD层的信息
static OPERATE_RET __tdl_get_operate_info(TDL_BUTTON_LIST_NODE_T *p_node, TDL_BUTTON_OPRT_INFO *oprt_info)
{
    if (NULL == oprt_info) {
        return OPRT_INVALID_PARM;
    }

    if (NULL == p_node) {
        return OPRT_INVALID_PARM;
    }

    memset(oprt_info, 0, sizeof(TDL_BUTTON_OPRT_INFO));
    oprt_info->dev_handle = p_node->device_data.dev_handle;
    oprt_info->irq_cb = __tdl_button_irq_cb;

    return OPRT_OK;
}

// 创建单个按键,返回句柄给用户使用
OPERATE_RET tdl_button_create(char *name, TDL_BUTTON_CFG_T *button_cfg, TDL_BUTTON_HANDLE *p_handle)
{
    OPERATE_RET ret = OPRT_COM_ERROR;
    TDL_BUTTON_LIST_NODE_T *p_node = NULL;
    TDL_BUTTON_OPRT_INFO button_oprt;

    if (NULL == p_handle) {
        PR_ERR("tdl create handle err");
        return OPRT_INVALID_PARM;
    }

    if (NULL == button_cfg) {
        PR_ERR("tdl create cfg err");
        return OPRT_INVALID_PARM;
    }

    // 更新某节点的用户数据
    p_node = __button_updata_userdata(name, button_cfg);
    if (NULL != p_node) {
        // PR_NOTICE("tdl create updata OK");
    } else {
        PR_ERR("tdl create updata err");
        return OPRT_COM_ERROR;
    }

    if (NULL == p_node->button_mutex) {
        ret = tal_mutex_create_init(&p_node->button_mutex);
        if (OPRT_OK != ret) {
            PR_ERR("button mutex create err");
            return OPRT_COM_ERROR;
        }
    }

    ret = __tdl_get_operate_info(p_node, &button_oprt);
    if (OPRT_OK != ret) {
        PR_ERR("tdl create err");
        return OPRT_COM_ERROR;
    }

    ret = p_node->device_data.ctrl_info.button_create(&button_oprt);
    if (OPRT_OK != ret) {
        PR_ERR("tdd creat err");
        return OPRT_COM_ERROR;
    }
    p_node->device_data.init_flag = TRUE;

    if (p_node->device_data.dev_cfg.button_mode == BUTTON_IRQ_MODE) {
        tdl_button_local.task_mode |= BUTTON_IRQ_TASK;
    } else if (p_node->device_data.dev_cfg.button_mode == BUTTON_TIMER_SCAN_MODE) {
        tdl_button_local.task_mode |= BUTTON_SCAN_TASK;
    }

    // 传出句柄
    *p_handle = (TDL_BUTTON_HANDLE)p_node;
    if ((g_tdl_button_scan_mode_exist != p_node->device_data.dev_cfg.button_mode) &&
        (g_tdl_button_scan_mode_exist != 0xFF)) {
        PR_ERR("buton scan_mode isn't same,please check!");
        return OPRT_COM_ERROR;
    }

    if (tdl_button_local.task_mode == BUTTON_IRQ_TASK) {
        __tdl_button_irq_task(1);
        if (OPRT_OK != ret) {
            PR_ERR("tdl create err");
            return OPRT_COM_ERROR;
        }
    } else {
        __tdl_button_scan_task(1);
        if (OPRT_OK != ret) {
            PR_ERR("tdl create err");
            return OPRT_COM_ERROR;
        }
    }
    g_tdl_button_scan_mode_exist = p_node->device_data.dev_cfg.button_mode;
    PR_DEBUG("tdl_button_create succ");
    return OPRT_OK;
}

#if (COMBINE_BUTTON_ENABLE == 1)
// 创建组合键：返回句柄给用户使用
OPERATE_RET tdl_combine_button_create(char *name, TDL_BUTTON_HANDLE *p_handle)
{
    OPERATE_RET ret = OPRT_COM_ERROR;
    TDL_BUTTON_COMBINE_LIST_NODE_T *p_node = NULL;
    uint8_t name_len = 0;

    if (NULL == p_handle) {
        PR_ERR("tdl create handle err");
        return OPRT_INVALID_PARM;
    }

    ret = __tdl_button_combine_list_init();
    if (OPRT_OK != ret) {
        PR_NOTICE("tdl combine list init err");
        return ret;
    }

    p_node = __tdl_button_find_node_combine_name(name);
    if (NULL != p_node) {
        PR_NOTICE("combine name exist");
        return OPRT_OK;
    }

    // 创建新节点
    p_node = (TDL_BUTTON_COMBINE_LIST_NODE_T *)tal_malloc(sizeof(TDL_BUTTON_COMBINE_LIST_NODE_T));
    if (NULL == p_node) {
        return OPRT_MALLOC_FAILED;
    }
    memset(p_node, 0, sizeof(TDL_BUTTON_COMBINE_LIST_NODE_T));

    // 创建新名称
    p_node->name = (char *)tal_malloc(TDL_BUTTON_NAME_LEN);
    if (NULL == p_node->name) {
        tal_free(p_node);
        p_node = NULL;
        return OPRT_MALLOC_FAILED;
    }
    memset(p_node->name, 0, TDL_BUTTON_NAME_LEN);

    // 存入名称
    name_len = strlen(name);
    if (name_len >= TDL_BUTTON_NAME_LEN) {
        name_len = TDL_BUTTON_NAME_LEN;
    }
    memcpy(p_node->name, name, name_len);

    // 添加新节点
    tal_mutex_lock(tdl_button_local.mutex);
    tuya_list_add(&(p_node->hdr), &(p_combine_button_list->hdr));
    tal_mutex_unlock(tdl_button_local.mutex);

    *p_handle = (TDL_BUTTON_HANDLE)p_node;
    PR_NOTICE("tdl_combine_button_create succ");
    return OPRT_OK;
}
#endif

// 删除单个按键：给用户删除
OPERATE_RET tdl_button_delete(TDL_BUTTON_HANDLE p_handle)
{
    OPERATE_RET ret = OPRT_COM_ERROR;
    TDL_BUTTON_LIST_NODE_T *p_node = NULL;
    TDL_BUTTON_OPRT_INFO button_oprt;

    if (NULL == p_handle) {
        return OPRT_INVALID_PARM;
    }

    p_node = __tdl_button_find_node(p_handle);
    if (NULL != p_node) {

        ret = __tdl_get_operate_info(p_node, &button_oprt);
        if (OPRT_OK != ret) {
            return OPRT_COM_ERROR;
        }

        ret = p_node->device_data.ctrl_info.button_delete(&button_oprt);
        if (OPRT_OK != ret) {
            return ret;
        }

        tal_free(p_node->name);
        p_node->name = NULL;

        tal_mutex_lock(tdl_button_local.mutex);
        tuya_list_del(&p_node->hdr);
        tal_mutex_unlock(tdl_button_local.mutex);

        tal_free(p_node); // 释放节点
        p_node = NULL;
        return OPRT_OK;
    }
    return ret;
}

OPERATE_RET tdl_button_delete_without_hardware(TDL_BUTTON_HANDLE handle)
{
    OPERATE_RET rt = OPRT_OK;
    TDL_BUTTON_LIST_NODE_T *p_node = NULL;

    TUYA_CHECK_NULL_RETURN(handle, OPRT_INVALID_PARM);

    p_node = __tdl_button_find_node(handle);
    TUYA_CHECK_NULL_RETURN(p_node, OPRT_NOT_FOUND);

    tal_mutex_lock(p_node->button_mutex);

    memset(&p_node->user_data, 0, sizeof(BUTTON_USER_DATA_T));
    p_node->device_data.pre_event = 0;
    p_node->device_data.now_event = 0;
    p_node->device_data.flag = 0;
    p_node->device_data.debounce_cnt = 0;
    p_node->device_data.ticks = 0;
    p_node->device_data.status = 0;
    p_node->device_data.repeat = 0;
    p_node->device_data.ready = 0;
    p_node->device_data.init_flag = 0;

    tal_mutex_unlock(p_node->button_mutex);

    return rt;
}

#if (COMBINE_BUTTON_ENABLE == 1)
// 删除组合按键
OPERATE_RET tdl_combine_button_delete(TDL_BUTTON_HANDLE p_handle)
{
    TDL_BUTTON_COMBINE_LIST_NODE_T *p_node = NULL;

    if (NULL == p_handle) {
        return OPRT_INVALID_PARM;
    }

    p_node = __tdl_button_find_combine_node(p_handle);
    if (NULL != p_node) {
        tal_free(p_node->name);
        p_node->name = NULL;

        tal_mutex_lock(tdl_button_local.mutex);
        tuya_list_del(&p_node->hdr);
        tal_mutex_unlock(tdl_button_local.mutex);

        tal_free(p_node); // 释放节点
        p_node = NULL;
        return OPRT_OK;
    }
    return OPRT_COM_ERROR;
}
#endif

// 按键流程：进行读取,消抖，生成事件
static void __tdl_button_handle(TDL_BUTTON_LIST_NODE_T *p_node)
{
    OPERATE_RET ret = OPRT_COM_ERROR;
    uint8_t status = 0;
    TDL_BUTTON_OPRT_INFO button_oprt;

    ret = __tdl_get_operate_info(p_node, &button_oprt);
    if (OPRT_OK != ret) {
        return;
    }

    if (p_node->device_data.init_flag == TRUE) {
        p_node->device_data.ctrl_info.read_value(&button_oprt, &status);
    } else {
        // PR_NOTICE("button is no init over, name=%s",p_node->name);
        return;
    }

    // 处理扫描模式下，长按按键时上电会触发短按，增加ready状态防止。中断模式下不会有问题,中断模式下不需要使用ready状态
    if ((p_node->device_data.dev_cfg.button_mode == BUTTON_TIMER_SCAN_MODE) && (p_node->device_data.ready == FALSE)) {
        if (status) {
            return;
        } else {
            // PR_NOTICE("device_data.ready=TRUE,%s,status=%d",p_node->name,status);
            p_node->device_data.flag = 6;
            p_node->device_data.ready = TRUE;
        }
    }

    if (p_node->device_data.flag > 0) {
        p_node->device_data.ticks++;
    }

    if (status != p_node->device_data.status) { // 按键状态发生改变，进行消抖
        if (++(p_node->device_data.debounce_cnt) >=
            (p_node->user_data.button_cfg.button_debounce_time / tdl_button_scan_time)) {
            p_node->device_data.status = status;
        }
    } else {
        p_node->device_data.debounce_cnt = 0;
    }

    __tdl_button_state_handle(p_node);
    return;
}

// 按键扫描任务：单个按键、组合键
static void __tdl_button_scan_thread(void *arg)
{
    TDL_BUTTON_LIST_HEAD_T *p_head = p_button_list;
    // TDL_BUTTON_LIST_HEAD_T *p_combine_head = p_combine_button_list;
    TDL_BUTTON_LIST_NODE_T *p_node = NULL;
    // TDL_BUTTON_COMBINE_LIST_NODE_T *p_combine_node = NULL;
    LIST_HEAD *pos1 = NULL;

    while (1) {
        tuya_list_for_each(pos1, &p_head->hdr)
        {
            p_node = tuya_list_entry(pos1, TDL_BUTTON_LIST_NODE_T, hdr);
            if ((p_node != NULL) && (p_node->device_data.dev_cfg.button_mode == BUTTON_TIMER_SCAN_MODE)) {
                tal_mutex_lock(p_node->button_mutex);
                __tdl_button_handle(p_node);
                tal_mutex_unlock(p_node->button_mutex);
            }
        }
#if (COMBINE_BUTTON_ENABLE == 1)
        // 组合键回调执行
        tuya_list_for_each(pos2, &p_combine_head->hdr)
        {
            p_combine_node = tuya_list_entry(pos2, TDL_BUTTON_COMBINE_LIST_NODE_T, hdr);
            if (p_combine_node->combine_cb) {
                p_combine_node->combine_cb();
            }
        }
#endif
        tal_system_sleep(tdl_button_scan_time);
    }
}

// 按键中断扫描任务
static void __tdl_button_irq_thread(void *arg)
{
    TDL_BUTTON_LIST_HEAD_T *p_head = p_button_list;
    // TDL_BUTTON_LIST_HEAD_T *p_combine_head = p_combine_button_list;
    TDL_BUTTON_LIST_NODE_T *p_node = NULL;
    // TDL_BUTTON_COMBINE_LIST_NODE_T *p_combine_node = NULL;
    LIST_HEAD *pos1 = NULL;

    while (1) {
        PR_NOTICE("semaphore wait");
        tal_semaphore_wait(tdl_button_local.irq_semaphore, SEM_WAIT_FOREVER);
        tdl_button_local.irq_scan_cnt = 0;
        PR_NOTICE("semaphore across");

        while (1) {
            tuya_list_for_each(pos1, &p_head->hdr)
            {
                p_node = tuya_list_entry(pos1, TDL_BUTTON_LIST_NODE_T, hdr);
                if ((p_node != NULL) && (p_node->device_data.dev_cfg.button_mode == BUTTON_IRQ_MODE)) {
                    tal_mutex_lock(p_node->button_mutex);
                    __tdl_button_handle(p_node);
                    tal_mutex_unlock(p_node->button_mutex);
                }
            }
#if (COMBINE_BUTTON_ENABLE == 1)
            // 组合键回调执行
            if (tdl_button_local.scan_task_flag == FALSE) {
                tuya_list_for_each(pos2, &p_combine_head->hdr)
                {
                    p_combine_node = tuya_list_entry(pos2, TDL_BUTTON_COMBINE_LIST_NODE_T, hdr);
                    if (p_combine_node->combine_cb) {
                        p_combine_node->combine_cb();
                    }
                }
            }
#endif
            // 中断断开计数判断
            if (++tdl_button_local.irq_scan_cnt >= TDL_BUTTON_IRQ_SCAN_CNT) {
                break;
            } else {
                tal_system_sleep(tdl_button_scan_time);
            }
        }
    }
}

// 按键扫描任务开启与关闭
static OPERATE_RET __tdl_button_scan_task(uint8_t enable)
{
    OPERATE_RET ret = OPRT_COM_ERROR;

    if (tdl_button_local.task_mode & BUTTON_SCAN_TASK) {
        if (enable != 0) {
            // 建立扫描任务
            if (tdl_button_local.scan_task_flag == FALSE) {

                THREAD_CFG_T thrd_param = {0};

                thrd_param.thrdname = "button_scan";
                thrd_param.priority = THREAD_PRIO_1;
                thrd_param.stackDepth = sg_bt_task_stack_size;
                if (NULL == scan_thread_handle) {
                    ret = tal_thread_create_and_start(&scan_thread_handle, NULL, NULL, __tdl_button_scan_thread, NULL,
                                                      &thrd_param);
                    if (OPRT_OK != ret) {
                        PR_ERR("scan_task create error!");
                        return ret;
                    }
                }
                tdl_button_local.scan_task_flag = TRUE;
                PR_DEBUG("button_scan task stack size:%d", sg_bt_task_stack_size);
            }
        } else {
            // 关闭扫描
            tal_thread_delete(scan_thread_handle);
            tdl_button_local.scan_task_flag = FALSE;
        }
    }
    return OPRT_OK;
}

// 按键扫描任务开启与关闭
static OPERATE_RET __tdl_button_irq_task(uint8_t enable)
{
    OPERATE_RET ret = OPRT_COM_ERROR;
    if (tdl_button_local.task_mode & BUTTON_IRQ_TASK) {
        if (enable != 0) {
            // 建立中断扫描任务
            if (tdl_button_local.irq_task_flag == FALSE) {
                THREAD_CFG_T thrd_param = {0};

                thrd_param.thrdname = "button_irq";
                thrd_param.priority = THREAD_PRIO_1;
                thrd_param.stackDepth = sg_bt_task_stack_size;
                if (NULL == irq_thread_handle) {
                    ret = tal_thread_create_and_start(&irq_thread_handle, NULL, NULL, __tdl_button_irq_thread, NULL,
                                                      &thrd_param);
                    if (OPRT_OK != ret) {
                        PR_ERR("irq_task create error!");
                        return ret;
                    }
                }
                tdl_button_local.irq_task_flag = TRUE;
                PR_DEBUG("button_irq task stack size:%d", sg_bt_task_stack_size);
            } else {
                PR_WARN("button irq tast have already creat");
            }
        } else {
            // 关闭中断扫描
            tal_thread_delete(irq_thread_handle);
            tdl_button_local.irq_task_flag = FALSE;
        }
    }
    return OPRT_OK;
}

OPERATE_RET tdl_button_deep_sleep_ctrl(uint8_t enable)
{
    OPERATE_RET ret = OPRT_COM_ERROR;

    if (tdl_button_local.task_mode == BUTTON_IRQ_TASK) {
        ret = __tdl_button_irq_task(enable);
        if (OPRT_OK != ret) {
            return ret;
        }
    } else {
        ret = __tdl_button_scan_task(enable);
        if (OPRT_OK != ret) {
            return ret;
        }
    }
    return OPRT_OK;
}

#if 0
// 设置按键长按开始有效时间  OK
void tdl_button_set_long_valid_time(TDL_BUTTON_HANDLE handle, uint16_t long_start_time)
{
    TDL_BUTTON_LIST_NODE_T *p_node = NULL;

    p_node = __tdl_button_find_node(handle);
    if (NULL != p_node) {
        p_node->user_data.button_cfg.long_start_valid_time = long_start_time;
    }
    return;
}

// 获取当前长按时间  OK
void tdl_button_get_current_long_press_time(TDL_BUTTON_HANDLE handle, uint16_t *long_start_time)
{
    TDL_BUTTON_LIST_NODE_T *p_node = NULL;

    p_node = __tdl_button_find_node(handle);
    if (NULL != p_node) {
        *long_start_time = p_node->device_data.ticks * tdl_button_scan_time;
    }
    return;
}

// 设置按键多击次数  OK
void tdl_button_set_repeat_cnt(TDL_BUTTON_HANDLE handle, uint8_t count)
{
    TDL_BUTTON_LIST_NODE_T *p_node = NULL;

    p_node = __tdl_button_find_node(handle);
    if (NULL != p_node) {
        p_node->user_data.button_cfg.button_repeat_valid_count = count;
    }
    return;
}

// 获取按键当前多击次数  OK
void tdl_button_get_repeat_cnt(TDL_BUTTON_HANDLE handle, uint8_t *count)
{
    TDL_BUTTON_LIST_NODE_T *p_node = NULL;

    p_node = __tdl_button_find_node(handle);
    if (NULL != p_node) {
        *count = p_node->device_data.repeat;
    }
    return;
}
#endif

// 设置单个按键回调函数  OK
void tdl_button_event_register(TDL_BUTTON_HANDLE handle, TDL_BUTTON_TOUCH_EVENT_E event, TDL_BUTTON_EVENT_CB cb)
{
    TDL_BUTTON_LIST_NODE_T *p_node = NULL;

    if (event >= TDL_BUTTON_PRESS_MAX) {
        PR_ERR("event is illegal");
        return;
    }

    p_node = __tdl_button_find_node(handle);
    if (NULL != p_node) {
        p_node->user_data.list_cb[event] = cb;
    }
    return;
}

#if (COMBINE_BUTTON_ENABLE == 1)
// 设置组合按键回调函数  OK
void tdl_button_set_combine_cb(TDL_BUTTON_HANDLE handle, TDL_BUTTON_FUNC_CB cb)
{
    TDL_BUTTON_COMBINE_LIST_NODE_T *p_node = NULL;

    p_node = __tdl_button_find_combine_node(handle);
    if (NULL != p_node) {
        p_node->combine_cb = cb;
    }
    return;
}
#endif

#if (COMBINE_BUTTON_ENABLE == 1)
// 获取按键触发事件 OK
void tdl_button_get_event(TDL_BUTTON_HANDLE handle, TDL_BUTTON_TOUCH_EVENT_E *pre_event,
                          TDL_BUTTON_TOUCH_EVENT_E *now_event)
{
    TDL_BUTTON_LIST_NODE_T *p_node = NULL;

    p_node = __tdl_button_find_node(handle);
    if (NULL != p_node) {
        *pre_event = p_node->device_data.pre_event;
        *now_event = p_node->device_data.now_event;
    }
    return;
}
#endif

// 按键控制参数的注册
OPERATE_RET tdl_button_register(char *name, TDL_BUTTON_CTRL_INFO *button_ctrl_info,
                                TDL_BUTTON_DEVICE_INFO_T *button_cfg_info)
{
    OPERATE_RET ret = OPRT_COM_ERROR;
    TDL_BUTTON_LIST_NODE_T *p_node = NULL;

    if (NULL == button_ctrl_info) {
        return OPRT_INVALID_PARM;
    }

    if (NULL == button_cfg_info) {
        return OPRT_INVALID_PARM;
    }

    ret = __tdl_button_list_init();
    if (OPRT_OK != ret) {
        PR_ERR("tdl button list init err");
        return ret;
    }

    p_node = __tdl_button_add_node(name, button_ctrl_info, button_cfg_info);
    if (NULL != p_node) {
        return ret;
    }

    return ret;
}

/**
 * @brief set button task stack size
 *
 * @param[in] size stack size
 * @return OPRT_OK if successful
 */
OPERATE_RET tdl_button_set_task_stack_size(uint32_t size)
{
    sg_bt_task_stack_size = size;

    return OPRT_OK;
}

/**
 * @brief set button ready flag (sensor special use)
 *		 if ready flag is false, software will filter the trigger for the first time,
 *		 if use this func,please call after registered.
 *        [ready flag default value is false.]
 * @param[in] name button name
 * @param[in] status true or false
 * @return OPRT_OK if successful
 */
OPERATE_RET tdl_button_set_ready_flag(char *name, uint8_t status)
{
    TDL_BUTTON_LIST_NODE_T *p_node = NULL;

    // 比对名称是否存在
    p_node = __tdl_button_find_node_name(name);
    if (NULL == p_node) {
        PR_NOTICE("button no existence");
        return OPRT_NOT_FOUND;
    }

    p_node->device_data.ready = status;
    return OPRT_OK;
}

OPERATE_RET tdl_button_read_status(TDL_BUTTON_HANDLE handle, uint8_t *status)
{
    OPERATE_RET rt = OPRT_OK;
    TDL_BUTTON_LIST_NODE_T *p_node = NULL;
    TDL_BUTTON_OPRT_INFO button_oprt;

    TUYA_CHECK_NULL_RETURN(handle, OPRT_INVALID_PARM);
    TUYA_CHECK_NULL_RETURN(status, OPRT_INVALID_PARM);

    p_node = __tdl_button_find_node(handle);
    TUYA_CHECK_NULL_RETURN(p_node, OPRT_COM_ERROR);

    TUYA_CALL_ERR_RETURN(__tdl_get_operate_info(p_node, &button_oprt));

    TUYA_CALL_ERR_RETURN(p_node->device_data.ctrl_info.read_value(&button_oprt, status));

    return rt;
}

OPERATE_RET tdl_button_set_level(TDL_BUTTON_HANDLE handle, TUYA_GPIO_LEVEL_E level)
{
    OPERATE_RET rt = OPRT_OK;
    TDL_BUTTON_LIST_NODE_T *p_node = NULL;
    TDL_BUTTON_OPRT_INFO button_oprt;

    TUYA_CHECK_NULL_RETURN(handle, OPRT_INVALID_PARM);

    p_node = __tdl_button_find_node(handle);
    TUYA_CHECK_NULL_RETURN(p_node, OPRT_COM_ERROR);

    TUYA_CALL_ERR_RETURN(__tdl_get_operate_info(p_node, &button_oprt));

    TUYA_CALL_ERR_RETURN(tdd_gpio_button_update_level(button_oprt.dev_handle, level));

    return rt;
}

OPERATE_RET tdl_button_set_scan_time(uint8_t time_ms)
{
    if (time_ms < TDL_BUTTON_SCAN_TIME)
        return OPRT_INVALID_PARM;
    tdl_button_scan_time = time_ms;
    tdl_button_local.irq_scan_cnt = TDL_BUTTON_IRQ_SCAN_TIME / time_ms;
    return OPRT_OK;
}
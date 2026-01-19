
/**
 * @file tdl_button_manage.c
 * @brief Implementation of Tuya Driver Layer button management system.
 *
 * This file implements the button management functionality for the Tuya Driver
 * Layer (TDL). It provides a sophisticated button event detection system that
 * supports various button interaction patterns including single click, double
 * click, multiple clicks, and long press detection. The implementation uses
 * timer-based scanning and interrupt handling to detect button state changes
 * with configurable debouncing and timing parameters.
 *
 * Key features implemented:
 * - State machine-based button event detection
 * - Configurable debouncing for reliable button state reading
 * - Support for both timer-based scanning and interrupt-driven modes
 * - Multiple simultaneous button management using linked lists
 * - Thread-safe operation with mutex protection
 * - Event callback system for button state notifications
 * - Power-on button state recovery detection
 * - Efficient resource management with dynamic allocation
 *
 * The implementation creates dedicated tasks for button scanning and provides
 * a unified interface for various button hardware implementations through the
 * TDD (Tuya Device Driver) layer abstraction.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 *
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
#include "tdd_button_gpio.h"

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
#define TOUCH_DELAY                500 // Interval time 500ms for single/double click recognition
#define PUT_EVENT_CB(btn, name, ev, arg)                                                                               \
    do {                                                                                                               \
        if (btn.list_cb[ev])                                                                                           \
            btn.list_cb[ev](name, ev, arg);                                                                            \
    } while (0)


#if defined(BUTTION_STACK_SIZE) && (BUTTION_STACK_SIZE > 0)    
#define TDL_BUTTON_TASK_STACK_SIZE BUTTION_STACK_SIZE
#else
#define TDL_BUTTON_TASK_STACK_SIZE 2048
#endif

/***********************************************************
***********************typedef define***********************
***********************************************************/
typedef struct {
    LIST_HEAD hdr; /* list head */
} TDL_BUTTON_LIST_HEAD_T;

typedef struct {
    TDL_BUTTON_MODE_E button_mode; // Button driver mode: scan, interrupt
} TDL_BUTTON_HARDWARE_CFG_T;

typedef struct {
    uint8_t pre_event : 4; // Previous event
    uint8_t now_event : 4; // Currently generated event
    uint8_t flag : 3;      // Button processing flow state
    uint8_t debounce_cnt;  // Debounce count
    uint16_t ticks;        // Press and hold count
    uint8_t status;        // Actual button status
    uint8_t repeat;        // Repeat press count
    uint8_t ready;         // Flag indicating if the button is ready after power-on
    uint8_t init_flag;     // Button initialized successfully

    TDL_BUTTON_CTRL_INFO ctrl_info;    // Driver mount information
    DEVICE_BUTTON_HANDLE dev_handle;   // Driver handle
    TDL_BUTTON_HARDWARE_CFG_T dev_cfg; // Hardware configuration
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
} TDL_BUTTON_LIST_NODE_T;             // Single button node

#if (COMBINE_BUTTON_ENABLE == 1)
typedef struct {
    LIST_HEAD hdr;                 /* list node */
    char *name;                    /* node name */
    TDL_BUTTON_FUNC_CB combine_cb; /*combine button cb*/
} TDL_BUTTON_COMBINE_LIST_NODE_T;  // Combination button node
#endif

typedef struct {
    uint8_t scan_task_flag;   /*Scan thread flag*/
    uint8_t irq_task_flag;    /*Interrupt thread flag*/
    uint8_t task_mode;        /*Thread type*/
    SEM_HANDLE irq_semaphore; /*Interrupt semaphore*/
    uint32_t irq_scan_cnt;    /*Interrupt thread disconnection count*/
    MUTEX_HANDLE mutex;       /*Mutex lock*/
} TDL_BUTTON_LOCAL_T;         // TDL local parameters

/***********************************************************
***********************variable define**********************
***********************************************************/
TDL_BUTTON_LOCAL_T tdl_button_local = {.irq_task_flag = FALSE,
                                       .scan_task_flag = FALSE,
                                       .task_mode = FALSE,
                                       .irq_semaphore = NULL,
                                       .irq_scan_cnt = TDL_BUTTON_IRQ_SCAN_TIME / TDL_BUTTON_SCAN_TIME,
                                       .mutex = NULL};

THREAD_HANDLE scan_thread_handle = NULL; // Scan thread handle
THREAD_HANDLE irq_thread_handle = NULL;  // Interrupt scan thread handle

TDL_BUTTON_LIST_HEAD_T *p_button_list = NULL; // Single button list head
// TDL_BUTTON_LIST_HEAD_T *p_combine_button_list = NULL;//Combination button list head

static uint8_t g_tdl_button_list_exist = FALSE; // Single button list head initialization flag
// static uint8_t g_tdl_combine_button_list_exist = FALSE;//Combination button list head initialization flag
static uint8_t g_tdl_button_scan_mode_exist = 0xFF;
static uint32_t sg_bt_task_stack_size = TDL_BUTTON_TASK_STACK_SIZE;
static uint8_t tdl_button_scan_time = TDL_BUTTON_SCAN_TIME;

/***********************************************************
***********************function define**********************
***********************************************************/
static OPERATE_RET __tdl_get_operate_info(TDL_BUTTON_LIST_NODE_T *p_node, TDL_BUTTON_OPRT_INFO *oprt_info);
static OPERATE_RET __tdl_button_scan_task(uint8_t enable);
static OPERATE_RET __tdl_button_irq_task(uint8_t enable);

// Generate single button list head
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
// Generate combination button list head
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

// Find button node by handle
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
            // Address comparison successful
            return p_node;
        }
    }
    return NULL;
}

#if (COMBINE_BUTTON_ENABLE == 1)
// Find combination button node by handle
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
            // Address comparison successful
            return p_node;
        }
    }
    return NULL;
}
#endif

// Find button node by name
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
            // Name comparison successful
            return p_node;
        }
    }
    return NULL;
}

#if (COMBINE_BUTTON_ENABLE == 1)
// Find combination button node by name
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
            // Name comparison successful
            return p_node;
        }
    }
    return NULL;
}
#endif

// Add a new node: create a node and store driver control information
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

    // Check if the name exists
    if (__tdl_button_find_node_name(name) != NULL) {
        PR_NOTICE("button name existence");
        return NULL; // return OPRT_COM_ERROR;
    }

    // Create new node
    p_node = (TDL_BUTTON_LIST_NODE_T *)tal_malloc(sizeof(TDL_BUTTON_LIST_NODE_T));
    if (NULL == p_node) {
        return NULL; // return OPRT_MALLOC_FAILED;
    }
    memset(p_node, 0, sizeof(TDL_BUTTON_LIST_NODE_T));

    // Create new name
    p_node->name = (char *)tal_malloc(TDL_BUTTON_NAME_LEN);
    if (NULL == p_node->name) {
        tal_free(p_node);
        p_node = NULL;
        return NULL; // return OPRT_MALLOC_FAILED;
    }
    memset(p_node->name, 0, TDL_BUTTON_NAME_LEN);

    // Store the name
    name_len = strlen(name);
    if (name_len >= TDL_BUTTON_NAME_LEN) {
        name_len = TDL_BUTTON_NAME_LEN;
    }

    memcpy(p_node->name, name, name_len);
    memcpy(&(p_node->device_data.ctrl_info), info, sizeof(TDL_BUTTON_CTRL_INFO));
    p_node->device_data.dev_cfg.button_mode = cfg->mode;
    p_node->device_data.dev_handle = cfg->dev_handle;

    // Add new node
    tal_mutex_lock(tdl_button_local.mutex);
    tuya_list_add(&p_node->hdr, &p_head->hdr);
    tal_mutex_unlock(tdl_button_local.mutex);

    return p_node;
}

// Update node user data: data content is fixed as user data
static TDL_BUTTON_LIST_NODE_T *__button_updata_userdata(char *name, TDL_BUTTON_CFG_T *button_cfg)
{
    TDL_BUTTON_LIST_NODE_T *p_node = NULL;

    // Check if the name exists
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

// Button scan state machine: generates button trigger events
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
            /*Trigger press down event*/
            /*Trigger press down event*/
            p_node->device_data.ticks = 0;
            p_node->device_data.repeat = 1;
            p_node->device_data.flag = 1;
            p_node->device_data.pre_event = p_node->device_data.now_event;
            p_node->device_data.now_event = TDL_BUTTON_PRESS_DOWN;
            PUT_EVENT_CB(p_node->user_data, p_node->name, TDL_BUTTON_PRESS_DOWN,
                         (void *)((uint32_t)p_node->device_data.repeat));

        } else {
            p_node->device_data.pre_event = p_node->device_data.now_event;
            p_node->device_data.now_event = TDL_BUTTON_PRESS_NONE; // Default state is no, no callback execution
        }
    } break;

    case 1: {
        // PR_NOTICE("case1:tick=%d",p_node->device_data.ticks);
        if (p_node->device_data.status != 0) {
            if (p_node->device_data.dev_cfg.button_mode == BUTTON_IRQ_MODE) {
                tdl_button_local.irq_scan_cnt = 0;
            }
            if (p_node->user_data.button_cfg.long_start_valid_time == 0) {
                // Long press valid time is 0, do not execute long press
                // Long press valid time is 0, do not execute long press
                p_node->device_data.pre_event = p_node->device_data.now_event;
            } else if (p_node->device_data.ticks >
                       (p_node->user_data.button_cfg.long_start_valid_time / tdl_button_scan_time)) {
                /*Trigger long press start event*/
                // PR_NOTICE("long tick =%d",p_node->device_data.ticks);
                p_node->device_data.pre_event = p_node->device_data.now_event;
                p_node->device_data.now_event = TDL_BUTTON_LONG_PRESS_START;
                PUT_EVENT_CB(p_node->user_data, p_node->name, TDL_BUTTON_LONG_PRESS_START,
                             (void *)((uint32_t)p_node->device_data.ticks * tdl_button_scan_time));
                p_node->device_data.flag = 5;
            } else {
                // First press, holding, has not reached the long press start event, update the front and back status in
                // time First press, holding, has not reached the long press start event, update the front and back
                // status in time
                p_node->device_data.pre_event = p_node->device_data.now_event;
            }
        } else {
            /*Trigger release event*/
            /*Trigger release event*/
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
                /*Release timeout triggers single click*/
                /*Release timeout triggers single click*/
                if (p_node->device_data.repeat == 1) {
                    p_node->device_data.pre_event = p_node->device_data.now_event;
                    p_node->device_data.now_event = TDL_BUTTON_PRESS_SINGLE_CLICK;
                    PUT_EVENT_CB(p_node->user_data, p_node->name, TDL_BUTTON_PRESS_SINGLE_CLICK,
                                 (void *)((uint32_t)p_node->device_data.repeat));
                } else if (p_node->device_data.repeat == 2) {
                    /*Release triggers double click event*/
                    /*Release triggers double click event*/
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
                // Not timed out after release, update front and back status in time
                // Not timed out after release, update front and back status in time
                p_node->device_data.pre_event = p_node->device_data.now_event;
            }
        }
    } break;

    case 3: {
        uint16_t repeat_tick = 0;
        // PR_NOTICE("case3:tick=%d",p_node->device_data.ticks);
        /*repeat up*/
        // Released after more than one press
        if (p_node->device_data.status == 0) {
            p_node->device_data.pre_event = p_node->device_data.now_event;
            p_node->device_data.now_event = TDL_BUTTON_PRESS_UP;
            PUT_EVENT_CB(p_node->user_data, p_node->name, TDL_BUTTON_PRESS_UP,
                         (void *)((uint32_t)p_node->device_data.repeat));
            repeat_tick = p_node->user_data.button_cfg.button_repeat_valid_time / tdl_button_scan_time;
            if (p_node->device_data.ticks >= repeat_tick) {
                // Timeout after release, double-click uses default interval, multiple-click uses user-configured
                // interval Timeout after release, double-click uses default interval, multiple-click uses
                // user-configured interval PR_NOTICE("3: tick=%d",p_node->device_data.ticks);
                // PR_NOTICE("%d",repeat_tick);
                p_node->device_data.flag = 0;
            } else {
                p_node->device_data.flag = 2;
                p_node->device_data.ticks = 0;
            }
        } else {
            // More than one press, holding, update front and back status in time
            // More than one press, holding, update front and back status in time
            p_node->device_data.pre_event = p_node->device_data.now_event;
        }

    } break;

    case 5: {
        if (p_node->device_data.status != 0) {
            /*Trigger long press hold event*/
            /*Trigger long press hold event*/
            if (p_node->device_data.dev_cfg.button_mode == BUTTON_IRQ_MODE) {
                tdl_button_local.irq_scan_cnt = 0;
            }
            hold_tick = p_node->user_data.button_cfg.long_keep_timer / tdl_button_scan_time;
            if (hold_tick == 0) {
                hold_tick = 1;
            }
            if (p_node->device_data.ticks >= hold_tick) {
                // If the count is greater than the hold count, refresh the status immediately
                // If the count is greater than the hold count, refresh the status immediately
                p_node->device_data.pre_event = p_node->device_data.now_event;
                p_node->device_data.now_event = TDL_BUTTON_LONG_PRESS_HOLD;
                if (p_node->device_data.ticks % hold_tick == 0) {
                    // Execute only when it is confirmed that it is an integer multiple of hold
                    // Execute only when it is confirmed that it is an integer multiple of hold
                    // PR_NOTICE("hold,tick=%d",hold_tick);
                    PUT_EVENT_CB(p_node->user_data, p_node->name, TDL_BUTTON_LONG_PRESS_HOLD,
                                 (void *)((uint32_t)p_node->device_data.ticks * tdl_button_scan_time));
                }
            }
        } else {
            /*hold release*/
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

// Button interrupt callback function
static void __tdl_button_irq_cb(void *arg)
{
    if (tdl_button_local.irq_scan_cnt >= TDL_BUTTON_IRQ_SCAN_CNT) {
        tal_semaphore_post(tdl_button_local.irq_semaphore);
    }
    return;
}

// Get the information to be passed to the TDD layer
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

// Create a single button and return the handle for user use
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

    // Update user data of a node
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

    // Pass out the handle
    // Pass out the handle
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
// Create a combination key: return a handle for user use
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

    // Create new node
    // Create new node
    p_node = (TDL_BUTTON_COMBINE_LIST_NODE_T *)tal_malloc(sizeof(TDL_BUTTON_COMBINE_LIST_NODE_T));
    if (NULL == p_node) {
        return OPRT_MALLOC_FAILED;
    }
    memset(p_node, 0, sizeof(TDL_BUTTON_COMBINE_LIST_NODE_T));

    // Create new name
    // Create new name
    p_node->name = (char *)tal_malloc(TDL_BUTTON_NAME_LEN);
    if (NULL == p_node->name) {
        tal_free(p_node);
        p_node = NULL;
        return OPRT_MALLOC_FAILED;
    }
    memset(p_node->name, 0, TDL_BUTTON_NAME_LEN);

    // Store the name
    // Store the name
    name_len = strlen(name);
    if (name_len >= TDL_BUTTON_NAME_LEN) {
        name_len = TDL_BUTTON_NAME_LEN;
    }
    memcpy(p_node->name, name, name_len);

    // Add new node
    // Add new node
    tal_mutex_lock(tdl_button_local.mutex);
    tuya_list_add(&(p_node->hdr), &(p_combine_button_list->hdr));
    tal_mutex_unlock(tdl_button_local.mutex);

    *p_handle = (TDL_BUTTON_HANDLE)p_node;
    PR_NOTICE("tdl_combine_button_create succ");
    return OPRT_OK;
}
#endif

// Delete a single button: for user deletion
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

        tal_free(p_node); // Release node
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
// Delete combination key
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

        tal_free(p_node); // Release node
        p_node = NULL;
        return OPRT_OK;
    }
    return OPRT_COM_ERROR;
}
#endif

// Button flow: read, debounce, generate event
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

    // In scan mode, a long press at power-on may trigger a short press. Added a 'ready' state to prevent this. This is
    // not an issue in interrupt mode, so the 'ready' state is not needed.
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

    if (status != p_node->device_data.status) { // Button status changed, perform debouncing
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

// Button scan task: single button, combination button
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
        // Combination key callback execution
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

// Button interrupt scan task
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
            // Combination key callback execution
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
            // Interrupt disconnection count check
            if (++tdl_button_local.irq_scan_cnt >= TDL_BUTTON_IRQ_SCAN_CNT) {
                break;
            } else {
                tal_system_sleep(tdl_button_scan_time);
            }
        }
    }
}

// Enable and disable button scan task
static OPERATE_RET __tdl_button_scan_task(uint8_t enable)
{
    OPERATE_RET ret = OPRT_COM_ERROR;

    if (tdl_button_local.task_mode & BUTTON_SCAN_TASK) {
        if (enable != 0) {
            // Create scan task
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
            // Close scan
            tal_thread_delete(scan_thread_handle);
            tdl_button_local.scan_task_flag = FALSE;
        }
    }
    return OPRT_OK;
}

// Enable and disable button scan task
static OPERATE_RET __tdl_button_irq_task(uint8_t enable)
{
    OPERATE_RET ret = OPRT_COM_ERROR;
    if (tdl_button_local.task_mode & BUTTON_IRQ_TASK) {
        if (enable != 0) {
            // Create interrupt scan task
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
            // Close interrupt scan
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
// Set button long press start valid time  OK
// Set button long press start valid time  OK
void tdl_button_set_long_valid_time(TDL_BUTTON_HANDLE handle, uint16_t long_start_time)
{
    TDL_BUTTON_LIST_NODE_T *p_node = NULL;

    p_node = __tdl_button_find_node(handle);
    if (NULL != p_node) {
        p_node->user_data.button_cfg.long_start_valid_time = long_start_time;
    }
    return;
}

// Get current long press time  OK
// Get current long press time  OK
void tdl_button_get_current_long_press_time(TDL_BUTTON_HANDLE handle, uint16_t *long_start_time)
{
    TDL_BUTTON_LIST_NODE_T *p_node = NULL;

    p_node = __tdl_button_find_node(handle);
    if (NULL != p_node) {
        *long_start_time = p_node->device_data.ticks * tdl_button_scan_time;
    }
    return;
}

// Set button repeat count  OK
// Set button repeat count  OK
void tdl_button_set_repeat_cnt(TDL_BUTTON_HANDLE handle, uint8_t count)
{
    TDL_BUTTON_LIST_NODE_T *p_node = NULL;

    p_node = __tdl_button_find_node(handle);
    if (NULL != p_node) {
        p_node->user_data.button_cfg.button_repeat_valid_count = count;
    }
    return;
}

// Get button current repeat count  OK
// Get button current repeat count  OK
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

// Set single button event callback  OK
// Set single button event callback  OK
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
// Set combination button callback  OK
// Set combination button callback  OK
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
// Get button trigger event OK
// Get button trigger event OK
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

// Button control parameter registration
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

    // Check if the name exists
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

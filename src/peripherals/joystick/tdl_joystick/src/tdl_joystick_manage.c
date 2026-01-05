/**
 * @file tdl_joystick_manage.c
 * @brief Joystick management module, provides base timer/semaphore/task functions
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 *
 * @date 2025-07-08     maidang      Initial version
 */

#include "string.h"
#include "stdint.h"

#include "tal_semaphore.h"
#include "tal_mutex.h"
#include "tal_system.h"
#include "tal_memory.h"
#include "tal_log.h"
#include "tal_thread.h"
#include "tuya_list.h"

#include "tdl_joystick_driver.h"
#include "tdl_joystick_manage.h"
#include "tdd_joystick.h"
#include "tkl_adc.h"

/***********************************************************
*************************micro define***********************
***********************************************************/
#define COMBINE_JOYSTICK_ENABLE 0

#define JOYSTICK_SCAN_TASK 0x01
#define JOYSTICK_IRQ_TASK  0x02

#define TDL_JOYSTICK_NAME_LEN        32    // button name max len 32byte
#define TDL_LONG_START_VAILD_TIMER   1500  // ms
#define TDL_LONG_KEEP_TIMER          100   // ms
#define TDL_JOYSTICK_DEBOUNCE_TIME   60    // ms
#define TDL_JOYSTICK_IRQ_SCAN_TIME   10000 // ms
#define TDL_JOYSTICK_SCAN_TIME       20    // 10ms
#define TDL_JOYSTICK_IRQ_SCAN_CNT    (TDL_JOYSTICK_IRQ_SCAN_TIME / TDL_JOYSTICK_SCAN_TIME)
#define TOUCH_DELAY                  500 // Click interval for single/double click differentiation
#define TDL_JOYSTICK_TASK_STACK_SIZE (4096)
#define PUT_EVENT_CB(btn, name, ev, arg)                                                                               \
    do {                                                                                                               \
        if (btn.list_cb[ev])                                                                                           \
            btn.list_cb[ev](name, ev, arg);                                                                            \
    } while (0)

/***********************************************************
***********************typedef define***********************
***********************************************************/
typedef struct {
    LIST_HEAD hdr; /* list head */
} TDL_JOYSTICK_LIST_HEAD_T;

typedef struct {
    TDL_JOYSTICK_MODE_E stick_mode; /* button working mode */
    TUYA_ADC_NUM_E adc_num;         /* adc num */
    uint8_t adc_ch_x;
    uint8_t adc_ch_y;
} TDL_JOYSTICK_HARDWARE_CFG_T;

typedef struct {
    uint8_t pre_event;      /* previous event */
    uint8_t now_event;      /* current event */
    uint8_t flag : 3;       /* button processing status */
    uint8_t debounce_cnt;   /* debounce time conversion count */
    uint16_t ticks;         /* press hold count */
    uint8_t status;         /* button actual status */
    uint8_t repeat;         /* repeat press count */
    uint8_t ready;          /* button power ready status */
    uint8_t init_flag;      /* button initialization success */
    uint8_t last_direction; /* last direction event */

    TDL_JOYSTICK_CTRL_INFO ctrl_info;    /* joystick control info */
    TDL_JOYSTICK_DEV_HANDLE dev_handle;  /* joystick device handle */
    TDL_JOYSTICK_HARDWARE_CFG_T dev_cfg; /* joystick hardware config */
} JOYSTICK_DRIVER_DATA_T;

typedef struct {
    TDL_JOYSTICK_CFG_T joystick_cfg;                             /*joystick data*/
    TDL_JOYSTICK_EVENT_CB list_cb[TDL_JOYSTICK_TOUCH_EVENT_MAX]; /*joystick cb*/
} JOYSTICK_USER_DATA_T;

typedef struct {
    LIST_HEAD hdr;                      /* list node */
    char *name;                         /* node name */
    MUTEX_HANDLE joystick_mutex;        /* mutex for joystick */
    JOYSTICK_USER_DATA_T user_data;     /* user data */
    JOYSTICK_DRIVER_DATA_T device_data; /* driver data */
} TDL_JOYSTICK_LIST_NODE_T;             /* TDL joystick list node */

typedef struct {
    uint8_t scan_task_flag;   /* scan task flag */
    uint8_t irq_task_flag;    /* irq task flag */
    uint8_t task_mode;        /* task mode */
    SEM_HANDLE irq_semaphore; /* irq semaphore */
    uint32_t irq_scan_cnt;    /* irq scan cnt */
    MUTEX_HANDLE mutex;       /* mutex */
} TDL_JOYSTICK_LOCAL_T;       /* TDL joystick local parameters */

/***********************************************************
***********************variable define**********************
***********************************************************/
TDL_JOYSTICK_LOCAL_T tdl_joystick_local = {.irq_task_flag = FALSE,
                                           .scan_task_flag = FALSE,
                                           .task_mode = FALSE,
                                           .irq_semaphore = NULL,
                                           .irq_scan_cnt = TDL_JOYSTICK_IRQ_SCAN_TIME / TDL_JOYSTICK_SCAN_TIME,
                                           .mutex = NULL};

THREAD_HANDLE stick_scan_thread_handle = NULL;    /* scan thread handle */
THREAD_HANDLE stick_irq_thread_handle = NULL;     /* irq thread handle */
TDL_JOYSTICK_LIST_HEAD_T *p_joystick_list = NULL; /* joystick list head */

static uint8_t g_tdl_joystick_list_exist = FALSE;                           /* joystick list head init flag */
static uint8_t g_tdl_joystick_scan_mode_exist = 0xFF;                       /* joystick scan mode init flag */
static uint32_t sg_joystick_task_stack_size = TDL_JOYSTICK_TASK_STACK_SIZE; /* joystick task stack size */
static uint8_t tdl_joystick_scan_time = TDL_JOYSTICK_SCAN_TIME;             /* joystick scan time */
static uint32_t joystick_ticks = 0;
/***********************************************************
***********************function define**********************
***********************************************************/
static OPERATE_RET __tdl_get_operate_info(TDL_JOYSTICK_LIST_NODE_T *p_node, TDL_JOYSTICK_OPRT_INFO *oprt_info);
static OPERATE_RET __tdl_joystick_scan_task(uint8_t enable);
static OPERATE_RET __tdl_joystick_irq_task(uint8_t enable);
OPERATE_RET tdl_joystick_calibrated_xy(TDL_JOYSTICK_HANDLE handle, int *x, int *y);
OPERATE_RET tdl_joystick_raw_xy(TDL_JOYSTICK_HANDLE handle, int channel_x, int channel_y, int *x, int *y);

/**
 * @brief Initialize joystick linked list structure
 * @return Operation result, OPRT_OK indicates success
 */
static OPERATE_RET __tdl_joystick_list_init(void)
{
    if (g_tdl_joystick_list_exist == FALSE) {
        /* Allocate memory for joystick list head */
        p_joystick_list = (TDL_JOYSTICK_LIST_HEAD_T *)tal_malloc(sizeof(TDL_JOYSTICK_LIST_HEAD_T));
        if (NULL == p_joystick_list) {
            return OPRT_MALLOC_FAILED;
        }

        /* Create semaphore for IRQ synchronization */
        if (tal_semaphore_create_init(&tdl_joystick_local.irq_semaphore, 0, 1) != 0) {
            PR_ERR("tdl_joystick_semaphore_init err");
            return OPRT_COM_ERROR;
        }

        /* Create mutex for thread-safe operations */
        if (tal_mutex_create_init(&tdl_joystick_local.mutex) != 0) {
            PR_ERR("tdl_joystick_mutex_init err");
            return OPRT_COM_ERROR;
        }

        /* Initialize list header */
        INIT_LIST_HEAD(&p_joystick_list->hdr);
        g_tdl_joystick_list_exist = TRUE; /* Mark list initialization completed */
    }
    return OPRT_OK;
}

/**
 * @brief Find joystick node by handle in the linked list
 * @param[in] handle Joystick handle to search for
 * @return Pointer to found node, NULL if not found
 */
static TDL_JOYSTICK_LIST_NODE_T *__tdl_joystick_find_node(TDL_JOYSTICK_HANDLE handle)
{
    TDL_JOYSTICK_LIST_HEAD_T *p_head = p_joystick_list; /* Pointer to joystick list head */
    TDL_JOYSTICK_LIST_NODE_T *p_node = NULL;            /* Current node pointer */
    LIST_HEAD *pos = NULL;                              /* Position pointer for traversal */

    /* Validate list head */
    if (NULL == p_head) {
        PR_ERR("__tdl_joystick_find_node err");
        return NULL;
    }

    /* Traverse the linked list */
    tuya_list_for_each(pos, &p_head->hdr)
    {
        /* Get node structure from list entry */
        p_node = tuya_list_entry(pos, TDL_JOYSTICK_LIST_NODE_T, hdr);

        /* Compare node address with target handle */
        if (p_node == handle) {
            return p_node; /* Return matched node */
        }
    }

    return NULL; /* Return NULL if no match found */
}

/**
 * @brief Find a joystick node by its name.
 * @param[in] name The name of the node to search for.
 * @return Pointer to the matched node, or NULL if not found.
 */
static TDL_JOYSTICK_LIST_NODE_T *__tdl_joystick_find_node_name(char *name)
{
    TDL_JOYSTICK_LIST_HEAD_T *p_head = p_joystick_list;
    TDL_JOYSTICK_LIST_NODE_T *p_node = NULL;
    LIST_HEAD *pos = NULL;

    if (NULL == p_head) {
        PR_ERR("__tdl_joystick_find_node_name err");
        return NULL;
    }
    tuya_list_for_each(pos, &p_head->hdr)
    {
        p_node = tuya_list_entry(pos, TDL_JOYSTICK_LIST_NODE_T, hdr);
        if (strcmp(name, p_node->name) == 0) {
            return p_node;
        }
    }
    return NULL;
}

/**
 * @brief Add a new joystick node and store driver control information.
 * @param[in] name The name of the node.
 * @param[in] info Pointer to the control information structure.
 * @param[in] cfg Pointer to the device hardware configuration structure.
 * @return Pointer to the newly created node, or NULL on failure.
 */
static TDL_JOYSTICK_LIST_NODE_T *__tdl_joystick_add_node(char *name, TDL_JOYSTICK_CTRL_INFO *info,
                                                         TDL_JOYSTICK_DEVICE_INFO_T *cfg)
{
    TDL_JOYSTICK_LIST_HEAD_T *p_head = p_joystick_list;
    TDL_JOYSTICK_LIST_NODE_T *p_node = NULL;
    uint8_t name_len = 0;

    if (NULL == p_head) {
        PR_ERR("__tdl_joystick_add_node err");
        return NULL;
    }

    if (NULL == info) {
        return NULL; // return OPRT_INVALID_PARM;
    }

    if (NULL == cfg) {
        return NULL; // return OPRT_INVALID_PARM;
    }

    // Check if joystick name already exists
    if (__tdl_joystick_find_node_name(name) != NULL) {
        PR_NOTICE("joystick name existence");
        return NULL; // return OPRT_COM_ERROR;
    }

    // Create new node
    p_node = (TDL_JOYSTICK_LIST_NODE_T *)tal_malloc(sizeof(TDL_JOYSTICK_LIST_NODE_T));
    if (NULL == p_node) {
        return NULL; // return OPRT_MALLOC_FAILED;
    }
    memset(p_node, 0, sizeof(TDL_JOYSTICK_LIST_NODE_T));

    // Create new name
    p_node->name = (char *)tal_malloc(TDL_JOYSTICK_NAME_LEN);
    if (NULL == p_node->name) {
        tal_free(p_node);
        p_node = NULL;
        return NULL; // return OPRT_MALLOC_FAILED;
    }
    memset(p_node->name, 0, TDL_JOYSTICK_NAME_LEN);

    // Store name
    name_len = strlen(name);
    if (name_len >= TDL_JOYSTICK_NAME_LEN) {
        name_len = TDL_JOYSTICK_NAME_LEN;
    }

    memcpy(p_node->name, name, name_len);
    memcpy(&(p_node->device_data.ctrl_info), info, sizeof(TDL_JOYSTICK_CTRL_INFO));
    p_node->device_data.dev_cfg.stick_mode = cfg->mode;
    p_node->device_data.dev_cfg.adc_num = cfg->adc_num;
    p_node->device_data.dev_cfg.adc_ch_x = cfg->adc_ch_x;
    p_node->device_data.dev_cfg.adc_ch_y = cfg->adc_ch_y;
    p_node->device_data.dev_handle = cfg->dev_handle;

    // Add new node
    tal_mutex_lock(tdl_joystick_local.mutex);
    tuya_list_add(&p_node->hdr, &p_head->hdr);
    tal_mutex_unlock(tdl_joystick_local.mutex);

    return p_node;
}

/**
 * @brief Update the user data of a node.
 * @param[in] name The name of the node.
 * @param[in] joystick_cfg Pointer to the user configuration structure.
 * @return Pointer to the matched node, or NULL if not found.
 */
static TDL_JOYSTICK_LIST_NODE_T *__tdl_joystick_updata_userdata(char *name, TDL_JOYSTICK_CFG_T *joystick_cfg)
{
    TDL_JOYSTICK_LIST_NODE_T *p_node = NULL;

    // Check if joystick name exists
    p_node = __tdl_joystick_find_node_name(name);
    if (NULL == p_node) {
        PR_NOTICE("button no existence");
        return NULL;
    }

    // Update joystick configuration
    if (NULL == joystick_cfg) {
        PR_NOTICE("user joystick_cfg NULL");
        p_node->user_data.joystick_cfg.button_cfg.long_start_valid_time = TDL_LONG_START_VAILD_TIMER;
        p_node->user_data.joystick_cfg.button_cfg.long_keep_timer = TDL_LONG_KEEP_TIMER;
        p_node->user_data.joystick_cfg.button_cfg.button_debounce_time = TDL_JOYSTICK_DEBOUNCE_TIME;
    } else {
        p_node->user_data.joystick_cfg.button_cfg.long_start_valid_time =
            joystick_cfg->button_cfg.long_start_valid_time;
        p_node->user_data.joystick_cfg.button_cfg.long_keep_timer = joystick_cfg->button_cfg.long_keep_timer;
        p_node->user_data.joystick_cfg.button_cfg.button_debounce_time = joystick_cfg->button_cfg.button_debounce_time;
        p_node->user_data.joystick_cfg.button_cfg.button_repeat_valid_time =
            joystick_cfg->button_cfg.button_repeat_valid_time;
        p_node->user_data.joystick_cfg.button_cfg.button_repeat_valid_count =
            joystick_cfg->button_cfg.button_repeat_valid_count;
        p_node->user_data.joystick_cfg.adc_cfg.adc_max_val = joystick_cfg->adc_cfg.adc_max_val;
        p_node->user_data.joystick_cfg.adc_cfg.adc_min_val = joystick_cfg->adc_cfg.adc_min_val;
        p_node->user_data.joystick_cfg.adc_cfg.normalized_range = joystick_cfg->adc_cfg.normalized_range;
        p_node->user_data.joystick_cfg.adc_cfg.sensitivity = joystick_cfg->adc_cfg.sensitivity;
    }

    p_node->device_data.pre_event = TDL_JOYSTICK_TOUCH_EVENT_NONE;
    p_node->device_data.now_event = TDL_JOYSTICK_TOUCH_EVENT_NONE;
    p_node->device_data.last_direction = TDL_JOYSTICK_TOUCH_EVENT_NONE;

    return p_node;
}

void tdl_joystick_direction_event_proc(TDL_JOYSTICK_HANDLE handle)
{
    OPERATE_RET ret = OPRT_OK;
    TDL_JOYSTICK_LIST_NODE_T *p_node = NULL;
    int x = 0, y = 0;
    int threshold;
    TDL_JOYSTICK_TOUCH_EVENT_E current_direction = TDL_JOYSTICK_TOUCH_EVENT_NONE;

    p_node = __tdl_joystick_find_node(handle);
    if (NULL == p_node) {
        PR_ERR("handle not get");
        return;
    }

    ret = tdl_joystick_calibrated_xy(handle, &x, &y);
    if (ret != OPRT_OK) {
        return;
    }

    threshold = p_node->user_data.joystick_cfg.adc_cfg.sensitivity;
    if (x < -threshold) {
        current_direction = TDL_JOYSTICK_RIGHT;
    } else if (x > threshold) {
        current_direction = TDL_JOYSTICK_LEFT;
    } else if (y < -threshold) {
        current_direction = TDL_JOYSTICK_UP;
    } else if (y > threshold) {
        current_direction = TDL_JOYSTICK_DOWN;
    }

    if (current_direction != p_node->device_data.last_direction) {
        if (p_node->device_data.last_direction != TDL_JOYSTICK_TOUCH_EVENT_NONE) {
            if (TDL_LONG_START_VAILD_TIMER / tdl_joystick_scan_time / 30 < joystick_ticks &&
                joystick_ticks < (TDL_LONG_START_VAILD_TIMER / tdl_joystick_scan_time)) {
                PUT_EVENT_CB(p_node->user_data, p_node->name, p_node->device_data.last_direction, NULL);
            }
        }
        joystick_ticks = 0;
        p_node->device_data.last_direction = current_direction;
    } else {
        if (current_direction != TDL_JOYSTICK_TOUCH_EVENT_NONE) {
            joystick_ticks++;

            if (joystick_ticks == (TDL_LONG_START_VAILD_TIMER / tdl_joystick_scan_time)) {
                TDL_JOYSTICK_TOUCH_EVENT_E long_event = TDL_JOYSTICK_TOUCH_EVENT_NONE;
                switch (p_node->device_data.last_direction) {
                case TDL_JOYSTICK_UP:
                    long_event = TDL_JOYSTICK_LONG_UP;
                    break;
                case TDL_JOYSTICK_DOWN:
                    long_event = TDL_JOYSTICK_LONG_DOWN;
                    break;
                case TDL_JOYSTICK_LEFT:
                    long_event = TDL_JOYSTICK_LONG_LEFT;
                    break;
                case TDL_JOYSTICK_RIGHT:
                    long_event = TDL_JOYSTICK_LONG_RIGHT;
                    break;
                default:
                    break;
                }
                if (long_event != TDL_JOYSTICK_TOUCH_EVENT_NONE) {
                    PUT_EVENT_CB(p_node->user_data, p_node->name, long_event, NULL);
                }
            }
        } else {
            joystick_ticks = 0;
        }
    }
}

/**
 * @brief Joystick state machine to generate joystick trigger events.
 * @param[in] p_node Pointer to the joystick node.
 */
static void __tdl_joystick_state_handle(TDL_JOYSTICK_LIST_NODE_T *p_node)
{
    uint16_t hold_tick = 0;

    if (NULL == p_node) {
        return;
    }

    switch (p_node->device_data.flag) {
    case 0: {
        // PR_NOTICE("case0:tick=%d",p_node->device_data.ticks);
        if (p_node->device_data.status != 0) {
            if (p_node->device_data.dev_cfg.stick_mode == JOYSTICK_IRQ_MODE) {
                tdl_joystick_local.irq_scan_cnt = 0;
            }
            // press down
            p_node->device_data.ticks = 0;
            p_node->device_data.repeat = 1;
            p_node->device_data.flag = 1;
            p_node->device_data.pre_event = p_node->device_data.now_event;
            p_node->device_data.now_event = TDL_JOYSTICK_BUTTON_PRESS_DOWN;
            PUT_EVENT_CB(p_node->user_data, p_node->name, TDL_JOYSTICK_BUTTON_PRESS_DOWN,
                         (void *)((uint32_t)p_node->device_data.repeat));

        } else {
            p_node->device_data.pre_event = p_node->device_data.now_event;
            p_node->device_data.now_event = TDL_JOYSTICK_TOUCH_EVENT_NONE; // Default state is none, no callback needed
        }

    } break;

    case 1: {
        // PR_NOTICE("case1:tick=%d",p_node->device_data.ticks);
        if (p_node->device_data.status != 0) {
            if (p_node->device_data.dev_cfg.stick_mode == JOYSTICK_IRQ_MODE) {
                tdl_joystick_local.irq_scan_cnt = 0;
            }
            if (p_node->user_data.joystick_cfg.button_cfg.long_start_valid_time == 0) {
                // Long press valid time is 0, do not execute long press
                p_node->device_data.pre_event = p_node->device_data.now_event;
            } else if (p_node->device_data.ticks >
                       (p_node->user_data.joystick_cfg.button_cfg.long_start_valid_time / tdl_joystick_scan_time)) {
                // Long press start event is triggered
                // PR_NOTICE("long tick =%d",p_node->device_data.ticks);
                p_node->device_data.pre_event = p_node->device_data.now_event;
                p_node->device_data.now_event = TDL_BUTTON_LONG_PRESS_START;
                PUT_EVENT_CB(p_node->user_data, p_node->name, TDL_BUTTON_LONG_PRESS_START,
                             (void *)((uint32_t)p_node->device_data.ticks * tdl_joystick_scan_time));
                p_node->device_data.flag = 5;
            } else {
                // First press, holding down, not reaching long press start event, timely update front and back status
                p_node->device_data.pre_event = p_node->device_data.now_event;
            }
        } else {
            // Trigger release event
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
            // Press again
            if (p_node->device_data.dev_cfg.stick_mode == JOYSTICK_IRQ_MODE) {
                tdl_joystick_local.irq_scan_cnt = 0;
            }
            p_node->device_data.repeat++;
            p_node->device_data.pre_event = p_node->device_data.now_event;
            p_node->device_data.now_event = TDL_BUTTON_PRESS_DOWN;
            PUT_EVENT_CB(p_node->user_data, p_node->name, TDL_BUTTON_PRESS_DOWN,
                         (void *)((uint32_t)p_node->device_data.repeat));
            p_node->device_data.flag = 3;
        } else {
            // Release timeout
            if (p_node->device_data.ticks >=
                (p_node->user_data.joystick_cfg.button_cfg.button_repeat_valid_time / tdl_joystick_scan_time)) {
                // Release timeout triggers single click
                if (p_node->device_data.repeat == 1) {
                    p_node->device_data.pre_event = p_node->device_data.now_event;
                    p_node->device_data.now_event = TDL_BUTTON_PRESS_SINGLE_CLICK;
                    PUT_EVENT_CB(p_node->user_data, p_node->name, TDL_BUTTON_PRESS_SINGLE_CLICK,
                                 (void *)((uint32_t)p_node->device_data.repeat));
                } else if (p_node->device_data.repeat == 2) {
                    // Release triggers double click event
                    p_node->device_data.pre_event = p_node->device_data.now_event;
                    p_node->device_data.now_event = TDL_BUTTON_PRESS_DOUBLE_CLICK;
                    PUT_EVENT_CB(p_node->user_data, p_node->name, TDL_BUTTON_PRESS_DOUBLE_CLICK,
                                 (void *)((uint32_t)p_node->device_data.repeat));
                } else if (p_node->device_data.repeat ==
                           p_node->user_data.joystick_cfg.button_cfg.button_repeat_valid_count) {
                    if (p_node->user_data.joystick_cfg.button_cfg.button_repeat_valid_count > 2) {
                        p_node->device_data.pre_event = p_node->device_data.now_event;
                        p_node->device_data.now_event = TDL_BUTTON_PRESS_REPEAT;
                        PUT_EVENT_CB(p_node->user_data, p_node->name, TDL_BUTTON_PRESS_REPEAT,
                                     (void *)((uint32_t)p_node->device_data.repeat));
                    }
                }
                p_node->device_data.flag = 0;
            } else {
                // Release not timed out, timely update front and back status
                p_node->device_data.pre_event = p_node->device_data.now_event;
            }
        }
    } break;

    case 3: {
        uint16_t repeat_tick = 0;
        // PR_NOTICE("case3:tick=%d",p_node->device_data.ticks);
        // repeat up
        if (p_node->device_data.status == 0) {
            p_node->device_data.pre_event = p_node->device_data.now_event;
            p_node->device_data.now_event = TDL_BUTTON_PRESS_UP;
            PUT_EVENT_CB(p_node->user_data, p_node->name, TDL_BUTTON_PRESS_UP,
                         (void *)((uint32_t)p_node->device_data.repeat));
            repeat_tick = p_node->user_data.joystick_cfg.button_cfg.button_repeat_valid_time / tdl_joystick_scan_time;
            if (p_node->device_data.ticks >= repeat_tick) {
                // Release timeout, double click uses default interval, multi-click uses user-configured interval
                // PR_NOTICE("3: tick=%d",p_node->device_data.ticks);
                // PR_NOTICE("%d",repeat_tick);
                p_node->device_data.flag = 0;
            } else {
                p_node->device_data.flag = 2;
                p_node->device_data.ticks = 0;
            }
        } else {
            // Greater than one press, keep pressing, timely update front and back status
            p_node->device_data.pre_event = p_node->device_data.now_event;
        }

    } break;

    case 5: {
        if (p_node->device_data.status != 0) {
            // Trigger long press hold event
            if (p_node->device_data.dev_cfg.stick_mode == JOYSTICK_IRQ_MODE) {
                tdl_joystick_local.irq_scan_cnt = 0;
            }
            hold_tick = p_node->user_data.joystick_cfg.button_cfg.long_keep_timer / TDL_JOYSTICK_SCAN_TIME;
            if (hold_tick == 0) {
                hold_tick = 1;
            }
            if (p_node->device_data.ticks >= hold_tick) {
                // Greater than hold count, immediately refresh status
                p_node->device_data.pre_event = p_node->device_data.now_event;
                p_node->device_data.now_event = TDL_BUTTON_LONG_PRESS_HOLD;
                if (p_node->device_data.ticks % hold_tick == 0) {
                    // Confirm that it reaches the hold integer multiple before executing
                    // PR_NOTICE("hold,tick=%d",hold_tick);
                    PUT_EVENT_CB(p_node->user_data, p_node->name, TDL_BUTTON_LONG_PRESS_HOLD,
                                 (void *)((uint32_t)p_node->device_data.ticks * tdl_joystick_scan_time));
                }
            }
        } else {
            // Hold release
            p_node->device_data.pre_event = p_node->device_data.now_event;
            p_node->device_data.now_event = TDL_BUTTON_PRESS_UP;
            PUT_EVENT_CB(p_node->user_data, p_node->name, TDL_BUTTON_PRESS_UP,
                         (void *)((uint32_t)p_node->device_data.ticks * tdl_joystick_scan_time));
            p_node->device_data.ticks = 0;
            p_node->device_data.flag = 0;
        }
    } break;
    case 6: {
        // If the power is continuously maintained at an effective level and triggered after recovery
        PUT_EVENT_CB(p_node->user_data, p_node->name, TDL_BUTTON_RECOVER_PRESS_UP, NULL);
        p_node->device_data.ticks = 0;
        p_node->device_data.flag = 0;
    } break;

    default:
        break;
    }

    // stick scan
    tdl_joystick_direction_event_proc(p_node);
    return;
}

/**
 * @brief Joystick scan task, periodically scans joystick status.
 * @param[in] arg Task argument, not used.
 */
static void __tdl_joystick_irq_cb(void *arg)
{
    if (tdl_joystick_local.irq_scan_cnt >= TDL_JOYSTICK_IRQ_SCAN_CNT) {
        tal_semaphore_post(tdl_joystick_local.irq_semaphore);
    }
    return;
}

/**
 * @brief Get the operate info for TDD layer.
 * @param[in] p_node Pointer to joystick node.
 * @param[out] oprt_info Pointer to operation information structure.
 * @return Operation result.
 */
static OPERATE_RET __tdl_get_operate_info(TDL_JOYSTICK_LIST_NODE_T *p_node, TDL_JOYSTICK_OPRT_INFO *oprt_info)
{
    if (NULL == oprt_info) {
        return OPRT_INVALID_PARM;
    }

    if (NULL == p_node) {
        return OPRT_INVALID_PARM;
    }

    memset(oprt_info, 0, sizeof(TDL_JOYSTICK_OPRT_INFO));
    oprt_info->dev_handle = p_node->device_data.dev_handle;
    oprt_info->irq_cb = __tdl_joystick_irq_cb;

    return OPRT_OK;
}
/**
 * @brief Pass in the button configuration and create a button handle
 * @param[in] name button name
 * @param[in] button_cfg button software configuration
 * @param[out] handle the handle of the control button
 * @return Function Operation Result  OPRT_OK is ok other is fail
 */
OPERATE_RET tdl_joystick_create(char *name, TDL_JOYSTICK_CFG_T *joystick_cfg, TDL_JOYSTICK_HANDLE *handle)
{
    OPERATE_RET ret = OPRT_COM_ERROR;
    TDL_JOYSTICK_LIST_NODE_T *p_node = NULL;
    TDL_JOYSTICK_OPRT_INFO joystick_oprt;

    if (NULL == handle) {
        PR_ERR("tdl create handle err");
        return OPRT_INVALID_PARM;
    }

    if (NULL == joystick_cfg) {
        PR_ERR("tdl create cfg err");
        return OPRT_INVALID_PARM;
    }

    // Create joystick node
    p_node = __tdl_joystick_updata_userdata(name, joystick_cfg);
    if (NULL != p_node) {
        // PR_NOTICE("tdl create updata OK");
    } else {
        PR_ERR("tdl joystick create updata err");
        return OPRT_COM_ERROR;
    }

    if (NULL == p_node->joystick_mutex) {
        ret = tal_mutex_create_init(&p_node->joystick_mutex);
        if (OPRT_OK != ret) {
            PR_ERR("tdl joystick mutex create err");
            return OPRT_COM_ERROR;
        }
    }

    ret = __tdl_get_operate_info(p_node, &joystick_oprt);
    if (OPRT_OK != ret) {
        PR_ERR("tdl joystick create err");
        return OPRT_COM_ERROR;
    }

    ret = p_node->device_data.ctrl_info.joystick_create(&joystick_oprt);
    if (OPRT_OK != ret) {
        PR_ERR("tdl joystick create err");
        return OPRT_COM_ERROR;
    }
    p_node->device_data.init_flag = TRUE;

    if (p_node->device_data.dev_cfg.stick_mode == JOYSTICK_IRQ_MODE) {
        tdl_joystick_local.task_mode |= JOYSTICK_IRQ_TASK;
    } else if (p_node->device_data.dev_cfg.stick_mode == JOYSTICK_TIMER_SCAN_MODE) {
        tdl_joystick_local.task_mode |= JOYSTICK_SCAN_TASK;
    }

    // Pass out the handle
    *handle = (TDL_JOYSTICK_HANDLE)p_node;
    if ((g_tdl_joystick_scan_mode_exist != p_node->device_data.dev_cfg.stick_mode) &&
        (g_tdl_joystick_scan_mode_exist != 0xFF)) {
        PR_ERR("joystick scan_mode isn't same,please check!");
        return OPRT_COM_ERROR;
    }

    if (tdl_joystick_local.task_mode == JOYSTICK_IRQ_TASK) {
        __tdl_joystick_irq_task(1);
        if (OPRT_OK != ret) {
            PR_ERR("tdl create err");
            return OPRT_COM_ERROR;
        }
    } else {
        __tdl_joystick_scan_task(1);
        if (OPRT_OK != ret) {
            PR_ERR("tdl create err");
            return OPRT_COM_ERROR;
        }
    }
    g_tdl_joystick_scan_mode_exist = p_node->device_data.dev_cfg.stick_mode;
    PR_DEBUG("tdl_joystick_create succ");

    return ret;
}

/**
 * @brief Handle joystick scanning and state management.
 * @param[in] p_node Pointer to the joystick node.
 */
static void __tdl_joystick_handle(TDL_JOYSTICK_LIST_NODE_T *p_node)
{
    OPERATE_RET ret = OPRT_COM_ERROR;
    uint8_t status = 0;
    TDL_JOYSTICK_OPRT_INFO joystick_oprt;

    ret = __tdl_get_operate_info(p_node, &joystick_oprt);
    if (OPRT_OK != ret) {
        return;
    }

    if (p_node->device_data.init_flag == TRUE) {
        p_node->device_data.ctrl_info.read_value(&joystick_oprt, &status);
    } else {
        PR_NOTICE("joystick is no init over, name=%s", p_node->name);
        return;
    }

    // Handle the case where a long press on the button triggers a short press when powered on in scan mode.
    // This is not an issue in interrupt mode, where the ready state is not needed.
    if ((p_node->device_data.dev_cfg.stick_mode == JOYSTICK_TIMER_SCAN_MODE) && (p_node->device_data.ready == FALSE)) {
        if (status) {
            return;
        } else {
            PR_NOTICE("device_data.ready=TRUE,%s,status=%d", p_node->name, status);
            p_node->device_data.flag = 6;
            p_node->device_data.ready = TRUE;
        }
    }

    // If the joystick is not ready, return
    if (p_node->device_data.flag > 0) {
        p_node->device_data.ticks++;
    }

    // If the status is not changed, reset the debounce count
    if (status != p_node->device_data.status) {
        if (++(p_node->device_data.debounce_cnt) >=
            (p_node->user_data.joystick_cfg.button_cfg.button_debounce_time / tdl_joystick_scan_time)) {
            p_node->device_data.status = status;
        }
    } else {
        p_node->device_data.debounce_cnt = 0;
    }

    // Update joystick state
    __tdl_joystick_state_handle(p_node);
    return;
}

/**
 * @brief Joystick scan task thread, periodically scans all joystick nodes.
 * @param[in] arg Thread argument.
 */
static void __tdl_joystick_scan_thread(void *arg)
{
    TDL_JOYSTICK_LIST_HEAD_T *p_head = p_joystick_list;
    // TDL_JOYSTICK_LIST_HEAD_T *p_combine_head = p_combine_joystick_list;
    TDL_JOYSTICK_LIST_NODE_T *p_node = NULL;
    // TDL_JOYSTICK_COMBINE_LIST_NODE_T *p_combine_node = NULL;
    LIST_HEAD *pos1 = NULL;

    while (1) {
        tuya_list_for_each(pos1, &p_head->hdr)
        {
            p_node = tuya_list_entry(pos1, TDL_JOYSTICK_LIST_NODE_T, hdr);
            if ((p_node != NULL) && (p_node->device_data.dev_cfg.stick_mode == JOYSTICK_TIMER_SCAN_MODE)) {
                tal_mutex_lock(p_node->joystick_mutex);
                __tdl_joystick_handle(p_node);
                tal_mutex_unlock(p_node->joystick_mutex);
            }
        }
#if (COMBINE_JOYSTICK_ENABLE == 1)
        // Handle the case where the interrupt scan count has reached the limit
        tuya_list_for_each(pos2, &p_combine_head->hdr)
        {
            p_combine_node = tuya_list_entry(pos2, TDL_JOYSTICK_COMBINE_LIST_NODE_T, hdr);
            if (p_combine_node->combine_cb) {
                p_combine_node->combine_cb();
            }
        }
#endif
        tal_system_sleep(tdl_joystick_scan_time);
    }
}

/**
 * @brief Joystick interrupt scan task thread, handles joystick nodes in interrupt mode.
 * @param[in] arg Thread argument.
 */
static void __tdl_joystick_irq_thread(void *arg)
{
    TDL_JOYSTICK_LIST_HEAD_T *p_head = p_joystick_list;
    // TDL_JOYSTICK_LIST_HEAD_T *p_combine_head = p_combine_joystick_list;
    TDL_JOYSTICK_LIST_NODE_T *p_node = NULL;
    // TDL_JOYSTICK_COMBINE_LIST_NODE_T *p_combine_node = NULL;
    LIST_HEAD *pos1 = NULL;

    while (1) {
        PR_NOTICE("semaphore wait");
        tal_semaphore_wait(tdl_joystick_local.irq_semaphore, SEM_WAIT_FOREVER);
        tdl_joystick_local.irq_scan_cnt = 0;
        PR_NOTICE("semaphore across");

        while (1) {
            tuya_list_for_each(pos1, &p_head->hdr)
            {
                p_node = tuya_list_entry(pos1, TDL_JOYSTICK_LIST_NODE_T, hdr);
                if ((p_node != NULL) && (p_node->device_data.dev_cfg.stick_mode == JOYSTICK_IRQ_MODE)) {
                    tal_mutex_lock(p_node->joystick_mutex);
                    __tdl_joystick_handle(p_node);
                    tal_mutex_unlock(p_node->joystick_mutex);
                }
            }
#if (COMBINE_JOYSTICK_ENABLE == 1)
            // Handle the case where the interrupt scan count has reached the limit
            if (tdl_joystick_local.scan_task_flag == FALSE) {
                tuya_list_for_each(pos2, &p_combine_head->hdr)
                {
                    p_combine_node = tuya_list_entry(pos2, TDL_JOYSTICK_COMBINE_LIST_NODE_T, hdr);
                    if (p_combine_node->combine_cb) {
                        p_combine_node->combine_cb();
                    }
                }
            }
#endif
            // Check if the interrupt scan count has reached the limit
            if (++tdl_joystick_local.irq_scan_cnt >= TDL_JOYSTICK_IRQ_SCAN_CNT) {
                break;
            } else {
                tal_system_sleep(tdl_joystick_scan_time);
            }
        }
    }
}

/**
 * @brief Enable or disable joystick scanning task.
 * @param[in] enable 1-Enable 0-Disable
 * @return Operation result
 */
static OPERATE_RET __tdl_joystick_scan_task(uint8_t enable)
{
    OPERATE_RET ret = OPRT_COM_ERROR;

    if (tdl_joystick_local.task_mode & JOYSTICK_SCAN_TASK) {
        if (enable != 0) {
            // Establish scan task
            if (tdl_joystick_local.scan_task_flag == FALSE) {

                THREAD_CFG_T thrd_param = {0};

                thrd_param.thrdname = "joystick_scan";
                thrd_param.priority = THREAD_PRIO_1;
                thrd_param.stackDepth = sg_joystick_task_stack_size;
                if (NULL == stick_scan_thread_handle) {
                    ret = tal_thread_create_and_start(&stick_scan_thread_handle, NULL, NULL, __tdl_joystick_scan_thread,
                                                      NULL, &thrd_param);
                    if (OPRT_OK != ret) {
                        PR_ERR("scan_task create error!");
                        return ret;
                    }
                }
                tdl_joystick_local.scan_task_flag = TRUE;
                PR_DEBUG("joystick_scan task stack size:%d", sg_joystick_task_stack_size);
            }
        } else {
            // Disable scan task
            tal_thread_delete(stick_scan_thread_handle);
            tdl_joystick_local.scan_task_flag = FALSE;
        }
    }
    return ret;
}

/**
 * @brief Enable or disable joystick interrupt scanning task.
 * @param[in] enable 1-Enable 0-Disable
 * @return Operation result
 */
static OPERATE_RET __tdl_joystick_irq_task(uint8_t enable)
{
    OPERATE_RET ret = OPRT_COM_ERROR;
    if (tdl_joystick_local.task_mode & JOYSTICK_IRQ_TASK) {
        if (enable != 0) {
            // Establish interrupt scan task
            if (tdl_joystick_local.irq_task_flag == FALSE) {
                THREAD_CFG_T thrd_param = {0};

                thrd_param.thrdname = "joystick_irq";
                thrd_param.priority = THREAD_PRIO_1;
                thrd_param.stackDepth = sg_joystick_task_stack_size;
                if (NULL == stick_irq_thread_handle) {
                    ret = tal_thread_create_and_start(&stick_irq_thread_handle, NULL, NULL, __tdl_joystick_irq_thread,
                                                      NULL, &thrd_param);
                    if (OPRT_OK != ret) {
                        PR_ERR("irq_task create error!");
                        return ret;
                    }
                }
                tdl_joystick_local.irq_task_flag = TRUE;
                PR_DEBUG("joystick_irq task stack size:%d", sg_joystick_task_stack_size);
            } else {
                PR_WARN("joystick irq tast have already creat");
            }
        } else {
            // Disable interrupt scan task
            tal_thread_delete(stick_irq_thread_handle);
            tdl_joystick_local.irq_task_flag = FALSE;
        }
    }
    return OPRT_OK;
}

/**
 * @brief Delete a joystick
 * @param[in] handle the handle of the joystick
 * @return Function Operation Result  OPRT_OK is ok other is fail
 */
OPERATE_RET tdl_joystick_delete(TDL_JOYSTICK_HANDLE handle)
{
    OPERATE_RET ret = OPRT_COM_ERROR;
    TDL_JOYSTICK_LIST_NODE_T *p_node = NULL;
    TDL_JOYSTICK_OPRT_INFO joystick_oprt;

    if (NULL == handle) {
        return OPRT_INVALID_PARM;
    }

    p_node = __tdl_joystick_find_node(handle);
    if (NULL != p_node) {

        ret = __tdl_get_operate_info(p_node, &joystick_oprt);
        if (OPRT_OK != ret) {
            return OPRT_COM_ERROR;
        }

        ret = p_node->device_data.ctrl_info.joystick_delete(&joystick_oprt);
        if (OPRT_OK != ret) {
            return ret;
        }

        tal_free(p_node->name);
        p_node->name = NULL;

        tal_mutex_lock(tdl_joystick_local.mutex);
        tuya_list_del(&p_node->hdr);
        tal_mutex_unlock(tdl_joystick_local.mutex);

        tal_free(p_node);
        p_node = NULL;
        return OPRT_OK;
    }
    return ret;
}

/**
 * @brief Delete a button without tdd info
 * @param[in] handle the handle of the control button
 * @return Function Operation Result  OPRT_OK is ok other is fail
 */
OPERATE_RET tdl_joystick_delete_without_hardware(TDL_JOYSTICK_HANDLE handle)
{
    OPERATE_RET rt = OPRT_OK;
    TDL_JOYSTICK_LIST_NODE_T *p_node = NULL;

    TUYA_CHECK_NULL_RETURN(handle, OPRT_INVALID_PARM);

    p_node = __tdl_joystick_find_node(handle);
    TUYA_CHECK_NULL_RETURN(p_node, OPRT_NOT_FOUND);

    tal_mutex_lock(p_node->joystick_mutex);

    memset(&p_node->user_data, 0, sizeof(JOYSTICK_USER_DATA_T));
    p_node->device_data.pre_event = 0;
    p_node->device_data.now_event = 0;
    p_node->device_data.flag = 0;
    p_node->device_data.debounce_cnt = 0;
    p_node->device_data.ticks = 0;
    p_node->device_data.status = 0;
    p_node->device_data.repeat = 0;
    p_node->device_data.ready = 0;
    p_node->device_data.init_flag = 0;

    tal_mutex_unlock(p_node->joystick_mutex);

    return rt;
}

/**
 * @brief Function registration for button events
 * @param[in] handle the handle of the control button
 * @param[in] event button trigger event
 * @param[in] cb The function corresponding to the button event
 * @return none
 */
void tdl_joystick_event_register(TDL_JOYSTICK_HANDLE handle, TDL_JOYSTICK_TOUCH_EVENT_E event, TDL_JOYSTICK_EVENT_CB cb)
{
    TDL_JOYSTICK_LIST_NODE_T *p_node = NULL;

    if (event >= TDL_JOYSTICK_TOUCH_EVENT_MAX) {
        PR_ERR("event is illegal");
        return;
    }

    p_node = __tdl_joystick_find_node(handle);
    if (NULL != p_node) {
        p_node->user_data.list_cb[event] = cb;
    }
    return;
}

/**
 * @brief Turn button function off or on
 * @param[in] enable 0-close  1-open
 * @return Function Operation Result  OPRT_OK is ok other is fail
 */
OPERATE_RET tdl_joystick_deep_sleep_ctrl(uint8_t enable)
{
    OPERATE_RET ret = OPRT_COM_ERROR;

    if (tdl_joystick_local.task_mode == JOYSTICK_IRQ_TASK) {
        ret = __tdl_joystick_irq_task(enable);
        if (OPRT_OK != ret) {
            return ret;
        }
    } else {
        ret = __tdl_joystick_scan_task(enable);
        if (OPRT_OK != ret) {
            return ret;
        }
    }
    return OPRT_OK;
}

/**
 * @brief set button task stack size
 *
 * @param[in] size stack size
 * @return Function Operation Result  OPRT_OK is ok other is fail
 */
OPERATE_RET tdl_joystick_set_task_stack_size(uint32_t size)
{
    sg_joystick_task_stack_size = size;

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
OPERATE_RET tdl_joystick_set_ready_flag(char *name, uint8_t status)
{
    TDL_JOYSTICK_LIST_NODE_T *p_node = NULL;

    // Compare name existence
    p_node = __tdl_joystick_find_node_name(name);
    if (NULL == p_node) {
        PR_NOTICE("joystick no existence");
        return OPRT_NOT_FOUND;
    }

    p_node->device_data.ready = status;
    return OPRT_OK;
}

/**
 * @brief read button status
 * @param[in] handle button handle
 * @param[out] status button status
 * @return OPRT_OK if successful
 */
OPERATE_RET tdl_joystick_read_status(TDL_JOYSTICK_HANDLE handle, uint8_t *status)
{
    OPERATE_RET rt = OPRT_OK;
    TDL_JOYSTICK_LIST_NODE_T *p_node = NULL;
    TDL_JOYSTICK_OPRT_INFO joystick_oprt;

    TUYA_CHECK_NULL_RETURN(handle, OPRT_INVALID_PARM);
    TUYA_CHECK_NULL_RETURN(status, OPRT_INVALID_PARM);

    p_node = __tdl_joystick_find_node(handle);
    TUYA_CHECK_NULL_RETURN(p_node, OPRT_COM_ERROR);

    TUYA_CALL_ERR_RETURN(__tdl_get_operate_info(p_node, &joystick_oprt));

    TUYA_CALL_ERR_RETURN(p_node->device_data.ctrl_info.read_value(&joystick_oprt, status));

    return rt;
}

/**
 * @brief set joystick level ( rocker button use)
 *		 The default configuration is toggle switch - when level flipping,
 *		 it is modified to level synchronization in the application - the default effective level is low effective
 * @param[in] handle joystick handle
 * @param[in] level TUYA_GPIO_LEVEL_E
 * @return OPRT_OK if successful
 */
OPERATE_RET tdl_joystick_set_level(TDL_JOYSTICK_HANDLE handle, TUYA_GPIO_LEVEL_E level)
{
    OPERATE_RET rt = OPRT_OK;
    TDL_JOYSTICK_LIST_NODE_T *p_node = NULL;
    TDL_JOYSTICK_OPRT_INFO joystick_oprt;

    TUYA_CHECK_NULL_RETURN(handle, OPRT_INVALID_PARM);

    p_node = __tdl_joystick_find_node(handle);
    TUYA_CHECK_NULL_RETURN(p_node, OPRT_COM_ERROR);

    TUYA_CALL_ERR_RETURN(__tdl_get_operate_info(p_node, &joystick_oprt));

    TUYA_CALL_ERR_RETURN(tdd_joystick_update_level(joystick_oprt.dev_handle, level));

    return rt;
}

/**
 * @brief set button scan time, default is 10ms
 * @param[in] time_ms button scan time
 * @return OPRT_OK if successful
 */
OPERATE_RET tdl_joystick_set_scan_time(uint8_t time_ms)
{
    if (time_ms < TDL_JOYSTICK_SCAN_TIME)
        return OPRT_INVALID_PARM;
    tdl_joystick_scan_time = time_ms;
    tdl_joystick_local.irq_scan_cnt = TDL_JOYSTICK_IRQ_SCAN_TIME / time_ms;
    return OPRT_OK;
}

/**
 * @brief Register joystick control parameters
 *
 * @param name Joystick identifier
 * @param joystick_ctrl_info Control methods structure
 * @param joystick_cfg_info Hardware config structure
 * @return Operation status
 */
OPERATE_RET tdl_joystick_register(char *name, TDL_JOYSTICK_CTRL_INFO *joystick_ctrl_info,
                                  TDL_JOYSTICK_DEVICE_INFO_T *joystick_cfg_info)
{
    OPERATE_RET ret = OPRT_COM_ERROR;
    TDL_JOYSTICK_LIST_NODE_T *p_node = NULL;

    if (NULL == joystick_ctrl_info) {
        return OPRT_INVALID_PARM;
    }

    if (NULL == joystick_cfg_info) {
        return OPRT_INVALID_PARM;
    }

    ret = __tdl_joystick_list_init();
    if (OPRT_OK != ret) {
        PR_ERR("tdl joystick list init err");
        return ret;
    }

    p_node = __tdl_joystick_add_node(name, joystick_ctrl_info, joystick_cfg_info);
    if (NULL != p_node) {
        return ret;
    }

    return ret;
}

/**
 * @brief Get the raw joystick data from ADC channels.
 * @param[in] handle Joystick handle.
 * @param[out] x Pointer to store X-axis value.
 * @param[out] y Pointer to store Y-axis value.
 */
OPERATE_RET tdl_joystick_get_raw_xy(TDL_JOYSTICK_HANDLE handle, int *x, int *y)
{
    OPERATE_RET ret = OPRT_OK;
    TDL_JOYSTICK_LIST_NODE_T *p_node = NULL;
    int adc_value[2] = {0};

    p_node = __tdl_joystick_find_node(handle);
    if (NULL == p_node) {
        PR_ERR("handle not get");
        return OPRT_COM_ERROR;
    }

    // Read the ADC values for the specified channels
    ret = tkl_adc_read_single_channel(p_node->device_data.dev_cfg.adc_num, p_node->device_data.dev_cfg.adc_ch_x,
                                      (int32_t *)adc_value);
    if (ret != OPRT_OK) {
        return OPRT_COM_ERROR;
    }
    ret = tkl_adc_read_single_channel(p_node->device_data.dev_cfg.adc_num, p_node->device_data.dev_cfg.adc_ch_y,
                                      (int32_t *)adc_value + 1);
    if (ret != OPRT_OK) {
        return OPRT_COM_ERROR;
    }
    // Get the raw joystick values
    *x = adc_value[0];
    *y = adc_value[1];
    return OPRT_OK;
}

/**
 * @brief Get the calibrated joystick data from ADC channels.
 * This function normalizes the joystick values based on the configured ADC range.
 * @param[in] handle Joystick handle.
 * @param[out] x Pointer to store normalized X-axis value.
 * @param[out] y Pointer to store normalized Y-axis value.
 */
OPERATE_RET tdl_joystick_calibrated_xy(TDL_JOYSTICK_HANDLE handle, int *x, int *y)
{
    OPERATE_RET ret = OPRT_OK;
    TDL_JOYSTICK_LIST_NODE_T *p_node = NULL;
    int mid_value = 1;

    p_node = __tdl_joystick_find_node(handle);
    if (NULL == p_node) {
        PR_ERR("handle not get");
        return OPRT_COM_ERROR;
    }

    // Calculate the mid value based on the ADC min and max values
    mid_value = p_node->user_data.joystick_cfg.adc_cfg.adc_max_val + p_node->user_data.joystick_cfg.adc_cfg.adc_min_val;
    mid_value /= 2;

    int adc_value[2] = {0};
    ret = tdl_joystick_get_raw_xy(handle, adc_value, adc_value + 1);
    if (ret != OPRT_OK) {
        return OPRT_COM_ERROR;
    }

    *y = adc_value[0];
    *x = adc_value[1];

    // Normalize the joystick values based on the mid value and configured range
    *x = (mid_value - *x) * p_node->user_data.joystick_cfg.adc_cfg.normalized_range / mid_value;
    *y = (mid_value - *y) * p_node->user_data.joystick_cfg.adc_cfg.normalized_range / mid_value;

    return OPRT_OK;
}

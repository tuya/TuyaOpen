/**
 * @file lv_port_indev_templ.c
 *
 */

/*********************
 *      INCLUDES
 *********************/
#include "tal_api.h"
#include "lv_port_indev.h"
#include "tuya_list.h"

#ifdef LVGL_ENABLE_TP
#include "tdl_tp_manage.h"
#endif

/*********************
 *      DEFINES
 *********************/

/**********************
 *      TYPEDEFS
 **********************/
#ifdef LVGL_ENABLE_TP
typedef struct {
    struct tuya_list_head   node;
    lv_indev_t             *lv_indev;
    TDL_TP_HANDLE_T         tp_hdl;
    int32_t                 last_x;
    int32_t                 last_y;
}LV_INDEV_TP_NODE_T;
#endif
/**********************
 *  STATIC PROTOTYPES
 **********************/
#ifdef LVGL_ENABLE_TP
static LV_INDEV_TP_NODE_T *__find_touchpad_node_by_hdl(TDL_TP_HANDLE_T tp_hdl);
static LV_INDEV_TP_NODE_T *__find_touchpad_node_by_lv_indev(lv_indev_t * lv_indev);
static lv_indev_t *__find_tp_lv_indev_by_name(char *device);
static LV_INDEV_TP_NODE_T *__create_touchpad_node(TDL_TP_HANDLE_T tp_hdl);
static void __release_touchpad_node(LV_INDEV_TP_NODE_T *node);
static void touchpad_read(lv_indev_t *indev, lv_indev_data_t *data);
#endif

#ifdef ENABLE_LVGL_ENCODER
static void encoder_init(void);
static void encoder_read(lv_indev_t *indev, lv_indev_data_t *data);
static void encoder_handler(void);
#endif
/**********************
 *  STATIC VARIABLES
 **********************/
lv_indev_t *indev_encoder;

#ifdef LVGL_ENABLE_TP
static struct tuya_list_head sg_lv_indev_tp_list = LIST_HEAD_INIT(sg_lv_indev_tp_list);
#endif

/**********************
 *      MACROS
 **********************/

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

void lv_port_indev_init(char *device)
{
    /**
     * Here you will find example implementation of input devices supported by LittelvGL:
     *  - Touchpad
     *  - Mouse (with cursor support)
     *  - Keypad (supports GUI usage only with key)
     *  - Encoder (supports GUI usage only with: left, right, push)
     *  - Button (external buttons to press points on the screen)
     *
     *  The `..._read()` function are only examples.
     *  You should shape them according to your hardware
     */

    /*------------------
     * Touchpad
     * -----------------*/
#ifdef LVGL_ENABLE_TP
    /*Initialize your touchpad if you have*/
    TDL_TP_HANDLE_T tp_hdl = tdl_tp_find_dev(device);
    if(NULL == tp_hdl) {
        PR_ERR("touchpad dev %s not found", device); 
    }

    LV_INDEV_TP_NODE_T *tp_node = __create_touchpad_node(tp_hdl);
    if(NULL == tp_node) {
        PR_ERR("create touchpad node failed"); 
        return;
    }

    tuya_list_add(&tp_node->node, &sg_lv_indev_tp_list);
#endif

    /*------------------
     * Encoder
     * -----------------*/
#ifdef ENABLE_LVGL_ENCODER
    /*Initialize your encoder if you have*/
    encoder_init();

    /*Register a encoder input device*/
    indev_encoder = lv_indev_create();
    lv_indev_set_type(indev_encoder, LV_INDEV_TYPE_ENCODER);
    lv_indev_set_read_cb(indev_encoder, encoder_read);
#endif
}

lv_indev_t *lv_port_get_lv_indev_by_name(char *device)
{
    lv_indev_t *in_dev = NULL;

#ifdef LVGL_ENABLE_TP
    in_dev = __find_tp_lv_indev_by_name(device);
    if(in_dev != NULL) {
        return in_dev;
    }
#endif

    return in_dev;
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

/*------------------
 * Touchpad
 * -----------------*/
#ifdef LVGL_ENABLE_TP
static LV_INDEV_TP_NODE_T *__find_touchpad_node_by_hdl(TDL_TP_HANDLE_T tp_hdl)
{
    LV_INDEV_TP_NODE_T *tp_node = NULL;
    struct tuya_list_head *pos = NULL;

    if(NULL == tp_hdl) {
        return NULL;
    }

    tuya_list_for_each(pos, &sg_lv_indev_tp_list) {
        tp_node = tuya_list_entry(pos, LV_INDEV_TP_NODE_T, node);
        if (tp_node->tp_hdl == tp_hdl) {
            return tp_node;
        }
    }

    return NULL;
}
static LV_INDEV_TP_NODE_T *__find_touchpad_node_by_lv_indev(lv_indev_t * lv_indev)
{
    LV_INDEV_TP_NODE_T *tp_node = NULL;
    struct tuya_list_head *pos = NULL;

    if(NULL == lv_indev) {
        lv_indev = lv_indev_get_next(NULL);
    }

    tuya_list_for_each(pos, &sg_lv_indev_tp_list) {
        tp_node = tuya_list_entry(pos, LV_INDEV_TP_NODE_T, node);
        if (tp_node->lv_indev == lv_indev) {
            return tp_node;
        }
    }

    return NULL;
}

static lv_indev_t *__find_tp_lv_indev_by_name(char *device)
{
    TDL_TP_HANDLE_T tp_hdl = NULL;
    LV_INDEV_TP_NODE_T *tp_node = NULL;

    tp_hdl = tdl_tp_find_dev(device);
    if(NULL == tp_hdl) {
        PR_ERR("touchpad dev %s not found", device);
        return NULL;
    }
    
    tp_node = __find_touchpad_node_by_hdl(tp_hdl);
    if(NULL == tp_node) {
        PR_ERR("lv touchpad node not found");   
        return NULL;
    }

    return tp_node->lv_indev;
}

/*Initialize your touchpad*/
static LV_INDEV_TP_NODE_T *__create_touchpad_node(TDL_TP_HANDLE_T tp_hdl)
{
    OPERATE_RET rt = OPRT_OK;
    LV_INDEV_TP_NODE_T *tp_node = NULL;

    NEW_LIST_NODE(LV_INDEV_TP_NODE_T, tp_node);
    if (NULL == tp_node) {
        return NULL;
    }
    memset(tp_node, 0, sizeof(LV_INDEV_TP_NODE_T));

    tp_node->tp_hdl = tp_hdl;
    TUYA_CALL_ERR_GOTO(tdl_tp_dev_open(tp_hdl), __CREATE_ERR);

    /*Register a touchpad input device*/
    tp_node->lv_indev = lv_indev_create();
    lv_indev_set_type(tp_node->lv_indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(tp_node->lv_indev, touchpad_read);

    return tp_node;

__CREATE_ERR:
    __release_touchpad_node(tp_node);

    return NULL;
}

static void __release_touchpad_node(LV_INDEV_TP_NODE_T *node)
{
    if(NULL == node) {
        return;
    }

    if(node->lv_indev) { 
        lv_indev_delete(node->lv_indev);
        node->lv_indev = NULL;
    }

    if(node->tp_hdl) {
        tdl_tp_dev_close(node->tp_hdl);
        node->tp_hdl = NULL;
    }

    if(node) {
        tal_free(node);
        node = NULL;
    }
}

/*Will be called by the library to read the touchpad*/
static void touchpad_read(lv_indev_t *indev_drv, lv_indev_data_t *data)
{
    LV_INDEV_TP_NODE_T *tp_node = NULL;
    uint8_t point_num = 0;
    TDL_TP_POS_T point;

    tp_node = __find_touchpad_node_by_lv_indev(indev_drv);
    if(NULL == tp_node) {
        PR_ERR("touchpad node not found"); 
        return;
    }

    tdl_tp_dev_read(tp_node->tp_hdl, 1, &point, &point_num);
    /*Save the pressed coordinates and the state*/
    if (point_num > 0) {
        data->state = LV_INDEV_STATE_PRESSED;
        tp_node->last_x = point.x;
        tp_node->last_y = point.y;
        // PR_DEBUG("touchpad_read: x=%d, y=%d", point.x, point.y);
    } else {
        data->state = LV_INDEV_STATE_RELEASED;
    }

    /*Set the last pressed coordinates*/
    data->point.x = tp_node->last_x;
    data->point.y = tp_node->last_y;
}
#endif

/*------------------
 * Encoder
 * -----------------*/
#ifdef ENABLE_LVGL_ENCODER
int32_t encoder_diff = 0;
lv_indev_state_t encoder_state;
/*Initialize your encoder*/
static void encoder_init(void)
{
    drv_encoder_init();
}

/*Will be called by the library to read the encoder*/
static void encoder_read(lv_indev_t *indev_drv, lv_indev_data_t *data)
{
    static int32_t last_diff = 0;
    int32_t diff;
    if (encoder_get_pressed()) {
        encoder_diff = 0;
        encoder_state = LV_INDEV_STATE_PRESSED;
    } else {
        diff = encoder_get_angle();

        encoder_diff = diff - last_diff;
        last_diff = diff;

        encoder_state = LV_INDEV_STATE_RELEASED;
    }

    data->enc_diff = encoder_diff;
    data->state = encoder_state;
}
#endif

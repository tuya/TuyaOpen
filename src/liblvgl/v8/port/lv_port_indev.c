/**
 * @file lv_port_indev_templ.c
 *
 */

/*********************
 *      INCLUDES
 *********************/
#include "tal_api.h"
#include "lv_port_indev.h"
#ifdef LVGL_ENABLE_TP
#include "tdl_tp_manage.h"
#endif

/*********************
 *      DEFINES
 *********************/

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 *  STATIC PROTOTYPES
 **********************/
#ifdef LVGL_ENABLE_TP
static void touchpad_init(void *device);
static void touchpad_read(lv_indev_drv_t * indev_drv, lv_indev_data_t * data);
#endif

#ifdef ENABLE_LVGL_ENCODER
static void encoder_init(void);
static void encoder_read(lv_indev_drv_t * indev_drv, lv_indev_data_t * data);
static void encoder_handler(void);
#endif
/**********************
 *  STATIC VARIABLES
 **********************/
lv_indev_t *indev_touchpad;
lv_indev_t *indev_encoder;

#ifdef LVGL_ENABLE_TP
static TDL_TP_HANDLE_T sg_tp_hdl = NULL; // Handle for tp device
#endif

/**********************
 *      MACROS
 **********************/

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

void lv_port_indev_init(void *device)
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
    static lv_indev_drv_t indev_drv;

    /*Initialize your touchpad if you have*/
    touchpad_init(device);

    /*Register a touchpad input device*/
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = touchpad_read;
    indev_touchpad = lv_indev_drv_register(&indev_drv);

#endif

    /*------------------
     * Encoder
     * -----------------*/
#ifdef ENABLE_LVGL_ENCODER
    static lv_indev_drv_t indev_encoder_drv;

    /*Initialize your encoder if you have*/
    encoder_init();

    /*Register a encoder input device*/

    /*Register a touchpad input device*/
    lv_indev_drv_init(&indev_encoder_drv);
    indev_encoder_drv.type = LV_INDEV_TYPE_ENCODER;
    indev_encoder_drv.read_cb = encoder_read;
    indev_encoder = lv_indev_drv_register(&indev_drv);
#endif
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

/*------------------
 * Touchpad
 * -----------------*/
#ifdef LVGL_ENABLE_TP

/*Initialize your touchpad*/
static void touchpad_init(void *device)
{
    OPERATE_RET rt = OPRT_OK;

    sg_tp_hdl = tdl_tp_find_dev(device);
    if(NULL == sg_tp_hdl) {
        PR_ERR("tp dev %s not found", device);
        return;
    }

    rt = tdl_tp_dev_open(sg_tp_hdl);
    if(rt != OPRT_OK) {
        PR_ERR("open tp dev failed, rt: %d", rt);
        return;
    }
}

/*Will be called by the library to read the touchpad*/
static void touchpad_read(lv_indev_drv_t * indev_drv, lv_indev_data_t * data)
{
    static int32_t last_x = 0;
    static int32_t last_y = 0;
    uint8_t point_num = 0;
    TDL_TP_POS_T point;

    tdl_tp_dev_read(sg_tp_hdl, 1, &point, &point_num);
    /*Save the pressed coordinates and the state*/
    if (point_num > 0) {
        data->state = LV_INDEV_STATE_PRESSED;
        last_x = point.x;
        last_y = point.y;
        // PR_DEBUG("touchpad_read: x=%d, y=%d", point.x, point.y);
    } else {
        data->state = LV_INDEV_STATE_RELEASED;
    }

    /*Set the last pressed coordinates*/
    data->point.x = last_x;
    data->point.y = last_y;
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

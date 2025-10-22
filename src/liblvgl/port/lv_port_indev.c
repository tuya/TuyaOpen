/**
 * @file lv_port_indev_templ.c
 *
 */

/*********************
 *      INCLUDES
 *********************/
#include "tal_api.h"
#include "lv_port_indev.h"
#ifdef LVGL_ENABLE_TOUCH
#include "tdl_touch_manage.h"
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
#ifdef LVGL_ENABLE_TOUCH
static void touchpad_init(void *device);
static void touchpad_read(lv_indev_t *indev, lv_indev_data_t *data);
#endif

#ifdef LVGL_ENABLE_ENCODER
static void encoder_init(void);
static void encoder_read(lv_indev_t *indev, lv_indev_data_t *data);
static void encoder_handler(void);
#endif
/**********************
 *  STATIC VARIABLES
 **********************/
lv_indev_t *indev_touchpad;
lv_indev_t *indev_encoder;

#ifdef LVGL_ENABLE_TOUCH
static TDL_TOUCH_HANDLE_T sg_touch_hdl = NULL; // Handle for touch device
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
#ifdef LVGL_ENABLE_TOUCH
    /*Initialize your touchpad if you have*/
    touchpad_init(device);

    /*Register a touchpad input device*/
    indev_touchpad = lv_indev_create();
    lv_indev_set_type(indev_touchpad, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(indev_touchpad, touchpad_read);
#endif

    /*------------------
     * Encoder
     * -----------------*/
#ifdef LVGL_ENABLE_ENCODER
    /*Initialize your encoder if you have*/
    encoder_init();

    /*Register a encoder input device*/
    indev_encoder = lv_indev_create();
    lv_indev_set_type(indev_encoder, LV_INDEV_TYPE_ENCODER);
    lv_indev_set_read_cb(indev_encoder, encoder_read);
#endif
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

/*------------------
 * Touchpad
 * -----------------*/
#ifdef LVGL_ENABLE_TOUCH

/*Initialize your touchpad*/
static void touchpad_init(void *device)
{
    OPERATE_RET rt = OPRT_OK;

    sg_touch_hdl = tdl_touch_find_dev(device);
    if(NULL == sg_touch_hdl) {
        PR_ERR("touch dev %s not found", device);
        return;
    }

    rt = tdl_touch_dev_open(sg_touch_hdl);
    if(rt != OPRT_OK) {
        PR_ERR("open touch dev failed, rt: %d", rt);
        return;
    }
}

/*Will be called by the library to read the touchpad*/
static void touchpad_read(lv_indev_t *indev_drv, lv_indev_data_t *data)
{
    static int32_t last_x = 0;
    static int32_t last_y = 0;
    uint8_t point_num = 0;
    TDL_TOUCH_POS_T point;

    tdl_touch_dev_read(sg_touch_hdl, 1, &point, &point_num);
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
#ifdef LVGL_ENABLE_ENCODER
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

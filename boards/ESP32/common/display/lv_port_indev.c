/**
 * @file lv_port_indev_templ.c
 *
 */

/*********************
 *      INCLUDES
 *********************/
#include "tuya_cloud_types.h"
#include "lv_port_indev.h"
#include "board_config.h"

#include "esp_err.h"
#include "esp_log.h"
#include "esp_lvgl_port.h"
/*********************
 *      DEFINES
 *********************/
#define TAG "esp32_lvgl"
/**********************
 *      TYPEDEFS
 **********************/

/**********************
 *  STATIC PROTOTYPES
 **********************/
#ifdef ENABLE_LVGL_ENCODER
static void encoder_init(void);
static void encoder_read(lv_indev_t *indev, lv_indev_data_t *data);
static void encoder_handler(void);
#endif
/**********************
 *  STATIC VARIABLES
 **********************/
lv_indev_t *indev_encoder;

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
    /*Initialize your touchpad if you have*/
    esp_lcd_touch_handle_t tp_handle = (esp_lcd_touch_handle_t)board_touch_get_handle();
    if (NULL == tp_handle) {
        ESP_LOGE(TAG, "Failed to get touch handle");
        return;
    }

    const lvgl_port_touch_cfg_t touch_cfg = {
        .disp = lv_display_get_default(),
        .handle = tp_handle,
    };
    lvgl_port_add_touch(&touch_cfg);
    ESP_LOGI(TAG, "Touch panel initialized successfully");
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

/**********************
 *   STATIC FUNCTIONS
 **********************/

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

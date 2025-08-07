/**
 * @file toast.c
 * Toast Message Component for AI Pocket Pet
 */

/*********************
 *      INCLUDES
 *********************/
#include "toast.h"
#include "ai_pocket_pet_app.h"
#include <stdio.h>

/*********************
 *      DEFINES
 *********************/
// Toast message constants
#define TOAST_PADDING 20
#define TOAST_MAX_WIDTH (AI_PET_SCREEN_WIDTH - 40)
#define TOAST_MIN_HEIGHT 60
#define TOAST_ANIMATION_DURATION 300
#define TOAST_DEFAULT_DELAY 3000

/**********************
 *      TYPEDEFS
 **********************/
typedef struct {
    lv_obj_t *toast_container;
    lv_obj_t *toast_label;
    lv_timer_t *toast_timer;
} toast_data_t;

/**********************
 *  STATIC PROTOTYPES
 **********************/
static void toast_timer_cb(lv_timer_t *timer);
// static void toast_anim_ready_cb(lv_anim_t *a);

/**********************
 *  STATIC VARIABLES
 **********************/
static toast_data_t g_toast_data;

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

void toast_create(lv_obj_t *parent)
{
    toast_data_t *data = &g_toast_data;

    // Create toast container
    data->toast_container = lv_obj_create(parent);
    lv_obj_set_size(data->toast_container, TOAST_MAX_WIDTH, TOAST_MIN_HEIGHT);
    lv_obj_align(data->toast_container, LV_ALIGN_CENTER, 0, 0);

    // Style the toast container
    lv_obj_set_style_bg_color(data->toast_container, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(data->toast_container, LV_OPA_80, 0);
    lv_obj_set_style_border_width(data->toast_container, 2, 0);
    lv_obj_set_style_border_color(data->toast_container, lv_color_white(), 0);
    lv_obj_set_style_radius(data->toast_container, 10, 0);
    lv_obj_set_style_pad_all(data->toast_container, TOAST_PADDING, 0);
    lv_obj_set_style_shadow_width(data->toast_container, 10, 0);
    lv_obj_set_style_shadow_color(data->toast_container, lv_color_black(), 0);
    lv_obj_set_style_shadow_opa(data->toast_container, LV_OPA_50, 0);

    // Create toast label
    data->toast_label = lv_label_create(data->toast_container);
    lv_label_set_text(data->toast_label, "");
    lv_obj_align(data->toast_label, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_text_color(data->toast_label, lv_color_white(), 0);
    lv_obj_set_style_text_font(data->toast_label, &lv_font_montserrat_14, 0);
    lv_label_set_long_mode(data->toast_label, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(data->toast_label, TOAST_MAX_WIDTH - (TOAST_PADDING * 2));

    // Move toast to top of screen (highest z-order)
    lv_obj_move_foreground(data->toast_container);

    // Initially hide the toast
    lv_obj_add_flag(data->toast_container, LV_OBJ_FLAG_HIDDEN);

    // Initialize timer to NULL
    data->toast_timer = NULL;
}

void toast_show(const char *message, uint32_t delay_ms)
{
    toast_data_t *data = &g_toast_data;

    printf("toast_show called with: '%s'\n", message);

    // Hide any existing toast first
    toast_hide();

    // Set the message text
    lv_label_set_text(data->toast_label, message);

    // Set a reasonable height for the toast container
    // The label will wrap text automatically within the container
    lv_obj_set_height(data->toast_container, TOAST_MIN_HEIGHT);

    // Move toast to top of screen (highest z-order)
    lv_obj_move_foreground(data->toast_container);

    // Show the toast immediately (without animation for testing)
    lv_obj_clear_flag(data->toast_container, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_style_opa(data->toast_container, LV_OPA_COVER, 0);
    printf("Toast container shown, opacity set to COVER\n");

    // Set up timer to hide the toast
    if (delay_ms > 0) {
        data->toast_timer = lv_timer_create(toast_timer_cb, delay_ms, NULL);
    } else {
        data->toast_timer = lv_timer_create(toast_timer_cb, TOAST_DEFAULT_DELAY, NULL);
    }
}

void toast_hide(void)
{
    toast_data_t *data = &g_toast_data;

    printf("toast_hide called\n");

    // Cancel existing timer
    if (data->toast_timer) {
        lv_timer_del(data->toast_timer);
        data->toast_timer = NULL;
    }

    // Hide immediately (without animation for testing)
    lv_obj_add_flag(data->toast_container, LV_OBJ_FLAG_HIDDEN);
    printf("Toast container hidden\n");
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

static void toast_timer_cb(lv_timer_t *timer)
{
    (void)timer;
    toast_hide();
}

// static void toast_anim_ready_cb(lv_anim_t *a)
// {
//     toast_data_t *data = &g_toast_data;
//     lv_obj_add_flag(data->toast_container, LV_OBJ_FLAG_HIDDEN);
// }

/**
 * @file lv_carousel.c
 *
 */

/*********************
 *      INCLUDES
 *********************/
#include "lv_carousel.h"
#if LV_USE_CAROUSEL

/*********************
 *      DEFINES
 *********************/

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 *  STATIC PROTOTYPES
 **********************/
static void lv_carousel_constructor(const lv_obj_class_t * class_p, lv_obj_t * obj);
static void lv_carousel_element_constructor(const lv_obj_class_t * class_p, lv_obj_t * obj);
static void carousel_event_cb(lv_event_t * e);

/**********************
 *  STATIC VARIABLES
 **********************/

const lv_obj_class_t lv_carousel_class = {.constructor_cb = lv_carousel_constructor,
                                          .base_class = &lv_obj_class,
                                          .instance_size = sizeof(lv_carousel_t)
                                         };

const lv_obj_class_t lv_carousel_element_class = {.constructor_cb = lv_carousel_element_constructor,
                                                  .base_class = &lv_obj_class,
                                                  .instance_size = sizeof(lv_carousel_element_t)
                                                 };

static uint32_t create_col_id;

/**********************
 *      MACROS
 **********************/

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

lv_obj_t * lv_carousel_create(lv_obj_t * parent)
{
    LV_LOG_INFO("begin");
    lv_obj_t * obj = lv_obj_class_create_obj(&lv_carousel_class, parent);
    lv_obj_class_init_obj(obj);
    return obj;
}

/*======================
 * Add/remove functions
 *=====================*/

lv_obj_t * lv_carousel_add_element(lv_obj_t * obj, uint8_t id)
{
    LV_LOG_INFO("begin");
    create_col_id = id;
    lv_carousel_t * carousel = (lv_carousel_t *) obj;

    lv_obj_t * element_obj = lv_obj_class_create_obj(&lv_carousel_element_class, obj);
    lv_obj_class_init_obj(element_obj);
    if(id == 0) {
        carousel->element_act = element_obj;
        lv_obj_add_state(carousel->element_act, LV_STATE_FOCUSED);
    }

    lv_obj_set_pos(carousel->sentinel, carousel->start_pos + ((carousel->element_width) * (create_col_id + 1)), 0);
    lv_obj_set_size(carousel->sentinel, carousel->start_pos, LV_PCT(100));
    lv_obj_update_layout(carousel->sentinel);
    lv_obj_remove_flag(element_obj, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    return element_obj;
}

/*=====================
 * Setter functions
 *====================*/

void lv_carousel_set_element_width(lv_obj_t * obj, int32_t w)
{
    lv_carousel_t * carousel = (lv_carousel_t *) obj;
    carousel->element_width = w;
}

void lv_obj_set_element(lv_obj_t * obj, lv_obj_t * element_obj, lv_anim_enable_t anim_en)
{
    lv_obj_update_layout(element_obj);
    int32_t tx = lv_obj_get_x(element_obj);
    int32_t ty = lv_obj_get_y(element_obj);

    lv_carousel_element_t * element = (lv_carousel_element_t *)element_obj;
    lv_carousel_t * carousel = (lv_carousel_t *) obj;
    lv_obj_t * element_pre = carousel->element_act;
    carousel->element_act = (lv_obj_t *)element;
    tx -= carousel->start_pos;

    lv_obj_remove_state(element_pre, LV_STATE_FOCUSED);
    lv_obj_add_state(carousel->element_act, LV_STATE_FOCUSED);
    lv_obj_scroll_to(obj, tx, ty, anim_en);
}

void lv_obj_set_element_id(lv_obj_t * obj, uint32_t id, lv_anim_enable_t anim_en)
{
    lv_obj_update_layout(obj);

    lv_carousel_t * carousel = (lv_carousel_t *) obj;
    int32_t tx = (carousel->element_width * id) + carousel->start_pos;

    uint32_t i;
    for(i = 0; i < lv_obj_get_child_count(obj); i++) {
        lv_obj_t * element_obj = lv_obj_get_child(obj, i);
        int32_t x = lv_obj_get_x(element_obj);
        if(x == tx) {
            lv_obj_set_element(obj, element_obj, anim_en);
            return;
        }
    }

    LV_LOG_WARN("No element found with at (%d) index", (int)id);
}

/*=====================
 * Getter functions
 *====================*/

lv_obj_t * lv_carousel_get_element_active(lv_obj_t * obj)
{
    lv_carousel_t * carousel = (lv_carousel_t *) obj;
    return carousel->element_act;
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

static void lv_carousel_constructor(const lv_obj_class_t * class_p, lv_obj_t * obj)
{
    LV_UNUSED(class_p);
    lv_carousel_t * carousel = (lv_carousel_t *) obj;
    lv_obj_set_size(obj, LV_PCT(100), LV_PCT(100));
    lv_carousel_set_element_width(obj, 100);
    lv_obj_add_event_cb(obj, carousel_event_cb, LV_EVENT_ALL, NULL);

    carousel->sentinel = lv_obj_create(obj);
    lv_obj_remove_style_all(carousel->sentinel);
    lv_obj_add_flag(obj, LV_OBJ_FLAG_SCROLL_ONE);
    lv_obj_set_scroll_snap_x(obj, LV_SCROLL_SNAP_CENTER);
}

static void lv_carousel_element_constructor(const lv_obj_class_t * class_p, lv_obj_t * obj)
{

    LV_UNUSED(class_p);
    lv_obj_t * parent = lv_obj_get_parent(obj);
    lv_carousel_t * carousel = (lv_carousel_t *) parent;
    int32_t carousel_width;
    lv_obj_set_size(obj, carousel->element_width, LV_PCT(100));
    lv_obj_update_layout(obj);  /*Be sure the size is correct*/

    carousel_width = lv_obj_get_content_width(parent);
    int32_t startpos = (carousel_width - carousel->element_width) / 2;
    carousel->start_pos = startpos;
    lv_obj_set_pos(obj, startpos + ((carousel->element_width) * create_col_id), 0);
}

static void carousel_event_cb(lv_event_t * e)
{
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t * obj = lv_event_get_target(e);
    lv_carousel_t * carousel = (lv_carousel_t *) obj;

    if(code == LV_EVENT_SCROLL_END) {
        lv_point_t scroll_end;
        lv_obj_get_scroll_end(obj, &scroll_end);
        int32_t left = scroll_end.x;

        lv_dir_t dir = LV_DIR_HOR;
        uint32_t child_count = lv_obj_get_child_count(obj);
        uint32_t i;
        lv_obj_t * element_pre = carousel->element_act;
        for(i = 0; i < child_count; i++) {
            lv_obj_t * element_obj = lv_obj_get_child(obj, i);
            int32_t x = lv_obj_get_x(element_obj) - carousel->start_pos;
            if(x == left) {
                lv_carousel_element_t * element = (lv_carousel_element_t *)element_obj;
                carousel->element_act = (lv_obj_t *)element;
                lv_obj_send_event(obj, LV_EVENT_VALUE_CHANGED, NULL);
                if(left == (int32_t)((child_count - 2) * carousel->element_width)) {
                    dir = LV_DIR_LEFT;
                }
                else if(left == 0) {
                    dir = LV_DIR_RIGHT;
                }
                break;
            }
        }
        lv_obj_remove_state(element_pre, LV_STATE_FOCUSED);
        lv_obj_add_state(carousel->element_act, LV_STATE_FOCUSED);
        lv_obj_set_scroll_dir(obj, dir);
    }
}
#endif /*LV_USE_CAROUSEL*/

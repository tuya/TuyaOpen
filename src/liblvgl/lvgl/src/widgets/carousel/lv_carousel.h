/**
 * @file lv_carousel.h
 *
 */

#ifndef LV_CAROUSEL_H
#define LV_CAROUSEL_H

#ifdef __cplusplus
extern "C" {
#endif

/*********************
 *      INCLUDES
 *********************/
#include "../../core/lv_obj.h"

#if LV_USE_CAROUSEL

/*********************
 *      DEFINES
 *********************/

/**********************
 *      TYPEDEFS
 **********************/
typedef struct {
    lv_obj_t obj;
    lv_obj_t * element_act;
    lv_obj_t * sentinel;
    int32_t element_width;
    int32_t start_pos;
} lv_carousel_t;

typedef struct {
    lv_obj_t obj;
    lv_dir_t dir;
} lv_carousel_element_t;

extern const lv_obj_class_t lv_carousel_class;
extern const lv_obj_class_t lv_carousel_element_class;

/**********************
 * GLOBAL PROTOTYPES
 **********************/

/**
 * Create a carousel object
 * @param parent pointer to an object, it will be the parent of the new carousel
 * @return pointer to the created carousel
 */
lv_obj_t * lv_carousel_create(lv_obj_t * parent);

lv_obj_t * lv_carousel_add_element(lv_obj_t * obj, uint8_t id);
void lv_carousel_set_element_width(lv_obj_t * obj, int32_t w);
void lv_obj_set_element(lv_obj_t * obj, lv_obj_t * element_obj, lv_anim_enable_t anim_en);
void lv_obj_set_element_id(lv_obj_t * obj, uint32_t id, lv_anim_enable_t anim_en);

lv_obj_t * lv_carousel_get_element_active(lv_obj_t * obj);

/*=====================
 * Other functions
 *====================*/

/**********************
 *      MACROS
 **********************/

#endif /*LV_USE_carousel*/

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif /*LV_carousel_H*/

/**
 * @file lv_malloc_core_tuya.c
 */

/*********************
 *      INCLUDES
 *********************/
#include "lvgl.h"
#include "tkl_memory.h"

/*********************
 *      DEFINES
 *********************/

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 *  STATIC PROTOTYPES
 **********************/

/**********************
 *  STATIC VARIABLES
 **********************/

/**********************
 *      MACROS
 **********************/

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

void lv_mem_init(void)
{
    return; /*Nothing to init*/
}

void lv_mem_deinit(void)
{
    return; /*Nothing to deinit*/
}

lv_mem_pool_t lv_mem_add_pool(void *mem, size_t bytes)
{
    /*Not supported*/
    LV_UNUSED(mem);
    LV_UNUSED(bytes);
    return NULL;
}

void lv_mem_remove_pool(lv_mem_pool_t pool)
{
    /*Not supported*/
    LV_UNUSED(pool);
    return;
}

void *lv_malloc_core(size_t size)
{
#if defined(ENABLE_EXT_RAM) && (ENABLE_EXT_RAM == 1)
    return tkl_system_psram_malloc(size);
#else
    return tkl_system_malloc(size);
#endif
}

void *lv_realloc_core(void *p, size_t new_size)
{
#if defined(ENABLE_EXT_RAM) && (ENABLE_EXT_RAM == 1)
    return tkl_system_psram_realloc(p, new_size);
#else
    return tkl_system_realloc(p, new_size);
#endif
}

void lv_free_core(void *p)
{
#if defined(ENABLE_EXT_RAM) && (ENABLE_EXT_RAM == 1)
    tkl_system_psram_free(p);
#else
    tkl_system_free(p);
#endif
}

void lv_mem_monitor_core(lv_mem_monitor_t *mon_p)
{
    /*Not supported*/
    LV_UNUSED(mon_p);
    return;
}

lv_result_t lv_mem_test_core(void)
{
    /*Not supported*/
    return LV_RESULT_OK;
}

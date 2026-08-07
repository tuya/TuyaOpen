/**
 * @file lv_port_disp.c
 *
 * @brief LVGL v9 display port - node management, rotation, and flush dispatch.
 *
 * Actual flush strategy is implemented in:
 *   - lv_port_disp_partial.c    (ENABLE_LVGL_PARTIAL_FLUSH == 1)
 *   - lv_port_disp_full_frame.c (otherwise)
 */

/*Copy this file as "lv_port_disp.c" and set this value to "1" to enable content*/
#if 1

/*********************
 *      INCLUDES
 *********************/
#include "lv_port_disp_internal.h"

/**********************
 *  STATIC PROTOTYPES
 **********************/
static LV_DISP_NODE_T *__find_lv_disp_node_by_hdl(TDL_DISP_HANDLE_T hdl);
static LV_DISP_NODE_T *__find_lv_disp_node_by_lv_disp(lv_display_t *lv_disp);
static LV_DISP_NODE_T *__create_lv_disp_dev(TDL_DISP_HANDLE_T dev_hdl);
static void __release_lv_disp_dev(LV_DISP_NODE_T *node);
static void disp_flush(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map);
static void __disp_dev_enable_update(LV_DISP_NODE_T *node);
static void __disp_dev_disable_update(LV_DISP_NODE_T *node);
static void __disp_dev_set_backlight(LV_DISP_NODE_T *node, uint8_t brightness);

/**********************
 *  STATIC VARIABLES
 **********************/
static struct tuya_list_head sg_lv_disp_list = LIST_HEAD_INIT(sg_lv_disp_list);

/**********************
 *      MACROS
 **********************/

/**********************
 *   GLOBAL FUNCTIONS
 **********************/
void lv_port_disp_init(char *device)
{
    LV_DISP_NODE_T *lv_disp_dev = NULL;
    TDL_DISP_HANDLE_T dev_hdl = NULL;

    dev_hdl = tdl_disp_find_dev(device);
    if(NULL == dev_hdl) {
        PR_ERR("display dev %s not found", device);
        return;
    }

    lv_disp_dev = __create_lv_disp_dev(dev_hdl);
    if(NULL == lv_disp_dev) {
        PR_ERR("create lv display dev failed");
        return;
    }

    tuya_list_add(&lv_disp_dev->node, &sg_lv_disp_list);
}

void lv_port_disp_deinit(char *device)
{
    LV_DISP_NODE_T *lv_disp_dev = NULL;
    TDL_DISP_HANDLE_T dev_hdl = NULL;

    dev_hdl = tdl_disp_find_dev(device);
    if(NULL == dev_hdl) {
        PR_ERR("display dev %s not found", device);
        return;
    }

    lv_disp_dev = __find_lv_disp_node_by_hdl(dev_hdl);
    if(NULL == lv_disp_dev) {
        PR_ERR("lv display dev not found");
        return;
    }

    tuya_list_del(&lv_disp_dev->node);

    __release_lv_disp_dev(lv_disp_dev);

    lv_disp_dev = NULL;
}

void disp_enable_update(lv_display_t *lv_disp)
{
    LV_DISP_NODE_T *node = __find_lv_disp_node_by_lv_disp(lv_disp);
    if(NULL == node) {
        PR_ERR("lv display dev not found");
        return;
    }

    __disp_dev_enable_update(node);
}

void disp_disable_update(lv_display_t *lv_disp)
{
    LV_DISP_NODE_T *node = __find_lv_disp_node_by_lv_disp(lv_disp);
    if(NULL == node) {
        PR_ERR("lv display dev not found");
        return;
    }

    __disp_dev_disable_update(node);
}

void disp_set_backlight(lv_display_t *lv_disp, uint8_t brightness)
{
    LV_DISP_NODE_T *node = __find_lv_disp_node_by_lv_disp(lv_disp);
    if(NULL == node) {
        PR_ERR("lv display dev not found");
        return;
    }

    __disp_dev_set_backlight(node, brightness);
}

lv_display_t *lv_port_get_lv_disp_by_name(char *device)
{
    TDL_DISP_HANDLE_T dev_hdl = NULL;
    LV_DISP_NODE_T *node = NULL;

    dev_hdl = tdl_disp_find_dev(device);
    if(NULL == dev_hdl) {
        PR_ERR("display dev %s not found", device);
        return NULL;
    }

    node = __find_lv_disp_node_by_hdl(dev_hdl);
    if(NULL == node) {
        PR_ERR("lv display dev not found");
        return NULL;
    }

    return node->lv_disp;
}


/**********************
 *   STATIC FUNCTIONS
 **********************/

static uint8_t *__disp_draw_buf_align_alloc(uint32_t size_bytes)
{
    uint8_t *buf_u8 = NULL;
    size_bytes += DISP_DRAW_BUF_ALIGN - 1;
    buf_u8 = (uint8_t *)LV_DRAW_BUF_ALLOC(size_bytes);
    if (buf_u8) {
        buf_u8 += DISP_DRAW_BUF_ALIGN - 1;
        buf_u8 = (uint8_t *)((uintptr_t)buf_u8 & ~(uintptr_t)(DISP_DRAW_BUF_ALIGN - 1));
    }

    return buf_u8;
}

static lv_color_format_t __disp_get_lv_color_format(TUYA_DISPLAY_PIXEL_FMT_E pixel_fmt)
{
    PR_NOTICE("pixel_fmt:%d", pixel_fmt);

    switch (pixel_fmt) {
        case TUYA_PIXEL_FMT_RGB565:
            return LV_COLOR_FORMAT_RGB565;
        case TUYA_PIXEL_FMT_RGB666:
            return LV_COLOR_FORMAT_RGB888;
        case TUYA_PIXEL_FMT_RGB888:
            return LV_COLOR_FORMAT_RGB888;
        case TUYA_PIXEL_FMT_MONOCHROME:
        case TUYA_PIXEL_FMT_I2:
            return LV_COLOR_FORMAT_RGB565;
        default:
            return LV_COLOR_FORMAT_RGB565;
    }
}

static LV_DISP_NODE_T *__find_lv_disp_node_by_hdl(TDL_DISP_HANDLE_T hdl)
{
    LV_DISP_NODE_T *lv_disp_node = NULL;
    struct tuya_list_head *pos = NULL;

    if (NULL == hdl) {
        return NULL;
    }

    tuya_list_for_each(pos, &sg_lv_disp_list) {
        lv_disp_node = tuya_list_entry(pos, LV_DISP_NODE_T, node);
        if (lv_disp_node->dev_hdl == hdl) {
            return lv_disp_node;
        }
    }

    return NULL;
}

static LV_DISP_NODE_T *__find_lv_disp_node_by_lv_disp(lv_display_t *lv_disp)
{
    LV_DISP_NODE_T *lv_disp_node = NULL;
    struct tuya_list_head *pos = NULL;

    if (NULL == lv_disp) {
        lv_disp = lv_display_get_default();
    }

    tuya_list_for_each(pos, &sg_lv_disp_list) {
        lv_disp_node = tuya_list_entry(pos, LV_DISP_NODE_T, node);
        if (lv_disp_node->lv_disp == lv_disp) {
            return lv_disp_node;
        }
    }

    return NULL;
}

static LV_DISP_NODE_T *__create_lv_disp_dev(TDL_DISP_HANDLE_T dev_hdl)
{
    uint8_t per_pixel_byte = 0;
    LV_DISP_NODE_T *lv_disp_node = NULL;
    OPERATE_RET rt = OPRT_OK;

    if (NULL == dev_hdl) {
        return NULL;
    }

    NEW_LIST_NODE(LV_DISP_NODE_T, lv_disp_node);
    if (NULL == lv_disp_node) {
        return NULL;
    }
    memset(lv_disp_node, 0, sizeof(LV_DISP_NODE_T));

    tal_mutex_create_init(&lv_disp_node->mutex);

    TUYA_CALL_ERR_GOTO(tdl_disp_dev_open(dev_hdl), __CREATE_ERR);

    lv_disp_node->dev_hdl = dev_hdl;
    TUYA_CALL_ERR_GOTO(tdl_disp_dev_get_info(lv_disp_node->dev_hdl, &lv_disp_node->dev_info), __CREATE_ERR);

    lv_port_flush_init(lv_disp_node);

    lv_display_t *disp = lv_display_create(lv_disp_node->dev_info.width, lv_disp_node->dev_info.height);
    lv_display_set_flush_cb(disp, disp_flush);
    lv_disp_node->lv_disp = disp;

    lv_color_format_t color_format = __disp_get_lv_color_format(lv_disp_node->dev_info.fmt);
    PR_NOTICE("lv_color_format:%d", color_format);
    lv_display_set_color_format(disp, color_format);

    per_pixel_byte = lv_color_format_get_size(color_format);

    uint32_t buf_len = (lv_disp_node->dev_info.height / LV_DRAW_BUF_PARTS) * lv_disp_node->dev_info.width * per_pixel_byte;

    lv_disp_node->buf_2_1 = __disp_draw_buf_align_alloc(buf_len);
    TUYA_CHECK_NULL_GOTO(lv_disp_node->buf_2_1, __CREATE_ERR);

    lv_disp_node->buf_2_2 = __disp_draw_buf_align_alloc(buf_len);
    TUYA_CHECK_NULL_GOTO(lv_disp_node->buf_2_2, __CREATE_ERR);

    lv_display_set_buffers(disp, lv_disp_node->buf_2_1, lv_disp_node->buf_2_2, buf_len, LV_DISPLAY_RENDER_MODE_PARTIAL);

    if (lv_disp_node->dev_info.rotation == TUYA_DISPLAY_ROTATION_90) {
        lv_display_set_rotation(disp, LV_DISPLAY_ROTATION_90);
    } else if (lv_disp_node->dev_info.rotation == TUYA_DISPLAY_ROTATION_180) {
        lv_display_set_rotation(disp, LV_DISPLAY_ROTATION_180);
    } else if (lv_disp_node->dev_info.rotation == TUYA_DISPLAY_ROTATION_270) {
        lv_display_set_rotation(disp, LV_DISPLAY_ROTATION_270);
    }

    PR_NOTICE("rotation:%d", lv_disp_node->dev_info.rotation);

    lv_disp_node->is_enable_flush = true;

    return lv_disp_node;

__CREATE_ERR:
    __release_lv_disp_dev(lv_disp_node);

    return NULL;
}

static void __release_lv_disp_dev(LV_DISP_NODE_T *node)
{
    if (NULL == node) {
        return;
    }

    if (node->lv_disp) {
        lv_display_delete(node->lv_disp);
        node->lv_disp = NULL;
    }

    if (node->buf_2_1) {
        LV_DRAW_BUF_FREE(node->buf_2_1);
        node->buf_2_1 = NULL;
    }

    if (node->buf_2_2) {
        LV_DRAW_BUF_FREE(node->buf_2_2);
        node->buf_2_2 = NULL;
    }

    if (node->rotate_buf) {
        LV_MEM_CUSTOM_FREE(node->rotate_buf);
        node->rotate_buf = NULL;
    }

    lv_port_flush_release(node);

    if (node->dev_hdl) {
        tdl_disp_dev_close(node->dev_hdl);
        node->dev_hdl = NULL;
    }

    if (node->mutex) {
        tal_mutex_release(node->mutex);
        node->mutex = NULL;
    }

    if (node) {
        tal_free(node);
        node = NULL;
    }
}

static void __disp_dev_enable_update(LV_DISP_NODE_T *node)
{
    tal_mutex_lock(node->mutex);

    lv_port_flush_on_enable(node);

    node->is_enable_flush = true;

    tal_mutex_unlock(node->mutex);
}

static void __disp_dev_disable_update(LV_DISP_NODE_T *node)
{
    tal_mutex_lock(node->mutex);

    node->is_enable_flush = false;

    tal_mutex_unlock(node->mutex);
}

static void __disp_dev_set_backlight(LV_DISP_NODE_T *node, uint8_t brightness)
{
    tdl_disp_set_brightness(node->dev_hdl, brightness);
}

static void disp_flush(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map)
{
    uint8_t *color_ptr = px_map;
    lv_area_t *target_area = (lv_area_t *)area;
    LV_DISP_NODE_T *node = __find_lv_disp_node_by_lv_disp(disp);

    if (NULL == node) {
        PR_ERR("lv display node not found");
        lv_display_flush_ready(disp);
        return;
    }

    if (node->mutex == NULL) {
        PR_ERR("lv display not ready (mutex null)");
        node->is_enable_flush = false;
        lv_display_flush_ready(disp);
        return;
    }

    tal_mutex_lock(node->mutex);

    if (node->is_enable_flush) {
        lv_color_format_t cf = lv_display_get_color_format(disp);
        lv_display_rotation_t rotation = lv_display_get_rotation(disp);
        lv_area_t rotated_area;

        if (rotation != LV_DISPLAY_ROTATION_0) {
            if (node->rotate_buf == NULL) {
                uint32_t per_pixel = lv_color_format_get_size(cf);
                uint32_t buf_len = (node->dev_info.height / LV_DRAW_BUF_PARTS) * node->dev_info.width * per_pixel;
                uint32_t alloc_size = buf_len + DISP_DRAW_BUF_ALIGN - 1;
                uint8_t *raw = (uint8_t *)LV_MEM_CUSTOM_ALLOC(alloc_size);
                if (raw) {
                    raw += DISP_DRAW_BUF_ALIGN - 1;
                    raw = (uint8_t *)((uintptr_t)raw & ~(uintptr_t)(DISP_DRAW_BUF_ALIGN - 1));
                }
                node->rotate_buf = raw;
            }
            if (node->rotate_buf != NULL) {
                rotated_area.x1 = area->x1;
                rotated_area.x2 = area->x2;
                rotated_area.y1 = area->y1;
                rotated_area.y2 = area->y2;

                lv_display_rotate_area(disp, &rotated_area);

                uint32_t src_stride = lv_draw_buf_width_to_stride(lv_area_get_width(area), cf);
                uint32_t dest_stride = lv_draw_buf_width_to_stride(lv_area_get_width(&rotated_area), cf);

                int32_t src_w = lv_area_get_width(area);
                int32_t src_h = lv_area_get_height(area);

                lv_draw_sw_rotate(px_map, node->rotate_buf, src_w, src_h, src_stride, dest_stride, rotation, cf);

                color_ptr = node->rotate_buf;
                target_area = &rotated_area;
            }
        }

        lv_port_flush_execute(node, disp, target_area, color_ptr);
    } else {
        lv_display_flush_ready(disp);
    }

    tal_mutex_unlock(node->mutex);
}

#else /*Enable this file at the top*/

/*This dummy typedef exists purely to silence -Wpedantic.*/
typedef int keep_pedantic_happy;
#endif

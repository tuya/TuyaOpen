/**
 * @file lv_port_disp.c
 *
 * @brief LVGL display port - node management, rotation, and flush dispatch.
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

static LV_DISP_NODE_T *__find_lv_disp_node_by_lv_disp(lv_disp_t *lv_disp);

static LV_DISP_NODE_T *__create_lv_disp_dev(TDL_DISP_HANDLE_T dev_hdl);

static void __release_lv_disp_dev(LV_DISP_NODE_T *node);

static void disp_flush(lv_disp_drv_t *disp_drv, const lv_area_t *area, lv_color_t *color_p);

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

void disp_enable_update(lv_disp_t *lv_disp)
{
    LV_DISP_NODE_T *node = __find_lv_disp_node_by_lv_disp(lv_disp);
    if(NULL == node) {
        PR_ERR("lv display dev not found");
        return;
    }

    __disp_dev_enable_update(node);
}

void disp_disable_update(lv_disp_t *lv_disp)
{
    LV_DISP_NODE_T *node = __find_lv_disp_node_by_lv_disp(lv_disp);
    if(NULL == node) {
        PR_ERR("lv display dev not found");
        return;
    }

    __disp_dev_disable_update(node);
}

void disp_set_backlight(lv_disp_t *lv_disp, uint8_t brightness)
{
    LV_DISP_NODE_T *node = __find_lv_disp_node_by_lv_disp(lv_disp);
    if(NULL == node) {
        PR_ERR("lv display dev not found");
        return;
    }

    __disp_dev_set_backlight(node, brightness);
}

lv_disp_t *lv_port_get_lv_disp_by_name(char *device)
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
        buf_u8 = (uint8_t *)((uint32_t)buf_u8 & ~(DISP_DRAW_BUF_ALIGN - 1));
    }

    return buf_u8;
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

static LV_DISP_NODE_T *__find_lv_disp_node_by_lv_disp(lv_disp_t *lv_disp)
{
    LV_DISP_NODE_T *lv_disp_node = NULL;
    struct tuya_list_head *pos = NULL;

    if (NULL == lv_disp) {
        lv_disp = lv_disp_get_default();
    }

    tuya_list_for_each(pos, &sg_lv_disp_list) {
        lv_disp_node = tuya_list_entry(pos, LV_DISP_NODE_T, node);
        if (lv_disp_node->lv_disp == lv_disp) {
            return lv_disp_node;
        }
    }

    return NULL;
}

static LV_DISP_NODE_T *__find_lv_disp_node_by_lv_disp_drv(lv_disp_drv_t *lv_disp_drv)
{
    LV_DISP_NODE_T *lv_disp_node = NULL;
    struct tuya_list_head *pos = NULL;

    if (NULL == lv_disp_drv) {
        lv_disp_t *lv_disp = lv_disp_get_default();
        if (lv_disp) {
            lv_disp_drv = lv_disp->driver;
        }
    }

    tuya_list_for_each(pos, &sg_lv_disp_list) {
        lv_disp_node = tuya_list_entry(pos, LV_DISP_NODE_T, node);
        if (&lv_disp_node->lv_disp_drv == lv_disp_drv) {
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

    per_pixel_byte = LV_COLOR_DEPTH / 8;

    uint32_t buf_len = (lv_disp_node->dev_info.height / LV_DRAW_BUF_PARTS) * lv_disp_node->dev_info.width * per_pixel_byte;

    lv_disp_node->buf_2_1 = __disp_draw_buf_align_alloc(buf_len);
    TUYA_CHECK_NULL_GOTO(lv_disp_node->buf_2_1, __CREATE_ERR);

    lv_disp_node->buf_2_2 = __disp_draw_buf_align_alloc(buf_len);
    TUYA_CHECK_NULL_GOTO(lv_disp_node->buf_2_2, __CREATE_ERR);

    lv_disp_draw_buf_init(&lv_disp_node->draw_buf_dsc, lv_disp_node->buf_2_1, lv_disp_node->buf_2_2, buf_len / per_pixel_byte);

    lv_disp_drv_init(&lv_disp_node->lv_disp_drv);

    lv_disp_node->lv_disp_drv.hor_res = lv_disp_node->dev_info.width;
    lv_disp_node->lv_disp_drv.ver_res = lv_disp_node->dev_info.height;

    lv_disp_node->lv_disp_drv.flush_cb = disp_flush;

    lv_disp_node->lv_disp_drv.draw_buf = &lv_disp_node->draw_buf_dsc;

    lv_disp_node->lv_disp = lv_disp_drv_register(&lv_disp_node->lv_disp_drv);

    if (lv_disp_node->dev_info.rotation == TUYA_DISPLAY_ROTATION_90) {
        lv_disp_set_rotation(lv_disp_node->lv_disp, LV_DISP_ROT_90);
    } else if (lv_disp_node->dev_info.rotation == TUYA_DISPLAY_ROTATION_180) {
        lv_disp_set_rotation(lv_disp_node->lv_disp, LV_DISP_ROT_180);
    } else if (lv_disp_node->dev_info.rotation == TUYA_DISPLAY_ROTATION_270) {
        lv_disp_set_rotation(lv_disp_node->lv_disp, LV_DISP_ROT_270);
    }

    if (lv_disp_node->dev_info.rotation != TUYA_DISPLAY_ROTATION_0) {
        lv_disp_node->rotate_buf = __disp_draw_buf_align_alloc(buf_len);
        TUYA_CHECK_NULL_GOTO(lv_disp_node->rotate_buf, __CREATE_ERR);
    }

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
        lv_disp_remove(node->lv_disp);
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

static void __disp_draw_buf_rotate(TDL_DISP_DEV_INFO_T *dev_info, lv_disp_rot_t rot, lv_area_t *area,
                                   uint8_t *src_buf, uint8_t *dst_buf)
{
    TDL_DISP_RECT_T rect = {0};
    TUYA_DISPLAY_ROTATION_E disp_rot = TUYA_DISPLAY_ROTATION_0;
    TDL_DISP_FRAME_BUFF_T in_fb, out_fb;

    if (NULL == dev_info || NULL == area || NULL == src_buf || NULL == dst_buf) {
        PR_ERR("Invalid parameters: area or src_buf or dst_buf is NULL");
        return;
    }

    if (rot == LV_DISP_ROT_NONE) {
        return;
    }

    rect.x0 = area->x1;
    rect.y0 = area->y1;
    rect.x1 = area->x2;
    rect.y1 = area->y2;

    disp_rot = (TUYA_DISPLAY_ROTATION_E)rot;

    memset(&in_fb, 0, sizeof(TDL_DISP_FRAME_BUFF_T));
    in_fb.fmt    = dev_info->fmt;
    in_fb.width  = lv_area_get_width(area);
    in_fb.height = lv_area_get_height(area);
    in_fb.frame  = src_buf;

    memset(&out_fb, 0, sizeof(TDL_DISP_FRAME_BUFF_T));
    out_fb.fmt    = dev_info->fmt;
    out_fb.width  = lv_area_get_width(area);
    out_fb.height = lv_area_get_height(area);
    out_fb.frame  = dst_buf;

    tdl_disp_rotate_rect(disp_rot, dev_info->width, dev_info->height, &rect);

    tdl_disp_draw_rotate(disp_rot, &in_fb, &out_fb, false);

    area->x1 = rect.x0;
    area->y1 = rect.y0;
    area->x2 = rect.x1;
    area->y2 = rect.y1;
}

static void disp_flush(lv_disp_drv_t *disp_drv, const lv_area_t *area, lv_color_t *color_p)
{
    uint8_t *color_ptr = (uint8_t *)color_p;
    lv_area_t *target_area = (lv_area_t *)area;
    LV_DISP_NODE_T *node = __find_lv_disp_node_by_lv_disp_drv(disp_drv);

    if (NULL == node) {
        PR_ERR("lv display node not found");
        lv_disp_flush_ready(disp_drv);
        return;
    }

    tal_mutex_lock(node->mutex);

    if (node->is_enable_flush) {
        lv_disp_rot_t rotation = lv_disp_get_rotation(node->lv_disp);
        if (rotation != LV_DISP_ROT_NONE) {
            if (node->rotate_buf == NULL) {
                uint32_t buf_len = (node->dev_info.height / LV_DRAW_BUF_PARTS) * node->dev_info.width * (LV_COLOR_DEPTH / 8);
                node->rotate_buf = __disp_draw_buf_align_alloc(buf_len);
            }
            if (node->rotate_buf != NULL) {
                __disp_draw_buf_rotate(&node->dev_info, rotation, target_area, color_ptr, node->rotate_buf);
                color_ptr = node->rotate_buf;
            }
        }

        lv_port_flush_execute(node, disp_drv, target_area, color_ptr);
    } else {
        lv_disp_flush_ready(disp_drv);
    }

    tal_mutex_unlock(node->mutex);
}

#else /*Enable this file at the top*/

/*This dummy typedef exists purely to silence -Wpedantic.*/
typedef int keep_pedantic_happy;
#endif

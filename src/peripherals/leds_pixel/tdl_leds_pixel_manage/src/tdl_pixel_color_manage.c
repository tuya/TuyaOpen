/**
 * @file tdl_pixel_color_manage.c
 * @brief TDL layer color management implementation for LED pixel devices
 *
 * This source file implements the TDL (Tuya Device Layer) color management functionality
 * for LED pixel devices. It provides color manipulation functions including setting
 * pixel colors, color shifting operations, pixel color copying, and specialized
 * color effects. The module manages pixel color buffers and provides high-level
 * abstractions for LED strip color operations across different pixel controller types.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 *
 */
#include <string.h>

#include "tal_log.h"
#include "tal_memory.h"
#include "tdl_pixel_color_manage.h"

/***********************************************************
*************************private include********************
***********************************************************/
#include "tdl_pixel_driver.h"
#include "tdl_pixel_struct.h"

/***********************************************************
*************************micro define***********************
***********************************************************/

/***********************************************************
***********************function define**********************
***********************************************************/
static void __tdl_pixel_only_set_cw(PIXEL_HANDLE_T handle, uint16_t *buff, PIXEL_COLOR_TP_E tp, uint8_t color_num,
                                    uint32_t index, PIXEL_COLOR_T *color)
{
    uint32_t pos = 0;
    PIXEL_DEV_NODE_T *device = (PIXEL_DEV_NODE_T *)handle;

    if (NULL == buff) {
        PR_ERR("buff is null");
        return;
    }

    if (((tp & COLOR_C_BIT) == 0) && ((tp & COLOR_W_BIT) == 0)) {
        return;
    }

    pos = color_num * index + 3;

    switch (tp) {
    case PIXEL_COLOR_TP_RGBC:
        buff[pos++] = color->cold * device->color_maximum / device->pixel_resolution;
        break;
    case PIXEL_COLOR_TP_RGBW:
        buff[pos++] = color->warm * device->color_maximum / device->pixel_resolution;
        break;
    case PIXEL_COLOR_TP_RGBCW:
        buff[pos++] = color->cold * device->color_maximum / device->pixel_resolution;
        buff[pos++] = color->warm * device->color_maximum / device->pixel_resolution;
        break;
    default:
        break;
    }

    return;
}

static void __tdl_pixel_set_color(PIXEL_HANDLE_T handle, uint16_t *buff, PIXEL_COLOR_TP_E tp, uint8_t color_num,
                                    uint32_t index, PIXEL_COLOR_T *color)
{
    uint32_t pos = 0;
    PIXEL_DEV_NODE_T *device = (PIXEL_DEV_NODE_T *)handle;

    if (NULL == buff) {
        PR_ERR("buff is null");
        return;
    }

    pos = color_num * index;

    switch (tp) {
    case PIXEL_COLOR_TP_RGB:
        buff[pos++] = color->red * device->color_maximum / device->pixel_resolution;
        buff[pos++] = color->green * device->color_maximum / device->pixel_resolution;
        buff[pos++] = color->blue * device->color_maximum / device->pixel_resolution;
        break;
    case PIXEL_COLOR_TP_RGBC:
        buff[pos++] = color->red * device->color_maximum / device->pixel_resolution;
        buff[pos++] = color->green * device->color_maximum / device->pixel_resolution;
        buff[pos++] = color->blue * device->color_maximum / device->pixel_resolution;
        if (!device->white_color_control) {
            buff[pos++] = color->cold * device->color_maximum / device->pixel_resolution;
        }
        break;
    case PIXEL_COLOR_TP_RGBW:
        buff[pos++] = color->red * device->color_maximum / device->pixel_resolution;
        buff[pos++] = color->green * device->color_maximum / device->pixel_resolution;
        buff[pos++] = color->blue * device->color_maximum / device->pixel_resolution;
        if (!device->white_color_control) {
            buff[pos++] = color->warm * device->color_maximum / device->pixel_resolution;
        }
        break;
    case PIXEL_COLOR_TP_RGBCW:
        buff[pos++] = color->red * device->color_maximum / device->pixel_resolution;
        buff[pos++] = color->green * device->color_maximum / device->pixel_resolution;
        buff[pos++] = color->blue * device->color_maximum / device->pixel_resolution;
        if (!device->white_color_control) {
            buff[pos++] = color->cold * device->color_maximum / device->pixel_resolution;
            buff[pos++] = color->warm * device->color_maximum / device->pixel_resolution;
        }
        break;
    default:
        break;
    }

    return;
}

static void __tdl_pixel_get_color(PIXEL_HANDLE_T handle, uint16_t *buff, PIXEL_COLOR_TP_E tp, uint8_t color_num,
                                    uint32_t index, PIXEL_COLOR_T *color)
{
    uint32_t pos = 0;
    PIXEL_DEV_NODE_T *device = (PIXEL_DEV_NODE_T *)handle;

    if (NULL == buff) {
        PR_ERR("buff is null");
        return;
    }

    pos = color_num * index;

    switch (tp) {
    case PIXEL_COLOR_TP_RGB:
        color->red = buff[pos++] * device->pixel_resolution / device->color_maximum;
        color->green = buff[pos++] * device->pixel_resolution / device->color_maximum;
        color->blue = buff[pos++] * device->pixel_resolution / device->color_maximum;
        break;
    case PIXEL_COLOR_TP_RGBC:
        color->red = buff[pos++] * device->pixel_resolution / device->color_maximum;
        color->green = buff[pos++] * device->pixel_resolution / device->color_maximum;
        color->blue = buff[pos++] * device->pixel_resolution / device->color_maximum;
        color->cold = buff[pos++] * device->pixel_resolution / device->color_maximum;
        break;
    case PIXEL_COLOR_TP_RGBW:
        color->red = buff[pos++] * device->pixel_resolution / device->color_maximum;
        color->green = buff[pos++] * device->pixel_resolution / device->color_maximum;
        color->blue = buff[pos++] * device->pixel_resolution / device->color_maximum;
        color->warm = buff[pos++] * device->pixel_resolution / device->color_maximum;
        break;
    case PIXEL_COLOR_TP_RGBCW:
        color->red = buff[pos++] * device->pixel_resolution / device->color_maximum;
        color->green = buff[pos++] * device->pixel_resolution / device->color_maximum;
        color->blue = buff[pos++] * device->pixel_resolution / device->color_maximum;
        color->cold = buff[pos++] * device->pixel_resolution / device->color_maximum;
        color->warm = buff[pos++] * device->pixel_resolution / device->color_maximum;
        break;
    default:
        break;
    }

    return;
}

static OPERATE_RET __tdl_pixel_right_shift(uint16_t *buff, uint8_t color_num, int32_t start, int32_t end, int32_t step)
{
    int32_t i, temp_len = 0, rang_size = 0;
    uint16_t *temp = NULL;

    if (NULL == buff || end < start || step > end - start) {
        return OPRT_INVALID_PARM;
    }

    if (end == start) {
        return OPRT_OK;
    }

    temp_len = step * color_num * sizeof(uint16_t);
    temp = (uint16_t *)tal_malloc(temp_len);
    if (temp == NULL) {
        PR_ERR("malloc failed !");

        return OPRT_MALLOC_FAILED;
    }
    memcpy(temp, buff + color_num * (end - step + 1), temp_len);

    rang_size = end - start + 1;
    for (i = 0; i < rang_size - step; i++) {
        memmove(buff + color_num * (end - i), buff + color_num * (end - step - i), color_num * sizeof(uint16_t));
    }
    memcpy(buff + color_num * start, temp, temp_len);

    tal_free(temp);

    return OPRT_OK;
}

static OPERATE_RET __tdl_pixel_left_shift(uint16_t *buff, uint8_t color_num, int32_t start, int32_t end, int32_t step)
{
    int32_t i, temp_len = 0, rang_size = 0;
    uint16_t *temp = NULL;

    if (NULL == buff || end < start || step > end - start) {
        return OPRT_INVALID_PARM;
    }

    if (end == start) {
        return OPRT_OK;
    }

    temp_len = step * color_num * sizeof(uint16_t);
    temp = (uint16_t *)tal_malloc(temp_len);
    if (temp == NULL) {
        PR_ERR("malloc failed !");
        return OPRT_MALLOC_FAILED;
    }
    memcpy(temp, buff + color_num * start, temp_len);

    rang_size = end - start + 1;
    for (i = 0; i < rang_size - step; i++) {
        memmove(buff + color_num * (start + i), buff + color_num * (start + i + step), color_num * sizeof(uint16_t));
    }
    memcpy(buff + color_num * (end - step + 1), temp, temp_len);

    tal_free(temp);

    return OPRT_OK;
}

/**
 * @brief    Set pixel segment color (single)
 *
 * @param[in]    handle           Device handle
 * @param[in]    index_start      Pixel start index
 * @param[in]    pixel_num        Pixel segment length
 * @param[in]    color            Target color
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
int tdl_pixel_set_single_color(PIXEL_HANDLE_T handle, uint32_t index_start, uint32_t pixel_num, PIXEL_COLOR_T *color)
{
    int32_t i = 0;
    PIXEL_DEV_NODE_T *device = (PIXEL_DEV_NODE_T *)handle;

    if (NULL == handle || NULL == color) {
        PR_ERR("param err <handle:%p, color:%p>", handle, color);
        return OPRT_INVALID_PARM;
    }

    if (0 == device->flag.is_start) {
        return OPRT_COM_ERROR;
    }

    if (index_start >= device->pixel_num || index_start + pixel_num > device->pixel_num) {
        PR_ERR("param err <index_start:%u, pixel_num:%u, device->pixel_num:%u>", index_start, pixel_num, device->pixel_num);
        return OPRT_INVALID_PARM;
    }

    tal_mutex_lock(device->mutex);
    for (i = 0; i < pixel_num; i++) {
        __tdl_pixel_set_color(handle, device->pixel_buffer, device->pixel_color, device->color_num, index_start + i,
                              color);
    }
    tal_mutex_unlock(device->mutex);

    return OPRT_OK;
}

/**
 * @brief        Set pixel segment color (multiple)
 *
 * @param[in]    handle           Device handle
 * @param[in]    index_start      Pixel start index
 * @param[in]    pixel_num        Pixel segment length
 * @param[in]    color_arr        Target color array
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
int tdl_pixel_set_multi_color(PIXEL_HANDLE_T handle, uint32_t index_start, uint32_t pixel_num, PIXEL_COLOR_T *color_arr)
{
    int32_t i = 0;
    PIXEL_DEV_NODE_T *device = (PIXEL_DEV_NODE_T *)handle;

    if (NULL == handle || NULL == color_arr) {
        return OPRT_INVALID_PARM;
    }

    if (0 == device->flag.is_start) {
        return OPRT_COM_ERROR;
    }

    if (index_start >= device->pixel_num || index_start + pixel_num > device->pixel_num) {
        return OPRT_INVALID_PARM;
    }

    tal_mutex_lock(device->mutex);
    for (i = 0; i < pixel_num; i++) {
        __tdl_pixel_set_color(handle, device->pixel_buffer, device->pixel_color, device->color_num, index_start + i,
                              &color_arr[i]);
    }
    tal_mutex_unlock(device->mutex);

    return OPRT_OK;
}

/**
 * @brief       Set pixel segment color with background color
 *
 * @param[in]     handle           Device handle
 * @param[in]    index_start      Pixel start index
 * @param[in]    pixel_num        Pixel segment length
 * @param[in]    backcolor        Background color
 * @param[in]    color            Target color
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
int tdl_pixel_set_single_color_with_backcolor(PIXEL_HANDLE_T handle, uint32_t index_start, uint32_t pixel_num,
                                              PIXEL_COLOR_T *backcolor, PIXEL_COLOR_T *color)
{
    int32_t i = 0;
    PIXEL_DEV_NODE_T *device = (PIXEL_DEV_NODE_T *)handle;

    if (NULL == handle || NULL == color || NULL == backcolor) {
        return OPRT_INVALID_PARM;
    }

    if (0 == device->flag.is_start) {
        return OPRT_COM_ERROR;
    }

    if (index_start >= device->pixel_num || index_start + pixel_num > device->pixel_num) {
        return OPRT_INVALID_PARM;
    }

    tal_mutex_lock(device->mutex);
    // background color
    for (i = 0; i < device->pixel_num; i++) {
        __tdl_pixel_set_color(handle, device->pixel_buffer, device->pixel_color, device->color_num, i, backcolor);
    }
    // dest color
    for (i = 0; i < pixel_num; i++) {
        __tdl_pixel_set_color(handle, device->pixel_buffer, device->pixel_color, device->color_num, index_start + i,
                              color);
    }
    tal_mutex_unlock(device->mutex);

    return OPRT_OK;
}

/**
 * @brief       Cyclically shift pixel colors
 *
 * @param[in]    handle           Device handle
 * @param[in]    dir              Move direction
 * @param[in]    index_start      Start index
 * @param[in]    end_start        End index
 * @param[in]    move_step        Move step
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
int tdl_pixel_cycle_shift_color(PIXEL_HANDLE_T handle, PIXEL_SHIFT_DIR_T dir, uint32_t index_start, uint32_t index_end,
                                uint32_t move_step)
{
    PIXEL_DEV_NODE_T *device = (PIXEL_DEV_NODE_T *)handle;
    OPERATE_RET op_ret = OPRT_OK;

    if (NULL == handle || dir > PIXEL_SHIFT_LEFT) {
        return OPRT_INVALID_PARM;
    }

    if (0 == device->flag.is_start) {
        return OPRT_COM_ERROR;
    }

    if (index_start >= device->pixel_num || index_end >= device->pixel_num) {
        return OPRT_INVALID_PARM;
    }

    tal_mutex_lock(device->mutex);
    if (PIXEL_SHIFT_RIGHT == dir) {
        op_ret = __tdl_pixel_right_shift(device->pixel_buffer, device->color_num, index_start, index_end, move_step);
    } else {
        op_ret = __tdl_pixel_left_shift(device->pixel_buffer, device->color_num, index_start, index_end, move_step);
    }
    tal_mutex_unlock(device->mutex);

    return op_ret;
}

/**
 * @brief        Mirror cyclically shift pixel colors
 *
 * @param[in]    handle           Device handle
 * @param[in]    dir              Move direction
 * @param[in]    index_start      Start index
 * @param[in]    end_start        End index
 * @param[in]    move_step        Move step
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
int tdl_pixel_mirror_cycle_shift_color(PIXEL_HANDLE_T handle, PIXEL_M_SHIFT_DIR_T dir, uint32_t index_start,
                                       uint32_t index_end, uint32_t move_step)
{
    int32_t half_len = 0;
    PIXEL_DEV_NODE_T *device = (PIXEL_DEV_NODE_T *)handle;
    OPERATE_RET op_ret = OPRT_OK;

    if (NULL == handle || dir > PIXEL_SHIFT_FAR) {
        return OPRT_INVALID_PARM;
    }

    if (0 == device->flag.is_start) {
        return OPRT_COM_ERROR;
    }

    if (index_start >= device->pixel_num || index_end >= device->pixel_num || index_start >= index_end) {
        return OPRT_INVALID_PARM;
    }

    half_len = (index_end - index_start + 1) / 2;

    tal_mutex_lock(device->mutex);
    if (PIXEL_SHIFT_CLOSE == dir) {
        op_ret = __tdl_pixel_right_shift(device->pixel_buffer, device->color_num, index_start,
                                         index_start + half_len - 1, move_step);
        if (op_ret != OPRT_OK) {
            goto END;
        }

        op_ret = __tdl_pixel_left_shift(device->pixel_buffer, device->color_num, index_start + half_len,
                                        index_start + 2 * half_len - 1, move_step);
        if (op_ret != OPRT_OK) {
            goto END;
        }

    } else {
        op_ret = __tdl_pixel_left_shift(device->pixel_buffer, device->color_num, index_start,
                                        index_start + half_len - 1, move_step);
        if (op_ret != OPRT_OK) {
            goto END;
        }

        op_ret = __tdl_pixel_right_shift(device->pixel_buffer, device->color_num, index_start + half_len,
                                         index_start + 2 * half_len - 1, move_step);
        if (op_ret != OPRT_OK) {
            goto END;
        }
    }

END:
    tal_mutex_unlock(device->mutex);

    return op_ret;
}

/**
 * @brief        Get pixel color
 *
 * @param[in]    handle           Device handle
 * @param[in]    index            Index
 * @param[out]   color            Color
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
int tdl_pixel_get_color(PIXEL_HANDLE_T handle, uint32_t index, PIXEL_COLOR_T *color)
{
    PIXEL_DEV_NODE_T *device = (PIXEL_DEV_NODE_T *)handle;

    if (NULL == handle || NULL == color) {
        return OPRT_INVALID_PARM;
    }

    if (0 == device->flag.is_start) {
        return OPRT_COM_ERROR;
    }

    if (index >= device->pixel_num) {
        return OPRT_INVALID_PARM;
    }

    __tdl_pixel_get_color(handle, device->pixel_buffer, device->pixel_color, device->color_num, index, color);

    return OPRT_OK;
}

/**
 * @brief    Set all pixel colors (single)
 *
 * @param[in]    handle           Device handle
 * @param[in]    color            Target color
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
int tdl_pixel_set_single_color_all(PIXEL_HANDLE_T handle, PIXEL_COLOR_T *color)
{
    int32_t i = 0;
    PIXEL_DEV_NODE_T *device = (PIXEL_DEV_NODE_T *)handle;

    if (NULL == handle || NULL == color) {
        return OPRT_INVALID_PARM;
    }

    if (0 == device->flag.is_start) {
        return OPRT_COM_ERROR;
    }

    tal_mutex_lock(device->mutex);
    for (i = 0; i < device->pixel_num; i++) {
        __tdl_pixel_set_color(handle, device->pixel_buffer, device->pixel_color, device->color_num, i, color);
    }
    tal_mutex_unlock(device->mutex);

    return OPRT_OK;
}

/**
 * @brief    Set only white light color, not color light
 *
 * @param[in]    handle           Device handle
 * @param[in]    color            Target color
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
int tdl_pixel_set_single_white_all(PIXEL_HANDLE_T handle, PIXEL_COLOR_T *color)
{
    int32_t i = 0;
    PIXEL_DEV_NODE_T *device = (PIXEL_DEV_NODE_T *)handle;

    if (NULL == handle || NULL == color) {
        return OPRT_INVALID_PARM;
    }

    if (0 == device->flag.is_start) {
        return OPRT_COM_ERROR;
    }

    tal_mutex_lock(device->mutex);
    for (i = 0; i < device->pixel_num; i++) {
        __tdl_pixel_only_set_cw(handle, device->pixel_buffer, device->pixel_color, device->color_num, i, color);
    }
    tal_mutex_unlock(device->mutex);

    return OPRT_OK;
}

/**
 * @brief        Copy pixel colors
 *
 * @param[in]    handle           Device handle
 * @param[in]    dst_idx          Destination index
 * @param[in]    src_idx          Source index
 * @param[in]    len              Number of pixels to copy
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
int tdl_pixel_copy_color(PIXEL_HANDLE_T handle, uint32_t dst_idx, uint32_t src_idx, uint32_t len)
{
    int32_t copy_len = 0;

    PIXEL_DEV_NODE_T *device = (PIXEL_DEV_NODE_T *)handle;

    if (NULL == handle || 0 == len) {
        return OPRT_INVALID_PARM;
    }

    if (0 == device->flag.is_start) {
        return OPRT_COM_ERROR;
    }

    if (src_idx >= device->pixel_num || dst_idx >= device->pixel_num || (src_idx + len) > device->pixel_num ||
        (dst_idx + len) > device->pixel_num) {
        return OPRT_INVALID_PARM;
    }

    copy_len = device->color_num * sizeof(uint16_t) * len;

    memmove((unsigned char *)&device->pixel_buffer[dst_idx * device->color_num],
            (unsigned char *)&device->pixel_buffer[src_idx * device->color_num], copy_len);

    return OPRT_OK;
}

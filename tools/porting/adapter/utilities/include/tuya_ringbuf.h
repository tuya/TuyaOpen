/**
 * @file tuya_ringbuff.h
 * @brief Common process - ring buff
 * @version 1.0.0
 * @date 2021-06-03
 *
 * @copyright Copyright 2018-2021 Tuya Inc. All Rights Reserved.
 *
 */
#ifndef __TUYA_RINGBUF_H__
#define __TUYA_RINGBUF_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "tuya_cloud_types.h"

typedef void *TUYA_RINGBUFF_T;

typedef enum {
    OVERFLOW_STOP_TYPE = 0, ///< unread buff area will not be overwritten when writing overflow
    OVERFLOW_COVERAGE_TYPE, ///< unread buff area will be overwritten when writing overflow
} RINGBUFF_TYPE_E;

/**
 * @brief ringbuff create
 *
 * @param[in]   len:      ringbuff length
 * @param[in]   type:     ringbuff type
 * @param[in]   ringbuff: ringbuff handle
 * @return  TRUE/ FALSE
 */
OPERATE_RET tuya_ring_buff_create(uint32_t len, RINGBUFF_TYPE_E type, TUYA_RINGBUFF_T *ringbuff);

/**
 * @brief ringbuff free
 *
 * @param[in]   ringbuff: ringbuff handle
 * @return  TRUE/ FALSE
 */
OPERATE_RET tuya_ring_buff_free(TUYA_RINGBUFF_T ringbuff);

/**
 * @brief ringbuff reset
 * this API not free buff
 *
 * @param[in]   ringbuff: ringbuff handle
 * @return  none
 */
OPERATE_RET tuya_ring_buff_reset(TUYA_RINGBUFF_T ringbuff);

/**
 * @brief ringbuff free size get
 *
 * @param[in]   ringbuff: ringbuff handle
 * @return  size of ringbuff not used
 */
uint32_t tuya_ring_buff_free_size_get(TUYA_RINGBUFF_T ringbuff);

/**
 * @brief ringbuff used size get
 *
 * @param[in]   ringbuff: ringbuff handle
 * @return  size of ringbuff used
 */
uint32_t tuya_ring_buff_used_size_get(TUYA_RINGBUFF_T ringbuff);

/**
 * @brief ringbuff data read
 *
 * @param[in]   ringbuff: ringbuff handle
 * @param[in]   data:     point to the data read cache
 * @param[in]   len:      read len
 * @return  length of the data read
 */
uint32_t tuya_ring_buff_read(TUYA_RINGBUFF_T ringbuff, void *data, uint32_t len);

/**
 * @brief ringbuff data peek
 * this API read data but not output position
 *
 * @param[in]   ringbuff: ringbuff handle
 * @param[in]   data:     point to the data read cache
 * @param[in]   len:      read len
 * @return  length of the data read
 */
uint32_t tuya_ring_buff_peek(TUYA_RINGBUFF_T ringbuff, void *data, uint32_t len);

/**
 * @brief ringbuff data write
 *
 * @param[in]   ringbuff: ringbuff handle
 * @param[in]   data:     point to the data to be write
 * @param[in]   len:      write len
 * @return  length of the data write
 */
uint32_t tuya_ring_buff_write(TUYA_RINGBUFF_T ringbuff, const void *data, uint32_t len);

#ifdef __cplusplus
}
#endif

#endif

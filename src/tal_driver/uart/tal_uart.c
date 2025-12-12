/**
 * @file tal_uart.c
 * @brief UART device management and operations for SDK.
 *
 * This file provides the implementation of UART device management, including
 * initialization, data transmission, and reception through UART peripherals. It
 * supports both blocking and non-blocking operations, configurable buffer
 * sizes, and optional asynchronous write capabilities. The implementation
 * ensures thread safety and efficient handling of UART data streams.
 *
 * @copyright Copyright (c) 2021-2024 Tuya Inc. All Rights Reserved.
 *
 */
#include "tkl_uart.h"
#include "tal_uart.h"
#include "tuya_slist.h"
#include "tuya_ringbuf.h"
#include "tal_api.h"

typedef struct uart_dev_node {
    SLIST_HEAD node;
    uint32_t port_num;
    uint32_t open_mode;
    SEM_HANDLE rx_ring_sem;
    TUYA_RINGBUFF_T rx_ring;
#ifdef CONFIG_UART_ASYNC_WRITE
    TKL_SEM_HANDLE tx_ring_sem;
    TUYA_RINGBUFF_T tx_ring;
#endif
    uint16_t wait_rx_flag;
    uint16_t wait_tx_flag;
    SEM_HANDLE rx_block_sem;
    SEM_HANDLE tx_block_sem;
} TAL_UART_DEV;

struct single_mutext_list {
    MUTEX_HANDLE mutex;
    SLIST_HEAD head;
};

struct single_mutext_list g_uart_list;

typedef void (*UART_ISR_CALL_BACK)(void *);

TAL_UART_DEV *uart_list_get_one_node(TUYA_UART_NUM_E port_num)
{
    SLIST_HEAD *node_index = &g_uart_list.head;
    TAL_UART_DEV *uart_dev;

    while (node_index->next != NULL) {
        uart_dev = (TAL_UART_DEV *)node_index->next;
        if (uart_dev->port_num == port_num) {
            return uart_dev;
        }
        node_index = node_index->next;
    }

    return NULL;
}

OPERATE_RET uart_list_add_one_node(TAL_UART_DEV *uart_info)
{
    OPERATE_RET ret = tal_mutex_lock(g_uart_list.mutex);
    if (ret != OPRT_OK) {
        return ret;
    }
    tuya_slist_add_head(&g_uart_list.head, &uart_info->node);

    tal_mutex_unlock(g_uart_list.mutex);
    return OPRT_OK;
}

OPERATE_RET uart_list_delete_one_node(TAL_UART_DEV *uart_info)
{
    OPERATE_RET ret = tal_mutex_lock(g_uart_list.mutex);
    if (ret != OPRT_OK) {
        return ret;
    }

    tuya_slist_del(&g_uart_list.head, &uart_info->node);

    tal_mutex_unlock(g_uart_list.mutex);
    return OPRT_OK;
}

#ifdef CONFIG_UART_ASYNC_WRITE
void uart_tx_chars_in_isr(uint32_t port_num)
{
    TAL_UART_DEV *uart_info = uart_list_get_one_node(port_num);
    if (uart_info == NULL) {
        return;
    }

    ring_buffer_s *tx_ring = uart_info->tx_ring;
    uint8_t tx_byte;
    uint8_t tx_count = 0;

    while (1) {
        ret = tuya_ring_buff_read(tx_ring, &tx_byte, 1);
        if (ret != 1) {
            break;
        }

        ret = tkl_uart_write(port_num, &tx_byte, 1);
        if (ret != 1) {
            break;
        }

        tx_count++;
    }

    if ((uart_info->open_mode & O_BLOCK) && (tx_count > 0)) {
        if (uart_info->wait_rx_flag == TRUE) {
            uart_info->wait_rx_flag = FALSE;
            tal_semaphore_post(uart_info->tx_block_sem);
        }
    }
}
#endif

void uart_rx_chars_in_isr(TUYA_UART_NUM_E port_num)
{
    TAL_UART_DEV *uart_info = uart_list_get_one_node(port_num);
    if (uart_info == NULL) {
        return;
    }

    uint8_t rx_char;
    int ret = 0;
    uint32_t rx_bytes = 0;

    /*
     * When the software buffer is full, the data read will not be written into
     * the software buffer. However, it will continue to read the content of the
     * hardware buffer until it is empty.
     */
    while (1) {
        ret = tkl_uart_read(port_num, &rx_char, 1);
        if (ret != 1) {
            break;
        }

        ret = tuya_ring_buff_write(uart_info->rx_ring, &rx_char, 1);
        if (ret != 1) {
            break;
        }

        rx_bytes++;

#if OPERATING_SYSTEM == SYSTEM_LINUX
        break;
#endif
    }

#ifdef CONFIG_UART_FLOW_CONTRAL

#endif

    if ((rx_bytes >= 1) && (uart_info->wait_rx_flag == TRUE)) {
        uart_info->wait_rx_flag = FALSE;
        tal_semaphore_post(uart_info->rx_block_sem);
    }

    return;
}

void uart_free_source(TAL_UART_DEV *uart_info)
{
    if (uart_info->rx_block_sem != NULL) {
        tal_semaphore_release(uart_info->rx_block_sem);
    }

    if (uart_info->tx_block_sem != NULL) {
        tal_semaphore_release(uart_info->tx_block_sem);
    }

#ifdef CONFIG_UART_ASYNC_WRITE
    if (uart_info->tx_ring != NULL) {
        tuya_ring_buff_free(uart_info->tx_ring);
    }

    if (uart_info->tx_ring_sem != NULL) {
        tal_semaphore_release(uart_info->tx_ring_sem);
    }
#endif

    if (uart_info->rx_ring != NULL) {
        tuya_ring_buff_free(uart_info->rx_ring);
    }

    if (uart_info->rx_ring_sem != NULL) {
        tal_semaphore_release(uart_info->rx_ring_sem);
    }

    tal_free(uart_info);
}

/**
 * @brief init uart
 *
 * @param[in] port_num: uart port number
 * @param[in] cfg: uart configure
 *
 * @note This API is used to init uart.
 *
 * @return the uart init result, 0, init success, other init error
 */
OPERATE_RET tal_uart_init(TUYA_UART_NUM_E port_num, TAL_UART_CFG_T *cfg)
{
    if (cfg == NULL) {
        return OPRT_INVALID_PARM;
    }

    OPERATE_RET ret = 0;

    if (g_uart_list.mutex == NULL) {
        ret = tal_mutex_create_init(&g_uart_list.mutex);
        if (ret != OPRT_OK) {
            return ret;
        }
    }

    TAL_UART_DEV *uart_info = uart_list_get_one_node(port_num);
    if (uart_info != NULL) {
        return OPRT_INVALID_PARM;
    }

    uart_info = tal_calloc(1, sizeof(TAL_UART_DEV));
    if (uart_info == NULL) {
        return OPRT_MALLOC_FAILED;
    }

    uart_info->port_num = port_num;
    uart_info->open_mode = cfg->open_mode;

    if (uart_info->open_mode & O_BLOCK) {
        ret = tal_semaphore_create_init(&uart_info->rx_block_sem, 0, 1);
        if (ret != OPRT_OK) {
            goto ERR_EXIT;
        }

        ret = tal_semaphore_create_init(&uart_info->tx_block_sem, 0, 1);
        if (ret != OPRT_OK) {
            goto ERR_EXIT;
        }
    }

    ret = tkl_uart_init(port_num, &cfg->base_cfg);
    if (ret != OPRT_OK) {
        goto ERR_EXIT;
    }

    ret = tuya_ring_buff_create(cfg->rx_buffer_size, OVERFLOW_STOP_TYPE, &uart_info->rx_ring);
    if (ret != OPRT_OK) {
        goto ERR_EXIT;
    }

    ret = tal_semaphore_create_init(&uart_info->rx_ring_sem, 1, 1);
    if (ret != OPRT_OK) {
        goto ERR_EXIT;
    }

#ifdef CONFIG_UART_ASYNC_WRITE
    tkl_uart_tx_irq_cb_reg(port_num, uart_tx_chars_in_isr);

    ret = tuya_ring_buff_create(cfg->tx_buffer_size, OVERFLOW_STOP_TYPE, &uart_info->tx_ring);
    if (ret != OPRT_OK) {
        goto ERR_EXIT;
    }

    ret = tal_semaphore_create_init(&uart_info->tx_ring_sem, 1, 1);
    if (ret != OPRT_OK) {
        goto ERR_EXIT;
    }
#endif

    ret = uart_list_add_one_node(uart_info);
    tkl_uart_rx_irq_cb_reg(port_num, uart_rx_chars_in_isr);

    return ret;

ERR_EXIT:
    uart_free_source(uart_info);
    return ret;
}

/**
 * @brief read data from uart
 *
 * @param[in] port_num: uart port number
 * @param[in] data: read data buffer
 * @param[in] len: the read size
 *
 * @note This API is used to read data from uart.
 *
 * @return >=0, the read size; < 0, read error
 */
int tal_uart_read(TUYA_UART_NUM_E port_num, uint8_t *data, uint32_t len)
{
    if (data == NULL) {
        return OPRT_INVALID_PARM;
    }

    TAL_UART_DEV *uart_info = uart_list_get_one_node(port_num);
    if (uart_info == NULL) {
        return OPRT_INVALID_PARM;
    }

    OPERATE_RET ret = tal_semaphore_wait(uart_info->rx_ring_sem, SEM_WAIT_FOREVER);
    if (ret != OPRT_OK) {
        return ret;
    }

    TUYA_RINGBUFF_T *rx_ring = uart_info->rx_ring;
    uint32_t buffer_size = tuya_ring_buff_used_size_get(rx_ring);
    uint32_t read_count = 0;

    if (buffer_size != 0) {
        read_count = tuya_ring_buff_read(rx_ring, data, len);
    } else {
        if (uart_info->open_mode & O_BLOCK) {
            while (read_count == 0) {
                read_count = tuya_ring_buff_read(rx_ring, data, len);
                if (read_count != 0) {
                    break;
                }

                uart_info->wait_rx_flag = TRUE;
                ret = tal_semaphore_wait(uart_info->rx_block_sem, SEM_WAIT_FOREVER);
                if (ret != OPRT_OK) {
                    break;
                }
            }
        }
    }

#ifdef CONFIG_UART_FLOW_CONTRAL
    if (uart_info->open_mode & O_FLOW_CTRL) {
        ret = tuya_ring_buff_read(rx_ring);
        if (ret == 0) {
            tkl_uart_set_rx_flowctrl(port_num, FALSE);
        }
    }
#endif

    tal_semaphore_post(uart_info->rx_ring_sem);
    return read_count;
}

#ifdef CONFIG_UART_WRITE_ASYNC
OPERATE_RET uart_async_write(TAL_UART_DEV *uart_info, uint8_t *data, uint32_t len)
{
    OPERATE_RET ret = tal_semaphore_wait(uart_info->tx_ring_sem, SEM_WAIT_FOREVER);
    if (uart_info == NULL) {
        return ret;
    }

    uint16_t tx_bytes = tuya_ring_buff_write(uart_info->tx_ring, data, len);

#ifdef CONFIG_UART_BLOCK
    if (uart_info->open_mode & O_BLOCK) {
        while (ret == OPRT_OK) {
            tx_bytes = tuya_ring_buff_write(uart_info->tx_ring, data, len);
        }
        if (tx_bytes != 0) {
            break;
        }

        uart_info->wait_tx_flag = TRUE;
        ret = tal_semaphore_wait(uart_info->tx_block_sem, SEM_WAIT_FOREVER);
    }
}
#endif

if (tx_bytes != 0) {
    set_tx_int(uart_info->port_num, true);
}

tal_semaphore_post(uart_info->tx_ring_sem);

return tx_bytes;
}
#endif

/**
 * @brief send data by uart
 *
 * @param[in] port_num: uart port number
 * @param[in] data: send data buffer
 * @param[in] len: the send size
 *
 * @note This API is used to send data by uart.
 *
 * @return >=0, the write size; < 0, write error
 */
int tal_uart_write(TUYA_UART_NUM_E port_num, const uint8_t *data, uint32_t len)
{
    if (data == NULL) {
        return OPRT_INVALID_PARM;
    }

    TAL_UART_DEV *uart_info = uart_list_get_one_node(port_num);
    if (uart_info == NULL) {
        return OPRT_INVALID_PARM;
    }

    int tx_bytes = 0;
    int ret;
    if ((uart_info->open_mode & O_ASYNC_WRITE) == 0) {
        while (tx_bytes != len) {
            ret = tkl_uart_write(port_num, (void *)&data[tx_bytes], 1);
            if (ret != 1) {
                break;
            }
            tx_bytes++;
        }
    }
#ifdef CONFIG_UART_WRITE_ASYNC
    else {
        tx_bytes = uart_async_write(uart_info, data, len);
    }
#endif

    return tx_bytes;
}

/**
 * @brief deinit uart
 *
 * @param[in] port_num: uart port number
 *
 * @note This API is used to deinit uart.
 *
 * @return the uart deinit result, 0, deinit success, other deinit error
 */
OPERATE_RET tal_uart_deinit(TUYA_UART_NUM_E port_num)
{
    TAL_UART_DEV *uart_info = uart_list_get_one_node(port_num);
    if (uart_info == NULL) {
        return OPRT_INVALID_PARM;
    }

    /**/
    OPERATE_RET ret = tkl_uart_deinit(port_num);
    if (ret != OPRT_OK) {
        return ret;
    }

    ret = uart_list_delete_one_node(uart_info);
    if (ret != OPRT_OK) {
        return ret;
    }

    tuya_ring_buff_free(uart_info->rx_ring);
    tal_semaphore_release(uart_info->rx_ring_sem);

#ifdef CONFIG_UART_ASYNC_WRITE
    tuya_ring_buff_free(uart_info->tx_ring);
    tal_semaphore_release(uart_info->tx_ring_sem);
#endif

    if (uart_info->open_mode & O_BLOCK) {
        tal_semaphore_release(uart_info->rx_block_sem);
        tal_semaphore_release(uart_info->tx_block_sem);
    }

    tal_free(uart_info);

    return ret;
}

int tal_uart_get_rx_data_size(TUYA_UART_NUM_E port_num)
{
    TAL_UART_DEV *uart_info = uart_list_get_one_node(port_num);
    if (uart_info == NULL) {
        return OPRT_INVALID_PARM;
    }

    TUYA_RINGBUFF_T *rx_ring = uart_info->rx_ring;
    uint32_t buffer_size = tuya_ring_buff_used_size_get(rx_ring);

    return buffer_size;
}

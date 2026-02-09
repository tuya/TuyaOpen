/**
 * @file u8g2_port.c
 * @version 0.1
 * @copyright Copyright (c) 2021-2026 Tuya Inc. All Rights Reserved.
 */

#include "tal_semaphore.h"
#include "tal_memory.h"
#include "tal_system.h"
#include "tal_log.h"
#include <string.h>

#include "tkl_gpio.h"

#if defined(ENABLE_SPI) && (ENABLE_SPI == 1)
#include "tkl_spi.h"
#endif

#if defined(ENABLE_I2C) && (ENABLE_I2C == 1)
#include "tkl_i2c.h"
#endif

#include "u8g2_port.h"
/***********************************************************
************************macro define************************
***********************************************************/
#define U8G2_IIC_SEND_BUF_SIZE  255

/***********************************************************
***********************typedef define***********************
***********************************************************/
#if defined(ENABLE_SPI) && (ENABLE_SPI == 1)
typedef struct {
    SEM_HANDLE    tx_sem;
}U8G2_SPI_SYNC_T;
#endif

#if defined(ENABLE_I2C) && (ENABLE_I2C == 1)
typedef struct {
    uint8_t *buffer;
    uint32_t data_len;
}U8G2_I2C_SEND_T;
#endif

/***********************************************************
***********************variable define**********************
***********************************************************/
#if defined(ENABLE_SPI) && (ENABLE_SPI == 1)
static U8G2_SPI_SYNC_T sg_u8g2_spi_sync[TUYA_SPI_NUM_MAX];
#endif

#if defined(ENABLE_I2C) && (ENABLE_I2C == 1)
static U8G2_I2C_SEND_T sg_u8g2_i2c_send[TUYA_I2C_NUM_MAX];
#endif

/***********************************************************
***********************function define**********************
***********************************************************/
static void __u8x8_init_output_pin(u8x8_t *u8x8, uint8_t pin_id) 
{
    TUYA_GPIO_BASE_CFG_T pin_cfg;

    if(NULL == u8x8 || pin_id >= U8X8_PIN_OUTPUT_CNT) {
        return;
    }

    if (u8x8->pins[pin_id] != U8X8_PIN_NONE) {
        pin_cfg.mode   = TUYA_GPIO_PUSH_PULL;
        pin_cfg.direct = TUYA_GPIO_OUTPUT;
        pin_cfg.level  = TUYA_GPIO_LEVEL_LOW;

        tkl_gpio_init(u8x8->pins[pin_id], &pin_cfg);
    }
}

static void __u8x8_gpio_write(u8x8_t *u8x8, uint8_t pin_id, uint8_t value)
{
    if(NULL == u8x8 || pin_id >= U8X8_PIN_OUTPUT_CNT) {
        return;
    }

	if (u8x8->pins[pin_id] != U8X8_PIN_NONE) {
		tkl_gpio_write(u8x8->pins[pin_id], value);
	}
}

uint8_t u8x8_gpio_and_delay_tkl(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr)
{
    switch(msg) {
        case U8X8_MSG_DELAY_NANO:
            // Delay in nanoseconds
            break;
        case U8X8_MSG_DELAY_100NANO:
            // Delay in nanoseconds
            break;
        case U8X8_MSG_DELAY_10MICRO:
            // Delay in microseconds
            break;
        case U8X8_MSG_DELAY_MILLI:
            // Delay in milliseconds
            tal_system_sleep(arg_int);
            break;
        case U8X8_MSG_GPIO_AND_DELAY_INIT:
            // Initialize GPIOs here
            break;
        case U8X8_MSG_GPIO_RESET:
            // Set Reset pin here
            __u8x8_gpio_write(u8x8, U8X8_PIN_RESET, arg_int);
            break;
        case U8X8_MSG_GPIO_CS:
            __u8x8_gpio_write(u8x8, U8X8_PIN_CS, arg_int);
            break; 
        case U8X8_MSG_GPIO_DC:
            __u8x8_gpio_write(u8x8, U8X8_PIN_DC, arg_int);
            break; 
        default:
            return 0;
    }

    return 1;
}


#if defined(ENABLE_SPI) && (ENABLE_SPI == 1)
static void __u8x8_spi_isr_cb(TUYA_SPI_NUM_E port, TUYA_SPI_IRQ_EVT_E event)
{
    if (event == TUYA_SPI_EVENT_TX_COMPLETE) {
        if (sg_u8g2_spi_sync[port].tx_sem) {
            tal_semaphore_post(sg_u8g2_spi_sync[port].tx_sem);
        }
    }
}

static OPERATE_RET __u8x8_spi_init(u8x8_t *u8x8)
{
    TUYA_SPI_BASE_CFG_T spi_cfg;
    OPERATE_RET rt = OPRT_OK;

    if(NULL == u8x8 || NULL == u8x8->display_info) {
        PR_ERR("u8x8 or display info is null\r\n");
        return OPRT_INVALID_PARM;
    }

    if(u8x8->port >= TUYA_SPI_NUM_MAX) {
        PR_ERR("invalid spi port:%d\r\n", u8x8->port);
        return OPRT_INVALID_PARM;
    }

    if(NULL == sg_u8g2_spi_sync[u8x8->port].tx_sem) {
        TUYA_CALL_ERR_RETURN(tal_semaphore_create_init(&(sg_u8g2_spi_sync[u8x8->port].tx_sem), 0, 1));
    }

    /* SPI initialization */
    switch(u8x8->display_info->spi_mode){   
        case 0: spi_cfg.mode = TUYA_SPI_MODE0; break;
        case 1: spi_cfg.mode = TUYA_SPI_MODE1; break;
        case 2: spi_cfg.mode = TUYA_SPI_MODE2; break;
        case 3: spi_cfg.mode = TUYA_SPI_MODE3; break;
        default: spi_cfg.mode = TUYA_SPI_MODE0; break;
    }

    spi_cfg.freq_hz  = u8x8->display_info->sck_clock_hz;
    spi_cfg.databits = TUYA_SPI_DATA_BIT8;
    spi_cfg.bitorder = TUYA_SPI_ORDER_MSB2LSB;
    spi_cfg.role     = TUYA_SPI_ROLE_MASTER;
    spi_cfg.type     = TUYA_SPI_SOFT_TYPE;
    spi_cfg.spi_dma_flags = 1;

    TUYA_SPI_NUM_E spi_port = (TUYA_SPI_NUM_E)(u8x8->port);
    TUYA_CALL_ERR_RETURN(tkl_spi_init(spi_port, &spi_cfg));
    TUYA_CALL_ERR_RETURN(tkl_spi_irq_init(spi_port, __u8x8_spi_isr_cb));
    TUYA_CALL_ERR_RETURN(tkl_spi_irq_enable(spi_port));

    __u8x8_init_output_pin(u8x8, U8X8_PIN_CS);
    __u8x8_init_output_pin(u8x8, U8X8_PIN_DC);
    __u8x8_init_output_pin(u8x8, U8X8_PIN_RESET);

    return OPRT_OK;
}

static OPERATE_RET __disp_spi_send(TUYA_SPI_NUM_E port, uint8_t *data, uint32_t size)
{
    OPERATE_RET rt = OPRT_OK;
    uint32_t left_len = size, send_len = 0;
    uint32_t dma_max_size = tkl_spi_get_max_dma_data_length();

    if(NULL == sg_u8g2_spi_sync[port].tx_sem) {
        PR_ERR("tx sem not init, port:%d\r\n", port);
        return OPRT_COM_ERROR;
    }

    while (left_len > 0) {
        send_len = (left_len > dma_max_size) ? dma_max_size : (left_len);

        TUYA_CALL_ERR_RETURN(tkl_spi_send(port, data + size - left_len, send_len));

        rt = tal_semaphore_wait(sg_u8g2_spi_sync[port].tx_sem, 100); 
        if(rt != OPRT_OK) {
            PR_ERR("spi tx wait timeout, port:%d len:%d\r\n", port, send_len);
            return rt;
        }

        left_len -= send_len;
    }

    return rt;
}


uint8_t u8x8_byte_tkl_spi(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr)
{
    OPERATE_RET rt = OPRT_OK;

    switch(msg) {
        case U8X8_MSG_BYTE_INIT: {
            // Initialize SPI here
            rt = __u8x8_spi_init(u8x8);
            if(rt != OPRT_OK) {
                return 0;
            }
        }
        break;
        case U8X8_MSG_BYTE_SET_DC:
            // Set Data/Command pin here
            u8x8_gpio_SetDC(u8x8, arg_int);
            break;
        case U8X8_MSG_BYTE_START_TRANSFER:
            // Start SPI transfer here
            u8x8_gpio_SetCS(u8x8, u8x8->display_info->chip_enable_level);  
            break;
        case U8X8_MSG_BYTE_END_TRANSFER:
            // End SPI transfer here
            u8x8_gpio_SetCS(u8x8, u8x8->display_info->chip_disable_level);
            break;
        case U8X8_MSG_BYTE_SEND:
            // Send data via SPI here
            __disp_spi_send((TUYA_SPI_NUM_E)(u8x8->port), (uint8_t *)arg_ptr, arg_int);
            break;
        default:
            return 0;
    }

    return 1;
}
#endif

#if defined(ENABLE_I2C) && (ENABLE_I2C == 1)
static OPERATE_RET __u8x8_i2c_init(u8x8_t *u8x8)
{
    TUYA_IIC_BASE_CFG_T i2c_cfg;
    OPERATE_RET rt = OPRT_OK;
    TUYA_I2C_NUM_E port;

    if(NULL == u8x8 || NULL == u8x8->display_info) {
        PR_ERR("invalid parm\r\n");
        return OPRT_INVALID_PARM;
    }

    if(u8x8->port >= TUYA_I2C_NUM_MAX) {
        PR_ERR("invalid i2c port:%d\r\n", u8x8->port);
        return OPRT_INVALID_PARM;
    }

    port = (TUYA_I2C_NUM_E)(u8x8->port);

    /* I2C initialization */
    i2c_cfg.role       = TUYA_IIC_MODE_MASTER;
    i2c_cfg.speed      = TUYA_IIC_BUS_SPEED_400K;
    i2c_cfg.addr_width = TUYA_IIC_ADDRESS_7BIT;

    TUYA_CALL_ERR_RETURN(tkl_i2c_init(port, &i2c_cfg));

    sg_u8g2_i2c_send[port].buffer = Malloc(U8G2_IIC_SEND_BUF_SIZE);
    if(NULL == sg_u8g2_i2c_send[port].buffer) {
        PR_ERR("i2c send buffer malloc failed\r\n");
        return OPRT_MALLOC_FAILED;
    }

    return OPRT_OK;
}

static OPERATE_RET __u8x8_i2c_send_data(u8x8_t *u8x8)
{
    TUYA_I2C_NUM_E port;
    uint16_t slave_addr;

    if(NULL == u8x8 || NULL == u8x8->display_info) {
        PR_ERR("invalid parm\r\n");
        return OPRT_INVALID_PARM;
    }

    if(u8x8->port >= TUYA_I2C_NUM_MAX) {
        PR_ERR("invalid i2c port:%d\r\n", u8x8->port);
        return OPRT_INVALID_PARM;
    }

    port = (TUYA_I2C_NUM_E)(u8x8->port);

    if(0 == sg_u8g2_i2c_send[port].data_len) {
        return OPRT_OK;
    }

    slave_addr = u8x8_GetI2CAddress(u8x8)>>1;

    PR_NOTICE("addr:%02x, len:%d\r\n", slave_addr, sg_u8g2_i2c_send[port].data_len);

    return tkl_i2c_master_send(port, slave_addr, (void *)sg_u8g2_i2c_send[port].buffer,\
                              sg_u8g2_i2c_send[port].data_len, true);

}

static OPERATE_RET __u8x8_i2c_set_data(u8x8_t *u8x8, uint8_t *data, uint32_t size)
{
    OPERATE_RET rt = OPRT_OK;
    TUYA_I2C_NUM_E port;
    uint8_t *buf_ptr = NULL;

    if(NULL == u8x8 || NULL == u8x8->display_info || NULL == data || size == 0) {
        PR_ERR("invalid parm\r\n");
        return OPRT_INVALID_PARM;
    }

    port = (TUYA_I2C_NUM_E)(u8x8->port);

    if(NULL == sg_u8g2_i2c_send[port].buffer) {
        PR_ERR("i2c send buffer not init\r\n");
        return OPRT_COM_ERROR;
    }

    if((sg_u8g2_i2c_send[port].data_len + size) > U8G2_IIC_SEND_BUF_SIZE) {
        PR_ERR("i2c send buffer overflow, len:%d add size:%d\r\n",
                            sg_u8g2_i2c_send[port].data_len, size);
        return OPRT_COM_ERROR;
    }

    buf_ptr = sg_u8g2_i2c_send[port].buffer + sg_u8g2_i2c_send[port].data_len;

    memcpy(buf_ptr, data, size);
    sg_u8g2_i2c_send[port].data_len += size;
    
    return rt;
} 

static OPERATE_RET __u8x8_i2c_buffer_clear(u8x8_t *u8x8)
{
    OPERATE_RET rt = OPRT_OK;
    TUYA_I2C_NUM_E port;

    if(NULL == u8x8 || NULL == u8x8->display_info) {
        PR_ERR("invalid parm\r\n");
        return OPRT_INVALID_PARM;
    }

    if(u8x8->port >= TUYA_I2C_NUM_MAX) {
        PR_ERR("invalid i2c port:%d\r\n", u8x8->port);
        return OPRT_INVALID_PARM;
    }

    port = (TUYA_I2C_NUM_E)(u8x8->port);
    sg_u8g2_i2c_send[port].data_len = 0;

    return rt;
}


uint8_t u8x8_byte_tkl_i2c(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr)
{
    OPERATE_RET rt = OPRT_OK;

    switch(msg) {
    case U8X8_MSG_BYTE_INIT:{
        // Initialize I2C here
        rt = __u8x8_i2c_init(u8x8);
        if(rt != OPRT_OK) {
            PR_ERR("i2c init failed\r\n");
            return 0;  
        }     
    }
    break;
    case U8X8_MSG_BYTE_START_TRANSFER:
        // Start I2C transfer here
        __u8x8_i2c_buffer_clear(u8x8);
        break;
    case U8X8_MSG_BYTE_END_TRANSFER:
        // End I2C transfer here
        __u8x8_i2c_send_data(u8x8);
        break;
    case U8X8_MSG_BYTE_SEND:
        // Send data via I2C here
        __u8x8_i2c_set_data(u8x8, (uint8_t *)arg_ptr, arg_int); //just to update data_len
        break;
    default:
        return 0;
    }

    return 1;

}
#endif

/**
 * @brief Allocate display buffer for u8g2
 * @param u8g2 Pointer to u8g2 object
 * @return OPERATE_RET Operation result
 */
OPERATE_RET u8g2_alloc_buffer(u8g2_t *u8g2)
{
    uint32_t buf_size;
    uint8_t *buf = NULL;
    
    if (NULL == u8g2) {
        PR_ERR("u8g2 is null\r\n");
        return OPRT_INVALID_PARM;
    }
    
    // Calculate required buffer size
    buf_size = u8g2_GetBufferSize(u8g2);
    
    // Allocate memory using Tuya memory management interface
    buf = (uint8_t *)Malloc(buf_size);
    if (NULL == buf) {
        PR_ERR("u8g2 buffer malloc failed, size:%d\r\n", buf_size);
        return OPRT_MALLOC_FAILED;
    }
    
    // Clear buffer to ensure it starts with all zeros
    memset(buf, 0, buf_size);
    
    // Set buffer pointer
    u8g2_SetBufferPtr(u8g2, buf);
    
    PR_DEBUG("u8g2 buffer allocated and cleared, size:%d\r\n", buf_size);
    return OPRT_OK;
}

/**
 * @brief Free u8g2 display buffer
 * @param u8g2 Pointer to u8g2 object
 * @return OPERATE_RET Operation result
 */
OPERATE_RET u8g2_free_buffer(u8g2_t *u8g2)
{
    uint8_t *buf = NULL;
    
    if (NULL == u8g2) {
        PR_ERR("u8g2 is null\r\n");
        return OPRT_INVALID_PARM;
    }
    
    // Get buffer pointer
    buf = u8g2_GetBufferPtr(u8g2);
    if (NULL != buf) {
        // Free memory using Tuya memory management interface
        Free(buf);
        u8g2_SetBufferPtr(u8g2, NULL);
        PR_DEBUG("u8g2 buffer freed\r\n");
    }
    
    return OPRT_OK;
}
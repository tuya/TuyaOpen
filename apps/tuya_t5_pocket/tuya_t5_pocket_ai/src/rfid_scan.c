/**
 * @file rfid_scan.c
 * @brief Implements RFID scanning functionality for IoT device
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 *
 */

#include "rfid_scan.h"
#include "tkl_pinmux.h"
#include "tal_uart.h"
#include "tal_api.h"

#include "app_display.h"
#include "rfid_scan_screen.h"

/***********************************************************
************************macro define************************
***********************************************************/
#define USR_UART_NUM      TUYA_UART_NUM_2
#define READ_BUFFER_SIZE  256
/***********************************************************
***********************typedef define***********************
***********************************************************/
typedef struct {
    RFID_DATA_TYPE_E data_type;
    RFID_TAG_TYPE_E tag_type;
    uint16_t block_addr;    // only data_type is RFID_DATA_TYPE_BLOCK_DATA used
    RFID_SCAN_LENGTH_E data_len;
    uint8_t data[16];       // For UIDs less than 16 bytes, all the remaining bytes are 00h.
} RFID_SCAN_DATA;

typedef struct {
    uint8_t dev_id;
    RFID_DATA_CMD_E cmd;
    uint8_t length;
    RFID_SCAN_DATA data;
    uint16_t crc;
} RFID_SCAN_FRAME;

/***********************************************************
***********************variable define**********************
***********************************************************/

static THREAD_HANDLE rfid_scan_thread = NULL;
static uint8_t sg_read_buffer[READ_BUFFER_SIZE];
RFID_SCAN_FRAME rfid_dev;

uint16_t crc16_mbrtu(uint8_t *buf, uint32_t len)
{
    uint32_t i;
    uint16_t crc_value = 0xFFFF;
    if ((buf == NULL) || (len == 0))
    {
        return 0;
    }
    while (len--) {
        crc_value ^= *buf++;
        for (i = 0; i < 8; i++)
        {
            if (crc_value & 0x0001)
                crc_value = (crc_value >> 1) ^ 0xA001;
            else
                crc_value = crc_value >> 1;
        }
    }
    return (crc_value);
 }


void __rfid_scan_thread(void *param)
{
    while (1) {
        // RFID scanning logic goes here
        int read_len = tal_uart_read(USR_UART_NUM, (uint8_t *)sg_read_buffer, READ_BUFFER_SIZE);
        PR_NOTICE("Read len: %d", read_len);
        if(read_len <= 0) {
            tal_system_sleep(100);
            continue;
        }

        rfid_dev.dev_id = sg_read_buffer[0];
        rfid_dev.cmd = sg_read_buffer[1];
        rfid_dev.length = sg_read_buffer[2];
        rfid_dev.data.data_type = (RFID_DATA_TYPE_E)((sg_read_buffer[3] << 8) | sg_read_buffer[4]);
        rfid_dev.data.tag_type = (RFID_TAG_TYPE_E)((sg_read_buffer[5] << 8) | sg_read_buffer[6]);
        rfid_dev.data.block_addr = (sg_read_buffer[7] << 8) | sg_read_buffer[8];
        rfid_dev.data.data_len = (RFID_SCAN_LENGTH_E)((sg_read_buffer[9] << 8) | sg_read_buffer[10]);
        memcpy(rfid_dev.data.data, &sg_read_buffer[11], 16);
        rfid_dev.crc = (sg_read_buffer[27] << 8) | sg_read_buffer[28];

        uint16_t calculated_crc = crc16_mbrtu((uint8_t *)&sg_read_buffer[0], read_len - 2);
        calculated_crc = calculated_crc << 8 | (calculated_crc >> 8);
        if (calculated_crc != rfid_dev.crc) {
            PR_ERR("CRC mismatch: received 0x%04X, calculated 0x%04X", rfid_dev.crc, calculated_crc);
            tal_system_sleep(100);
            continue;
        }

        // Call callback function to update UI with RFID data
        rfid_scan_screen_data_update(rfid_dev.dev_id,
                                       rfid_dev.data.tag_type, 
                                       rfid_dev.data.data, 
                                       rfid_dev.data.data_len);
        app_display_send_msg(POCKET_DISP_TP_RFID_SCAN_SUCCESS, NULL, 0);

        // PR_NOTICE("dev_id: %02x, cmd: %02x, length: %d, data type: %d, tag type: %d, block addr: %d, data len: %d",
        //           rfid_dev.dev_id, rfid_dev.cmd, rfid_dev.length, rfid_dev.data.data_type,
        //           rfid_dev.data.tag_type, rfid_dev.data.block_addr, rfid_dev.data.data_len);
        // PR_NOTICE("UID : %02x %02x %02x %02x %02x %02x %02x %02x",
        //           rfid_dev.data.data[0], rfid_dev.data.data[1], rfid_dev.data.data[2], rfid_dev.data.data[3],
        //           rfid_dev.data.data[4], rfid_dev.data.data[5], rfid_dev.data.data[6], rfid_dev.data.data[7]);
        tal_system_sleep(100);
    }
}

OPERATE_RET rfid_scan_init(void)
{
    OPERATE_RET rt = OPRT_OK;

    tkl_io_pinmux_config(TUYA_IO_PIN_40, TUYA_UART2_RX);
    tkl_io_pinmux_config(TUYA_IO_PIN_41, TUYA_UART2_TX);

    /* UART 2 init */
    TAL_UART_CFG_T cfg = {0};
    cfg.base_cfg.baudrate = 115200;
    cfg.base_cfg.databits = TUYA_UART_DATA_LEN_8BIT;
    cfg.base_cfg.stopbits = TUYA_UART_STOP_LEN_1BIT;
    cfg.base_cfg.parity = TUYA_UART_PARITY_TYPE_NONE;
    cfg.rx_buffer_size = 256;
    cfg.open_mode = O_BLOCK;
    rt = tal_uart_init(USR_UART_NUM, &cfg);

    THREAD_CFG_T thrd_param = {2048, 4, "rfid_scan_thread"};
    tal_thread_create_and_start(&rfid_scan_thread, NULL, NULL, __rfid_scan_thread, NULL, &thrd_param);

    return rt;
}


/**
 * @file rfid_scan.c
 * @brief Implementation of RFID scan functions
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 *
 */
#include "rfid_scan.h"
#include "tal_api.h"

/**
 * @brief Calculate CRC16 Modbus RTU checksum
 * @param buf Data buffer
 * @param len Buffer length
 * @return CRC16 value
 */
static uint16_t crc16_mbrtu(uint8_t *buf, uint32_t len)
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

/**
 * @brief Process RFID scan data from UART buffer
 * @param buffer UART read buffer
 * @param len Buffer length
 * @param callback Callback function when valid RFID data is detected
 * @return OPRT_OK on success, error code otherwise
 */
OPERATE_RET rfid_scan_process(const uint8_t *buffer, int len, rfid_scan_callback_t callback)
{
    RFID_SCAN_FRAME rfid_frame;
    
    if (buffer == NULL || len <= 28) {
        PR_ERR("Invalid buffer or length");
        return OPRT_INVALID_PARM;
    }
    
    if (callback == NULL) {

        return OPRT_INVALID_PARM;
    }
    
    // Parse RFID frame
    rfid_frame.dev_id = buffer[0];
    rfid_frame.cmd = buffer[1];
    rfid_frame.length = buffer[2];
    rfid_frame.data.data_type = (RFID_DATA_TYPE_E)((buffer[3] << 8) | buffer[4]);
    rfid_frame.data.tag_type = (RFID_TAG_TYPE_E)((buffer[5] << 8) | buffer[6]);
    rfid_frame.data.block_addr = (buffer[7] << 8) | buffer[8];
    rfid_frame.data.data_len = (RFID_SCAN_LENGTH_E)((buffer[9] << 8) | buffer[10]);
    memcpy(rfid_frame.data.data, &buffer[11], 16);
    rfid_frame.crc = (buffer[27] << 8) | buffer[28];
    
    // Validate CRC
    uint16_t calculated_crc = crc16_mbrtu((uint8_t *)&buffer[0], len - 2);
    calculated_crc = calculated_crc << 8 | (calculated_crc >> 8);
    if (calculated_crc != rfid_frame.crc) {
        // PR_DEBUG("CRC mismatch: received 0x%04X, calculated 0x%04X", rfid_frame.crc, calculated_crc);
        return OPRT_INVALID_PARM;
    }
    
    // Call callback function with parsed data
    callback(rfid_frame.dev_id, 
             rfid_frame.data.tag_type, 
             rfid_frame.data.data, 
             rfid_frame.data.data_len);
    
    return OPRT_OK;
}
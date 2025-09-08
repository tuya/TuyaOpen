/**
 * @file tdl_nec_protocol.h
 * @brief NEC infrared protocol implementation for Tuya IoT devices.
 *
 * This file implements the NEC infrared protocol, one of the most widely used
 * IR communication protocols for consumer electronics. The NEC protocol provides
 * reliable infrared data transmission with built-in error detection and repeat
 * code handling, making it ideal for remote control applications and IR-based
 * device communication.
 *
 * Key features of the NEC protocol implementation:
 * - Complete NEC protocol encoding and decoding functionality
 * - Support for both MSB (Most Significant Bit) and LSB (Least Significant Bit) first transmission
 * - Configurable error tolerance for timing variations
 * - Leader code detection and validation
 * - Repeat code handling for continuous button presses
 * - Conversion between NEC format and raw timecode format
 * - Frame synchronization and data integrity verification
 *
 * The implementation supports standard NEC protocol timing specifications
 * including leader codes, logic 0/1 distinction, repeat codes, and proper
 * frame structure validation with configurable error margins to accommodate
 * real-world timing variations in IR communication.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 *
 */

#ifndef __TDL_NEC_PROTOCOL_H__
#define __TDL_NEC_PROTOCOL_H__

#include "tuya_cloud_types.h"

#include "tdl_ir_dev_manage.h"

#ifdef __cplusplus
extern "C" {
#endif

/***********************************************************
************************macro define************************
***********************************************************/
#define IR_NEC_MIN_LENGTH       (68)
#define IR_NEC_REPEAT_LENGTH    (4)

/***********************************************************
***********************typedef define***********************
***********************************************************/


/***********************************************************
********************function declaration********************
***********************************************************/

/**
 * @brief NEC build function, converts NEC format to timecode format, need to call tdl_ir_nec_build_release to release timecode after building
 *
 * @param[in] is_msb: 1: MSB first, 0: LSB first
 * @param[in] ir_nec_data: NEC data
 * @param[out] timecode: converted timecode format
 *
 * @return OPRT_OK on success. Others on error, please refer to "tuya_error_code.h"
 */
OPERATE_RET tdl_ir_nec_build(BOOL_T is_msb, IR_DATA_NEC_T ir_nec_data, IR_DATA_TIMECODE_T **timecode);

/**
 * @brief Release timecode
 *
 * @param[in] timecode: timecode to be released
 *
 * @return none
 */
void tdl_ir_nec_build_release(IR_DATA_TIMECODE_T *timecode);

/**
 * @brief Initialize error rate function, need to call tdl_ir_nec_err_val_init_release to release nec_err_val after initialization
 *
 * @param[out] nec_err_val: initialized error rate data parameters
 * @param[in] nec_cfg: error rate parameters to be set
 *
 * @return OPRT_OK on success. Others on error, please refer to "tuya_error_code.h"
 */
OPERATE_RET tdl_ir_nec_err_val_init(void **nec_err_val, IR_NEC_CFG_T *nec_cfg);

/**
 * @brief Release nec_err_val parameters obtained from error rate initialization
 *
 * @param[in] nec_err_val: nec_err_val parameters to be released
 *
 * @return none
 */
void tdl_ir_nec_err_val_init_release(void *nec_err_val);

/**
 * @brief Get NEC leader code data index
 *
 * @param[in] data: input data
 * @param[in] len: input data length
 * @param[in] nec_err_val: error rate parameters
 * @param[out] head_idx: leader code position index
 *
 * @return OPRT_OK on success. Others on error, please refer to "tuya_error_code.h"
 */
OPERATE_RET tdl_nec_get_frame_head(uint32_t *data, uint16_t len, void *nec_err_val, uint16_t *head_idx);

/**
 * @brief Single NEC data parsing
 *
 * @param[in] data: input data
 * @param[in] len: input data length
 * @param[in] nec_err_val: error rate parameters
 * @param[in] is_msb: 1: MSB first, 0: LSB first
 * @param[out] nec_code: parsed NEC data
 *
 * @return OPRT_OK on success. Others on error, please refer to "tuya_error_code.h"
 */
uint16_t tdl_ir_nec_parser_single(uint32_t *data, uint16_t len, void *nec_err_val, uint8_t is_msb, IR_DATA_NEC_T *nec_code);

/**
 * @brief NEC repeat code parsing
 *
 * @param[in] data: input data
 * @param[in] len: input data length
 * @param[in] nec_err_val: error rate parameters
 * @param[out] repeat_cnt: parsed repeat code
 *
 * @return OPRT_OK on success. Others on error, please refer to "tuya_error_code.h"
 */
uint16_t tdl_ir_nec_parser_repeat(uint32_t *data, uint32_t len, void *nec_err_val, uint16_t *repeat_cnt);

#ifdef __cplusplus
}
#endif

#endif /* __TDL_NEC_PROTOCOL_H__ */

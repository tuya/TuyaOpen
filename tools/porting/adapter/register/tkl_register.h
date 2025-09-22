#ifndef __TKL_REGISTER_H__
#define __TKL_REGISTER_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "tuya_cloud_types.h"

/**
 * @brief read of chip addr
 *
 * @param[in] addr: chip addr
 *
 * @return return  data of addr
 */
uint32_t tkl_reg_read(uint32_t addr);

/**
 * @brief bit read
 *
 * @param[in] addr: chip addr
 *
 * @return return data of bits
 */
uint32_t tkl_reg_bit_read(uint32_t addr, TUYA_ADDR_BITS_DEF_E start_bit, TUYA_ADDR_BITS_DEF_E end_bit);

/**
 * @brief tuya write of chip addr
 *
 * @param[in] addr :  chip addr
 * @param[in] data: data  , write data
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tkl_reg_write(uint32_t addr, uint32_t data);

/**
 * @brief bit write of chip addr
 *
 * @param[in] addr : chip addr
 * @param[in] start_bit : start bit to write
 *  @param[in] end_bit  : end bit to write
 *  @param[in] data     : data to write
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tkl_reg_bit_write(uint32_t addr, TUYA_ADDR_BITS_DEF_E start_bit, TUYA_ADDR_BITS_DEF_E end_bit,
                              uint32_t data);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // __TKL_REGISTER_H__

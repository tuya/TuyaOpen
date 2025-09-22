/**
 * @file tuya_tools.h
 * @brief tuya common api module
 * @version 1.0
 * @date 2019-10-13
 *
 * @copyright Copyright 2021-2025 Tuya Inc. All Rights Reserved.
 *
 */
#ifndef __TUYA_TOOLS_H__
#define __TUYA_TOOLS_H__

#include "tuya_cloud_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief align and round down
 *
 * @param[in] x:     number to be aligned
 * @param[in] align: align size
 *
 * @return
 */
#define TUYA_PALIGN_DOWN(x, align) (x & ~(align - 1))

/**
 * @brief align and round up
 *
 * @param[in] x:     number to be aligned
 * @param[in] align: align size
 *
 * @return
 */
#define TUYA_PALIGN_UP(x, align) ((x + (align - 1)) & ~(align - 1))

/**
 * @brief string len
 *
 * @param[in] str: string to calculate length
 *
 * @return length of string
 */
size_t tuya_strlen(const char *str);

/**
 * @brief string copy
 *
 * @param[in] dst: target array used to store the copied content
 * @param[in] src: string to copy
 *
 * @return pointer to the final destination string dest
 */
char *tuya_strcpy(char *dst, const char *src);

/**
 * @brief string cat
 *
 * @param[in] dst: point to target string array
 * @param[in] src: point to the string to append
 *
 * @return pointer to the final destination string dest
 */
char *tuya_strcat(char *dst, const char *src);

/**
 * @brief compare the first n characters of two string
 *
 * @param[in] s1: the string 1
 * @param[in] s2: the string 2
 * @param[in] n:  the first n characters
 *
 * @return 0 one equal, 1 on s1 longer than s2, -1 on s2 longer than s1
 */
int tuya_strncasecmp(const char *s1, const char *s2, size_t n);

/**
 * @brief compare characters of two string
 *
 * @param[in] src: the source string
 * @param[in] dst: the dest string
 *
 * @return 0 one equal, 1 on s1 longer than s2, -1 on s2 longer than s1
 */
int tuya_strcmp(const char *src, const char *dst);

/**
 * @brief convert character to hex, if '0'-'f'/'F' -> 0-15, else 0
 *
 * @param[in] asccode the input character
 *
 * @return the hex value
 */
uint8_t tuya_asc2hex(char asccode);

/**
 * @brief convert the hex character array to hex array
 *
 * @param[out] hex: the out hex array
 * @param[in] ascs: the input hex character array
 * @param[in] srclen: the length of input character array
 *
 * @return none
 */
void tuya_ascs2hex(uint8_t *hex, uint8_t *ascs, int srclen);

/**
 * @brief convert the input hex array to string array
 *
 * @param[out] str: the out string array
 * @param[in] hex:  the input hex array
 * @param[in] hexlen: the length of input hex array
 *
 * @return none
 * @note the size of <str> must >= hexlen * 2, convert example 00-ff -> "00"-"FF"
 */
void tuya_hex2str(uint8_t *str, uint8_t *hex, int hexlen);

/**
 * @brief convert the input string to number
 *
 * @param[out] number: the out number
 * @param[in]  str:    the input string
 * @param[in]  strlen: the length of input hex array
 *
 * @return none
 * @note the size of <pbDest> must >= nLen * 2, convert example 00-ff -> "00"-"FF"
 */
BOOL_T tuya_str2num(uint32_t *number, const char *str, uint8_t strlen);

/**
 * @brief int number convert to buff
 *
 * @param[in]  num: the number need to convert
 * @param[out] intArray: point to array
 * @param[in]  len: the length of array
 *
 * @return the arry length of converted
 */
uint32_t tuya_int2intArray(uint32_t num, uint8_t *intArray, uint8_t len);

/**
 * @brief int array convert to a int number
 *
 * @param[in] intArray: the array need to convert
 * @param[in] index: arry start index
 * @param[in] len: the length of array
 *
 * @return the number of converted
 */
uint32_t tuya_intArray2int(uint8_t *intArray, uint32_t index, uint8_t len);

/**
 * @note buff reversal function
 * @param[in/out] buf: the buff need to be reverse
 * @param[in]     len: the length of buff
 *
 * @return none
 */
void tuya_buff_reverse(uint8_t *buf, uint16_t len);

/**
 * @note data reversal function
 * @param[out] dst: reversal dest data
 * @param[in]  src: reversal source data
 * @param[in]  srclen: reversal data buffer length
 *
 * @return none
 */
void tuya_data_reverse(uint8_t *dst, uint8_t *src, uint16_t srclen);

/**
 * @brief sort the char in the buf according ascii value
 *
 * @param[in] is_ascend: sorting mode
 * @param[in] buf: the buffer need to sort
 * @param[in] len:  the length of character in buffer
 *
 * @return none
 */
void tuya_byte_sort(uint8_t is_ascend, uint8_t *buf, int len);

/**
 * @brief find <ch> in <str>, start find in index <revr_index>, find in reverse order.
 *
 * @param[in] str:   the string for find
 * @param[in] index: the reverse index start to find
 * @param[in] ch:    the character to find
 *
 * @return -2 on not found, -1 on invalid parameter, >=0 on the position of the character to find
 */
int tuya_find_char_with_reverse_idx(const char *str, const int index, const char ch);

/**
 * @brief calculate the number of bits 1 in a number
 *
 * @param[in] num: number
 *
 * @return number of bits 1
 */
uint32_t tuya_bit1_count(uint32_t num);

/**
 * @brief the software calculates the number of leading zeros
 *
 * @param[in] num: number
 *
 * @return number of leading zeros
 */
uint32_t tuya_leading_zeros_count(uint32_t num);

/**
 * @brief 8-bit cumulative checksum calculation
 *
 * @param[in] buf: the buffer to calculate
 * @param[in] len: length of the buffer
 *
 * @return 8-bit cumulative checksum
 */
uint8_t tuya_check_sum8(uint8_t *buf, uint32_t len);

/**
 * @brief 16-bit cumulative checksum calculation
 *
 * @param[in] buf: the buffer to calculate
 * @param[in] len: length of the buffer
 *
 * @return 16-bit cumulative checksum
 */
uint16_t tuya_check_sum16(uint8_t *buf, uint32_t len);

#ifdef __cplusplus
}
#endif
#endif

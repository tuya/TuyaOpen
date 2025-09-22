/**
 * @file tuya_hashmap.h
 * @brief tuya hasmap module
 * @version 1.0
 * @date 2019-10-30
 *
 * @copyright Copyright 2021-2025 Tuya Inc. All Rights Reserved.
 *
 */

#ifndef __TUYA_HASHMAP_H__
#define __TUYA_HASHMAP_H__

#include "tuya_cloud_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief define some special error code for hashmap module
 *
 */
#define MAP_MISSING -2 /* No such element */
#define MAP_OMEM    -1 /* Out of Memory */
#define MAP_OK      0  /* OK */

/**
 * @brief any_t is a pointer.  This allows you to put arbitrary structures in
 * the hashmap.
 */
typedef void *ANY_T;

/**
 * @brief map_t is a pointer to an internally maintained data structure.
 * Clients of this package do not need to know how hashmaps are
 * represented.  They see and manipulate only map_t's.
 */
typedef ANY_T MAP_T;

/**
 * @brief any_t_iter is a iterator which used to traverse the hashmap
 *
 */
typedef ANY_T *ANY_T_ITER;

/**
 * @brief create a new empty hashmap
 *
 * @param[in] table_size the hash table size
 * @return a new empty hashmap
 */
MAP_T tuya_hashmap_new(uint32_t table_size);

/**
 * @brief Add an element to the hashmap
 *
 * @param[in] in the hashmap
 * @param[in] key the key of hash element
 * @param[in] data the data of hash element
 * @return MAP_OK on success, others on failed, please refer to the define of hashmap error code
 *
 * @note For same key, it does not replace it. it is inserted in the head of the list
 */
int tuya_hashmap_put(MAP_T in, const char *key, const ANY_T data);

/**
 * @brief get an element from the hashmap
 *
 * @param[in] in the hashmap
 * @param[in] key the key of the element
 * @param[out] arg the first value that the key matches
 * @return MAP_OK on success, others on failed, please refer to the define of hashmap error code
 */
int tuya_hashmap_get(MAP_T in, const char *key, ANY_T *arg);

/**
 * @brief traverse all data with same key
 *
 * @param[in] in the hashmap
 * @param[in] key the key of element
 * @param[inout] arg_iterator the traverse iterator
 * @return MAP_OK on success, others on failed, please refer to the define of hashmap error code
 *
 * @note if arg_iterator is NULL, fetch the first element, otherwise, fetch the next element
 */
int tuya_hashmap_data_traversal(MAP_T in, const char *key, ANY_T_ITER *arg_iterator);

/**
 * @brief traverse all data with same key
 *
 */
#define TUYA_HASHMAP_FOR_EACH_DATA(in, key, data_iter)                                                                 \
    for (data_iter = NULL; tuya_hashmap_data_traversal(in, key, (ANY_T_ITER *)&data_iter) == MAP_OK; /*empty*/)

/**
 * @brief remove an element from the hashmap
 *
 * @param[in] in the hashmap
 * @param[in] key the key of element
 * @param[in] data the data of the element
 * @return MAP_OK on success, others on failed, please refer to the define of hashmap error code
 *
 * @note if data is NULL,then delete the first note match key.if data is not null, then delete the node match key and
 * data.
 */
int tuya_hashmap_remove(MAP_T in, char *key, ANY_T data);

/**
 * @brief free the hashmap
 *
 * @param[in] in the hashmap need to free
 *
 * @warning must remove all element first, otherwise, it will cause element leak
 */
void tuya_hashmap_free(MAP_T in);

/**
 * @brief get current size of the hashmap
 *
 * @param[in] in the hashmap
 * @return the current size
 */
int tuya_hashmap_length(MAP_T in);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif // __HASHMAP_H__

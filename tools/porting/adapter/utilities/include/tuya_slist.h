/**
 * @file tuya_slist.h
 * @brief tuya sigle direciton list module
 * @version 1.0
 * @date 2019-10-30
 *
 * @copyright Copyright 2021-2025 Tuya Inc. All Rights Reserved.
 *
 */

#ifndef __TUYA_SLIST_H__
#define __TUYA_SLIST_H__

#include "tuya_cloud_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief sigle list head
 *
 */
typedef struct slist_head {
    struct slist_head *next;
} SLIST_HEAD;

/**
 * @brief define a sigle list head and initialize to empty
 *
 */
#define SLIST_HEAD(name) SLIST_HEAD name = {NULL}

/**
 * @brief sigle list head initialization
 *
 */
#define INIT_SLIST_HEAD(ptr)                                                                                           \
    do {                                                                                                               \
        (ptr)->next = NULL;                                                                                            \
    } while (0)

/**
 * @brief new a sigle list head, will call Malloc
 *
 */
#define NEW_SLIST_NODE(type, node)                                                                                     \
    {                                                                                                                  \
        node = (type *)Malloc(sizeof(type));                                                                           \
    }

/**
 * @brief cast the slist entry to special type
 *
 */
#define SLIST_ENTRY(ptr, type, member) CNTR_OF(ptr, type, member)

/**
 * @brief traverse each object of the sigle list, cannot change the sigle list
 *
 */
#define SLIST_FOR_EACH_ENTRY(tpos, type, pos, list, member)                                                            \
    for (pos = (list)->next; pos && (tpos = SLIST_ENTRY(pos, type, member), 1); pos = pos->next)

/**
 * @brief traverse each object of the sigle list, you can add or del the object during traverse
 *
 */
#define SLIST_FOR_EACH_ENTRY_SAFE(tpos, type, pos, n, list, member)                                                    \
    for (pos = (list)->next; pos && (n = pos->next, 1) && (tpos = SLIST_ENTRY(pos, type, member), 1); pos = n)

/**
 * @brief traverse each node of the sigle list, cannot change the sigle list
 *
 */
#define SLIST_FOR_EACH(pos, list) for (pos = (list)->next; pos; pos = pos->next)

/**
 * @brief traverse each node of the sigle list, you can add or del the object during traverse
 *
 */
#define SLIST_FOR_EACH_SAFE(pos, n, list)                                                                              \
    for (pos = (list)->next; pos && ({                                                                                 \
                                 n = pos->next;                                                                        \
                                 1;                                                                                    \
                             });                                                                                       \
         pos = n)

/**
 * @brief traverse and free each object of the sigle list
 *
 */
#define FREE_SLIST_SAFE(tpos, type, pos, n, list, member)                                                              \
    {                                                                                                                  \
        type *posnode;                                                                                                 \
        SLIST_FOR_EACH_ENTRY_SAFE(tpos, type, pos, n, list, member)                                                    \
        {                                                                                                              \
            (list)->next = n;                                                                                          \
            posnode = tpos;                                                                                            \
            Free(posnode);                                                                                             \
        }                                                                                                              \
    }

/**
 * @brief sigle list head initialization
 *
 * @param[in] node the sigle list head
 * @return void
 */
static inline void tuya_init_slist_node(SLIST_HEAD *node)
{
    node->next = NULL;
}

/**
 * @brief check if the sigle list is empty
 *
 * @param[in] list the sigle list head
 * @return 0 means not empty, others means empty
 */
static inline int tuya_slist_empty(const SLIST_HEAD *list)
{
    return !(list->next);
}

/**
 * @brief delete the special sigle list node
 *
 * @param[in] list the sigle list head
 * @param[in] node the node need delete
 * @return void
 */
static inline void tuya_slist_del(SLIST_HEAD *list, SLIST_HEAD *node)
{
    SLIST_HEAD *pos = NULL;
    SLIST_HEAD *last = list;

    SLIST_FOR_EACH(pos, list)
    {
        if (pos != node) {
            last = pos;
            continue;
        }

        // success find and delete
        last->next = pos->next;
        node->next = NULL;
        break;
    }
}

/**
 * @brief add node at head of the sigle list
 *
 * @param[in] list the sigle list head
 * @param[in] n the new node
 * @return void
 */
static inline void tuya_slist_add_head(SLIST_HEAD *list, SLIST_HEAD *n)
{
    n->next = list->next;
    list->next = n;
}

/**
 * @brief add node at the tail of the sigle list
 *
 * @param[in] list the sigle list head
 * @param[in] n the new node
 * @return void
 */
static inline void tuya_slist_add_tail(SLIST_HEAD *list, SLIST_HEAD *n)
{
    SLIST_HEAD *pos = NULL;
    SLIST_HEAD *last = list;

    SLIST_FOR_EACH(pos, list)
    {
        last = pos;
    }
    last->next = n;
}

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif

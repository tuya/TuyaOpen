/**
 * @file tuya_hlist.h
 * @brief tuya hash list module
 * @version 1.0
 * @date 2019-10-30
 *
 * @copyright Copyright 2021-2025 Tuya Inc. All Rights Reserved.
 *
 */
#ifndef __TUYA_HLIST_H__
#define __TUYA_HLIST_H__

#include "tuya_cloud_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief hash list node
 *
 */
typedef struct hlist_node {
    struct hlist_node *next, **pprev;
} HLIST_NODE;

/**
 * @brief hash list head
 *
 */
typedef struct hlist_head {
    struct hlist_node *first;
} HLIST_HEAD;

/**
 * @brief hash list head initialization
 *
 */
#define HLIST_HEAD_INIT {.first = NULL}

/**
 * @brief hash list head define and initialization
 *
 */
#define HLIST_HEAD(name) HLIST_HEAD name = {.first = NULL}

/**
 * @brief hash list head pointer initialization
 *
 */
#define INIT_HLIST_HEAD(ptr) ((ptr->first) = NULL)

/**
 * @brief convert hash list node to object
 *
 */
#define HLIST_ENTRY(ptr, type, member) CNTR_OF(ptr, type, member)

/**
 * @brief traverse the member of the hash list
 *
 */
#define HLIST_FOR_EACH_ENTRY(tpos, type, pos, head, member)                                                            \
    for (pos = (head)->first; pos && (tpos = HLIST_ENTRY(pos, type, member), 1); pos = pos->next)

/**
 * @brief traverse the member of the hash list from a special member
 *
 */
#define HLIST_FOR_EACH_ENTRY_CURR(tpos, type, pos, curr, member)                                                       \
    for (pos = (curr)->next; pos && (tpos = HLIST_ENTRY(pos, type, member), 1); pos = pos->next)

/**
 * @brief traverse the member of the hash list safe mode, can change the hash list when traverse
 *
 */
#define HLIST_FOR_EACH_ENTRY_SAFE(tpos, type, pos, n, head, member)                                                    \
    for (pos = (head)->first; pos && (n = pos->next, 1) && (tpos = HLIST_ENTRY(pos, type, member), 1); pos = n)

/**
 * @brief traverse the hash list node
 *
 */
#define HLIST_FOR_EACH(pos, head) for (pos = (head)->first; pos; pos = pos->next)

/**
 * @brief traverse the hash list node safe mode, can change the hash list when traverse
 *
 */
#define HLIST_FOR_EACH_SAFE(pos, n, head)                                                                              \
    for (pos = (head)->first; pos && ({                                                                                \
                                  n = pos->next;                                                                       \
                                  1;                                                                                   \
                              });                                                                                      \
         pos = n)

/**
 * @brief hash list initialization
 *
 * @param[in] h the hash list head
 * @return void
 */
static inline void tuya_init_hlist_node(HLIST_NODE *h)
{
    h->next = NULL;
    h->pprev = NULL;
}

/**
 * @brief check if the hash list is empty
 *
 * @param[in] h the hash list head
 * @return 1 means empty,  0 means not empty
 */
static inline int tuya_hlist_empty(const HLIST_HEAD *h)
{
    return !h->first;
}

/**
 * @brief check if the hash list is unhashed
 *
 * @param[in] h the hash list head
 * @return 1 means unhashed,  0 means not hashed
 */
static inline int tuya_hlist_unhashed(const HLIST_NODE *h)
{
    return !h->pprev;
}

/**
 * @brief a internal function used to delete member from hash list
 *
 * @param[in] n the hash node need to delete
 * @return void
 *
 * @note call  tuya_hlist_del
 */
static inline void __tuya_hlist_del(HLIST_NODE *n)
{
    HLIST_NODE *next = n->next;
    HLIST_NODE **pprev = n->pprev;
    *pprev = next;
    if (next)
        next->pprev = pprev;
}

/**
 * @brief delete a hash node from hash list
 *
 * @param[in] n the hash node need to delete
 * @return void
 */
static inline void tuya_hlist_del(HLIST_NODE *n)
{
    __tuya_hlist_del(n);
    // n->next = NULL;
    // n->pprev = NULL;
}

/**
 * @brief delete a hash node from hash list and initialize it if the hash is is hashed
 *
 * @param[in] n the hash node need to delete
 * @return void
 */
static inline void tuya_hlist_del_init(HLIST_NODE *n)
{
    if (!tuya_hlist_unhashed(n)) {
        __tuya_hlist_del(n);
        tuya_init_hlist_node(n);
    }
}

/**
 * @brief add a hash node to the head of the hash list
 *
 * @param[in] n the hash node need to add to head
 * @param[in] h the hash list head
 * @return void
 */
static inline void tuya_hlist_add_head(HLIST_NODE *n, HLIST_HEAD *h)
{
    struct hlist_node *first = h->first;
    n->next = first;
    if (first)
        first->pprev = &n->next;
    h->first = n;
    n->pprev = &h->first;
}

/**
 * @brief add a hash node before a special hash node
 *
 * @param[in] n the hash node need to add
 * @param[in] next the special hash node
 * @return void
 *
 * @note the next must not NULL
 */
static inline void tuya_hlist_add_before(HLIST_NODE *n, HLIST_NODE *next)
{
    n->pprev = next->pprev;
    n->next = next;
    next->pprev = &n->next;
    *(n->pprev) = n;
}

/**
 * @brief add a hash node after a special hash node
 *
 * @param[in] n the hash node need to add
 * @param[in] next the spcial hash node
 * @return void
 *
 * @note the next must not NULL
 */
static inline void tuya_hlist_add_after(HLIST_NODE *n, HLIST_NODE *next)
{
    next->next = n->next;
    n->next = next;
    next->pprev = &n->next;

    if (next->next)
        next->next->pprev = &next->next;
}

#ifdef __cplusplus
}
#endif /* __cplusplus */
#endif

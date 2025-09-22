/**
 * @file tuya_list.h
 * @brief tuya bidirection list module
 * @version 1.0
 * @date 2019-10-30
 *
 * @copyright Copyright 2021-2025 Tuya Inc. All Rights Reserved.
 *
 */
#ifndef __TUYA_LIST_H__
#define __TUYA_LIST_H__

#include "tuya_cloud_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief bidirection list head
 *
 */
typedef struct tuya_list_head {
    struct tuya_list_head *next, *prev;
} LIST_HEAD, *P_LIST_HEAD;

/**
 * @brief define and initialize bidirection list head
 *
 */
#define LIST_HEAD_INIT(name) {&(name), &(name)}
#define LIST_HEAD(name)      LIST_HEAD name = LIST_HEAD_INIT(name)

/**
 * @brief bidirection list initialization
 *
 */
#define INIT_LIST_HEAD(ptr)                                                                                            \
    do {                                                                                                               \
        (ptr)->next = (ptr);                                                                                           \
        (ptr)->prev = (ptr);                                                                                           \
    } while (0)

/**
 * @brief create a new bidirection list, will call malloc
 *
 */
#define NEW_LIST_NODE(type, node)                                                                                      \
    {                                                                                                                  \
        node = (type *)tal_malloc(sizeof(type));                                                                       \
    }

/**
 * @brief free all objects the bidirection list
 *
 */
#define FREE_LIST(type, p, list_name)                                                                                  \
    {                                                                                                                  \
        type *posnode;                                                                                                 \
        while (!tuya_list_empty(&(p)->list_name)) {                                                                    \
            posnode = tuya_list_entry((&(p)->list_name)->next, type, list_name);                                       \
            tuya_list_del((&(p)->list_name)->next);                                                                    \
            tal_free(posnode);                                                                                         \
        }                                                                                                              \
    }

/**
 * @brief get the first object of the bidirection list
 *
 */
#define GetFirstNode(type, p, list_name, pGetNode)                                                                     \
    {                                                                                                                  \
        pGetNode = NULL;                                                                                               \
        while (!tuya_list_empty(&(p)->list_name)) {                                                                    \
            pGetNode = tuya_list_entry((&(p)->list_name)->next, type, list_name);                                      \
            break;                                                                                                     \
        }                                                                                                              \
    }

/**
 * @brief remove the object from bidirection list and free the memory
 *
 * @note the pDelNode must be the object pointer
 */
#define DeleteNodeAndFree(pDelNode, list_name)                                                                         \
    {                                                                                                                  \
        tuya_list_del(&(pDelNode->list_name));                                                                         \
        tal_free(pDelNode);                                                                                            \
    }

/**
 * @brief remove the object from the bidirection list
 *
 */
#define DeleteNode(pDelNode, list_name)                                                                                \
    {                                                                                                                  \
        tuya_list_del(&(pDelNode->list_name));                                                                         \
    }

/**
 * @brief free the object bidirection list
 *
 */
#define FreeNode(pDelNode)                                                                                             \
    {                                                                                                                  \
        tal_free(pDelNode);                                                                                            \
    }

/**
 * @brief cast the bidirection list node to object
 *
 */
#define tuya_list_entry(ptr, type, member) ((type *)((char *)(ptr) - (size_t)(&((type *)0)->member)))

/**
 * @brief traverse the bidirection list, cannot change the bidiretion list during traverse
 *
 */
#define tuya_list_for_each(pos, head) for (pos = (head)->next; (pos != NULL) && (pos != (head)); pos = pos->next)

/**
 * @brief traverse the bidirection list, can change the bidiretion list during traverse
 *
 */
#define tuya_list_for_each_safe(p, n, head) for (p = (head)->next; n = p->next, p != (head); p = n)

/**
 * @brief check if the bidirection list is empty
 *
 * @param[in] pHead the bidirection list
 * @return 0 means empty, others means empty
 */
int tuya_list_empty(const P_LIST_HEAD pHead);

/**
 * @brief add new list node into bidirection list
 *
 * @param[in] pNew the new list node
 * @param[in] pHead the bidirection list
 * @return void
 */
void tuya_list_add(const P_LIST_HEAD pNew, const P_LIST_HEAD pHead);

/**
 * @brief add new list node to the tail of the bidirection list
 *
 * @param[in] pNew the new list node
 * @param[in] pHead the bidirection list
 * @return void
 */
void tuya_list_add_tail(const P_LIST_HEAD pNew, const P_LIST_HEAD pHead);

/**
 * @brief splice two dibrection list
 *
 * @param[in] pList the bidirection list need to splice
 * @param[in] pHead the bidirection list
 * @return void
 */
void tuya_list_splice(const P_LIST_HEAD pList, const P_LIST_HEAD pHead);

/**
 * @brief remove a list node from bidirection list
 *
 * @param[in] pEntry the list node need to remove
 * @return void
 */
void tuya_list_del(const P_LIST_HEAD pEntry);

/**
 * @brief remove a list node from bidirection list and initialize it
 *
 * @param[in] pEntry the list node need to remove and initialize
 * @return void
 */
void tuya_list_del_init(const P_LIST_HEAD pEntry);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif

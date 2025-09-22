/**
 * @file tuya_list.c
 * @brief tuya bidirection list module
 * @version 1.0
 * @date 2019-10-30
 *
 * @copyright Copyright 2021-2025 Tuya Inc. All Rights Reserved.
 *
 */

#include "tuya_list.h"

/**
 * @brief add a new to the liste between prev and next
 *
 * @param[in] pNew  the new node
 * @param[in] pPrev the prev node
 * @param[in] pNext the next node
 * @return void
 */
static void __list_add(const P_LIST_HEAD pNew, const P_LIST_HEAD pPrev, const P_LIST_HEAD pNext)
{
    pNext->prev = pNew;
    pNew->next = pNext;
    pNew->prev = pPrev;
    pPrev->next = pNew;
}

/**
 * @brief delete the node between prev and next
 *
 * @param[in] pPrev the prev node
 * @param[in] pNext the next node
 * @return void
 */
static void __list_del(const P_LIST_HEAD pPrev, const P_LIST_HEAD pNext)
{
    pNext->prev = pPrev;
    pPrev->next = pNext;
}

/**
 * @brief check if the bidirection list is empty
 *
 * @param[in] pHead the bidirection list
 * @return 0 means empty, others means empty
 */
int tuya_list_empty(const P_LIST_HEAD pHead)
{
    return pHead->next == pHead;
}

/**
 * @brief add new list node into bidirection list
 *
 * @param[in] pNew the new list node
 * @param[in] pHead the bidirection list
 * @return void
 */
void tuya_list_add(const P_LIST_HEAD pNew, const P_LIST_HEAD pHead)
{
    __list_add(pNew, pHead, pHead->next);
}

/**
 * @brief add new list node to the tail of the bidirection list
 *
 * @param[in] pNew the new list node
 * @param[in] pHead the bidirection list
 * @return void
 */
void tuya_list_add_tail(const P_LIST_HEAD pNew, const P_LIST_HEAD pHead)
{
    __list_add(pNew, pHead->prev, pHead);
}

/**
 * @brief splice two dibrection list
 *
 * @param[in] pList the bidirection list need to splice
 * @param[in] pHead the bidirection list
 * @return void
 */
void tuya_list_splice(const P_LIST_HEAD pList, const P_LIST_HEAD pHead)
{
    P_LIST_HEAD pFirst = pList->next;

    if (pFirst != pList) // 该链表不为空
    {
        P_LIST_HEAD pLast = pList->prev;
        P_LIST_HEAD pAt = pHead->next; // list接合处的节点

        pFirst->prev = pHead;
        pHead->next = pFirst;
        pLast->next = pAt;
        pAt->prev = pLast;
    }
}

/**
 * @brief remove a list node from bidirection list
 *
 * @param[in] pEntry the list node need to remove
 * @return void
 */
void tuya_list_del(const P_LIST_HEAD pEntry)
{
    __list_del(pEntry->prev, pEntry->next);
}

/**
 * @brief remove a list node from bidirection list and initialize it
 *
 * @param[in] pEntry the list node need to remove and initialize
 * @return void
 */
void tuya_list_del_init(const P_LIST_HEAD pEntry)
{
    __list_del(pEntry->prev, pEntry->next);
    INIT_LIST_HEAD(pEntry);
}

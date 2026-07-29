/* host stub: minimal intrusive list (subset used by tdl_power.c) */
#ifndef __STUB_TUYA_LIST_H__
#define __STUB_TUYA_LIST_H__
#include <stddef.h>
typedef struct tuya_list_head {
    struct tuya_list_head *next, *prev;
} LIST_HEAD;
#define INIT_LIST_HEAD(ptr)                                                                                            \
    do {                                                                                                               \
        (ptr)->next = (ptr);                                                                                           \
        (ptr)->prev = (ptr);                                                                                           \
    } while (0)
static inline void tuya_list_add(LIST_HEAD *n, LIST_HEAD *head)
{
    n->next          = head->next;
    n->prev          = head;
    head->next->prev = n;
    head->next       = n;
}
#define tuya_list_entry(ptr, type, member) ((type *)((char *)(ptr) - (size_t)(&((type *)0)->member)))
#define tuya_list_for_each(pos, head) for (pos = (head)->next; (pos != NULL) && (pos != (head)); pos = pos->next)
#endif

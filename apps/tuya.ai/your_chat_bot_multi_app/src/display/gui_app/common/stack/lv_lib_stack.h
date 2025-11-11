#ifndef LV_LIB_STACK_H
#define LV_LIB_STACK_H

#ifdef __cplusplus
extern "C" {
#endif

#include "../lv_lib_conf.h"

#if LV_USE_LIB_STACK

#include "../stack/lv_lib_stack.h"
#include <stdio.h>
#include <stdlib.h>

// 栈节点类型
typedef struct lv_lib_stack_node_t {
    void *data;  // 任意数据指针
} lv_lib_stack_node_t;

// 栈管理结构体
typedef struct {
    lv_lib_stack_node_t *stack; // 栈数组
    int top;                    // 栈顶索引
    int capacity;               // 栈的容量
} lv_lib_stack_t;

// 栈管理接口函数

void lv_lib_stack_init(lv_lib_stack_t *stack, int capacity);
int lv_lib_stack_is_empty(const lv_lib_stack_t *stack);
int lv_lib_stack_is_full(const lv_lib_stack_t *stack);
int lv_lib_stack_push(lv_lib_stack_t *stack, void *data);
void *lv_lib_stack_pop(lv_lib_stack_t *stack);
void *lv_lib_stack_top(const lv_lib_stack_t *stack);
void lv_lib_stack_clear_content(lv_lib_stack_t *stack);
void lv_lib_stack_destroy(lv_lib_stack_t *stack);

#endif // LV_USE_LIB_STACK

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif // LV_LIB_STACK_H
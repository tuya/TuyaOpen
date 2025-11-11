#include "lv_lib_stack.h"
#include <string.h>
#include <stdlib.h>

/**
 * @brief 初始化栈
 * 
 * 分配内存空间，并初始化栈的结构，设置栈顶为-1，栈容量为指定值。
 * 
 * @param stack     指向栈结构体的指针
 * @param capacity  栈的最大容量
 */
void lv_lib_stack_init(lv_lib_stack_t *stack, int capacity) {
    // 分配内存给栈数组并检查是否分配成功
    stack->stack = (lv_lib_stack_node_t *)malloc(sizeof(lv_lib_stack_node_t) * capacity);
    if (!stack->stack) {
        LV_LOG_WARN("Stack initialization failed: Memory allocation error.");
        stack->top = -1; // 初始化为空栈
        stack->capacity = 0;
        return;
    }

    // 初始化栈的状态
    stack->top = -1;
    stack->capacity = capacity;
}

/**
 * @brief 判断栈是否为空
 * 
 * 判断栈顶索引是否为-1，表示栈为空。
 * 
 * @param stack 指向栈结构体的指针
 * @return      1: 栈为空, 0: 栈不为空
 */
int lv_lib_stack_is_empty(const lv_lib_stack_t *stack) {
    if (!stack->stack) {
        LV_LOG_WARN("Stack not initialized.");
        return 1; // 栈为空或未初始化
    }
    return stack->top == -1; // 栈为空时，top=-1
}

/**
 * @brief 判断栈是否已满
 * 
 * 判断栈顶索引是否等于容量减1，表示栈已满。
 * 
 * @param stack 指向栈结构体的指针
 * @return      1: 栈已满, 0: 栈未满
 */
int lv_lib_stack_is_full(const lv_lib_stack_t *stack) {
    if (!stack->stack) {
        LV_LOG_WARN("Stack not initialized.");
        return 1; // 栈未初始化
    }
    return stack->top == stack->capacity - 1; // 栈满时，top=capacity-1
}

/**
 * @brief 入栈操作，将数据压入栈中
 * 
 * 判断栈是否已满，如果已满返回错误，未满则将数据压入栈中。
 * 
 * @param stack 指向栈结构体的指针
 * @param data  要压入栈的数据指针
 * @return      0: 成功, -1: 失败（如栈已满）
 */
int lv_lib_stack_push(lv_lib_stack_t *stack, void *data) {
    if (!stack->stack) {
        LV_LOG_WARN("Stack not initialized.");
        return -1; // 栈未初始化
    }
    if (lv_lib_stack_is_full(stack)) {
        LV_LOG_ERROR("Stack overflow! Cannot push data.");
        return -1; // 栈已满
    }

    // 将数据压入栈中
    stack->stack[++stack->top].data = data;
    return 0; // 入栈成功
}

/**
 * @brief 出栈操作，从栈顶弹出数据
 * 
 * 判断栈是否为空，如果为空返回NULL，栈不为空则返回栈顶数据并将栈顶索引减1。
 * 
 * @param stack 指向栈结构体的指针
 * @return      栈顶的数据指针，若栈为空返回NULL
 */
void *lv_lib_stack_pop(lv_lib_stack_t *stack) {
    if (!stack->stack) {
        LV_LOG_WARN("Stack not initialized.");
        return NULL; // 栈未初始化
    }
    if (lv_lib_stack_is_empty(stack)) {
        LV_LOG_INFO("Stack underflow! No data to pop.");
        return NULL; // 栈为空
    }

    // 获取栈顶数据并更新栈顶索引
    return stack->stack[stack->top--].data;
}

/**
 * @brief 获取栈顶数据，但不弹出
 * 
 * 判断栈是否为空，如果为空返回NULL，栈不为空则返回栈顶数据。
 * 
 * @param stack 指向栈结构体的指针
 * @return      栈顶的数据指针，若栈为空返回NULL
 */
void *lv_lib_stack_top(const lv_lib_stack_t *stack) {
    if (!stack->stack) {
        LV_LOG_WARN("Stack not initialized.");
        return NULL; // 栈未初始化
    }
    if (lv_lib_stack_is_empty(stack)) {
        LV_LOG_INFO("Stack is empty! No data on top.");
        return NULL; // 栈为空
    }

    // 返回栈顶数据
    return stack->stack[stack->top].data;
}

/**
 * @brief 清空栈内容，但保留栈结构
 * 
 * 将栈的top索引重置为-1，表示清空栈中的所有数据。
 * 
 * @param stack 指向栈结构体的指针
 */
void lv_lib_stack_clear_content(lv_lib_stack_t *stack) {
    if (!stack->stack) {
        LV_LOG_WARN("Stack not initialized.");
        return; // 栈未初始化
    }
    stack->top = -1; // 清空栈内容
}

/**
 * @brief 销毁栈并释放内存
 * 
 * 释放栈数组占用的内存，并将栈结构重置。
 * 
 * @param stack 指向栈结构体的指针
 */
void lv_lib_stack_destroy(lv_lib_stack_t *stack) {
    if (stack->stack) {
        free(stack->stack);  // 释放内存
        stack->stack = NULL;  // 设置栈数组为NULL
        stack->top = -1;      // 重置栈顶索引
        stack->capacity = 0;  // 重置栈容量
    }
}

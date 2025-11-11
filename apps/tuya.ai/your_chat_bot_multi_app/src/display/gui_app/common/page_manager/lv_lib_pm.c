#include "lv_lib_pm.h"
#include <string.h>
#include <stdlib.h>

lv_lib_pm_page_t* _getPage(const lv_lib_pm_t *manager, const char *name) {
    if (!manager || !name ) return NULL;

    // 遍历所有页面，查找名称匹配的页面
    for (int i = 0; i < manager->num_pages; i++) {
        lv_lib_pm_page_t *page = manager->all_pages[i];
        if (page && page->name && strcmp(page->name, name) == 0) {
            return page;
        }
    }

    // 若未找到，返回 NULL
    return NULL;
}

/**
 * @brief 初始化页面管理器
 * 
 * 初始化页面栈，设置当前页面为 NULL，当前页面深度为 0。
 * 
 * @param manager 指向页面管理器的指针
 */
void lv_lib_pm_Init(lv_lib_pm_t *manager) {
    if (!manager) {
        LV_LOG_WARN("Invalid parameters for init a page.");
        return;
    }

    // 初始化页面栈
    lv_lib_stack_init(&manager->page_stack, LV_PM_MAX_PAGE_HISTORY); // 设定页面栈的容量为
    manager->current_page = NULL;  // 没有当前页面
    manager->cur_depth = 0;        // 当前深度为 0
}

/**
 * @brief 销毁页面管理器
 * 
 * 销毁页面栈并释放相关资源。
 * 
 * @param manager 指向页面管理器的指针
 */
void lv_lib_pm_Deinit(lv_lib_pm_t *manager) {
    if (!manager) {
        LV_LOG_WARN("Invalid parameters for deinit a page.");
        return;
    }

    // 销毁页面栈
    lv_lib_stack_destroy(&manager->page_stack);
    manager->current_page = NULL; // 清空当前页面
    manager->cur_depth = 0;       // 重置深度
}


/**
 * @brief 创建页面
 * 
 * 创建一个页面并返回页面指针
 * 
 * @param manager  页面管理器
 * @param name     页面名称
 * @param init     页面初始化函数
 * @param deinit   页面销毁函数
 * @param page_obj 页面对象
 * @return         创建的页面指针，如果创建失败则返回 NULL
 */
lv_lib_pm_page_t* lv_lib_pm_CreatePage(lv_lib_pm_t *manager, const char *name, void (*init)(void), void (*deinit)(void), lv_obj_t *page_obj) {
    if (!manager || !name || !init || !deinit) {
        LV_LOG_WARN("PageManager: Invalid parameters for creating a page.");
        return NULL;
    }

    // 检查是否已有同名的页面
    if (_getPage(manager, name)) {
        LV_LOG_WARN("PageManager: A page with the name '%s' already exists.", name);
        return NULL;
    }

    // 分配内存创建页面
    lv_lib_pm_page_t *page = (lv_lib_pm_page_t *)malloc(sizeof(lv_lib_pm_page_t));
    if (!page) {
        LV_LOG_WARN("PageManager: Failed to allocate memory for the page.");
        return NULL;
    }

    // 分配并复制页面名称
    page->name = (char *)malloc(strlen(name) + 1);
    if (!page->name) {
        free(page);
        LV_LOG_WARN("PageManager: Failed to allocate memory for the page name.");
        return NULL;
    }
    strcpy(page->name, name);

    // 设置页面的初始化和销毁函数
    page->init = init;
    page->deinit = deinit;
    page->page_obj = page_obj;
    // 将页面添加到所有页面列表
    if (manager->num_pages < LV_PM_MAX_PAGES) {  // 假设你设置了最大页面数
        LV_LOG_INFO("Page created");
        manager->all_pages[manager->num_pages++] = page;
    } else {
        LV_LOG_WARN("PageManager: Maximum number of pages reached.");
        free(page->name);
        free(page);
        return NULL;
    }

    return page;
}

/**
 * @brief 打开指定页面
 * 
 * 打开指定页面并将当前页面压入页面栈，以便后续返回到该页面。
 * 
 * @param manager 指向页面管理器的指针
 * @param page    要打开的页面
 * @param name    查找页面名, 可在没有page参数时使用, 优先级低于page参数
 */
void lv_lib_pm_OpenPage(lv_lib_pm_t *manager, lv_lib_pm_page_t *page, char *name) {
    if (!manager) return;

    // 如果传入的 page 不为空，则优先使用 page
    if (page) {
        // 如果当前页面存在，先销毁它
        if (manager->current_page != NULL && manager->current_page->deinit) {
            manager->current_page->deinit(); // 调用当前页面的销毁函数
        }

        // 将当前页面压入栈中
        lv_lib_stack_push(&manager->page_stack, manager->current_page);

        // 设置新页面为当前页面，并调用其初始化函数
        manager->current_page = page;
        if (page->init) {
            page->init(); // 调用新页面的初始化函数
        }

        manager->cur_depth++; // 增加页面深度
        LV_LOG_INFO("Opened page: %s, depth: %d", page->name, manager->cur_depth);

    } else if (name) { // 如果没有 page，使用 name 查找页面
        lv_lib_pm_page_t *found_page = _getPage(manager, name);
        if (found_page) {
            // 如果找到了页面，打开它
            lv_lib_pm_OpenPage(manager, found_page, NULL); // 递归调用打开页面
        } else {
            LV_LOG_WARN("Page with name '%s' not found.", name);
        }
    } else {
        LV_LOG_WARN("Both page and name are NULL. Cannot open page.");
    }
}

/**
 * @brief 打开上一个页面（返回到上一页面）
 * 
 * 从页面栈中弹出上一个页面并显示，当前页面深度减1。
 * 
 * @param manager 指向页面管理器的指针
 */
void lv_lib_pm_OpenPrePage(lv_lib_pm_t *manager) {
    if (!manager) return;

    // 如果栈为空，表示没有历史页面，无法返回
    if (lv_lib_stack_is_empty(&manager->page_stack)) {
        LV_LOG_WARN("No previous page to return to.");
        return;
    }

    // 获取栈顶的页面作为新页面
    lv_lib_pm_page_t *prev_page = (lv_lib_pm_page_t *)lv_lib_stack_pop(&manager->page_stack);
    
    // 先销毁当前页面
    if (manager->current_page != NULL && manager->current_page->deinit) {
        manager->current_page->deinit(); // 销毁当前页面
    }

    // 设置上一页面为当前页面并初始化
    manager->current_page = prev_page;
    if (prev_page && prev_page->init) {
        prev_page->init(); // 初始化上一页面
    }

    manager->cur_depth--; // 减少当前页面深度
    LV_LOG_INFO("Returned to previous page, depth: %d .", manager->cur_depth);
}

/**
 * @brief 获取当前页面
 * 
 * 获取当前活动页面，通常用于获取页面信息或者进行操作。
 * 
 * @param manager 指向页面管理器的指针
 * @return        当前页面指针，如果没有当前页面则返回 NULL
 */
lv_lib_pm_page_t* lv_lib_pm_GetCurrentPage(const lv_lib_pm_t *manager) {
    if (!manager) return NULL;
    return manager->current_page;
}

/**
 * @brief 返回到栈底页面（底部页面）
 * 
 * 清空页面栈并返回到栈底页面，即回到最初页面。
 * 
 * @param manager 指向页面管理器的指针
 */
void lv_lib_pm_ReturnToBottom(lv_lib_pm_t *manager) {
    if (!manager) return;

    // 如果当前页面存在，销毁它
    if (manager->current_page != NULL && manager->current_page->deinit) {
        manager->current_page->deinit();
    }

    // 获取栈底页面
    lv_lib_pm_page_t *bottom_page = (lv_lib_pm_page_t *)manager->page_stack.stack[0].data;
    if (!bottom_page) {
        LV_LOG_WARN("No pages in stack.");
        return;  // 如果栈中没有页面，直接返回
    }

    // 弹出所有
    while (!lv_lib_stack_is_empty(&manager->page_stack)) {
        lv_lib_stack_pop(&manager->page_stack); // 弹出栈中的页面
    }

    // 设置栈底页面为当前页面并初始化
    manager->current_page = bottom_page;
    if (manager->current_page->init) {
        manager->current_page->init(); // 初始化栈底页面
    }

    manager->cur_depth = 1; // 当前深度恢复为 1, 因为只保留了栈底页面
    LV_LOG_INFO("Returned to bottom page.");
}

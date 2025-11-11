#ifndef LV_LIB_PM_H
#define LV_LIB_PM_H

#ifdef __cplusplus
extern "C" {
#endif

#include "../lv_lib_conf.h"

#if LV_USE_LIB_PAGE_MANAGER

#include "../stack/lv_lib_stack.h"

#define LV_PM_MAX_PAGE_HISTORY 5
#define LV_PM_MAX_PAGES 20

// 定义页面结构
// init() should include load function, 
// deint() should include unload function
typedef struct {
    char *name;                 // 页面名称
    void (*init)(void);         // 页面初始化函数
    void (*deinit)(void);       // 页面销毁函数
    lv_obj_t *page_obj;         // 页面上的主要 LVGL 对象
} lv_lib_pm_page_t;

// 定义 PageManager 类型
typedef struct {
    lv_lib_stack_t page_stack;                      // 页面栈，用于存储历史页面
    lv_lib_pm_page_t *current_page;                 // 当前活动页面
    uint8_t cur_depth;                              // 当前页面深度
    lv_lib_pm_page_t *all_pages[LV_PM_MAX_PAGES];   // 用于存储所有创建的页面
    uint8_t num_pages;                              // 注册页面数量
} lv_lib_pm_t;


void lv_lib_pm_Init(lv_lib_pm_t *manager);                                          
void lv_lib_pm_Deinit(lv_lib_pm_t *manager);
lv_lib_pm_page_t* lv_lib_pm_CreatePage(lv_lib_pm_t *manager, const char *name, void (*init)(void), void (*deinit)(void), lv_obj_t *page_obj);                                       
void lv_lib_pm_OpenPage(lv_lib_pm_t *manager, lv_lib_pm_page_t *page, char *name);  
void lv_lib_pm_OpenPrePage(lv_lib_pm_t *manager);    
lv_lib_pm_page_t* lv_lib_pm_GetCurrentPage(const lv_lib_pm_t *manager);   
void lv_lib_pm_ReturnToBottom(lv_lib_pm_t *manager);                            

#endif  /*LV_USE_100ASK_PAGE_MANAGER*/

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif // LV_LIB_PM_H

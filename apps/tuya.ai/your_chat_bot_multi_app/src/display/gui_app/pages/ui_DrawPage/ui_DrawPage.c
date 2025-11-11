#include "ui_DrawPage.h"
#include <stdlib.h>  // for abs()
#include <stddef.h>  // for size_t
#include "tal_log.h"  // for PR_DEBUG
#include "tkl_memory.h"  // for tal_malloc, tal_free

///////////////////// VARIABLES ////////////////////

// 优化：只分配实际需要的画布大小 (200x200, RGB565格式 = 2字节/像素)
#define CANVAS_WIDTH  300
#define CANVAS_HEIGHT 200
static uint8_t *buf_raw = NULL; // 动态分配的内存指针
lv_obj_t * canvas;

typedef enum {
    UI_DRAW_COLOR_RED,
    UI_DRAW_COLOR_BLUE,
    UI_DRAW_COLOR_BLACK,
    UI_DRAW_COLOR_GREEN,
    UI_DRAW_COLOR_YELLOW,
    UI_DRAW_COLOR_PURPLE,
    UI_DRAW_COLOR_ORANGE,
    UI_DRAW_COLOR_CYAN,
    UI_DRAW_COLOR_PINK,
    UI_DRAW_COLOR_BROWN,
    UI_DRAW_COLOR_MAX // 用于验证颜色范围
} ui_draw_color_t;

struct ui_Draw_para_t{
    uint8_t line_width; // 2, 3, 4, 5 (优化：从uint32_t改为uint8_t，节省3字节)
    lv_color_t line_color; // 当前选中的颜色
    uint8_t line_color_index; // 颜色索引 (0-9: 红、蓝、黑、绿、黄、紫、橙、青、粉、棕)
};

struct ui_Draw_para_t ui_Draw_para;

///////////////////// ANIMATIONS ////////////////////


///////////////////// FUNCTIONS ////////////////////

static void _ui_Draw_clear(void)
{
    // 使用LV_OPA_COVER而不是LV_OPA_100（LVGL v9标准）
    lv_canvas_fill_bg(canvas, lv_color_hex(0xFFFFFF), LV_OPA_COVER);
    // 强制刷新
    lv_obj_invalidate(canvas);
} 

static void ui_canvas_event(lv_event_t * e)
{
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t * obj = lv_event_get_target(e);

    static lv_coord_t last_x = -65535, last_y = -65535;

    // 调试：输出所有事件（只输出触摸相关事件，避免日志过多）
    if(code == LV_EVENT_PRESSED || code == LV_EVENT_PRESSING || code == LV_EVENT_RELEASED || 
       code == LV_EVENT_CLICKED || code == LV_EVENT_SHORT_CLICKED) {
        // 检查事件目标是否是canvas
        if(obj == canvas) {
            // PR_DEBUG("Canvas event: code=%d (target=canvas)", code);
        } else {
            // PR_DEBUG("Page event: code=%d (target=page, canvas=%p)", code, canvas);
            // 如果事件来自父容器，检查触摸点是否在画布上
            if(code == LV_EVENT_PRESSED || code == LV_EVENT_PRESSING) {
                lv_indev_t * indev = lv_event_get_indev(e);
                if(indev == NULL) {
                    indev = lv_indev_active();
                }
                if(indev != NULL) {
                    lv_point_t point;
                    lv_indev_get_point(indev, &point);
                    lv_area_t canvas_area;
                    lv_obj_get_coords(canvas, &canvas_area);
                    if(point.x >= canvas_area.x1 && point.x <= canvas_area.x2 &&
                       point.y >= canvas_area.y1 && point.y <= canvas_area.y2) {
                        PR_DEBUG("Touch on canvas area via page event! Processing as canvas touch...");
                        // 事件来自父容器，但触摸点在画布上，将obj设置为canvas继续处理
                        obj = canvas;
                        // 继续执行下面的处理逻辑，不返回
                    } else {
                        return; // 触摸点不在画布上，忽略
                    }
                } else {
                    return; // 没有输入设备，忽略
                }
            } else {
                return; // 不是触摸事件，忽略
            }
        }
    }

    // 只有canvas事件或已转换为canvas的事件才会执行到这里
    if(code == LV_EVENT_PRESSED || code == LV_EVENT_PRESSING)
    {
        // 获取触摸点的屏幕坐标
        lv_indev_t * indev = lv_event_get_indev(e);
        if(indev == NULL) {
            indev = lv_indev_active();
            if(indev == NULL) {
                PR_DEBUG("Canvas: No input device found!");
                return;
            }
        }
        
        lv_point_t point;
        lv_indev_get_point(indev, &point);
        
        // 获取画布在屏幕上的位置（区域）
        lv_area_t canvas_area;
        lv_obj_get_coords(obj, &canvas_area);
        
        // 将屏幕坐标转换为画布相对坐标
        lv_coord_t canvas_x = point.x - canvas_area.x1;
        lv_coord_t canvas_y = point.y - canvas_area.y1;
        
        // 调试：输出坐标信息
        // PR_DEBUG("Touch: screen(%d,%d) canvas_area(%d,%d,%d,%d) canvas_rel(%d,%d)", 
        //          point.x, point.y, 
        //          canvas_area.x1, canvas_area.y1, canvas_area.x2, canvas_area.y2,
        //          canvas_x, canvas_y);
        
        // 检查坐标是否在画布范围内
        if(canvas_x < 0 || canvas_x >= CANVAS_WIDTH ||
           canvas_y < 0 || canvas_y >= CANVAS_HEIGHT)
        {
            // 触摸点在画布外，重置last坐标
            // PR_DEBUG("Touch outside canvas: (%d,%d) not in [0,%d)x[0,%d)", 
            //          canvas_x, canvas_y, CANVAS_WIDTH, CANVAS_HEIGHT);
            if(code == LV_EVENT_PRESSED) {
                last_x = -65535;
                last_y = -65535;
            }
            return;
        }

        PR_DEBUG("Drawing at canvas (%d,%d)", canvas_x, canvas_y);

        // 如果是第一次按下，直接绘制一个点
        if(last_x == -65535 || last_y == -65535)
        {
            // 使用简单的方法：直接设置像素点（先测试这种方法是否能工作）
            int half_width = ui_Draw_para.line_width / 2;
            for(int dy = -half_width; dy <= half_width; dy++) {
                for(int dx = -half_width; dx <= half_width; dx++) {
                    int px = canvas_x + dx;
                    int py = canvas_y + dy;
                    if(px >= 0 && px < CANVAS_WIDTH && py >= 0 && py < CANVAS_HEIGHT) {
                        // 简单的圆形判断
                        if(dx*dx + dy*dy <= half_width*half_width) {
                            lv_canvas_set_px(canvas, px, py, ui_Draw_para.line_color, LV_OPA_COVER);
                        }
                    }
                }
            }
            // 强制刷新画布
            lv_obj_invalidate(canvas);
        }
        else
        {
            // 绘制从上一个点到当前点的线段
            // 使用Bresenham算法绘制直线
            int x0 = last_x, y0 = last_y;
            int x1 = canvas_x, y1 = canvas_y;
            int dx = abs(x1 - x0);
            int dy = abs(y1 - y0);
            int sx = (x0 < x1) ? 1 : -1;
            int sy = (y0 < y1) ? 1 : -1;
            int err = dx - dy;
            
            // 绘制线条上的每个点
            while(1) {
                // 在点周围绘制一个小圆
                int half_width = ui_Draw_para.line_width / 2;
                for(int dy2 = -half_width; dy2 <= half_width; dy2++) {
                    for(int dx2 = -half_width; dx2 <= half_width; dx2++) {
                        int px = x0 + dx2;
                        int py = y0 + dy2;
                        if(px >= 0 && px < CANVAS_WIDTH && py >= 0 && py < CANVAS_HEIGHT) {
                            if(dx2*dx2 + dy2*dy2 <= half_width*half_width) {
                                lv_canvas_set_px(canvas, px, py, ui_Draw_para.line_color, LV_OPA_COVER);
                            }
                        }
                    }
                }
                
                if(x0 == x1 && y0 == y1) break;
                
                int e2 = 2 * err;
                if(e2 > -dy) {
                    err -= dy;
                    x0 += sx;
                }
                if(e2 < dx) {
                    err += dx;
                    y0 += sy;
                }
            }
            // 强制刷新画布
            lv_obj_invalidate(canvas);
        }

        last_x = canvas_x;
        last_y = canvas_y;
    }
    else if(code == LV_EVENT_RELEASED)
    {
        last_x = -65535;
        last_y = -65535;
    }
}

static void ui_event_clear_btn(lv_event_t * e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if(code == LV_EVENT_CLICKED)
    {
        _ui_Draw_clear();
    }
}

static void ui_event_back_btn(lv_event_t * e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if(code == LV_EVENT_CLICKED)
    {
        lv_lib_pm_OpenPrePage(&page_manager);
    }
}

static void ui_event_color_btn(lv_event_t * e)
{
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t * color_btn = lv_event_get_target(e);
    if(code == LV_EVENT_CLICKED)
    {
        ui_Draw_para.line_color_index += 1;
        if(ui_Draw_para.line_color_index >= UI_DRAW_COLOR_MAX)
        {
            ui_Draw_para.line_color_index = UI_DRAW_COLOR_RED;
        }
        switch(ui_Draw_para.line_color_index)
        {
            case UI_DRAW_COLOR_RED:
                ui_Draw_para.line_color = lv_palette_main(LV_PALETTE_RED);
                break;
            case UI_DRAW_COLOR_BLUE:
                ui_Draw_para.line_color = lv_palette_main(LV_PALETTE_BLUE);
                break;
            case UI_DRAW_COLOR_BLACK:
                ui_Draw_para.line_color = lv_color_black();
                break;
            case UI_DRAW_COLOR_GREEN:
                ui_Draw_para.line_color = lv_palette_main(LV_PALETTE_GREEN);
                break;
            case UI_DRAW_COLOR_YELLOW:
                ui_Draw_para.line_color = lv_palette_main(LV_PALETTE_YELLOW);
                break;
            case UI_DRAW_COLOR_PURPLE:
                ui_Draw_para.line_color = lv_palette_main(LV_PALETTE_PURPLE);
                break;
            case UI_DRAW_COLOR_ORANGE:
                ui_Draw_para.line_color = lv_palette_main(LV_PALETTE_ORANGE);
                break;
            case UI_DRAW_COLOR_CYAN:
                ui_Draw_para.line_color = lv_palette_main(LV_PALETTE_CYAN);
                break;
            case UI_DRAW_COLOR_PINK:
                ui_Draw_para.line_color = lv_palette_main(LV_PALETTE_PINK);
                break;
            case UI_DRAW_COLOR_BROWN:
                ui_Draw_para.line_color = lv_palette_main(LV_PALETTE_BROWN);
                break;
            default:
                ui_Draw_para.line_color = lv_palette_main(LV_PALETTE_RED);
                break;
        }
        lv_obj_set_style_bg_color(color_btn, ui_Draw_para.line_color, 0);
    }
}

static void ui_event_width_btn(lv_event_t * e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if(code == LV_EVENT_CLICKED)
    {
        ui_Draw_para.line_width++;
        if(ui_Draw_para.line_width > 5)
        {
            ui_Draw_para.line_width = 2;
        }
        lv_obj_t * width_btn = lv_event_get_target(e);
        lv_obj_t * width_btn_label = lv_obj_get_child(width_btn, 0);
        char width_str[5];
        sprintf(width_str, "W: %d", ui_Draw_para.line_width);
        lv_label_set_text(width_btn_label, width_str);
    }
}

///////////////////// SCREEN init ////////////////////

void ui_DrawPage_init()
{
    lv_obj_t * DrawPage = lv_obj_create(NULL);
    // 确保父容器不遮挡子控件
    lv_obj_clear_flag(DrawPage, LV_OBJ_FLAG_SCROLLABLE);
    // 注意：不要清除CLICKABLE，这样父容器可以传递事件给子控件
    // 但我们需要确保画布能接收事件
    lv_obj_set_style_bg_color(DrawPage, lv_color_hex(0x808080), LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_Draw_para.line_color_index = UI_DRAW_COLOR_RED;
    ui_Draw_para.line_color = lv_palette_main(LV_PALETTE_RED);
    ui_Draw_para.line_width = 2;

    // 先创建按钮，确保画布在按钮之上
    lv_obj_t * clear_btn = lv_btn_create(DrawPage);
    lv_obj_align(clear_btn, LV_ALIGN_TOP_RIGHT, -5, 10);
    lv_obj_set_size(clear_btn, 75,40);
    lv_obj_t * clear_btn_label = lv_label_create(clear_btn);
    lv_label_set_text(clear_btn_label,"Clear");
    lv_obj_align(clear_btn_label, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_text_font(clear_btn_label, &lv_font_montserrat_16, 0);
    lv_obj_add_event_cb(clear_btn, ui_event_clear_btn, LV_EVENT_ALL, NULL);

    lv_obj_t * back_btn = lv_btn_create(DrawPage);
    lv_obj_align(back_btn, LV_ALIGN_TOP_LEFT, 5, 10);
    lv_obj_set_size(back_btn, 75,40);
    lv_obj_t * back_btn_label = lv_label_create(back_btn);
    lv_label_set_text(back_btn_label,"Back");
    lv_obj_align(back_btn_label, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_text_font(back_btn_label, &lv_font_montserrat_16, 0);
    lv_obj_add_event_cb(back_btn, ui_event_back_btn, LV_EVENT_ALL, NULL);

    lv_obj_t * color_btn = lv_btn_create(DrawPage);
    lv_obj_align(color_btn, LV_ALIGN_TOP_MID, -35, 10);
    lv_obj_set_size(color_btn, 60, 40);
    lv_obj_set_style_bg_color(color_btn, ui_Draw_para.line_color, 0);
    lv_obj_t * color_btn_label = lv_label_create(color_btn);
    lv_label_set_text(color_btn_label,"Color");
    lv_obj_align(color_btn_label, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_text_font(color_btn_label, &lv_font_montserrat_16, 0);
    lv_obj_add_event_cb(color_btn, ui_event_color_btn, LV_EVENT_ALL, NULL);

    lv_obj_t * width_btn = lv_btn_create(DrawPage);
    lv_obj_align(width_btn, LV_ALIGN_TOP_MID, 35, 10);
    lv_obj_set_size(width_btn, 60, 40);
    lv_obj_set_style_bg_color(width_btn, lv_palette_main(LV_PALETTE_CYAN), 0);
    lv_obj_t * width_btn_label = lv_label_create(width_btn);
    char width_str[5];
    sprintf(width_str, "W: %d", ui_Draw_para.line_width);
    lv_label_set_text(width_btn_label, width_str);
    lv_obj_align(width_btn_label, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_text_font(width_btn_label, &lv_font_montserrat_16, 0);
    lv_obj_add_event_cb(width_btn, ui_event_width_btn, LV_EVENT_ALL, NULL);

    // 动态分配画布buffer内存
    size_t buf_size = CANVAS_WIDTH * CANVAS_HEIGHT * 2; // RGB565格式 = 2字节/像素
    buf_raw = (uint8_t *)tkl_system_psram_malloc(buf_size);
    if(buf_raw == NULL) {
        PR_ERR("Failed to allocate canvas buffer! Size: %zu bytes", buf_size);
        return; // 内存分配失败，无法继续
    }
    PR_DEBUG("Canvas buffer allocated: %zu bytes", buf_size);
    
    // 创建画布，放在按钮之后以确保在正确的层级
    canvas = lv_canvas_create(DrawPage);
    
    // 先设置大小和buffer（必须在填充背景之前）
    lv_obj_set_size(canvas, CANVAS_WIDTH, CANVAS_HEIGHT);
    lv_canvas_set_buffer(canvas, buf_raw, CANVAS_WIDTH, CANVAS_HEIGHT, LV_COLOR_FORMAT_NATIVE);
    
    // 关键：立即填充背景色，这样canvas才会显示内容
    // 使用浅灰色以便与白色页面背景区分
    lv_canvas_fill_bg(canvas, lv_color_hex(0xF0F0F0), LV_OPA_COVER);
    
    // 确保画布没有被隐藏
    lv_obj_clear_flag(canvas, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(canvas, LV_OBJ_FLAG_SCROLLABLE);
    // 确保画布可以接收点击事件
    lv_obj_add_flag(canvas, LV_OBJ_FLAG_CLICKABLE);
    // 确保画布可以接收所有输入事件
    lv_obj_clear_flag(canvas, LV_OBJ_FLAG_ADV_HITTEST); // 清除高级点击测试，使用简单模式
    
    // 设置画布对象样式背景色（虽然canvas内容已填充，但样式背景可以作为备用）
    lv_obj_set_style_bg_color(canvas, lv_color_hex(0xF0F0F0), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(canvas, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);
    
    // 设置明显的边框以便看到画布位置（红色边框，宽度3）
    lv_obj_set_style_border_color(canvas, lv_color_hex(0xFF0000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(canvas, 3, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(canvas, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);
    
    // 将画布位置稍微下移，避免与按钮重叠
    lv_obj_align(canvas, LV_ALIGN_CENTER, 0, 20);
    
    // 获取画布位置用于调试
    lv_area_t canvas_pos;
    lv_obj_get_coords(canvas, &canvas_pos);
    PR_DEBUG("Canvas created: size=%dx%d, pos=(%d,%d,%d,%d)", 
             CANVAS_WIDTH, CANVAS_HEIGHT,
             canvas_pos.x1, canvas_pos.y1, canvas_pos.x2, canvas_pos.y2);
    
    // 确保画布在最上层，可以接收触摸事件
    lv_obj_move_foreground(canvas);
    
    // 确保画布可以接收所有输入事件
    lv_obj_clear_flag(canvas, LV_OBJ_FLAG_GESTURE_BUBBLE); // 不冒泡手势事件
    
    // 测试：在画布上绘制一个明显的测试图案，确认画布绘制功能正常
    // 绘制一个红色十字
    for(int i = 0; i < 20; i++) {
        lv_canvas_set_px(canvas, 10 + i, 10, lv_palette_main(LV_PALETTE_RED), LV_OPA_COVER);
        lv_canvas_set_px(canvas, 20, 10 + i, lv_palette_main(LV_PALETTE_RED), LV_OPA_COVER);
    }
    PR_DEBUG("Test pattern drawn at (10,10) - red cross");
    
    // 强制刷新画布显示（确保canvas内容被渲染）
    lv_obj_invalidate(canvas);
    lv_obj_refresh_ext_draw_size(canvas);

    // 将事件绑定到canvas上，监听触摸相关事件
    // 使用 LV_EVENT_ALL 确保捕获所有可能的事件（用于调试）
    // 注意：生产环境可以改为只监听需要的触摸事件
    lv_obj_add_event_cb(canvas, ui_canvas_event, LV_EVENT_ALL, NULL);
    
    // 调试：也在父容器上添加事件监听，看看事件是否被父容器拦截
    lv_obj_add_event_cb(DrawPage, ui_canvas_event, LV_EVENT_ALL, NULL);

    // load page
    lv_scr_load_anim(DrawPage, LV_SCR_LOAD_ANIM_MOVE_RIGHT, 100, 0, true);
}

/////////////////// SCREEN deinit ////////////////////

void ui_DrawPage_deinit()
{
    // 释放动态分配的画布buffer内存
    if(buf_raw != NULL) {
        tkl_system_psram_free(buf_raw);
        buf_raw = NULL;
        PR_DEBUG("Canvas buffer freed");
    }
    
    // 清理canvas对象（如果需要的话）
    if(canvas != NULL) {
        canvas = NULL;
    }
}
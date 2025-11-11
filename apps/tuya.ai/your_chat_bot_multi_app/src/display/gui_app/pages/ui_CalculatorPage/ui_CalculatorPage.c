#include "ui_CalculatorPage.h"
#include "app_CalculatorPage.h"
#include <string.h>  // for memset

///////////////////// VARIABLES ////////////////////

#define TEXT_FULL 10
// 修复：确保全局变量被初始化为0
StrStack_t CalStr = {0};
NumStack_t NumStack = {0};
SymStack_t SymStack = {0};
static const char * ui_ComPageBtnmap[] ={"1", "2", "3", "+", "\n",
                                         "4", "5", "6", "-", "\n",
                                         "7", "8", "9", "×", "\n",
                                         ".", "0", "=", "÷", ""};
lv_obj_t * ui_CompageBtnM = NULL;
///////////////////// ANIMATIONS ////////////////////


///////////////////// FUNCTIONS ////////////////////

static void ui_enent_Gesture(lv_event_t * e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if(code == LV_EVENT_GESTURE)
    {
        if(lv_indev_get_gesture_dir(lv_indev_get_act()) == LV_DIR_LEFT || lv_indev_get_gesture_dir(lv_indev_get_act()) == LV_DIR_RIGHT)
        {
            lv_lib_pm_OpenPrePage(&page_manager);
        }
    }
}

void ui_CompageBtnM_event_cb(lv_event_t * e)
{
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t * obj = lv_event_get_target(e);
    lv_obj_t * ui_CompageTextarea = lv_event_get_user_data(e);
    if(code == LV_EVENT_DRAW_TASK_ADDED)
    {
        // 修复：添加空指针检查
        if(obj == NULL)
        {
            return;
        }
        
        lv_draw_task_t * draw_task = lv_event_get_draw_task(e);
        if(draw_task == NULL || draw_task->draw_dsc == NULL)
        {
            return;
        }
        
        lv_draw_dsc_base_t * base_dsc = draw_task->draw_dsc;
        if(base_dsc == NULL)
        {
            return;
        }
        
        if(base_dsc->part == LV_PART_ITEMS)
        {
            if(base_dsc->id1 >= 0)
            {
                lv_draw_fill_dsc_t * fill_draw_dsc = (lv_draw_fill_dsc_t *)draw_task->draw_dsc;
                if(fill_draw_dsc != NULL)
                {
                    if(base_dsc->id1 == 3 || base_dsc->id1 == 7 || base_dsc->id1 == 11 || base_dsc->id1 == 14 || base_dsc->id1 == 15)
                    {
                        fill_draw_dsc->radius = LV_RADIUS_CIRCLE;
                        uint16_t selected_btn = lv_btnmatrix_get_selected_btn(obj);
                        if (selected_btn == base_dsc->id1)
                        {
                            fill_draw_dsc->color = lv_palette_darken(LV_PALETTE_BLUE,3);
                            //lv_btnmatrix_set_selected_btn(ui_CompageBtnM,NULL);
                        }
                        else
                        {
                            fill_draw_dsc->color = lv_palette_main(LV_PALETTE_BLUE);
                        }
                    }
                }
            }
        }
    }
    if(code == LV_EVENT_VALUE_CHANGED)
    {
        // 修复：添加空指针检查
        if(obj == NULL || ui_CompageTextarea == NULL)
        {
            return;
        }
        
        uint16_t btn_id = lv_btnmatrix_get_selected_btn(obj); // 获取当前选中的按键的id
        
        // 修复：检查btn_id是否有效（按钮ID应该在0-15之间）
        if(btn_id > 15)
        {
            return;
        }
        
        const char * txt = lv_btnmatrix_get_btn_text(obj, btn_id); // 获取当前按键的文本

        // 修复：特殊处理 "=" 按钮，先计算再显示
        if(btn_id == 14) // "=" 按钮
        {
            //calculate
            // 修复：检查字符串是否为空（在添加 '=' 之前检查）
            if(CalStr.Top_Point == 0 || CalStr.strque[0] == '\0')
            {
                if(ui_CompageTextarea != NULL)
                {
                    lv_textarea_add_text(ui_CompageTextarea,"erro");
                }
            }
            else
            {
                // 修复：确保字符串以\0结尾，并移除\n和=字符
                char calc_str[11];
                int i, j = 0;
                for(i = 0; i < CalStr.Top_Point && i < 10; i++)
                {
                    if(CalStr.strque[i] != '\n' && CalStr.strque[i] != '=')
                    {
                        calc_str[j++] = CalStr.strque[i];
                    }
                }
                calc_str[j] = '\0'; // 确保字符串以\0结尾
                
                if(j == 0)
                {
                    if(ui_CompageTextarea != NULL)
                    {
                        lv_textarea_add_text(ui_CompageTextarea,"erro");
                    }
                }
                else if(StrCalculate(calc_str,&NumStack,&SymStack))
                {
                    if(ui_CompageTextarea != NULL)
                    {
                        lv_textarea_add_text(ui_CompageTextarea,"erro");
                    }
                }
                else
                {
                    // 修复：检查NumStack是否有结果
                    if(NumStack.Top_Point > 0 && ui_CompageTextarea != NULL)
                    {
                        char strout[16];  // 修复：增加缓冲区大小，避免溢出
                        float result = NumStack.data[NumStack.Top_Point-1];
                        
                        // 修复：检查结果是否为有效数字（非NaN、非Inf）
                        // 使用简单的范围检查，避免使用可能不支持的Inf常量
                        if(result == result && result >= -1000000.0f && result <= 1000000.0f)
                        {
                            if(isIntNumber(result))
                            {
                                // 修复：使用整数格式化，避免浮点数问题
                                int int_result = (int)result;
                                // 手动格式化整数，避免sprintf的浮点数问题
                                if(int_result == 0)
                                {
                                    strout[0] = '0';
                                    strout[1] = '\0';
                                }
                                else
                                {
                                    int idx = 0;
                                    int temp = int_result;
                                    if(temp < 0)
                                    {
                                        strout[idx++] = '-';
                                        temp = -temp;
                                    }
                                    char digits[10];
                                    int digit_count = 0;
                                    while(temp > 0 && digit_count < 10)
                                    {
                                        digits[digit_count++] = '0' + (temp % 10);
                                        temp /= 10;
                                    }
                                    for(int i = digit_count - 1; i >= 0 && idx < 15; i--)
                                    {
                                        strout[idx++] = digits[i];
                                    }
                                    strout[idx] = '\0';
                                }
                            }
                            else
                            {
                                // 修复：对于浮点数，使用简化的格式化方法
                                int int_part = (int)result;
                                float frac_part = result - (float)int_part;
                                if(frac_part < 0.0f) frac_part = -frac_part;
                                
                                // 格式化整数部分
                                int idx = 0;
                                if(int_part == 0 && result < 0.0f)
                                {
                                    strout[idx++] = '-';
                                }
                                int temp = (int_part < 0) ? -int_part : int_part;
                                if(temp == 0)
                                {
                                    strout[idx++] = '0';
                                }
                                else
                                {
                                    char digits[10];
                                    int digit_count = 0;
                                    while(temp > 0 && digit_count < 10)
                                    {
                                        digits[digit_count++] = '0' + (temp % 10);
                                        temp /= 10;
                                    }
                                    for(int i = digit_count - 1; i >= 0 && idx < 10; i--)
                                    {
                                        strout[idx++] = digits[i];
                                    }
                                }
                                
                                // 格式化小数部分（最多4位）
                                if(frac_part > 0.0001f && idx < 14)
                                {
                                    strout[idx++] = '.';
                                    int frac_int = (int)(frac_part * 10000.0f);
                                    if(frac_int < 0) frac_int = -frac_int;
                                    
                                    // 去除末尾的0
                                    while(frac_int > 0 && frac_int % 10 == 0 && idx < 15)
                                    {
                                        frac_int /= 10;
                                    }
                                    
                                    char frac_digits[4];
                                    int frac_count = 0;
                                    while(frac_int > 0 && frac_count < 4 && idx < 15)
                                    {
                                        frac_digits[frac_count++] = '0' + (frac_int % 10);
                                        frac_int /= 10;
                                    }
                                    for(int i = frac_count - 1; i >= 0 && idx < 15; i--)
                                    {
                                        strout[idx++] = frac_digits[i];
                                    }
                                }
                                strout[idx] = '\0';
                            }
                            
                            lv_textarea_add_text(ui_CompageTextarea,"\n");
                            lv_textarea_add_text(ui_CompageTextarea,strout);
                        }
                        else
                        {
                            // 无效数字
                            lv_textarea_add_text(ui_CompageTextarea,"erro");
                        }
                    }
                    else
                    {
                        if(ui_CompageTextarea != NULL)
                        {
                            lv_textarea_add_text(ui_CompageTextarea,"erro");
                        }
                    }
                }
            }
            strclear(&CalStr);
            NumStackClear(&NumStack);
            SymStackClear(&SymStack);
            // 修复：检查ui_CompageBtnM是否已初始化
            if(ui_CompageBtnM != NULL)
            {
                lv_obj_clear_flag(ui_CompageBtnM,LV_OBJ_FLAG_CLICKABLE);
            }
            return; // 修复：计算完成后直接返回，不执行下面的字符添加逻辑
        }

        // 其他按钮的处理
        if (txt != NULL)
        {
            if (ui_CompageTextarea != NULL)
            {
                if(lv_textarea_get_cursor_pos(ui_CompageTextarea) <= TEXT_FULL)
                {
                    lv_textarea_add_text(ui_CompageTextarea, txt); // 文本框追加字符
                    switch(btn_id)
                    {
                        case 0:
                                strput(&CalStr,'1');
                                break;
                        case 1:
                                strput(&CalStr,'2');
                                break;
                        case 2:
                                strput(&CalStr,'3');
                                break;
                        case 3:
                                strput(&CalStr,'+');
                                break;
                        case 4:
                                strput(&CalStr,'4');
                                break;
                        case 5:
                                strput(&CalStr,'5');
                                break;
                        case 6:
                                strput(&CalStr,'6');
                                break;
                        case 7:
                                strput(&CalStr,'-');
                                break;
                        case 8:
                                strput(&CalStr,'7');
                                break;
                        case 9:
                                strput(&CalStr,'8');
                                break;
                        case 10:
                                strput(&CalStr,'9');
                                break;
                        case 11:
                                strput(&CalStr,'*');
                                break;
                        case 12:
                                strput(&CalStr,'.');
                                break;
                        case 13:
                                strput(&CalStr,'0');
                                break;
                        case 15:
                                strput(&CalStr,'/');
                                break;
                        default:
                                // 未知按钮，不做处理
                                break;
                    }
                }
            }
        }
    }
}


void ui_CompageBackBtn_event_cb(lv_event_t * e)
{
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t * obj = lv_event_get_target(e);
    (void)obj; // 避免未使用变量警告
    lv_obj_t * ui_CompageTextarea = lv_event_get_user_data(e);
    if(code == LV_EVENT_CLICKED)
    {
        if (ui_CompageTextarea != NULL)
        {
            if(!strstack_isEmpty(&CalStr))
            {
                lv_textarea_delete_char(ui_CompageTextarea);
                strdel(&CalStr);
            }
            else
            {
                int i = 0;
                for (i = 0; i < (TEXT_FULL*2); i++)
                {
                    lv_textarea_delete_char(ui_CompageTextarea);
                }
                // 修复：检查ui_CompageBtnM是否已初始化
                if(ui_CompageBtnM != NULL)
                {
                    lv_obj_add_flag(ui_CompageBtnM,LV_OBJ_FLAG_CLICKABLE);
                }
            }
        }
    }
		if(code == LV_EVENT_LONG_PRESSED)
		{
			if (ui_CompageTextarea != NULL)
			{
				if(!strstack_isEmpty(&CalStr))
				{
                    strclear(&CalStr);
                    int i = 0;
                    for (i = 0; i < (TEXT_FULL*2); i++)
                    {
                        lv_textarea_delete_char(ui_CompageTextarea);
                    }
				}
			}
		}
}

///////////////////// SCREEN init ////////////////////

void ui_CalculatorPage_init(void)
{
    // 修复：确保栈被正确初始化
    CalStr.Top_Point = 0;
    memset(CalStr.strque, 0, sizeof(CalStr.strque));
    
    NumStack.Top_Point = 0;
    memset(NumStack.data, 0, sizeof(NumStack.data));
    
    SymStack.Top_Point = 0;
    memset(SymStack.data, 0, sizeof(SymStack.data));
    
    strclear(&CalStr);
    NumStackClear(&NumStack);
    SymStackClear(&SymStack);
    
    lv_obj_t * ui_CalculatorPage = lv_obj_create(NULL);
    // 修复：检查页面创建是否成功
    if(ui_CalculatorPage == NULL)
    {
        return; // 创建失败，直接返回
    }

    lv_obj_clear_flag(ui_CalculatorPage,LV_OBJ_FLAG_SCROLLABLE);
    ui_CompageBtnM = lv_btnmatrix_create(ui_CalculatorPage);
    // 修复：检查ui_CompageBtnM是否创建成功
    if(ui_CompageBtnM == NULL)
    {
        // 创建失败，清理并返回
        lv_obj_del(ui_CalculatorPage);
        return;
    }
    
    lv_btnmatrix_set_map(ui_CompageBtnM, ui_ComPageBtnmap);

    lv_obj_set_style_text_font(ui_CompageBtnM, &ui_font_heiti24, 0);
    lv_btnmatrix_set_one_checked(ui_CompageBtnM,true);
    int i = 0;
    for (i = 0; i < 16; i++)
    {
        lv_btnmatrix_set_btn_ctrl(ui_CompageBtnM, i, LV_BTNMATRIX_CTRL_NO_REPEAT); // 长按按钮时禁用重复
    }
    lv_obj_clear_flag(ui_CompageBtnM, LV_OBJ_FLAG_CLICK_FOCUSABLE);
    lv_obj_set_style_border_width(ui_CompageBtnM,0,0);
    lv_obj_set_style_bg_opa(ui_CompageBtnM,0,0);
    lv_obj_set_size(ui_CompageBtnM,240,240);
    lv_obj_set_align(ui_CompageBtnM,LV_ALIGN_LEFT_MID);


    lv_obj_t * ui_CompageTextarea = lv_textarea_create(ui_CalculatorPage);
    // 修复：检查文本区域创建是否成功
    if(ui_CompageTextarea == NULL)
    {
        lv_obj_del(ui_CompageBtnM);
        lv_obj_del(ui_CalculatorPage);
        return;
    }
    
    lv_textarea_set_one_line(ui_CompageTextarea, false); // 将文本区域配置为一行
    //lv_textarea_set_password_mode(obj_text_area, true); // 将文本区域配置为密码模式
    lv_textarea_set_max_length(ui_CompageTextarea, TEXT_FULL*2); // 设置文本区域可输入的字符长度最大值
    lv_obj_add_state(ui_CompageTextarea, LV_STATE_FOCUSED); // 显示光标
    lv_obj_set_style_radius(ui_CompageTextarea, 0, 0); // 设置样式的圆角弧度
    lv_obj_set_style_border_width(ui_CompageTextarea, 0, 0); //设置边框宽度
    lv_obj_set_style_bg_color(ui_CompageTextarea, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_CompageTextarea, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_size(ui_CompageTextarea, 100, 240); // 设置对象大小
    lv_obj_align(ui_CompageTextarea, LV_ALIGN_TOP_RIGHT, 0, 0);
    lv_obj_clear_flag(ui_CompageTextarea,LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_text_font(ui_CompageTextarea, &ui_font_heiti24, 0);
    lv_textarea_set_align(ui_CompageTextarea, LV_TEXT_ALIGN_RIGHT);

    lv_obj_t * ui_CompageBackBtn = lv_btn_create(ui_CalculatorPage);
    // 修复：检查按钮创建是否成功
    if(ui_CompageBackBtn == NULL)
    {
        lv_obj_del(ui_CompageTextarea);
        lv_obj_del(ui_CompageBtnM);
        lv_obj_del(ui_CalculatorPage);
        return;
    }
    
    lv_obj_align(ui_CompageBackBtn,LV_ALIGN_RIGHT_MID,-10,-110);
    lv_obj_set_width(ui_CompageBackBtn,50);
    lv_obj_set_height(ui_CompageBackBtn,50);
    lv_obj_set_style_radius(ui_CompageBackBtn, 25, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_CompageBackBtn, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_align(ui_CompageBackBtn, LV_ALIGN_BOTTOM_RIGHT, -16, -12);
    lv_obj_set_style_bg_color(ui_CompageBackBtn, lv_color_hex(0x808080), LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_t * btnlabel = lv_label_create(ui_CompageBackBtn);
    // 修复：检查标签创建是否成功
    if(btnlabel == NULL)
    {
        lv_obj_del(ui_CompageBackBtn);
        lv_obj_del(ui_CompageTextarea);
        lv_obj_del(ui_CompageBtnM);
        lv_obj_del(ui_CalculatorPage);
        return;
    }
    
    lv_label_set_text(btnlabel, LV_SYMBOL_BACKSPACE);
    lv_obj_set_style_text_font(btnlabel, &lv_font_montserrat_24, 0);
    lv_obj_center(btnlabel);
    
    // event
    lv_obj_add_event_cb(ui_CalculatorPage, ui_enent_Gesture, LV_EVENT_ALL, NULL);

    lv_obj_add_flag(ui_CompageBtnM, LV_OBJ_FLAG_SEND_DRAW_TASK_EVENTS);
    lv_obj_add_event_cb(ui_CompageBtnM, ui_CompageBtnM_event_cb, LV_EVENT_ALL, ui_CompageTextarea);
    lv_obj_add_event_cb(ui_CompageBackBtn, ui_CompageBackBtn_event_cb, LV_EVENT_ALL, ui_CompageTextarea);

    // load page
    lv_scr_load_anim(ui_CalculatorPage, LV_SCR_LOAD_ANIM_MOVE_RIGHT, 100, 0, true);

}

/////////////////// SCREEN deinit ////////////////////

void ui_CalculatorPage_deinit(void)
{
    
}

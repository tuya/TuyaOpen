#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include <time.h>
#include "ui_GameMemoryPage.h"

LV_FONT_DECLARE(font_puhui_18_2);

#define MATRIX_SIZE 4

typedef struct {
    uint8_t matrix[MATRIX_SIZE * MATRIX_SIZE + 1];
    const char * btnm_map[MATRIX_SIZE * MATRIX_SIZE + MATRIX_SIZE];
    uint8_t label_opa_map[MATRIX_SIZE * MATRIX_SIZE];
    uint8_t previous_id;
    uint8_t present_id;
} memory_game_t;


static char * num_list[] = {"0","1","2","3","4","5","6","7","8"};
void Game_Mem_Init(void);
void list_rand_number(uint8_t arry[], uint8_t max_count, uint8_t count);
void updata_btnm(const char * btnm_map[], uint8_t matrix[MATRIX_SIZE * MATRIX_SIZE + 1]);
uint8_t Game_Mem_Finish(void);

///////////////////// VARIABLES ////////////////////
memory_game_t Game_Mem;

lv_obj_t * ui_GameMem_Page;
lv_obj_t * ui_GameMem_BtnM;
lv_obj_t * ui_new_game_btn;
lv_obj_t * ui_game_info_label;

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

void ui_event_GameMemBtnM_handler(lv_event_t * e)
{
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t * obj = lv_event_get_target(e);
    
    if(code == LV_EVENT_VALUE_CHANGED)
    {
        uint32_t id = lv_btnmatrix_get_selected_btn(obj);
        //const char * txt = lv_btnmatrix_get_btn_text(obj, id);
        Game_Mem.present_id = (uint8_t)id;
        if(Game_Mem.present_id != Game_Mem.previous_id)
        {

            if(Game_Mem.matrix[Game_Mem.present_id] == Game_Mem.matrix[Game_Mem.previous_id])
            {

                lv_btnmatrix_set_btn_ctrl(ui_GameMem_BtnM,Game_Mem.present_id,LV_BTNMATRIX_CTRL_HIDDEN);
                lv_btnmatrix_set_btn_ctrl(ui_GameMem_BtnM,Game_Mem.previous_id,LV_BTNMATRIX_CTRL_HIDDEN);
                Game_Mem.matrix[Game_Mem.present_id] = 0;
                Game_Mem.matrix[Game_Mem.previous_id] = 0;
                if(Game_Mem_Finish())
                {
                    lv_obj_clear_flag(ui_new_game_btn,LV_OBJ_FLAG_HIDDEN);
                }
                Game_Mem.previous_id = Game_Mem.present_id;

            }
            else
            {
                if(Game_Mem.previous_id >= 0 && Game_Mem.previous_id < 17 )
                    Game_Mem.label_opa_map[Game_Mem.previous_id] = LV_OPA_0;
                Game_Mem.label_opa_map[Game_Mem.present_id] = LV_OPA_100;
                Game_Mem.previous_id = Game_Mem.present_id;
            }

        }
    }
    if(code == LV_EVENT_DRAW_TASK_ADDED)
    {
        lv_draw_task_t * draw_task = lv_event_get_draw_task(e);
        lv_draw_dsc_base_t * base_dsc = draw_task->draw_dsc;
        if(base_dsc->part == LV_PART_ITEMS)
        {
            if(base_dsc->id1 >= 0)
            {
                lv_draw_fill_dsc_t * fill_draw_dsc = lv_draw_task_get_fill_dsc(draw_task);
                if(fill_draw_dsc)
                    fill_draw_dsc->color = lv_color_hex(0xB07010);
                lv_draw_label_dsc_t * label_draw_dsc = lv_draw_task_get_label_dsc(draw_task);
                if(label_draw_dsc)
                {
                    label_draw_dsc->color = lv_color_white();
                    label_draw_dsc->opa = Game_Mem.label_opa_map[base_dsc->id1];
                }
            }
        }
    }
}


void ui_event_NewGameBtn_handler(lv_event_t * e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if(code == LV_EVENT_CLICKED)
    {
        Game_Mem_Init();
        lv_btnmatrix_set_map(ui_GameMem_BtnM, Game_Mem.btnm_map);
        lv_btnmatrix_clear_btn_ctrl_all(ui_GameMem_BtnM,LV_BTNMATRIX_CTRL_HIDDEN);
        lv_obj_add_flag(ui_new_game_btn,LV_OBJ_FLAG_HIDDEN);
    }
}

///////////////////// SCREEN init ////////////////////
void ui_GameMemoryPage_init(void)
{
    Game_Mem_Init();
    ui_GameMem_Page = lv_obj_create(NULL);
    lv_obj_remove_flag(ui_GameMem_Page, LV_OBJ_FLAG_SCROLLABLE);
    
    // 创建游戏简介标签
    ui_game_info_label = lv_label_create(ui_GameMem_Page);
    lv_label_set_text(ui_game_info_label, "记忆翻牌游戏\n点击卡片翻开，找到相同数字的配对");
    lv_obj_set_width(ui_game_info_label, 460);
    lv_obj_set_style_text_align(ui_game_info_label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_game_info_label, &font_puhui_18_2, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui_game_info_label, lv_color_hex(0x333333), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(ui_game_info_label, 5, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_align(ui_game_info_label, LV_ALIGN_TOP_MID, 0, 5);
    
    ui_GameMem_BtnM = lv_btnmatrix_create(ui_GameMem_Page);
    lv_btnmatrix_set_map(ui_GameMem_BtnM, Game_Mem.btnm_map);
    // lv_obj_set_style_text_font(ui_GameMem_BtnM, &ui_font_Cuyuan24, 0);
    lv_btnmatrix_set_one_checked(ui_GameMem_BtnM,true);
    lv_obj_set_style_border_width(ui_GameMem_BtnM,0,0);
    lv_obj_set_style_bg_opa(ui_GameMem_BtnM,0,0);
    lv_obj_set_size(ui_GameMem_BtnM,240,240);
    // 调整游戏区域位置，为简介留出空间
    lv_obj_align(ui_GameMem_BtnM, LV_ALIGN_CENTER, 0, 20);


    ui_new_game_btn = lv_btn_create(ui_GameMem_Page);
    lv_obj_align(ui_new_game_btn, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_size(ui_new_game_btn,90,45);
    lv_obj_t * new_game_btn_label = lv_label_create(ui_new_game_btn);
    lv_label_set_text(new_game_btn_label,"Replay");
    lv_obj_align(new_game_btn_label, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_text_font(new_game_btn_label, &lv_font_montserrat_24, 0);
    // lv_obj_set_style_text_font(new_game_btn_label, &ui_font_Cuyuan24, 0);
    lv_obj_add_flag(ui_new_game_btn,LV_OBJ_FLAG_HIDDEN);

    //lv_btnmatrix_set_btn_ctrl(ui_GameMem_BtnM,1,LV_BTNMATRIX_CTRL_HIDDEN);
    lv_obj_add_flag(ui_GameMem_BtnM, LV_OBJ_FLAG_SEND_DRAW_TASK_EVENTS);
    lv_obj_add_event_cb(ui_GameMem_BtnM, ui_event_GameMemBtnM_handler, LV_EVENT_ALL, NULL);
    lv_obj_add_event_cb(ui_new_game_btn, ui_event_NewGameBtn_handler, LV_EVENT_ALL, NULL);
    lv_obj_add_event_cb(ui_GameMem_Page, ui_enent_Gesture, LV_EVENT_ALL, NULL);

    // load page
    lv_scr_load_anim(ui_GameMem_Page, LV_SCR_LOAD_ANIM_MOVE_RIGHT, 100, 0, true);
}

///////////////////// SCREEN deinit ////////////////////
void ui_GameMemoryPage_deinit(void)
{}

///////////////////// FUNCTIONS ////////////////////
void Game_Mem_Init(void)
{
    uint8_t x;

    for (x = 0; x < MATRIX_SIZE * MATRIX_SIZE + 1; x++) {
            Game_Mem.matrix[x] = 0;
    }

    for (x = 0; x < MATRIX_SIZE * MATRIX_SIZE; x++) {
        Game_Mem.label_opa_map[x] = LV_OPA_0;
    }
    Game_Mem.previous_id = -1;
    Game_Mem.present_id = -1;
    list_rand_number(Game_Mem.matrix,MATRIX_SIZE * MATRIX_SIZE,MATRIX_SIZE*MATRIX_SIZE/2);
    updata_btnm(Game_Mem.btnm_map,Game_Mem.matrix);
    //Game_Mem.label_opa_map[6] = LV_OPA_100;

}

void list_rand_number(uint8_t arry[], uint8_t max_count, uint8_t count)
{
    int w, t;
	srand((unsigned int)time(NULL));

    for (int i = 0; i < max_count; i++)
        arry[i] = (i % count) + 1;
    for (int i = 0; i < max_count; i++)
    {
        w = rand() % (count - (i % count)) + i;
        if (w > max_count)  w = max_count - 1;

        t = arry[i];
        arry[i] = arry[w];
        arry[w] = t;
    }
}

void updata_btnm(const char * btnm_map[], uint8_t matrix[MATRIX_SIZE * MATRIX_SIZE + 1])
{
    uint8_t x,index;
    index = 0;
    for(x=0;x<MATRIX_SIZE * MATRIX_SIZE;x++)
    {
        if((index + 1) % (MATRIX_SIZE + 1) == 0)
        {
            btnm_map[index] = "\n";
            index++;
        }
        btnm_map[index] = num_list[matrix[x]];
        index++;
    }
    btnm_map[MATRIX_SIZE * MATRIX_SIZE + MATRIX_SIZE - 1] = "";
}

uint8_t Game_Mem_Finish(void)
{
    for(uint16_t x = 0; x < MATRIX_SIZE*MATRIX_SIZE; x++)
    {
        if(Game_Mem.matrix[x]!=0)
            return 0;
    }
    return 1;
}


#include "lvgl/lvgl.h"
#include "gui_common.h"
#include "tuya_ai_display.h"

#define SCREEN_WIDTH  LCD_WIDTH
#define SCREEN_HEIGHT LCD_HEIGHT

/* 样式定义 */
static lv_style_t style_avatar;
static lv_style_t style_ai_bubble;
static lv_style_t style_user_bubble;
static lv_style_t style_time;

lv_obj_t* msg_container;
lv_obj_t* title;

static  lv_obj_t    *status_bar_ ;
static  lv_obj_t    *network_label_;
static  lv_obj_t    *status_label_;
static  lv_obj_t    *mode_label_;


/* 计算动态气泡宽度 */
static inline uint32_t calc_bubble_width() {
    return SCREEN_WIDTH - 85; // 调整后更精确的宽度计算
}
LV_FONT_DECLARE(puhui_3bp_18);
LV_FONT_DECLARE(FONT_SY_20);
LV_IMG_DECLARE(ai);
LV_IMG_DECLARE(user);
LV_FONT_DECLARE(font_awesome_20_4);

#define AI_MESSAGE_FONT    &puhui_3bp_18

static void SetStatus(uint8_t stat);

/* 创建消息元素 */
void create_message(const char* text, bool is_ai) {
    // 主消息容器
    lv_obj_t* msg_cont = lv_obj_create(msg_container);
    lv_obj_remove_style_all(msg_cont);
    lv_obj_set_size(msg_cont, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_style_pad_ver(msg_cont, 6, 0);
    lv_obj_set_flex_flow(msg_cont, is_ai ? LV_FLEX_FLOW_ROW : LV_FLEX_FLOW_ROW_REVERSE);
    lv_obj_set_style_pad_column(msg_cont, 10, 0);

    /*---- 头像 ----*/
    lv_obj_t* icon = lv_img_create(msg_cont);
    lv_obj_set_size(icon, 40, 40);
    lv_img_set_src(icon, is_ai ? &ai : &user);
    lv_obj_center(icon);

    /*---- 消息气泡 ----*/
    lv_obj_t* bubble = lv_obj_create(msg_cont);
    lv_obj_set_width(bubble, calc_bubble_width());
    lv_obj_set_height(bubble, LV_SIZE_CONTENT);
    lv_obj_add_style(bubble, is_ai ? &style_ai_bubble : &style_user_bubble, 0);
    
    // 禁用所有滚动条
    lv_obj_set_scrollbar_mode(bubble, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_scroll_dir(bubble, LV_DIR_NONE);

    /*---- 消息内容 ----*/
    lv_obj_t* text_cont = lv_obj_create(bubble);
    lv_obj_remove_style_all(text_cont);
    lv_obj_set_size(text_cont, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(text_cont, LV_FLEX_FLOW_COLUMN);

    // 消息文本
    lv_obj_t* label = lv_label_create(text_cont);
    lv_label_set_text(label, text);
    lv_obj_set_width(label, calc_bubble_width() - 24);
    lv_label_set_long_mode(label, LV_LABEL_LONG_WRAP);

    // 确保滚动到底部（使用动画更流畅）
    lv_obj_scroll_to_view(msg_cont, LV_ANIM_ON);
    lv_obj_update_layout(msg_container);
}


/* 创建消息元素 */
lv_obj_t* create_dyanmic_message() {
    // 主消息容器
    lv_obj_t* msg_cont = lv_obj_create(msg_container);
    lv_obj_remove_style_all(msg_cont);
    lv_obj_set_size(msg_cont, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_style_pad_ver(msg_cont, 6, 0);
    // lv_obj_set_flex_flow(msg_cont, is_ai ? LV_FLEX_FLOW_ROW : LV_FLEX_FLOW_ROW_REVERSE);
    lv_obj_set_flex_flow(msg_cont, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(msg_cont, 10, 0);

    /*---- 头像 ----*/
    lv_obj_t* icon = lv_img_create(msg_cont);
    lv_obj_set_size(icon, 40, 40);
    lv_img_set_src(icon, &ai);
    lv_obj_center(icon);

    /*---- 消息气泡 ----*/
    lv_obj_t* bubble = lv_obj_create(msg_cont);
    lv_obj_set_width(bubble, calc_bubble_width());
    lv_obj_set_height(bubble, LV_SIZE_CONTENT);
    lv_obj_add_style(bubble, &style_ai_bubble, 0);
    
    // 禁用所有滚动条
    lv_obj_set_scrollbar_mode(bubble, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_scroll_dir(bubble, LV_DIR_NONE);

    /*---- 消息内容 ----*/
    lv_obj_t* text_cont = lv_obj_create(bubble);
    lv_obj_remove_style_all(text_cont);
    lv_obj_set_size(text_cont, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(text_cont, LV_FLEX_FLOW_COLUMN);

    // 消息文本
    lv_obj_t* label = lv_label_create(text_cont);
    // lv_label_set_text(label, text);
    // lv_obj_set_width(label, calc_bubble_width() - 24);
    // lv_label_set_long_mode(label, LV_LABEL_LONG_WRAP);

    // // 确保滚动到底部（使用动画更流畅）
    // lv_obj_scroll_to_view(msg_cont, LV_ANIM_ON);
    // lv_obj_update_layout(msg_container);
    // bk_printf("new container and lable 0x%x", label);
    return label;
}

void send_dynamic_message(lv_obj_t *label, const char* text)
{
    // bk_printf("update %s to lable 0x%x", text,  label);
    lv_label_set_text(label, text);
    lv_obj_set_width(label, calc_bubble_width() - 24);
    lv_label_set_long_mode(label, LV_LABEL_LONG_WRAP);

    // 确保滚动到底部（使用动画更流畅）
    lv_obj_t* text_cont = lv_obj_get_parent(label); // 获取label的父对象，即text_cont
    lv_obj_t* bubble = lv_obj_get_parent(text_cont); // 获取text_cont的父对象，即bubble
    lv_obj_t* msg_cont = lv_obj_get_parent(bubble); // 获取bubble的父对象，即msg_cont
    lv_obj_scroll_to_view(msg_cont, LV_ANIM_ON);
    lv_obj_update_layout(msg_container);    
}

/* 初始化样式 */
void init_styles(void) {
    // 头像样式
    lv_style_init(&style_avatar);
    lv_style_set_radius(&style_avatar, LV_RADIUS_CIRCLE);
    lv_style_set_bg_color(&style_avatar, lv_palette_main(LV_PALETTE_GREY));
    lv_style_set_border_width(&style_avatar, 1);
    lv_style_set_border_color(&style_avatar, lv_palette_darken(LV_PALETTE_GREY, 2));

    // AI气泡样式
    lv_style_init(&style_ai_bubble);
    lv_style_set_bg_color(&style_ai_bubble, lv_color_white());
    lv_style_set_radius(&style_ai_bubble, 15);
    lv_style_set_pad_all(&style_ai_bubble, 12);
    lv_style_set_shadow_width(&style_ai_bubble, 12);
    lv_style_set_shadow_color(&style_ai_bubble, lv_color_hex(0xCCCCCC));

    // 用户气泡样式
    lv_style_init(&style_user_bubble);
    lv_style_set_bg_color(&style_user_bubble, lv_palette_main(LV_PALETTE_GREEN));
    lv_style_set_text_color(&style_user_bubble, lv_color_white());
    lv_style_set_radius(&style_user_bubble, 15);
    lv_style_set_pad_all(&style_user_bubble, 12);
    lv_style_set_shadow_width(&style_user_bubble, 12);
    lv_style_set_shadow_color(&style_user_bubble, lv_palette_darken(LV_PALETTE_GREEN, 2));
}


void create_ai_chat_ui(void) {
    init_styles();

    // 主容器
    lv_obj_t* main_cont = lv_obj_create(lv_scr_act());
    lv_obj_set_size(main_cont, SCREEN_WIDTH, SCREEN_HEIGHT);
    lv_obj_set_style_bg_color(main_cont, lv_color_hex(0xF0F0F0), 0);
    lv_obj_set_style_pad_all(main_cont, 0, 0);
    // lv_obj_set_flex_flow(main_cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_text_font(main_cont, AI_MESSAGE_FONT, 0);
    lv_obj_set_style_text_color(main_cont, lv_color_black(), 0);
    lv_obj_set_scrollbar_mode(main_cont, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_scroll_dir(main_cont, LV_DIR_NONE);

    // 标题栏
    status_bar_ = lv_obj_create(main_cont);
    lv_obj_set_size(status_bar_, LV_HOR_RES, 40);
    lv_obj_set_flex_flow(status_bar_, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_all(status_bar_, 0, 0);
    lv_obj_set_style_border_width(status_bar_, 0, 0);
    lv_obj_set_style_pad_column(status_bar_, 0, 0);
    lv_obj_set_style_pad_left(status_bar_, 5, 0);
    lv_obj_set_style_pad_right(status_bar_, 5, 0);
    lv_obj_set_flex_align( status_bar_, LV_FLEX_ALIGN_CENTER,  LV_FLEX_ALIGN_CENTER,  LV_FLEX_ALIGN_CENTER);

    mode_label_ = lv_label_create(status_bar_);
    lv_obj_set_style_text_align(mode_label_, LV_TEXT_ALIGN_LEFT, 0);
    lv_label_set_text(mode_label_, gui_mode_desc_get(0));

    // status = 0;
    status_label_ = lv_label_create(status_bar_);
    lv_obj_set_flex_grow(status_label_, 1);
    lv_label_set_long_mode(status_label_, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_obj_set_style_text_align(status_label_, LV_TEXT_ALIGN_CENTER, 0);
    SetStatus(GUI_STAT_INIT);

    network_label_ = lv_label_create(status_bar_);
    lv_obj_set_style_text_font(network_label_, &font_awesome_20_4, 0);
    lv_label_set_text(network_label_, FONT_AWESOME_WIFI_OFF);


    // 消息容器（关键修改）
    msg_container = lv_obj_create(main_cont);
    lv_obj_set_size(msg_container, SCREEN_WIDTH, SCREEN_HEIGHT - 40); // 精确高度计算
    lv_obj_set_flex_flow(msg_container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_border_width(msg_container, 0, 0);
    lv_obj_set_style_pad_ver(msg_container, 8, 0);
    lv_obj_set_style_pad_hor(msg_container, 10, 0);
    lv_obj_set_y(msg_container, 40);

    lv_obj_move_background(msg_container);

    // 禁用横向滚动
    lv_obj_set_scroll_dir(msg_container, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(msg_container, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_bg_opa(msg_container, LV_OPA_TRANSP, 0);
}

void tuya_wechat_init(void)
{
    create_ai_chat_ui();
    create_message("hi, 你好啊，我是小康", true);
}

static void SetStatus(uint8_t stat) 
{
    if (status_label_ == NULL) {
        return;
    }

    char *text;

    if (OPRT_OK != gui_status_desc_get(stat, &text, NULL)) {
        return;
    }

    lv_label_set_text(status_label_, text);
}


static const char *power_txet  = "你好啊，我来了，让我们一起玩耍吧";
static const char *netok_txet  = "我已联网，让我们开始对话吧";
static const char *netcfg_txet = "我已进入配网状态, 你能帮我用涂鸦智能app配网嘛";

#define MAX_LEN_DISPLAY_ONE_TIME 4096
void tuya_wechat_app(TY_DISPLAY_MSG_T *msg)
{
    static lv_obj_t *label = NULL;
    static BOOL_T ai_speaking = FALSE;
    static char  ai_text_window[MAX_LEN_DISPLAY_ONE_TIME + 1];
    static int   ai_text_window_len = 0;

    switch (msg->type) {

    case TY_DISPLAY_TP_LANGUAGE:
        gui_lang_set(msg->data[0]);
        SetStatus(GUI_STAT_INIT);
        lv_label_set_text(mode_label_, gui_mode_desc_get(0));
        break;

    case TY_DISPLAY_TP_HUMAN_CHAT:
        create_message(msg->data, FALSE);
        ai_speaking = FALSE;
        break;

    case TY_DISPLAY_TP_AI_CHAT:
        if (!ai_speaking) {
            label = create_dyanmic_message();
            ai_speaking = TRUE;
            ai_text_window_len = 0;
            memset(ai_text_window, 0, MAX_LEN_DISPLAY_ONE_TIME);
        }
        if (ai_text_window_len < MAX_LEN_DISPLAY_ONE_TIME) {
            memcpy(ai_text_window + ai_text_window_len, msg->data, 
                (ai_text_window_len+msg->len > MAX_LEN_DISPLAY_ONE_TIME) ? (MAX_LEN_DISPLAY_ONE_TIME - ai_text_window_len):msg->len);
            ai_text_window_len += msg->len;
        }
        send_dynamic_message(label, ai_text_window);
        break;


    case TY_DISPLAY_TP_STAT_POWERON:
        create_message(power_txet, TRUE);
        break;

    case TY_DISPLAY_TP_STAT_ONLINE:
        SetStatus(GUI_STAT_IDLE);
        create_message(netok_txet, TRUE);
        break;

    case TY_DISPLAY_TP_CHAT_STAT:
        SetStatus(msg->data[0]);
        break;

    case TY_DISPLAY_TP_STAT_SLEEP:
        SetStatus(GUI_STAT_IDLE);
        break;

    case TY_DISPLAY_TP_STAT_NET:
        lv_label_set_text(network_label_, gui_wifi_level_get(msg->data[0]));
        break;

    case TY_DISPLAY_TP_CHAT_MODE: 
        lv_label_set_text(mode_label_, gui_mode_desc_get(msg->data[0])); 
        SetStatus(GUI_STAT_IDLE);
        break;

    case TY_DISPLAY_TP_STAT_NETCFG:
        SetStatus(GUI_STAT_PROV);
        create_message(netcfg_txet, TRUE);
        break;

    }

}

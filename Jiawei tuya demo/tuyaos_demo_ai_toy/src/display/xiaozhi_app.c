#include "gui_common.h"
#include "tuya_ai_display.h"

LV_FONT_DECLARE(FONT_SY_20);
LV_FONT_DECLARE(puhui_3bp_18);
LV_FONT_DECLARE(font_awesome_16_4);
typedef struct  {
    const lv_font_t *text_font;
    const lv_font_t *icon_font;
    const lv_font_t *emoji_font;
} DisplayFonts;

static DisplayFonts  fonts_;
static  lv_obj_t    *status_bar_ ;
static  lv_obj_t    *content_;
static  lv_obj_t    *container_;
static  lv_obj_t    *emotion_label_;
static  lv_obj_t    *battery_label_;
static  lv_obj_t    *chat_message_label_;
static  lv_obj_t    *network_label_;
static  lv_obj_t    *mode_label_;
static  lv_obj_t    *status_label_;
static  lv_obj_t    *vol_label_;

typedef struct  {
    const lv_font_t *text_font;
    const lv_font_t *icon_font;
    const lv_font_t *emoji_font;
} DisplayFonts_t;

const lv_font_t* font_emoji_64_init(void);
void SetEmotion(const char* emotion);
void SetStatus(uint8_t stat);

void tuya_xiaozhi_init(void)
{
    fonts_.text_font = &puhui_3bp_18; 
    fonts_.icon_font = &font_awesome_16_4; 
    fonts_.emoji_font = font_emoji_64_init(); 

    lv_obj_t *screen = lv_scr_act();
    lv_obj_set_style_text_font(screen, fonts_.text_font, 0);
    lv_obj_set_style_text_color(screen, lv_color_black(), 0);

    /* Container */
    container_ = lv_obj_create(screen);
    lv_obj_set_size(container_, LV_HOR_RES, LV_VER_RES);
    lv_obj_set_flex_flow(container_, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(container_, 0, 0);
    lv_obj_set_style_border_width(container_, 0, 0);
    lv_obj_set_style_pad_row(container_, 0, 0);

    /* Status bar */
    status_bar_ = lv_obj_create(container_);
    lv_obj_set_size(status_bar_, LV_HOR_RES, fonts_.text_font->line_height);
    lv_obj_set_style_radius(status_bar_, 0, 0);
    
    /* Content */
    content_ = lv_obj_create(container_);
    lv_obj_set_scrollbar_mode(content_, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_radius(content_, 0, 0);
    lv_obj_set_width(content_, LV_HOR_RES);
    lv_obj_set_flex_grow(content_, 1);


    lv_obj_set_flex_flow(content_, LV_FLEX_FLOW_COLUMN); // 垂直布局（从上到下）
    lv_obj_set_flex_align(content_, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_SPACE_EVENLY); // 子对象居中对齐，等距分布

    emotion_label_ = lv_label_create(content_);
    lv_obj_set_style_text_font(emotion_label_, fonts_.emoji_font, 0);
    lv_label_set_text(emotion_label_, "😶");

    chat_message_label_ = lv_label_create(content_);
    lv_label_set_text(chat_message_label_, "");
    lv_obj_set_width(chat_message_label_, LV_HOR_RES * 0.9); // 限制宽度为屏幕宽度的 90%
    lv_label_set_long_mode(chat_message_label_, LV_LABEL_LONG_WRAP); // 设置为自动换行模式
    lv_obj_set_style_text_align(chat_message_label_, LV_TEXT_ALIGN_CENTER, 0); // 设置文本居中对齐

    /* Status bar */
    lv_obj_set_flex_flow(status_bar_, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_all(status_bar_, 0, 0);
    lv_obj_set_style_border_width(status_bar_, 0, 0);
    lv_obj_set_style_pad_column(status_bar_, 0, 0);
    lv_obj_set_style_pad_left(status_bar_, 2, 0);
    lv_obj_set_style_pad_right(status_bar_, 2, 0);

    mode_label_ = lv_label_create(status_bar_);
    lv_obj_set_style_text_align(mode_label_, LV_TEXT_ALIGN_LEFT, 0);

    status_label_ = lv_label_create(status_bar_);
    lv_obj_set_flex_grow(status_label_, 1);
    lv_label_set_long_mode(status_label_, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_obj_set_style_text_align(status_label_, LV_TEXT_ALIGN_CENTER, 0);

    network_label_ = lv_label_create(status_bar_);
    lv_obj_set_style_text_font(network_label_, fonts_.icon_font, 0);
    lv_label_set_text(network_label_, FONT_AWESOME_WIFI_OFF);
    
    battery_label_ = lv_label_create(status_bar_);
    lv_label_set_text(battery_label_, FONT_AWESOME_BATTERY_FULL);
    lv_obj_set_style_text_font(battery_label_, fonts_.icon_font, 0);

    vol_label_ = lv_label_create(status_bar_);
    lv_obj_set_style_text_font(vol_label_, fonts_.icon_font, 0);
    lv_label_set_text(vol_label_, FONT_AWESOME_VOLUME_MEDIUM);

    SetStatus(GUI_STAT_INIT);
    SetEmotion("neutral");
    lv_label_set_text(mode_label_, gui_mode_desc_get(0));
}


void SetEmotion(const char* emotion) 
{
    static const gui_emotion_t emotions[] = {
        {"😶", "neutral"},
        {"🙂", "happy"},
        {"😆", "laughing"},
        {"😂", "funny"},
        {"😔", "sad"},
        {"😠", "angry"},
        {"😭", "crying"},
        {"😍", "loving"},
        {"😳", "embarrassed"},
        {"😯", "surprise"},
        {"😱", "shocked"},
        {"🤔", "thinking"},
        {"😉", "winking"},
        {"😎", "cool"},
        {"😌", "relaxed"},
        {"🤤", "delicious"},
        {"😘", "kissy"},
        {"😏", "confident"},
        {"😴", "sleepy"},
        {"😜", "silly"},
        {"🙄", "confused"}
    };

    uint8_t index = 0;
    index = gui_emotion_find(emotions, CNTSOF(emotions), emotion);
    lv_label_set_text(emotion_label_, emotions[index].source);
}

void SetChatMessage(const char* role, const char* content) {
    if (chat_message_label_ == NULL) {
        return;
    }
    lv_label_set_text(chat_message_label_, content);
}

void SetStatus(uint8_t stat) 
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


#define MAX_LEN_DISPLAY_ONE_TIME 200
void SetDynamicMessage(int is_ai, const char *data, int len) 
{
    if (chat_message_label_ == NULL) {
        return;
    }

    static char  ai_text_window[MAX_LEN_DISPLAY_ONE_TIME + 1];
    static int   ai_text_window_len = 0;
    static int   cur_is_ai = 0;
    
    if (cur_is_ai != is_ai) {
        memset(ai_text_window, 0, MAX_LEN_DISPLAY_ONE_TIME+  1);
        ai_text_window_len = 0;
        cur_is_ai = is_ai;
    }

    int tmp_len = len;
    while (tmp_len > 0) {
        // reset window
        if (ai_text_window_len >= MAX_LEN_DISPLAY_ONE_TIME) {
            memset(ai_text_window, 0, MAX_LEN_DISPLAY_ONE_TIME+  1);
            ai_text_window_len = 0;
        }

        int cp_len = (tmp_len > (MAX_LEN_DISPLAY_ONE_TIME - ai_text_window_len))? (MAX_LEN_DISPLAY_ONE_TIME-ai_text_window_len):tmp_len;
        memcpy(ai_text_window+ai_text_window_len, data+len-tmp_len, cp_len);
        ai_text_window_len += cp_len;
        tmp_len -= cp_len;
        // bk_printf("%s\n", ai_text_window);

        lv_label_set_text(chat_message_label_, ai_text_window);
    }
}

void tuya_xiaozhi_app(TY_DISPLAY_MSG_T *msg)
{   
    switch (msg->type) {

    case TY_DISPLAY_TP_LANGUAGE:
        gui_lang_set(msg->data[0]);
        SetStatus(GUI_STAT_INIT);
        lv_label_set_text(mode_label_, gui_mode_desc_get(0));
        break;

    case TY_DISPLAY_TP_STAT_CHARGING:
        lv_label_set_text(battery_label_, FONT_AWESOME_BATTERY_CHARGING); 
        break;

    case TY_DISPLAY_TP_STAT_BATTERY:
        lv_label_set_text(battery_label_, gui_battery_level_get(msg->data[0]));
        break;

    case TY_DISPLAY_TP_STAT_NET:
        lv_label_set_text(network_label_, gui_wifi_level_get(msg->data[0]));
        break;

    case TY_DISPLAY_TP_CHAT_MODE: 
        lv_label_set_text(mode_label_, gui_mode_desc_get(msg->data[0])); 
        SetStatus(GUI_STAT_IDLE);
        break;

    case TY_DISPLAY_TP_AI_CHAT:
        SetDynamicMessage(1, msg->data, msg->len);
        break;

    case TY_DISPLAY_TP_HUMAN_CHAT:
        SetDynamicMessage(0, msg->data, msg->len);
        break;

    case TY_DISPLAY_TP_EMOJI:
    case TY_DISPLAY_TP_ASR_EMOJI:
        SetEmotion(msg->data);
        break;

    case TY_DISPLAY_TP_CHAT_STAT: {
        if (GUI_STAT_IDLE == msg->data[0]) {
            SetEmotion("neutral");
            SetChatMessage(NULL, "");
        } else if (GUI_STAT_LISTEN == msg->data[0]) {
            SetEmotion("neutral");
            SetChatMessage(NULL, "");
        }
        SetStatus(msg->data[0]);

    } break;

    case TY_DISPLAY_TP_STAT_POWERON:
    case TY_DISPLAY_TP_STAT_ONLINE:
    case TY_DISPLAY_TP_STAT_SLEEP:
        SetStatus(GUI_STAT_IDLE);
        SetEmotion("neutral");
        SetChatMessage(NULL, "");
        break;

    case TY_DISPLAY_TP_STAT_NETCFG:
        SetStatus(GUI_STAT_PROV);
        break;

    case TY_DISPLAY_TP_VOLUME: 
        lv_label_set_text(vol_label_, gui_volum_level_get(msg->data[0]));
        break;
    }
}

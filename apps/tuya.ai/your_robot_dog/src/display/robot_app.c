#include "tuya_ai_display.h"
#include "gui_common.h"
#include "lvgl.h"
#include "tal_log.h"

#include <string.h>
#include <strings.h>

LV_IMG_DECLARE(neutral);
LV_IMG_DECLARE(annoyed);
LV_IMG_DECLARE(cool);
LV_IMG_DECLARE(delicious);
LV_IMG_DECLARE(fearful);
LV_IMG_DECLARE(lovestruck);
LV_IMG_DECLARE(unamused);
LV_IMG_DECLARE(winking);
LV_IMG_DECLARE(zany);

LV_FONT_DECLARE(font_puhui_18_2);
LV_FONT_DECLARE(font_awesome_16_4);

static lv_obj_t *gif_full;
static lv_obj_t *gif_stat;
static int current_gif_index = -1;
static bool gif_load_init = false;
static int s_current_gui_stat = GUI_STAT_INIT;

/* Emoji vertical offset: positive moves down, negative moves up */
#define EMOJI_Y_OFFSET -28
/* Container vertical offset: positive moves down, negative moves up */
#define CONTAINER_Y_OFFSET 32

//! gif file
static lv_img_dsc_t gif_files[11]; 

#define GIF_EMOTION_FILE_INDEX       9

static const gui_emotion_t s_gif_emotion[] = {
    {&neutral,          "neutral"},
    {&annoyed,          "annoyed"},
    {&cool,             "cool"},
    {&delicious,        "delicious"},
    {&fearful,          "fearful"},
    {&lovestruck,       "lovestruck"},
    {&unamused,         "unamused"},
    {&winking,          "winking"},
    {&zany,             "zany"},
    /*----------------------------------- */
    {&gif_files[0],     "angry"},
    {&gif_files[1],     "confused" },
    {&gif_files[2],     "disappointed"},
    {&gif_files[3],     "embarrassed"},
    {&gif_files[4],     "happy"},
    {&gif_files[5],     "laughing"},
    {&gif_files[6],     "relaxed"},
    {&gif_files[7],     "sad"},
    {&gif_files[8],     "surprise"},
    {&gif_files[9],     "thinking"},
};

void robot_gif_load(void)
{
    int i = 0;

    if (gif_load_init) {
        return;
    }
    
    char *gif_file_path[] = {
        "/angry.gif",
        "/confused.gif",
        "/disappointed.gif",
        "/embarrassed.gif",
        "/happy.gif",
        "/laughing.gif",
        "/relaxed.gif",
        "/sad.gif",
        "/surprise.gif",
        "/thinking.gif",
    };

    for (i = 0; i < sizeof(gif_file_path) / sizeof(gif_file_path[0]); i++) {
        gui_img_load_psram(gif_file_path[i], &gif_files[i]);
    }

    gif_load_init = true;
}

void robot_emotion_flush(char  *emotion)
{
    uint8_t index = 0;

    index = gui_emotion_find((gui_emotion_t *)s_gif_emotion, CNTSOF(s_gif_emotion), emotion);
        
    if (index >= GIF_EMOTION_FILE_INDEX && !gif_load_init) {
        return;
    }

    if (current_gif_index == index) {
        return;
    }

    current_gif_index = index;

    lv_gif_set_src(gif_full, s_gif_emotion[index].source); 
}

static  lv_obj_t    *status_bar_ ;
static  lv_obj_t    *battery_label_;
static  lv_obj_t    *network_label_;
static  lv_obj_t    *status_label_;

void robot_status_bar_init(lv_obj_t *container)
{
    /* Status bar */
    status_bar_ = lv_obj_create(container);
    lv_obj_set_size(status_bar_, LV_HOR_RES, font_puhui_18_2.line_height);
    lv_obj_set_style_text_font(status_bar_, &font_puhui_18_2, 0);
    lv_obj_set_style_bg_color(status_bar_, lv_color_black(), 0);
    lv_obj_set_style_text_color(status_bar_, lv_color_white(), 0);
    lv_obj_set_style_radius(status_bar_, 0, 0);

    lv_obj_set_style_pad_all(status_bar_, 0, 0);
    lv_obj_set_style_border_width(status_bar_, 0, 0);

    network_label_ = lv_label_create(status_bar_);
    lv_obj_set_style_text_font(network_label_, &font_awesome_16_4, 0);
    lv_label_set_text(network_label_, FONT_AWESOME_WIFI_OFF);
    lv_obj_align(network_label_, LV_ALIGN_LEFT_MID, 10, 0);
    PR_DEBUG("[robot_display] network_label_ created, init=%s", FONT_AWESOME_WIFI_OFF);
    PR_DEBUG("[robot_display] network_label_ pos=(%d,%d) size=(%d,%d) hidden=%d parent_hidden=%d",
             lv_obj_get_x(network_label_), lv_obj_get_y(network_label_),
             lv_obj_get_width(network_label_), lv_obj_get_height(network_label_),
             lv_obj_has_flag(network_label_, LV_OBJ_FLAG_HIDDEN),
             lv_obj_has_flag(status_bar_, LV_OBJ_FLAG_HIDDEN));

    gif_stat = lv_gif_create(status_bar_);
    lv_obj_set_height(gif_stat, font_puhui_18_2.line_height);

    status_label_ = lv_label_create(status_bar_);
    lv_obj_set_height(status_bar_, font_puhui_18_2.line_height);
    lv_label_set_long_mode(status_label_, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_obj_set_style_text_align(status_label_, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_center(status_label_);

    battery_label_ = lv_label_create(status_bar_);
    lv_label_set_text(battery_label_, FONT_AWESOME_BATTERY_FULL);
    lv_obj_set_style_text_font(battery_label_, &font_awesome_16_4, 0);
    lv_obj_align(battery_label_, LV_ALIGN_RIGHT_MID, -10, 0);
    PR_DEBUG("[robot_display] battery_label_ created, init=%s", FONT_AWESOME_BATTERY_FULL);
    PR_DEBUG("[robot_display] battery_label_ pos=(%d,%d) size=(%d,%d) hidden=%d parent_hidden=%d",
             lv_obj_get_x(battery_label_), lv_obj_get_y(battery_label_),
             lv_obj_get_width(battery_label_), lv_obj_get_height(battery_label_),
             lv_obj_has_flag(battery_label_, LV_OBJ_FLAG_HIDDEN),
             lv_obj_has_flag(status_bar_, LV_OBJ_FLAG_HIDDEN));
}

void robot_set_status(int stat)
{
    const char          *text;
    const lv_img_dsc_t  *gif;

    if (OPRT_OK != gui_status_desc_get(stat, &text, &gif)) {
        return;
    }
    
    lv_label_set_text(status_label_, text);
    lv_obj_align_to(gif_stat, status_label_, LV_ALIGN_OUT_LEFT_MID, -5, -1);
    lv_gif_set_src(gif_stat, gif); 

    s_current_gui_stat = stat;
}

void tuya_robot_init(void)
{
    /* Container */
    lv_obj_t * container_ = lv_obj_create(lv_scr_act());
    lv_obj_set_size(container_, LV_HOR_RES, LV_VER_RES);
    lv_obj_set_style_pad_all(container_, 0, 0);
    lv_obj_set_style_border_width(container_, 0, 0);
    lv_obj_set_scrollbar_mode(container_, LV_SCROLLBAR_MODE_OFF);
    /* Move the container down by the specified pixels */
    lv_obj_set_y(container_, CONTAINER_Y_OFFSET);

    robot_status_bar_init(container_);

    gif_full = lv_gif_create(container_);
    lv_obj_set_size(gif_full, LV_HOR_RES, LV_VER_RES);
    /* Set vertical offset for the full-screen emotion GIF */
    lv_obj_set_y(gif_full, EMOJI_Y_OFFSET);

    robot_set_status(GUI_STAT_INIT);
    robot_emotion_flush("neutral");
    lv_obj_move_background(gif_full);
}

/* UVC camera stream controls disabled to avoid link errors */
/* void bk_uvc_camera_stream_start(void); */
/* void bk_uvc_camera_stream_stop(void); */

void tuya_robot_app(TY_DISPLAY_MSG_T *msg)
{
    /* Debug print of incoming message: type, len, and a safe data preview */
    const char *data_str = "<null>";
    char preview[64] = {0};
    if (msg && msg->data) {
        int n = msg->len;
        if (n > (int)(sizeof(preview) - 1)) {
            n = (int)(sizeof(preview) - 1);
        }
        if (n > 0) {
            memcpy(preview, msg->data, (size_t)n);
            preview[n] = '\0';
            data_str = preview;
        } else {
            data_str = "";
        }
    }
    PR_DEBUG("tuya_robot_app: type=%d len=%d data_preview=\"%s\"",
             msg ? msg->type : -1, msg ? msg->len : 0, data_str);
    if (!msg) {
        return;
    }
    switch (msg->type)
    {
    case TY_DISPLAY_TP_LANGUAGE:
        gui_lang_set(((uint8_t *)msg->data)[0]);
        robot_set_status(GUI_STAT_INIT);
        break;
    case TY_DISPLAY_TP_EMOJI:{
        robot_emotion_flush(msg->data);
         robot_set_status(GUI_STAT_THINK);   
    }
        break;
    case TY_DISPLAY_TP_STAT_CHARGING:
        lv_label_set_text(battery_label_, FONT_AWESOME_BATTERY_CHARGING); 
        break;
    case TY_DISPLAY_TP_STAT_BATTERY: 
        lv_label_set_text(battery_label_, gui_battery_level_get(((uint8_t *)msg->data)[0]));
        break;
    case TY_DISPLAY_TP_STAT_NETCFG:
        robot_set_status(GUI_STAT_PROV);
        break;
    case TY_DISPLAY_TP_CHAT_STAT: {
        uint8_t stat = 0xFF;
        /* Support two payload formats: 1-byte enum or text (multibyte, Chinese/English) */
        if (msg->len == 1) {
            stat = ((uint8_t *)msg->data)[0];
        } else {
            const char *s = (const char *)msg->data;
            /* Simple text-to-status mapping (Chinese/English keywords) */
            if (s) {
                if (strstr(s, "聆听") || strstr(s, "Listening")) {
                    stat = GUI_STAT_LISTEN;
                } else if (strstr(s, "上传") || strstr(s, "Uploading")) {
                    stat = GUI_STAT_UPLOAD;
                } else if (strstr(s, "思考") || strstr(s, "Thinking")) {
                    stat = GUI_STAT_THINK;
                } else if (strstr(s, "说话") || strstr(s, "Speaking")) {
                    stat = GUI_STAT_SPEAK;
                } else if (strstr(s, "待命") || strstr(s, "Standby")) {
                    stat = GUI_STAT_IDLE;
                } else if (strstr(s, "连接") || strstr(s, "Connecting")) {
                    stat = GUI_STAT_CONN;
                } else if (strstr(s, "初始化") || strstr(s, "Initializing")) {
                    stat = GUI_STAT_INIT;
                } else if (strstr(s, "配网") || strstr(s, "Provisioning")) {
                    stat = GUI_STAT_PROV;
                } else {
                    /* Default to idle when no match */
                    stat = GUI_STAT_IDLE;
                }
            } else {
                stat = GUI_STAT_IDLE;
            }
        }
        if (GUI_STAT_IDLE == stat) {
            robot_emotion_flush("neutral");
        } else if (GUI_STAT_LISTEN == stat) {
            robot_emotion_flush("neutral");
        } else if (GUI_STAT_UPLOAD == stat) {
            robot_emotion_flush("thinking");
        }
        if (s_current_gui_stat == GUI_STAT_PROV && stat != GUI_STAT_PROV) {
            break;
        }
        robot_set_status(stat);
    } break;
    case TY_DISPLAY_TP_STAT_SPEAK:
        /* No action needed; speak status handled in CHAT_STAT */
        robot_set_status(GUI_STAT_SPEAK);
        break;
    case TY_DISPLAY_TP_STAT_NET:
        if (((uint8_t *)msg->data)[0]) {
            robot_gif_load();
            robot_set_status(GUI_STAT_IDLE);
        }
        lv_label_set_text(network_label_, gui_wifi_level_get(((uint8_t *)msg->data)[0]));
        break;
    default:
        break;
    }
}


#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "ai_ui_manage.h"
#include "gui_common.h"

#include "lang_config.h"
#include "lv_vendor.h"
#include "lvgl.h"
#include "tal_api.h"
#include "tal_log.h"

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

static lv_obj_t *status_bar_;
static lv_obj_t *battery_label_;
static lv_obj_t *network_label_;
static lv_obj_t *status_label_;

static bool force_thinking = false;

//! gif file
static lv_img_dsc_t gif_files[11]; 

#define GIF_EMOTION_FILE_INDEX       9

static uint8_t s_last_wifi_status = 0;

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

static void __lvgl_init(void)
{
    lv_vendor_init(DISPLAY_NAME);

    lv_vendor_start(5, 1024*8);
}

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
}

void robot_set_status(int stat)
{
    const char          *text;
    const lv_img_dsc_t  *gif;

    if (OPRT_OK != gui_status_desc_get(stat, &text, &gif)) {
        return;
    }
    
    lv_label_set_text(status_label_, text);
    lv_obj_center(status_label_);
    lv_obj_update_layout(status_label_);

    lv_gif_set_src(gif_stat, gif);
    lv_obj_update_layout(gif_stat);

    lv_obj_align_to(gif_stat, status_label_, LV_ALIGN_OUT_LEFT_MID, -5, -1);

    s_current_gui_stat = stat;
}

void tuya_robot_init(void)
{
    /* Make sure the base background isn't theme-white. */
    lv_obj_set_style_bg_color(lv_scr_act(), lv_color_black(), 0);
    lv_obj_set_style_bg_opa(lv_scr_act(), LV_OPA_COVER, 0);

    /* Container */
    lv_obj_t * container_ = lv_obj_create(lv_scr_act());
    lv_obj_set_size(container_, LV_HOR_RES, LV_VER_RES);
    lv_obj_set_style_pad_all(container_, 0, 0);
    lv_obj_set_style_border_width(container_, 0, 0);
    lv_obj_set_scrollbar_mode(container_, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_bg_color(container_, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(container_, LV_OPA_COVER, 0);

    robot_status_bar_init(container_);

    gif_full = lv_gif_create(container_);
    lv_obj_set_size(gif_full, LV_HOR_RES, LV_VER_RES);
    lv_obj_set_style_bg_color(gif_full, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(gif_full, LV_OPA_COVER, 0);

    robot_set_status(GUI_STAT_INIT);
    robot_emotion_flush("neutral");
    lv_obj_move_background(gif_full);
}

void robot_ui_battery_update(bool is_charging, uint8_t percentage)
{
    lv_vendor_disp_lock();
    if (battery_label_) {
        if (is_charging) {
            lv_label_set_text(battery_label_, FONT_AWESOME_BATTERY_CHARGING);
        } else {
            lv_label_set_text(battery_label_, gui_battery_level_get(percentage));
        }
    }
    lv_vendor_disp_unlock();
}

void robot_ui_enter_netcfg(void)
{
    lv_vendor_disp_lock();
    if (status_label_) {
        robot_set_status(GUI_STAT_PROV);
    }
    lv_vendor_disp_unlock();
}

void robot_ui_wifi_update(uint8_t wifi_status)
{
    lv_vendor_disp_lock();
    if (network_label_) {
        if (!wifi_status && !force_thinking) {
            /* After init, show "Connecting" until Wi-Fi is up. */
            if (gif_full && status_label_ &&
                (s_current_gui_stat == GUI_STAT_INIT || s_current_gui_stat == GUI_STAT_CONN)) {
                robot_set_status(GUI_STAT_CONN);
            }
        }

        if (wifi_status && !s_last_wifi_status) {
            robot_gif_load();
            if (gif_full && status_label_) {
                robot_emotion_flush("neutral");
                robot_set_status(GUI_STAT_IDLE);
            }
        }
        lv_label_set_text(network_label_, gui_wifi_level_get(wifi_status));
    }
    lv_vendor_disp_unlock();

    s_last_wifi_status = wifi_status;
}

#if defined(ENABLE_COMP_AI_DISPLAY) && (ENABLE_COMP_AI_DISPLAY == 1)
static bool __robot_gui_stat_from_text(const char *text, uint8_t *stat)
{
    if (!text || !stat) {
        return false;
    }

    /* Prefix match to allow minor suffix differences. */
    if (strncmp(text, STANDBY, strlen(STANDBY)) == 0) {
        *stat = (uint8_t)GUI_STAT_IDLE;
        return true;
    }
    if (strncmp(text, LISTENING, strlen(LISTENING)) == 0) {
        *stat = (uint8_t)GUI_STAT_LISTEN;
        return true;
    }
    if (strncmp(text, UPLOADING, strlen(UPLOADING)) == 0) {
        *stat = (uint8_t)GUI_STAT_UPLOAD;
        return true;
    }
    if (strncmp(text, THINKING, strlen(THINKING)) == 0) {
        *stat = (uint8_t)GUI_STAT_THINK;
        return true;
    }
    if (strncmp(text, SPEAKING, strlen(SPEAKING)) == 0) {
        *stat = (uint8_t)GUI_STAT_SPEAK;
        return true;
    }
    if (strncmp(text, INITIALIZING, strlen(INITIALIZING)) == 0) {
        *stat = (uint8_t)GUI_STAT_INIT;
        return true;
    }
    if (strncmp(text, CONNECTING, strlen(CONNECTING)) == 0 ||
        strncmp(text, CONNECT_SERVER, strlen(CONNECT_SERVER)) == 0) {
        *stat = (uint8_t)GUI_STAT_CONN;
        return true;
    }

    return false;
}

static void __robot_apply_chat_stat(uint8_t stat)
{
    if (!gif_full || !status_label_) {
        return;
    }

    if (force_thinking) {
        /* Keep showing "Thinking" after ASR until we really enter speaking. */
        if (stat == GUI_STAT_IDLE || stat == GUI_STAT_LISTEN) {
            return;
        }
        if (stat == GUI_STAT_SPEAK) {
            force_thinking = false;
        }
    }

    if (GUI_STAT_IDLE == stat || GUI_STAT_LISTEN == stat) {
        robot_emotion_flush("neutral");
    }

    robot_set_status(stat);
}

static OPERATE_RET __ui_init(void)
{
    __lvgl_init();
    PR_DEBUG("lvgl init success");

    lv_vendor_disp_lock();
    tuya_robot_init();
    lv_vendor_disp_unlock();
    PR_DEBUG("ui init success");

    return OPRT_OK;
}

static void __ui_disp_emotion(char *emotion)
{
    if (!emotion) {
        return;
    }

    lv_vendor_disp_lock();
    if (gif_full) {
        robot_emotion_flush(emotion);
    }
    lv_vendor_disp_unlock();
}

static void __ui_disp_user_msg(char *string)
{
    (void)string;

    lv_vendor_disp_lock();
    if (status_label_) {
        force_thinking = true;
        /* Ensure thinking animation is available if the filesystem gifs are used. */
        robot_gif_load();
        robot_set_status(GUI_STAT_THINK);
    }
    lv_vendor_disp_unlock();
}

static void __ui_disp_ai_mode_state(char *string)
{
    uint8_t stat = 0;
    if (string && __robot_gui_stat_from_text(string, &stat)) {
        lv_vendor_disp_lock();
        __robot_apply_chat_stat(stat);
        lv_vendor_disp_unlock();
    }
}

#endif

OPERATE_RET ai_ui_robot_dog_register(void)
{
#if defined(ENABLE_COMP_AI_DISPLAY) && (ENABLE_COMP_AI_DISPLAY == 1)
    AI_UI_INTFS_T intfs;

    memset(&intfs, 0, sizeof(AI_UI_INTFS_T));

    intfs.disp_init = __ui_init;
    intfs.disp_user_msg = __ui_disp_user_msg;
    intfs.disp_emotion = __ui_disp_emotion;
    intfs.disp_ai_mode_state = __ui_disp_ai_mode_state;

    return ai_ui_register(&intfs);
#else
    return OPRT_OK;
#endif
}

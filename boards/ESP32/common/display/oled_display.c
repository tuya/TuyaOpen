/**
 * @file oled_display.c
 * @brief oled_display module is used to
 * @version 0.1
 * @date 2025-04-23
 */

#include "oled_display.h"

#include "font_awesome_symbols.h"

#include "esp_err.h"
#include "esp_log.h"
#include <driver/i2c_master.h>
#include "driver/gpio.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lvgl_port.h"
#include "lvgl.h"

/***********************************************************
************************macro define************************
***********************************************************/
#define TAG "oled_display"

LV_FONT_DECLARE(font_awesome_30_1);
LV_FONT_DECLARE(font_awesome_14_1);
LV_FONT_DECLARE(font_puhui_14_1);

/***********************************************************
***********************typedef define***********************
***********************************************************/
typedef struct {
    int scl;
    int sda;
    int width;
    int height;

    i2c_master_bus_handle_t i2c_bus;
    esp_lcd_panel_io_handle_t panel_io;
    esp_lcd_panel_handle_t panel;
} OLED_CONFIG_T;

typedef struct {
    lv_obj_t *container;
    lv_obj_t *content;
    lv_obj_t *emotion_label;
    lv_obj_t *side_bar;
    lv_obj_t *status_bar;
    lv_obj_t *notification_label;
    lv_obj_t *mute_label;
    lv_obj_t *network_label;
    lv_obj_t *battery_label;
    lv_obj_t *status_label;
    lv_obj_t *chat_message_label;
    lv_obj_t *content_left;
    lv_obj_t *content_right;
    lv_anim_t msg_anim;
} OLED_DISPLAY_UI_HANDLE_T;

typedef struct {
    char emo_text[32];
    char *emo_icon;
} OLED_EMOJI_T;

/***********************************************************
********************function declaration********************
***********************************************************/

/***********************************************************
***********************variable define**********************
***********************************************************/
static const lv_font_t *text_font = &font_puhui_14_1;
static const lv_font_t *icon_font = &font_awesome_14_1;
static lv_font_t *emoji_font = NULL;

static OLED_DISPLAY_UI_HANDLE_T ui_hdl = {0};
static OLED_CONFIG_T oled_config = {0};

static OLED_EMOJI_T emo_list[] = {
    {"SAD", FONT_AWESOME_EMOJI_SAD},           {"ANGRY", FONT_AWESOME_EMOJI_ANGRY},
    {"NEUTRAL", FONT_AWESOME_EMOJI_NEUTRAL},   {"SURPRISE", FONT_AWESOME_EMOJI_SURPRISED},
    {"CONFUSED", FONT_AWESOME_EMOJI_CONFUSED}, {"THINKING", FONT_AWESOME_EMOJI_THINKING},
    {"HAPPY", FONT_AWESOME_EMOJI_HAPPY},
};

/***********************************************************
***********************function define**********************
***********************************************************/

int oled_ssd1306_init(int scl, int sda, int width, int height)
{
    oled_config.scl = scl;
    oled_config.sda = sda;
    oled_config.width = width;
    oled_config.height = height;

    // Initialize I2C and SSD1306 here
    ESP_LOGI(TAG, "SSD1306 initialized");

    i2c_master_bus_config_t bus_config = {
        .i2c_port = (i2c_port_t)0,
        .sda_io_num = sda,
        .scl_io_num = scl,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .intr_priority = 0,
        .trans_queue_depth = 0,
        .flags =
            {
                .enable_internal_pullup = 1,
            },
    };
    ESP_ERROR_CHECK(i2c_new_master_bus(&bus_config, &oled_config.i2c_bus));

    ESP_LOGI(TAG, "InitializeDisplayI2c");

    esp_lcd_panel_io_i2c_config_t io_config = {
        .dev_addr = 0x3C,
        .on_color_trans_done = NULL,
        .user_ctx = NULL,
        .control_phase_bytes = 1,
        .dc_bit_offset = 6,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
        .flags =
            {
                .dc_low_on_data = 0,
                .disable_control_phase = 0,
            },
        .scl_speed_hz = 400 * 1000,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_i2c_v2(oled_config.i2c_bus, &io_config, &oled_config.panel_io));

    ESP_LOGI(TAG, "Install SSD1306 driver");
    esp_lcd_panel_dev_config_t panel_config = {};
    panel_config.reset_gpio_num = -1;
    panel_config.bits_per_pixel = 1;

    esp_lcd_panel_ssd1306_config_t ssd1306_config = {
        .height = height,
    };
    panel_config.vendor_config = &ssd1306_config;

    ESP_ERROR_CHECK(esp_lcd_new_panel_ssd1306(oled_config.panel_io, &panel_config, &oled_config.panel));

    ESP_ERROR_CHECK(esp_lcd_panel_reset(oled_config.panel));

_RETRY:
    if (esp_lcd_panel_init(oled_config.panel)) {
        ESP_LOGE(TAG, "Failed to initialize panel");
        vTaskDelay(1000 / portTICK_PERIOD_MS);
        goto _RETRY;
    }

    // Set the display to on
    ESP_LOGI(TAG, "Turning display on");
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(oled_config.panel, true));

    ESP_LOGI(TAG, "Initialize LVGL");
    lvgl_port_cfg_t port_cfg = ESP_LVGL_PORT_INIT_CONFIG();
    port_cfg.task_priority = 1;
    port_cfg.timer_period_ms = 50;
    lvgl_port_init(&port_cfg);

    const lvgl_port_display_cfg_t disp_cfg = {
        .io_handle = oled_config.panel_io,
        .panel_handle = oled_config.panel,
        .control_handle = NULL,
        .buffer_size = height * width,
        .double_buffer = false,
        .hres = width,
        .vres = height,
        .monochrome = true,
        .rotation =
            {
                .swap_xy = false,
                .mirror_x = true,
                .mirror_y = true,
            },
        .flags =
            {
                .buff_dma = 0,
                .buff_spiram = 1,
                .sw_rotate = 0,
                .full_refresh = 0,
                .direct_mode = 0,
            },
    };
    lv_disp_t *disp = lvgl_port_add_disp(&disp_cfg);

    return 0;
}

void oled_setup_ui_128x32(void)
{
    lvgl_port_lock(0);

    lv_obj_t *screen = lv_screen_active();
    lv_obj_set_style_text_font(screen, &font_puhui_14_1, 0);

    ui_hdl.container = lv_obj_create(screen);
    lv_obj_set_size(ui_hdl.container, LV_HOR_RES, LV_VER_RES);
    lv_obj_set_flex_flow(ui_hdl.container, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_all(ui_hdl.container, 0, 0);
    lv_obj_set_style_border_width(ui_hdl.container, 0, 0);
    lv_obj_set_style_pad_column(ui_hdl.container, 0, 0);

    ui_hdl.content = lv_obj_create(ui_hdl.container);
    lv_obj_set_size(ui_hdl.content, 32, 32);
    lv_obj_set_style_pad_all(ui_hdl.content, 0, 0);
    lv_obj_set_style_border_width(ui_hdl.content, 0, 0);
    lv_obj_set_style_radius(ui_hdl.content, 0, 0);

    ui_hdl.emotion_label = lv_label_create(ui_hdl.content);
    lv_obj_set_style_text_font(ui_hdl.emotion_label, &font_awesome_30_1, 0);
    lv_label_set_text(ui_hdl.emotion_label, FONT_AWESOME_AI_CHIP);
    lv_obj_center(ui_hdl.emotion_label);

    /* Right side */
    ui_hdl.side_bar = lv_obj_create(ui_hdl.container);
    lv_obj_set_size(ui_hdl.side_bar, LV_HOR_RES - 32, 32);
    lv_obj_set_flex_flow(ui_hdl.side_bar, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(ui_hdl.side_bar, 0, 0);
    lv_obj_set_style_border_width(ui_hdl.side_bar, 0, 0);
    lv_obj_set_style_radius(ui_hdl.side_bar, 0, 0);
    lv_obj_set_style_pad_row(ui_hdl.side_bar, 0, 0);

    /* Status bar */
    ui_hdl.status_bar = lv_obj_create(ui_hdl.side_bar);
    lv_obj_set_size(ui_hdl.status_bar, LV_HOR_RES - 32, 16);
    lv_obj_set_style_radius(ui_hdl.status_bar, 0, 0);
    lv_obj_set_flex_flow(ui_hdl.status_bar, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_all(ui_hdl.status_bar, 0, 0);
    lv_obj_set_style_border_width(ui_hdl.status_bar, 0, 0);
    lv_obj_set_style_pad_column(ui_hdl.status_bar, 0, 0);

    ui_hdl.status_label = lv_label_create(ui_hdl.status_bar);
    lv_obj_set_flex_grow(ui_hdl.status_label, 1);
    lv_obj_set_style_pad_left(ui_hdl.status_label, 2, 0);
    lv_label_set_text(ui_hdl.status_label, "正在初始化...");

    ui_hdl.notification_label = lv_label_create(ui_hdl.status_bar);
    lv_obj_set_flex_grow(ui_hdl.notification_label, 1);
    lv_obj_set_style_pad_left(ui_hdl.notification_label, 2, 0);
    lv_label_set_text(ui_hdl.notification_label, "");
    lv_obj_add_flag(ui_hdl.notification_label, LV_OBJ_FLAG_HIDDEN);

    ui_hdl.mute_label = lv_label_create(ui_hdl.status_bar);
    lv_label_set_text(ui_hdl.mute_label, "");
    lv_obj_set_style_text_font(ui_hdl.mute_label, icon_font, 0);

    ui_hdl.network_label = lv_label_create(ui_hdl.status_bar);
    lv_label_set_text(ui_hdl.network_label, "");
    lv_obj_set_style_text_font(ui_hdl.network_label, icon_font, 0);

    ui_hdl.chat_message_label = lv_label_create(ui_hdl.side_bar);
    lv_obj_set_size(ui_hdl.chat_message_label, LV_HOR_RES - 32, LV_SIZE_CONTENT);
    lv_obj_set_style_pad_left(ui_hdl.chat_message_label, 2, 0);
    lv_label_set_long_mode(ui_hdl.chat_message_label, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_label_set_text(ui_hdl.chat_message_label, "");

    lv_anim_init(&ui_hdl.msg_anim);
    lv_anim_set_delay(&ui_hdl.msg_anim, 1000);
    lv_anim_set_repeat_count(&ui_hdl.msg_anim, LV_ANIM_REPEAT_INFINITE);
    lv_obj_set_style_anim(ui_hdl.chat_message_label, &ui_hdl.msg_anim, LV_PART_MAIN);
    lv_obj_set_style_anim_duration(ui_hdl.chat_message_label, lv_anim_speed_clamped(60, 300, 60000), LV_PART_MAIN);

    lvgl_port_unlock();

    return;
}

void oled_setup_ui_128x64(void)
{
    lvgl_port_lock(0);

    lv_obj_t *screen = lv_screen_active();
    lv_obj_set_style_text_font(screen, &font_puhui_14_1, 0);
    lv_obj_set_style_text_color(screen, lv_color_black(), 0);

    /* Container */
    ui_hdl.container = lv_obj_create(screen);
    lv_obj_set_size(ui_hdl.container, LV_HOR_RES, LV_VER_RES);
    lv_obj_set_flex_flow(ui_hdl.container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(ui_hdl.container, 0, 0);
    lv_obj_set_style_border_width(ui_hdl.container, 0, 0);
    lv_obj_set_style_pad_row(ui_hdl.container, 0, 0);

    /* Status bar */
    ui_hdl.status_bar = lv_obj_create(ui_hdl.container);
    lv_obj_set_size(ui_hdl.status_bar, LV_HOR_RES, 16);
    lv_obj_set_style_border_width(ui_hdl.status_bar, 0, 0);
    lv_obj_set_style_pad_all(ui_hdl.status_bar, 0, 0);
    lv_obj_set_style_radius(ui_hdl.status_bar, 0, 0);

    /* Content */
    ui_hdl.content = lv_obj_create(ui_hdl.container);
    lv_obj_set_scrollbar_mode(ui_hdl.content, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_radius(ui_hdl.content, 0, 0);
    lv_obj_set_style_pad_all(ui_hdl.content, 0, 0);
    lv_obj_set_width(ui_hdl.content, LV_HOR_RES);
    lv_obj_set_flex_grow(ui_hdl.content, 1);
    lv_obj_set_flex_flow(ui_hdl.content, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_flex_main_place(ui_hdl.content, LV_FLEX_ALIGN_CENTER, 0);

    ui_hdl.content_left = lv_obj_create(ui_hdl.content);
    lv_obj_set_size(ui_hdl.content_left, 32, LV_SIZE_CONTENT);
    lv_obj_set_style_pad_all(ui_hdl.content_left, 0, 0);
    lv_obj_set_style_border_width(ui_hdl.content_left, 0, 0);

    ui_hdl.emotion_label = lv_label_create(ui_hdl.content_left);
    lv_obj_set_style_text_font(ui_hdl.emotion_label, &font_awesome_30_1, 0);
    lv_label_set_text(ui_hdl.emotion_label, FONT_AWESOME_AI_CHIP);
    lv_obj_center(ui_hdl.emotion_label);
    lv_obj_set_style_pad_top(ui_hdl.emotion_label, 8, 0);

    ui_hdl.content_right = lv_obj_create(ui_hdl.content);
    lv_obj_set_size(ui_hdl.content_right, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_pad_all(ui_hdl.content_right, 0, 0);
    lv_obj_set_style_border_width(ui_hdl.content_right, 0, 0);
    lv_obj_set_flex_grow(ui_hdl.content_right, 1);
    lv_obj_add_flag(ui_hdl.content_right, LV_OBJ_FLAG_HIDDEN);

    ui_hdl.chat_message_label = lv_label_create(ui_hdl.content_right);
    lv_label_set_text(ui_hdl.chat_message_label, "");
    lv_label_set_long_mode(ui_hdl.chat_message_label, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_obj_set_style_text_align(ui_hdl.chat_message_label, LV_TEXT_ALIGN_LEFT, 0);
    lv_obj_set_width(ui_hdl.chat_message_label, LV_HOR_RES - 32);
    lv_obj_set_style_pad_top(ui_hdl.chat_message_label, 14, 0);

    lv_anim_init(&ui_hdl.msg_anim);
    lv_anim_set_delay(&ui_hdl.msg_anim, 1000);
    lv_anim_set_repeat_count(&ui_hdl.msg_anim, LV_ANIM_REPEAT_INFINITE);
    lv_obj_set_style_anim(ui_hdl.chat_message_label, &ui_hdl.msg_anim, LV_PART_MAIN);
    lv_obj_set_style_anim_duration(ui_hdl.chat_message_label, lv_anim_speed_clamped(60, 300, 60000), LV_PART_MAIN);

    /* Status bar */
    lv_obj_set_flex_flow(ui_hdl.status_bar, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_all(ui_hdl.status_bar, 0, 0);
    lv_obj_set_style_border_width(ui_hdl.status_bar, 0, 0);
    lv_obj_set_style_pad_column(ui_hdl.status_bar, 0, 0);

    ui_hdl.network_label = lv_label_create(ui_hdl.status_bar);
    lv_label_set_text(ui_hdl.network_label, "");
    lv_obj_set_style_text_font(ui_hdl.network_label, icon_font, 0);

    ui_hdl.notification_label = lv_label_create(ui_hdl.status_bar);
    lv_obj_set_flex_grow(ui_hdl.notification_label, 1);
    lv_obj_set_style_text_align(ui_hdl.notification_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(ui_hdl.notification_label, "");
    lv_obj_add_flag(ui_hdl.notification_label, LV_OBJ_FLAG_HIDDEN);

    ui_hdl.status_label = lv_label_create(ui_hdl.status_bar);
    lv_obj_set_flex_grow(ui_hdl.status_label, 1);
    lv_label_set_text(ui_hdl.status_label, "正在初始化...");
    lv_obj_set_style_text_align(ui_hdl.status_label, LV_TEXT_ALIGN_CENTER, 0);

    ui_hdl.mute_label = lv_label_create(ui_hdl.status_bar);
    lv_label_set_text(ui_hdl.mute_label, "");
    lv_obj_set_style_text_font(ui_hdl.mute_label, icon_font, 0);

    ui_hdl.battery_label = lv_label_create(ui_hdl.status_bar);
    lv_label_set_text(ui_hdl.battery_label, "");
    lv_obj_set_style_text_font(ui_hdl.battery_label, icon_font, 0);

    lvgl_port_unlock();

    return;
}

void oled_set_status(const char *status)
{
    if (NULL == ui_hdl.status_label) {
        return;
    }

    lvgl_port_lock(0);

    lv_label_set_text(ui_hdl.status_label, status);
    lv_obj_clear_flag(ui_hdl.status_label, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(ui_hdl.notification_label, LV_OBJ_FLAG_HIDDEN);

    lvgl_port_unlock();

    return;
}

void oled_show_notification(const char *notification)
{
    if (NULL == ui_hdl.notification_label) {
        return;
    }

    lvgl_port_lock(0);

    lv_label_set_text(ui_hdl.notification_label, notification);
    lv_obj_clear_flag(ui_hdl.notification_label, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(ui_hdl.status_label, LV_OBJ_FLAG_HIDDEN);

    lvgl_port_unlock();

    // TODO sw timer

    return;
}

void oled_set_emotion(const char *emotion)
{
    char *emo_icon = FONT_AWESOME_EMOJI_NEUTRAL;

    if (NULL == ui_hdl.emotion_label) {
        return;
    }

    for (int i = 0; i < sizeof(emo_list) / sizeof(OLED_EMOJI_T); i++) {
        if (strcmp(emotion, emo_list[i].emo_text) == 0) {
            emo_icon = emo_list[i].emo_icon;
            break;
        }
    }

    lvgl_port_lock(0);
    lv_label_set_text(ui_hdl.emotion_label, emo_icon);
    lvgl_port_unlock();

    return;
}

void oled_set_chat_message(CHAT_ROLE_E role, const char *content)
{
    if (NULL == ui_hdl.chat_message_label) {
        return;
    }

    lvgl_port_lock(0);
    lv_label_set_text(ui_hdl.chat_message_label, content);
    lvgl_port_unlock();

    return;
}

void oled_set_wifi_status(DIS_WIFI_STATUS_E status)
{
    if (NULL == ui_hdl.network_label) {
        return;
    }

    char *wifi_icon = NULL;

    switch (status) {
    case DIS_WIFI_STATUS_DISCONNECTED:
        wifi_icon = FONT_AWESOME_WIFI_OFF;
        break;
    case DIS_WIFI_STATUS_GOOD:
        wifi_icon = FONT_AWESOME_WIFI;
        break;
    case DIS_WIFI_STATUS_FAIR:
        wifi_icon = FONT_AWESOME_WIFI_FAIR;
        break;
    case DIS_WIFI_STATUS_WEAK:
        wifi_icon = FONT_AWESOME_WIFI_WEAK;
        break;
    default:
        wifi_icon = FONT_AWESOME_WIFI_OFF;
        break;
    }

    lvgl_port_lock(0);
    lv_label_set_text(ui_hdl.network_label, wifi_icon);
    lvgl_port_unlock();

    return;
}

/**
 * @file bread_compact_wifi.c
 * @brief bread_compact_wifi module is used to
 * @version 0.1
 * @date 2025-04-23
 */

#include "tuya_cloud_types.h"

#include "app_board_api.h"

#include "board_config.h"
#include "oled_display.h"

#include "tdd_audio_no_codec.h"

/***********************************************************
************************macro define************************
***********************************************************/

/***********************************************************
***********************typedef define***********************
***********************************************************/

/***********************************************************
********************function declaration********************
***********************************************************/

/***********************************************************
***********************variable define**********************
***********************************************************/

/***********************************************************
***********************function define**********************
***********************************************************/

int app_audio_driver_init(const char *name)
{
    TDD_AUDIO_NO_CODEC_T cfg = {0};
    cfg.i2s_id = 0;
    cfg.mic_sample_rate = 16000;
    cfg.spk_sample_rate = 16000;

    return tdd_audio_no_codec_register(name, cfg);
}

void app_display_init(void)
{
    oled_ssd1306_init(OLED_I2C_SCL, OLED_I2C_SDA, OLED_WIDTH, OLED_HEIGHT);

#if defined(OLED_128x32)
    oled_setup_ui_128x32();
#elif defined(OLED_128x64)
    oled_setup_ui_128x64();
#endif

    return;
}

void app_display_set_status(const char *status)
{
    oled_set_status(status);
}

void app_display_show_notification(const char *notification)
{
    oled_show_notification(notification);
}

void app_display_set_emotion(const char *emotion)
{
    oled_set_emotion(emotion);
}

void app_display_set_chat_massage(CHAT_ROLE_E role, const char *content)
{
    oled_set_chat_message(role, content);
}

void app_display_set_wifi_status(DIS_WIFI_STATUS_E status)
{
    oled_set_wifi_status(status);
}

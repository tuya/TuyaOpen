#include "ui.h"
#include "./pages/ui_HomePage/ui_HomePage.h"
#include "./pages/ui_SettingPage/ui_SettingPage.h"
#include "./pages/ui_WeatherPage/ui_WeatherPage.h"
#include "./pages/ui_CalendarPage/ui_CalendarPage.h"
#include "./pages/ui_GameMuyuPage/ui_GameMuyuPage.h"
#include "./pages/ui_Game2048Page/ui_Game2048Page.h"
#include "ui_display.h"
#include "./pages/ui_CameraPage/ui_CameraPage.h"    //gui_app/pages/ui_CameraPage/ui_CameraPage.h
#include "./pages/ui_GameMemoryPage/ui_GameMemoryPage.h"
#include "./pages/ui_DrawPage/ui_DrawPage.h"
#include "./pages/ui_CalculatorPage/ui_CalculatorPage.h"
#include "./pages/ui_Game_dinoso_jump/ui_dino.h"
///////////////////// VARIABLES ////////////////////

lv_lib_pm_t page_manager;

system_para_t ui_system_para;

///////////////////// TEST LVGL SETTINGS ////////////////////

#if LV_COLOR_DEPTH != 16
    #error "LV_COLOR_DEPTH should be 16bit to match SquareLine Studio's settings"
#endif

///////////////////// all apps ////////////////////

#define _APP_NUMS 8 // number of apps (including HomePage)

ui_app_data_t ui_apps[] = 
{
    {
        .name = "HomePage",
        .init = ui_HomePage_init,
        .deinit = ui_HomePage_deinit,
        .page_obj = NULL
    },
    {
        .name = "SettingPage",
        .init = ui_SettingPage_init,
        .deinit = ui_SettingPage_deinit,
        .page_obj = NULL
    },
    {
        .name = "WeatherPage",
        .init = ui_WeatherPage_init,
        .deinit = ui_WeatherPage_deinit,
        .page_obj = NULL
    },
    {        
        .name = "Game2048Page",
        .init = ui_Game2048Page_init,
        .deinit = ui_Game2048Page_deinit,
        .page_obj = NULL
    },
    {
        .name = "GameMuyuPage",
        .init = ui_GameMuyuPage_init,
        .deinit = ui_GameMuyuPage_deinit,
        .page_obj = NULL
    },
    // {
    //     .name = "CalculatorPage",//有BUG暂时没有使用
    //     .init = ui_CalculatorPage_init,
    //     .deinit = ui_CalculatorPage_deinit,
    //     .page_obj = NULL
    // },
    {
        .name = "GameDinoPage",
        .init = ui_dino_init,
        .deinit = ui_dino_deinit,
        .page_obj = NULL
    },
    {
        .name = "CalendarPage",
        .init = ui_CalendarPage_init,
        .deinit = ui_CalendarPage_deinit,
        .page_obj = NULL
    },
    {
        .name = "ChatBotPage",
        .init = ui_init,
        .deinit = ui_deinit,
        .page_obj = NULL
    },
    {
        .name = "CameraPage",
        .init = ui_CameraPage_init,
        .deinit = ui_CameraPage_deinit,
        .page_obj = NULL
    },
    {
        .name = "GameMemoryPage",
        .init = ui_GameMemoryPage_init,
        .deinit = ui_GameMemoryPage_deinit,
        .page_obj = NULL
    },
    {
        .name = "DrawPage",
        .init = ui_DrawPage_init,
        .deinit = ui_DrawPage_deinit,
        .page_obj = NULL
    },
}; 

const int ui_app_nums = sizeof(ui_apps) / sizeof(ui_apps[0]);
///////////////////// Function ////////////////////

static void msgbox_close_click_event_cb(lv_event_t * e)
{
    lv_obj_t * mbox = lv_event_get_target(e);
    (void)mbox; // Avoid unused variable warning
    bool * mbox_exist = lv_event_get_user_data(e);
    *mbox_exist = false;
}

void ui_msgbox_info(const char * title, const char * text)
{
    static lv_obj_t * current_mbox;
    static bool mbox_exist = false;
    if(mbox_exist)
    {
        lv_msgbox_close(current_mbox);
        mbox_exist = false;
    }
    // 创建新消息框
    current_mbox = lv_msgbox_create(NULL);
    mbox_exist = true;
    lv_msgbox_add_title(current_mbox, title);
    lv_msgbox_add_text(current_mbox, text);
    lv_obj_t * close_btn = lv_msgbox_add_close_button(current_mbox);
    lv_obj_add_event_cb(close_btn, msgbox_close_click_event_cb, LV_EVENT_PRESSED, &mbox_exist);
}

static void _sys_para_init(void)
{
    if(1)
    {
        LV_LOG_WARN("Load system parameters failed, create a new config file.");
        ui_system_para.year = 2025;
        ui_system_para.month = 1;
        ui_system_para.day = 1;
        ui_system_para.hour = 0;
        ui_system_para.minute = 0;
        ui_system_para.brightness = 100;//屏幕亮度
        ui_system_para.sound = 100;//音量大小
        ui_system_para.wifi_connected = false;//wifi连接状态
        ui_system_para.auto_time = true;//自动时间
        ui_system_para.auto_location = true;//自动定位
        strcpy(ui_system_para.location.city, "西湖区");//城市名称
       
        // create a new config file and save
        sys_save_system_parameters(sys_config_path, &ui_system_para);
    }
    // WIFI
    ui_system_para.wifi_connected = sys_get_wifi_status();
    // TIME
    if(ui_system_para.auto_time == true)
    {
        if(sys_get_time_from_ntp("ntp.aliyun.com", &ui_system_para.year, &ui_system_para.month, &ui_system_para.day, &ui_system_para.hour, &ui_system_para.minute, NULL) != 0)
        {
            LV_LOG_WARN("Get time from NTP failed, use system time.");
        }
        else
        {
            sys_set_time(ui_system_para.year, ui_system_para.month, ui_system_para.day, ui_system_para.hour, ui_system_para.minute, 0);
            LV_LOG_USER("Auto NTP time year: %d, month: %d, day: %d, hour: %d, minute: %d", ui_system_para.year, ui_system_para.month, ui_system_para.day, ui_system_para.hour, ui_system_para.minute);
        }
    }
    else
    {
        sys_set_time(ui_system_para.year, ui_system_para.month, ui_system_para.day, ui_system_para.hour, ui_system_para.minute, 0);
        LV_LOG_USER("Manual time year: %d, month: %d, day: %d, hour: %d, minute: %d", ui_system_para.year, ui_system_para.month, ui_system_para.day, ui_system_para.hour, ui_system_para.minute);
    }
    // LOCATION
    if(ui_system_para.auto_location == true)
    {
        if(sys_get_auto_location_by_tuya_weather(&ui_system_para.location) == 0)
        {
            LV_LOG_USER("Auto location city: %s, adcode: %s", ui_system_para.location.city, ui_system_para.location.adcode);
        }
        else
        {
            LV_LOG_WARN("Get location by IP failed, use system location.");
            const char *city_name = sys_get_city_name();
            strcpy(ui_system_para.location.city, city_name);
            LV_LOG_USER("Manual location city: %s, adcode: %s", ui_system_para.location.city, ui_system_para.location.adcode);
        }
    }
    else
    {
        const char *city_name = sys_get_city_name();
        strcpy(ui_system_para.location.city, city_name);
        LV_LOG_USER("Manual location city: %s, adcode: %s", ui_system_para.location.city, ui_system_para.location.adcode);
    }
    LV_LOG_USER("System para init done.");
}



///////////////////// timer //////////////////////
#if 0
// 1s timer
void _maintimer_cb(void)
{
    static uint16_t time_count2 = 299;
    time_count2++;
    // 每秒闪烁一次LED
    if(time_count2 % 2 == 0)
    {
        gpio_set_value(LED_BLUE, 1);
    }
    else
    {
        gpio_set_value(LED_BLUE, 0);
    }
    // 每5分钟保存一次系统参数
    if(time_count2 >= 300)
    {
        ui_system_para.wifi_connected = sys_get_wifi_status();
        if(ui_system_para.auto_time == true)
        {
            if(sys_get_time_from_ntp("ntp.aliyun.com", &ui_system_para.year, &ui_system_para.month, &ui_system_para.day, &ui_system_para.hour, &ui_system_para.minute, NULL))
            {
                LV_LOG_WARN("Get time from NTP failed, use system time.");
            }
            else
            {
                sys_set_time(ui_system_para.year, ui_system_para.month, ui_system_para.day, ui_system_para.hour, ui_system_para.minute, 0);
                LV_LOG_USER("Auto NTP time year: %d, month: %d, day: %d, hour: %d, minute: %d", ui_system_para.year, ui_system_para.month, ui_system_para.day, ui_system_para.hour, ui_system_para.minute);
            }
        }
        else
        {
            sys_set_time(ui_system_para.year, ui_system_para.month, ui_system_para.day, ui_system_para.hour, ui_system_para.minute, 0);
        }
        sys_save_system_parameters(sys_config_path, &ui_system_para);
        time_count2 = 0; 
    }
}
#endif
///////////////////// SCREENS ////////////////////

void app_ui_init(void)
{
    _sys_para_init();

    lv_disp_t * dispp = lv_display_get_default();
    lv_theme_t * theme = lv_theme_default_init(dispp, lv_palette_main(LV_PALETTE_BLUE), lv_palette_main(LV_PALETTE_RED),
                                               true, LV_FONT_DEFAULT);
    lv_disp_set_theme(dispp, theme);

    lv_lib_pm_Init(&page_manager);
    lv_lib_pm_page_t *pm_page[ui_app_nums];
    for(int i = 0; i < ui_app_nums; i++)
    {
        pm_page[i] = lv_lib_pm_CreatePage(&page_manager, ui_apps[i].name, ui_apps[i].init, ui_apps[i].deinit, NULL);
        (void)pm_page[i]; // Avoid unused variable warning
    }
    lv_lib_pm_OpenPage(&page_manager, NULL, "HomePage");
    // lv_timer_create(_maintimer_cb, 1000, NULL);
}


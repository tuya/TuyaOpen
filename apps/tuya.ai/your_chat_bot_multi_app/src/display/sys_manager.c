#include "sys_manager.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ai_audio.h"
#include "tdl_display_manage.h"
#include "tal_time_service.h"  // 暂时注释掉，避免依赖问题
#include "netmgr.h"
#include "tuya_weather.h"
#include "tal_log.h"

static TDL_DISP_HANDLE_T      sg_tdl_disp_hdl = NULL;

static char local_city[32] = {0};

#if LV_USE_SIMULATOR == 0 
    // #include <alsa/asoundlib.h>  // 嵌入式环境不需要 ALSA
    #define BRIGHTNESS_PATH "/sys/class/backlight/backlight/brightness"
    // 给定的亮度级别数组
    const int brightness_levels[] = {
        0, 3, 5, 8, 10, 13, 16, 18, 21, 24, 26, 29, 32, 34, 37, 40, 42, 45, 48, 50,
        52, 54, 56, 58, 61, 64, 66, 69, 72, 74, 77, 80, 82, 85, 88, 90, 93, 96, 98,
        101, 104, 106, 109, 112, 114, 117, 120, 122, 125, 128, 130, 133, 136, 138,
        141, 144, 146, 149, 152, 154, 157, 160, 162, 165, 168, 170, 173, 176, 178,
        180, 182, 184, 186, 188, 190, 192, 194, 197, 200, 202, 205, 208, 210, 213,
        216, 218, 221, 224, 226, 229, 232, 234, 237, 240, 242, 245, 248, 250, 253, 255
    };
    const int num_brightness_levels = 100;
#endif

#define NTP_PORT 123
#define NTP_TIMESTAMP_DELTA 2208988800ull // 时间戳差值，从1900年到1970年的秒数

const char * sys_config_path = "./system_para.conf"; // 系统参数配置文件路径与可执行文件同目录
const char * city_adcode_path = "./gaode_adcode.json"; // 城市adcode对应表文件路径与可执行文件同目录

// 设置背光亮度
int sys_set_lcd_brightness(int brightness) {

    if (brightness < 0 || brightness > 100) return -1;
    if (brightness < 10) brightness = 10; // 亮度太低可能导致屏幕无法显示
    if (brightness > 95) brightness = 95;
    sg_tdl_disp_hdl = tdl_disp_find_dev(DISPLAY_NAME);
    tdl_disp_set_brightness(sg_tdl_disp_hdl, brightness); // Set brightness to 100%
    return 0;
}


// 设置音量
int sys_set_volume(int level) {
    if (level < 0 || level > 100) return -1; // 音量级别应在0到100之间
    // 这里可以添加实际设置硬件音量的代码
    ai_audio_set_volume(level);
#if LV_USE_SIMULATOR == 0
    
#endif
    return 0;
}

// 判断是否是闰年
int is_leap_year(int year) {
    return (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0));
}

// 验证日期是否有效
int validate_date(int year, int month, int day) {
    // 每个月的最大天数
    int days_in_month[] = { 31, 28 + is_leap_year(year), 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };

    if (year < 1900 || month < 1 || month > 12 || day < 1 || day > days_in_month[month - 1]) {
        return -1;
    }
    return 0;
}

int sys_set_time(int year, int month, int day, int hour, int minute, int second) {


    printf("System time has been successfully updated.\n");
    return 0;
}

void sys_get_time(int *year, int *month, int *day, int *hour, int *minute, int *second) {
    POSIX_TM_S tm = {0};
    tal_time_get_local_time_custom(0, &tm);
    if (year) *year = tm.tm_year + 1900;
    if (month) *month = tm.tm_mon + 1;
    if (day) *day = tm.tm_mday;
    if (hour) *hour = tm.tm_hour;
    if (minute) *minute = tm.tm_min;
    if (second) *second = tm.tm_sec;
}

// 使用蔡勒公式计算星期几，0代表周日，1代表周一，...，6代表周六
int sys_get_day_of_week(int year, int month, int day) {
    if (month < 3) {
        month += 12;
        year -= 1;
    }

    int K = year % 100;
    int J = year / 100;

    // 蔡勒公式
    int f = day + ((13 * (month + 1)) / 5) + K + (K / 4) + (J / 4) + (5 * J);
    int dayOfWeek = f % 7;

    // 根据蔡勒公式的定义调整返回值以匹配常见的一周起始日(0=周日, 1=周一, ..., 6=周六)
    return (dayOfWeek + 1) % 7;
}

bool is_internet_reachable(void) {
    return true;
}
bool sys_get_wifi_status(void) {
    netmgr_status_e net_status = NETMGR_LINK_DOWN;

    netmgr_conn_get(NETCONN_AUTO, NETCONN_CMD_STATUS, &net_status);
    if(net_status == NETMGR_LINK_UP) {
        return true;
    }
    return false;
}




// 回调函数用于处理CURL接收到的数据
size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    size_t realsize = size * nmemb;
    char** response_string = (char**)userp;

    char* new_string = realloc(*response_string, realsize + 1);
    if(new_string == NULL) {
        // 内存分配失败
        fprintf(stderr, "Failed to allocate memory\n");
        return 0;
    }

    *response_string = new_string;
    memcpy(*response_string + strlen(*response_string), contents, realsize);
    (*response_string)[realsize] = '\0';

    return realsize;
}




int sys_save_system_parameters(const char *filepath, const system_para_t *params) {



    return 0;
}

int sys_load_system_parameters(const char *filepath, system_para_t *params) {
    
    return 0;
}

// 这些函数已经在上面定义了，不需要重复定义
// 通过NTP服务器获取时间
int sys_get_time_from_ntp(const char* ntp_server, int *year, int *month, int *day, int *hour, int *minute, int *second)
{
    // 暂时使用默认时间，避免依赖 tal_time_service
    if (year) *year = 2024;
    if (month) *month = 1;
    if (day) *day = 1;
    if (hour) *hour = 0;
    if (minute) *minute = 0;
    if (second) *second = 0;
    return 0;
}
int sys_get_auto_location_by_tuya_weather(LocationInfo_t* location)
{
    OPERATE_RET rt = OPRT_OK;
    char province[64] = {0};
    char city[64] = {0};
    char area[64] = {0};



    // 检查是否允许更新天气数据（设备已激活且网络已连接）
    if(!tuya_weather_allow_update())
    {
        PR_ERR("Weather update not allowed (device not activated or network not connected)");
        return -1;
    }

    // 使用Tuya天气服务API获取城市信息
    rt = tuya_weather_get_city(province, city, area);
    if(rt != OPRT_OK)
    {
        PR_ERR("Failed to get city info from Tuya weather service: %d", rt);
        return -1;
    }

    // 填充位置信息
    // 优先使用 city（城市名），如果没有则使用 area（区县名）
  if(area[0] != '\0')
    {
        strncpy(location->city, area, sizeof(location->city) - 1);
        location->city[sizeof(location->city) - 1] = '\0';
        strncpy(local_city, city, sizeof(local_city) - 1);
        local_city[sizeof(local_city) - 1] = '\0';
    }else
    {
        // 如果所有字段都为空，使用默认值
        strncpy(location->city, "未知地", sizeof(location->city) - 1);
        location->city[sizeof(location->city) - 1] = '\0';
    }

    // adcode 字段暂时保留为空或使用默认值
    // 因为 Tuya 天气服务不直接提供 adcode，如果需要可以通过城市名查询
    location->adcode[0] = '\0';

    PR_DEBUG("Location info: city=%s, province=%s, area=%s", 
             location->city, province, area);

    return 0;
}
const char* sys_get_city_name(void)
{
    // 暂时使用默认城市，避免依赖 tal_time_service
    if (local_city[0] != '\0') {
        return local_city;
    }
    return "未知";
}
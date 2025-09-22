/**
 * @file status_bar.c
 * Status Bar Component for AI Pocket Pet
 */

/*********************
 *      INCLUDES
 *********************/
#include "status_bar.h"
#include "ai_pocket_pet_app.h"

// Network status icon declarations
LV_IMG_DECLARE(wifi_1_bar_icon);
LV_IMG_DECLARE(wifi_2_bar_icon);
LV_IMG_DECLARE(wifi_3_bar_icon);
LV_IMG_DECLARE(wifi_off_icon);
LV_IMG_DECLARE(wifi_find_icon);
LV_IMG_DECLARE(wifi_add_icon);
LV_IMG_DECLARE(four_g_logo_icon);
LV_IMG_DECLARE(cellular_1_bar_icon);
LV_IMG_DECLARE(cellular_2_bar_icon);
LV_IMG_DECLARE(cellular_3_bar_icon);
LV_IMG_DECLARE(cellular_off_icon);
LV_IMG_DECLARE(cellular_connected_no_internet_icon);

// Battery icon declarations
LV_IMG_DECLARE(battery_0_icon);
LV_IMG_DECLARE(battery_1_icon);
LV_IMG_DECLARE(battery_2_icon);
LV_IMG_DECLARE(battery_3_icon);
LV_IMG_DECLARE(battery_4_icon);
LV_IMG_DECLARE(battery_5_icon);
LV_IMG_DECLARE(battery_full_icon);
LV_IMG_DECLARE(battery_charging_icon);

/*********************
 *      DEFINES
 *********************/
#define STATUS_BAR_HEIGHT 24

/**********************
 *      TYPEDEFS
 **********************/
typedef struct {
    lv_obj_t *status_bar;
    lv_obj_t *wifi_icon;
    lv_obj_t *four_g_logo_icon;
    lv_obj_t *network_icon;
    lv_obj_t *battery_icon;

    // Network status tracking
    uint8_t wifi_signal_strength;  // 0 = off, 1-3 = bars, 4 = find, 5 = add
    uint8_t cellular_signal_strength;  // 0 = off, 1-3 = bars, 4 = no internet
    bool cellular_connected;

    // Battery status tracking
    uint8_t battery_level;  // 0-6 (0 = empty, 5 = 5 bars, 6 = full)
    bool battery_charging;
} status_bar_data_t;

/**********************
 *  STATIC PROTOTYPES
 **********************/
static void update_wifi_icon(status_bar_data_t *data, uint8_t signal_strength);
static void update_cellular_icon(status_bar_data_t *data, uint8_t signal_strength, bool connected);
static void update_battery_icon(status_bar_data_t *data, uint8_t level, bool charging);
static const lv_img_dsc_t* get_wifi_icon_by_strength(uint8_t strength);
static const lv_img_dsc_t* get_cellular_icon_by_strength(uint8_t strength, bool connected);
static const lv_img_dsc_t* get_battery_icon_by_level(uint8_t level, bool charging);

/**********************
 *  STATIC VARIABLES
 **********************/
static status_bar_data_t g_status_bar_data;

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

lv_obj_t* status_bar_create(lv_obj_t *parent)
{
    status_bar_data_t *data = &g_status_bar_data;

    data->status_bar = lv_obj_create(parent);
    lv_obj_set_size(data->status_bar, AI_PET_SCREEN_WIDTH, STATUS_BAR_HEIGHT);
    lv_obj_align(data->status_bar, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_opa(data->status_bar, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(data->status_bar, 0, 0);
    lv_obj_set_style_pad_all(data->status_bar, 2, 0);

    // Disable scrolling for status bar
    lv_obj_clear_flag(data->status_bar, LV_OBJ_FLAG_SCROLLABLE);

    // WiFi icon (image widget)
    data->wifi_icon = lv_img_create(data->status_bar);
    lv_obj_set_size(data->wifi_icon, 24, 24);
    lv_obj_align(data->wifi_icon, LV_ALIGN_LEFT_MID, 5, 0);

    // 4G logo icon (static, 24px) -- moved before cellular icon
    data->four_g_logo_icon = lv_img_create(data->status_bar);
    lv_obj_set_size(data->four_g_logo_icon, 24, 24);
    lv_obj_align(data->four_g_logo_icon, LV_ALIGN_LEFT_MID, 35, 0);
    lv_img_set_src(data->four_g_logo_icon, &four_g_logo_icon);

    // Network icon (cellular signal) -- moved after 4G icon
    data->network_icon = lv_img_create(data->status_bar);
    lv_obj_set_size(data->network_icon, 24, 24);
    lv_obj_align(data->network_icon, LV_ALIGN_LEFT_MID, 55, 0);

    // Battery icon (image widget)
    data->battery_icon = lv_img_create(data->status_bar);
    lv_obj_set_size(data->battery_icon, 24, 24);
    lv_obj_align(data->battery_icon, LV_ALIGN_RIGHT_MID, -5, 0);

    // Initialize network status
    data->wifi_signal_strength = 4;  // Default to not connected
    data->cellular_signal_strength = 4;  // Default to not connected
    data->cellular_connected = false;    // Default to not connected

    // Initialize battery status
    data->battery_level = 5;  // Default to full battery
    data->battery_charging = false;  // Default to not charging

    // Set initial icons
    update_wifi_icon(data, data->wifi_signal_strength);
    update_cellular_icon(data, data->cellular_signal_strength, data->cellular_connected);
    update_battery_icon(data, data->battery_level, data->battery_charging);

    return data->status_bar;
}

void status_bar_set_wifi_strength(uint8_t strength)
{
    if (strength <= 5) {
        update_wifi_icon(&g_status_bar_data, strength);
    }
}

void status_bar_set_cellular_status(uint8_t strength, bool connected)
{
    if (strength <= 4) {
        update_cellular_icon(&g_status_bar_data, strength, connected);
    }
}

void status_bar_set_battery_status(uint8_t level, bool charging)
{
    if (level <= 6) {
        update_battery_icon(&g_status_bar_data, level, charging);
    }
}

uint8_t status_bar_get_wifi_strength(void)
{
    return g_status_bar_data.wifi_signal_strength;
}

uint8_t status_bar_get_cellular_strength(void)
{
    return g_status_bar_data.cellular_signal_strength;
}

bool status_bar_get_cellular_connected(void)
{
    return g_status_bar_data.cellular_connected;
}

uint8_t status_bar_get_battery_level(void)
{
    return g_status_bar_data.battery_level;
}

bool status_bar_get_battery_charging(void)
{
    return g_status_bar_data.battery_charging;
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

static void update_wifi_icon(status_bar_data_t *data, uint8_t signal_strength)
{
    const lv_img_dsc_t* icon = get_wifi_icon_by_strength(signal_strength);
    if (icon) {
        lv_img_set_src(data->wifi_icon, icon);
    }
    data->wifi_signal_strength = signal_strength;
}

static void update_cellular_icon(status_bar_data_t *data, uint8_t signal_strength, bool connected)
{
    const lv_img_dsc_t* icon = get_cellular_icon_by_strength(signal_strength, connected);
    if (icon) {
        lv_img_set_src(data->network_icon, icon);
    }
    data->cellular_signal_strength = signal_strength;
    data->cellular_connected = connected;
}

static void update_battery_icon(status_bar_data_t *data, uint8_t level, bool charging)
{
    const lv_img_dsc_t* icon = get_battery_icon_by_level(level, charging);
    if (icon) {
        lv_img_set_src(data->battery_icon, icon);
    }
    data->battery_level = level;
    data->battery_charging = charging;
}

static const lv_img_dsc_t* get_wifi_icon_by_strength(uint8_t strength)
{
    switch (strength) {
        case 0:
            return &wifi_off_icon;
        case 1:
            return &wifi_1_bar_icon;
        case 2:
            return &wifi_2_bar_icon;
        case 3:
            return &wifi_3_bar_icon;
        case 4:
            return &wifi_find_icon;
        case 5:
            return &wifi_add_icon;
        default:
            return &wifi_off_icon;
    }
}

static const lv_img_dsc_t* get_cellular_icon_by_strength(uint8_t strength, bool connected)
{
    if (strength == 0) {
        return &cellular_off_icon;
    }

    if (strength == 4 || !connected) {
        return &cellular_connected_no_internet_icon;
    }

    switch (strength) {
        case 1:
            return &cellular_1_bar_icon;
        case 2:
            return &cellular_2_bar_icon;
        case 3:
            return &cellular_3_bar_icon;
        default:
            return &cellular_off_icon;
    }
}

static const lv_img_dsc_t* get_battery_icon_by_level(uint8_t level, bool charging)
{
    if (charging) {
        return &battery_charging_icon;
    }

    switch (level) {
        case 0:
            return &battery_0_icon;
        case 1:
            return &battery_1_icon;
        case 2:
            return &battery_2_icon;
        case 3:
            return &battery_3_icon;
        case 4:
            return &battery_4_icon;
        case 5:
            return &battery_5_icon;
        case 6:
            return &battery_full_icon;
        default:
            return &battery_full_icon;
    }
}

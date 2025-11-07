/**
 * @file temp_humidity_screen.c
 * @brief Implementation of the temperature and humidity sensor screen
 *
 * This file contains the implementation of the temperature and humidity sensor screen
 * which displays real-time temperature and humidity readings from sensors.
 * Currently uses fixed values for demonstration purposes.
 *
 * The temperature humidity screen includes:
 * - Real-time temperature and humidity display
 * - Data update timer (updates every 2 seconds)
 * - Keyboard event handling for navigation
 * - Modern UI with icons and formatted layout
 *
 * @copyright Copyright (c) 2024 LVGL PC Simulator Project
 */

#include "temp_humidity_screen.h"
#include <stdio.h>
#include <stdlib.h>

#ifdef ENABLE_LVGL_HARDWARE
// #include "tuya_cloud_types.h"
// #include "tal_api.h"
// #include "tkl_output.h"
#include "tkl_gpio.h"
#include "tkl_i2c.h"
#include "tkl_pinmux.h"
#endif

/***********************************************************
************************macro define************************
***********************************************************/
#define UPDATE_INTERVAL_MS  2000    // Update data every 2 seconds

// Hardware configuration (only used when ENABLE_LVGL_HARDWARE is defined)
#ifdef ENABLE_LVGL_HARDWARE
#define SHT3X_I2C_PORT    TUYA_I2C_NUM_1

#ifndef SHT3X_I2C_SCL_PIN
#define SHT3X_I2C_SCL_PIN TUYA_GPIO_NUM_6
#endif

#ifndef SHT3X_I2C_SDA_PIN  
#define SHT3X_I2C_SDA_PIN TUYA_GPIO_NUM_7
#endif

#define SR_I2C_ADDR_SHT3X_A 0x44 // SHT3x : ADDR pin - GND
#define SHT3X_CMD_FETCH_DATA    0xE000 // readout measurements for periodic mode
#define SHT3X_CMD_MEAS_PERI_1_H 0x2130 // measurement: periodic 1 mps, high repeatability

#define CRC_OK  (0)
#define CRC_ERR (-1)
#endif

/***********************************************************
***********************variable define**********************
***********************************************************/

static lv_obj_t *ui_temp_humidity_screen;
static lv_obj_t *temp_container;
static lv_obj_t *humidity_container;
static lv_obj_t *error_label;
static lv_obj_t *temp_value_label;
static lv_obj_t *humidity_value_label;
static lv_obj_t *update_time_label;
static lv_timer_t *update_timer;

// Fixed values for demonstration (will be replaced with actual sensor readings)
static float current_temperature = 23.5f;
static float current_humidity = 65.0f;
static int update_count = 0;
static uint8_t sensor_connected = 0;        // Sensor connection status

#ifdef ENABLE_LVGL_HARDWARE
// Hardware related variables
static uint8_t hardware_initialized = 0;
static uint8_t sensor_first_read = 0;
#endif

Screen_t temp_humidity_screen = {
    .init = temp_humidity_screen_init,
    .deinit = temp_humidity_screen_deinit,
    .screen_obj = &ui_temp_humidity_screen,
    .name = "temp_humidity",
};

/***********************************************************
********************function declaration********************
***********************************************************/

static void update_timer_cb(lv_timer_t *timer);
static void keyboard_event_cb(lv_event_t *e);
static void update_sensor_data(void);
static void update_display(void);

#ifdef ENABLE_LVGL_HARDWARE
// Hardware related functions
static uint8_t sht3x_get_crc8(const uint8_t *data, uint16_t len);
static int sht3x_check_crc8(const uint8_t *data, const uint16_t len, const uint8_t crc_val);
static OPERATE_RET sht3x_read_data(const uint8_t port, const uint16_t len, uint8_t *data);
static OPERATE_RET sht3x_write_cmd(const uint8_t port, const uint16_t cmd);
static OPERATE_RET sht3x_read_temp_humi(int port, uint16_t *temp, uint16_t *humi);
static OPERATE_RET hardware_init(void);
static void hardware_deinit(void);
#endif

/***********************************************************
***********************function define**********************
***********************************************************/

#ifdef ENABLE_LVGL_HARDWARE
/**
 * @brief get CRC8 value for sht3x
 *
 * @param[in] data: data to be calculated
 * @param[in] len: data length
 *
 * @return CRC8 value
 */
static uint8_t sht3x_get_crc8(const uint8_t *data, uint16_t len)
{
    uint8_t i;
    uint8_t crc = 0xFF;

    while (len--) {
        crc ^= *data;

        for (i = 8; i > 0; --i) {
            if (crc & 0x80) {
                crc = (crc << 1) ^ 0x31;
            } else {
                crc = (crc << 1);
            }
        }
        data++;
    }

    return crc;
}

/**
 * @brief check CRC8
 *
 * @param[in] data: data to be checked
 * @param[in] len: data length
 * @param[in] crc_val: crc value
 *
 * @return check result
 */
static int sht3x_check_crc8(const uint8_t *data, const uint16_t len, const uint8_t crc_val)
{
    if (sht3x_get_crc8(data, len) != crc_val) {
        return CRC_ERR;
    }
    return CRC_OK;
}

/**
 * @brief read data from sht3x
 *
 * @param[in] port: I2C port
 * @param[in] len: data length
 * @param[out] data: data received from sht3x
 *
 * @return operation result
 */
static OPERATE_RET sht3x_read_data(const uint8_t port, const uint16_t len, uint8_t *data)
{
    return tkl_i2c_master_receive(port, SR_I2C_ADDR_SHT3X_A, data, len, FALSE);
}

/**
 * @brief write command to sht3x
 *
 * @param[in] port: I2C port
 * @param[in] cmd: control command
 *
 * @return operation result
 */
static OPERATE_RET sht3x_write_cmd(const uint8_t port, const uint16_t cmd)
{
    uint8_t cmd_bytes[2];
    cmd_bytes[0] = (uint8_t)(cmd >> 8);
    cmd_bytes[1] = (uint8_t)(cmd & 0x00FF);

    return tkl_i2c_master_send(port, SR_I2C_ADDR_SHT3X_A, cmd_bytes, 2, FALSE);
}

/**
 * @brief read temperature and humidity from sht3x
 *
 * @param[in] port: I2C port
 * @param[out] temp: temperature value
 * @param[out] humi: humidity value
 *
 * @return OPRT_OK on success, others on error
 */
static OPERATE_RET sht3x_read_temp_humi(int port, uint16_t *temp, uint16_t *humi)
{
    uint8_t buf[6] = {0};
    OPERATE_RET ret = OPRT_OK;

    if (sensor_first_read == 0) {
        ret = sht3x_write_cmd(port, SHT3X_CMD_MEAS_PERI_1_H);
        tal_system_sleep(20);
        if (ret != OPRT_OK) {
            return ret;
        }
        sensor_first_read = 1;
    }

    ret = sht3x_write_cmd(port, SHT3X_CMD_FETCH_DATA);
    if (ret != OPRT_OK) {
        return ret;
    }

    ret = sht3x_read_data(port, 6, buf);
    if (ret != OPRT_OK)
        return ret;

    if ((CRC_ERR == sht3x_check_crc8(buf, 2, buf[2])) || (CRC_ERR == sht3x_check_crc8(buf + 3, 2, buf[5]))) {
        printf("[SHT3x] The received temp_humi data can't pass the CRC8 check.\n");
        return OPRT_CRC32_FAILED;
    }

    *temp = ((uint16_t)buf[0] << 8) | buf[1];
    *humi = ((uint16_t)buf[3] << 8) | buf[4];

    return OPRT_OK;
}

/**
 * @brief Initialize hardware (I2C and SHT3x sensor)
 *
 * @return operation result
 */
static OPERATE_RET hardware_init(void)
{
    OPERATE_RET op_ret = OPRT_OK;
    TUYA_IIC_BASE_CFG_T cfg;

    if (hardware_initialized) {
        return OPRT_OK;
    }

    // Configure I2C pins
    tkl_io_pinmux_config(SHT3X_I2C_SCL_PIN, TUYA_IIC1_SCL);
    tkl_io_pinmux_config(SHT3X_I2C_SDA_PIN, TUYA_IIC1_SDA);

    // Initialize I2C
    cfg.role = TUYA_IIC_MODE_MASTER;
    cfg.speed = TUYA_IIC_BUS_SPEED_100K;
    cfg.addr_width = TUYA_IIC_ADDRESS_7BIT;

    op_ret = tkl_i2c_init(SHT3X_I2C_PORT, &cfg);
    if (OPRT_OK != op_ret) {
        printf("[%s] I2C init fail, err<%d>!\n", temp_humidity_screen.name, op_ret);
        return op_ret;
    }

    hardware_initialized = 1;
    sensor_first_read = 0;
    printf("[%s] Hardware initialized successfully\n", temp_humidity_screen.name);
    
    return OPRT_OK;
}

/**
 * @brief Deinitialize hardware
 */
static void hardware_deinit(void)
{
    if (hardware_initialized) {
        tkl_i2c_deinit(SHT3X_I2C_PORT);
        hardware_initialized = 0;
        sensor_first_read = 0;
        printf("[%s] Hardware deinitialized\n", temp_humidity_screen.name);
    }
}
#endif

/**
 * @brief Timer callback for updating sensor data
 *
 * This function is called periodically to update the temperature and humidity
 * readings and refresh the display.
 *
 * @param timer The timer object
 */
static void update_timer_cb(lv_timer_t *timer)
{
    update_sensor_data();
    update_display();
    update_count++;
    printf("[%s] Data updated (count: %d) - Temp: %.1f°C, Humidity: %.1f%%\n", 
           temp_humidity_screen.name, update_count, current_temperature, current_humidity);
}

/**
 * @brief Keyboard event callback
 *
 * This function handles keyboard events for the temperature humidity screen.
 *
 * @param e The event object
 */
static void keyboard_event_cb(lv_event_t *e)
{
    uint32_t key = lv_event_get_key(e);
    printf("[%s] Keyboard event received: key = %d\n", temp_humidity_screen.name, key);

    switch (key) {
        case KEY_UP:
            printf("UP key pressed\n");
            break;
        case KEY_DOWN:
            printf("DOWN key pressed\n");
            break;
        case KEY_LEFT:
            printf("LEFT key pressed\n");
            break;
        case KEY_RIGHT:
            printf("RIGHT key pressed\n");
            break;
        case KEY_ENTER:
            printf("ENTER key pressed - Manual refresh\n");
            update_sensor_data();
            // update_display();
            break;
        case KEY_ESC:
            printf("ESC key pressed - Exit screen\n");
            screen_back(); // Uncomment when screen navigation is available
            break;
        default:
            printf("Unknown key pressed\n");
            break;
    }
}

/**
 * @brief Update sensor data with real hardware readings or simulated values
 *
 * This function reads temperature and humidity from the SHT3x sensor when hardware
 * is enabled, otherwise uses simulated values for demonstration.
 */
static void update_sensor_data(void)
{
#ifdef ENABLE_LVGL_HARDWARE
    uint16_t temp_raw = 0;
    uint16_t humi_raw = 0;
    OPERATE_RET ret = sht3x_read_temp_humi(SHT3X_I2C_PORT, &temp_raw, &humi_raw);
    
    if (ret == OPRT_OK) {
        // I2C read successful - convert and update values
        current_temperature = (float)temp_raw / 1000.0f;
        current_humidity = (float)humi_raw / 1000.0f;
        
        sensor_connected = 1;
        printf("[%s] Hardware read successful - temp=%.1f°C, humi=%.1f%%\n", 
               temp_humidity_screen.name, current_temperature, current_humidity);
    } else {
        sensor_connected = 0;
        printf("[%s] I2C read failed, err<%d>\n", temp_humidity_screen.name, ret);
    }
#else
    // Simulation mode - generate demo data
    sensor_connected = 1;
    
    // Simulate fluctuating sensor readings
    static float temp_offset = 0.0f;
    static float humidity_offset = 0.0f;
    static int direction = 1;
    
    temp_offset += direction * 0.2f;
    humidity_offset += direction * 0.5f;
    
    if (temp_offset > 2.0f || temp_offset < -2.0f) {
        direction *= -1;
    }
    
    current_temperature = 23.5f + temp_offset;
    current_humidity = 65.0f + humidity_offset;
    
    printf("[%s] Simulation mode - temp=%.1f°C, humi=%.1f%%\n", 
           temp_humidity_screen.name, current_temperature, current_humidity);
#endif
}

/**
 * @brief Update the display with current sensor values
 *
 * This function switches between showing temperature/humidity data or error message.
 */
static void update_display(void)
{
    char time_str[64];
    
#ifdef ENABLE_LVGL_HARDWARE
    if (sensor_connected) {
        // Show temperature and humidity containers, hide error message
        lv_obj_clear_flag(temp_container, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(humidity_container, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(error_label, LV_OBJ_FLAG_HIDDEN);
        
        // Update temperature and humidity values
        char temp_str[32];
        char humidity_str[32];
        snprintf(temp_str, sizeof(temp_str), "%.1f°C", current_temperature);
        snprintf(humidity_str, sizeof(humidity_str), "%.1f%%", current_humidity);
        lv_label_set_text(temp_value_label, temp_str);
        lv_label_set_text(humidity_value_label, humidity_str);
    } else {
        // Hide temperature and humidity containers, show error message
        lv_obj_add_flag(temp_container, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(humidity_container, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(error_label, LV_OBJ_FLAG_HIDDEN);
        
        lv_label_set_text(error_label, "Temperature Humidity Sensor\n Connection Failed !");
    }
#else
    // Simulation mode - always show data
    lv_obj_clear_flag(temp_container, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(humidity_container, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(error_label, LV_OBJ_FLAG_HIDDEN);
    
    char temp_str[32];
    char humidity_str[32];
    snprintf(temp_str, sizeof(temp_str), "%.1f°C", current_temperature);
    snprintf(humidity_str, sizeof(humidity_str), "%.1f%%", current_humidity);
    lv_label_set_text(temp_value_label, temp_str);
    lv_label_set_text(humidity_value_label, humidity_str);
#endif
    
    // Update time string
    snprintf(time_str, sizeof(time_str), "Last Update: %d", update_count);
    lv_label_set_text(update_time_label, time_str);
}

/**
 * @brief Initialize the temperature humidity screen
 *
 * This function creates the temperature humidity screen UI with a modern layout
 * displaying temperature and humidity readings with icons and labels.
 * Also initializes hardware components when ENABLE_LVGL_HARDWARE is defined.
 */
void temp_humidity_screen_init(void)
{
#ifdef ENABLE_LVGL_HARDWARE
    // Initialize hardware first
    OPERATE_RET hw_ret = hardware_init();
    if (hw_ret != OPRT_OK) {
        printf("[%s] Hardware initialization failed\n", temp_humidity_screen.name);
        sensor_connected = 0; // Ensure sensor is marked as disconnected
    }
#else
    // In simulation mode, always mark as connected
    sensor_connected = 1;
#endif

    // Create main screen
    ui_temp_humidity_screen = lv_obj_create(NULL);
    lv_obj_set_size(ui_temp_humidity_screen, AI_PET_SCREEN_WIDTH, AI_PET_SCREEN_HEIGHT);
    lv_obj_set_style_bg_color(ui_temp_humidity_screen, lv_color_white(), 0);
    lv_obj_set_style_pad_all(ui_temp_humidity_screen, 10, 0);
    
    // Create title
    lv_obj_t *title = lv_label_create(ui_temp_humidity_screen);
    lv_label_set_text(title, "Temperature & Humidity");
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 5);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(title, lv_color_black(), 0);
    
    // Create temperature container
    temp_container = lv_obj_create(ui_temp_humidity_screen);
    lv_obj_set_size(temp_container, 180, 60);
    lv_obj_align(temp_container, LV_ALIGN_LEFT_MID, 10, -10);
    lv_obj_set_style_bg_color(temp_container, lv_color_white(), 0);
    lv_obj_set_style_border_width(temp_container, 0, 0);
    lv_obj_set_style_radius(temp_container, 8, 0);
    
    // Temperature icon
    lv_obj_t *temp_icon = lv_label_create(temp_container);
    lv_label_set_text(temp_icon, LV_SYMBOL_LOOP);
    lv_obj_align(temp_icon, LV_ALIGN_LEFT_MID, 10, -10);
    lv_obj_set_style_text_font(temp_icon, &lv_font_montserrat_16, 0);
    
    // Temperature label
    lv_obj_t *temp_label = lv_label_create(temp_container);
    lv_label_set_text(temp_label, "Temperature");
    lv_obj_align(temp_label, LV_ALIGN_LEFT_MID, 40, -10);
    lv_obj_set_style_text_font(temp_label, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(temp_label, lv_color_black(), 0);
    
    // Temperature value
    temp_value_label = lv_label_create(temp_container);
    lv_label_set_text(temp_value_label, "23.5°C");
    lv_obj_align(temp_value_label, LV_ALIGN_LEFT_MID, 40, 10);
    lv_obj_set_style_text_font(temp_value_label, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(temp_value_label, lv_color_black(), 0);
    
    // Create humidity container
    humidity_container = lv_obj_create(ui_temp_humidity_screen);
    lv_obj_set_size(humidity_container, 180, 60);
    lv_obj_align(humidity_container, LV_ALIGN_RIGHT_MID, -10, -10);
    lv_obj_set_style_bg_color(humidity_container, lv_color_white(), 0);
    lv_obj_set_style_border_width(humidity_container, 0, 0);
    lv_obj_set_style_radius(humidity_container, 8, 0);
    
    // Humidity icon
    lv_obj_t *humidity_icon = lv_label_create(humidity_container);
    lv_label_set_text(humidity_icon, LV_SYMBOL_LOOP);
    lv_obj_align(humidity_icon, LV_ALIGN_LEFT_MID, 10, -10);
    lv_obj_set_style_text_font(humidity_icon, &lv_font_montserrat_16, 0);
    
    // Humidity label
    lv_obj_t *humidity_label = lv_label_create(humidity_container);
    lv_label_set_text(humidity_label, "Humidity");
    lv_obj_align(humidity_label, LV_ALIGN_LEFT_MID, 40, -10);
    lv_obj_set_style_text_font(humidity_label, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(humidity_label, lv_color_black(), 0);
    
    // Humidity value
    humidity_value_label = lv_label_create(humidity_container);
    lv_label_set_text(humidity_value_label, "65.0%");
    lv_obj_align(humidity_value_label, LV_ALIGN_LEFT_MID, 40, 10);
    lv_obj_set_style_text_font(humidity_value_label, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(humidity_value_label, lv_color_black(), 0);
    
    // Create error message label (initially hidden)
    error_label = lv_label_create(ui_temp_humidity_screen);
    lv_label_set_text(error_label, "Temperature Humidity Sensor Connection Failed");
    lv_obj_align(error_label, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_text_font(error_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(error_label, lv_color_black(), 0); // Black text
    lv_obj_set_style_text_align(error_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_add_flag(error_label, LV_OBJ_FLAG_HIDDEN); // Initially hidden
    
    // Create update time info
    update_time_label = lv_label_create(ui_temp_humidity_screen);
    lv_label_set_text(update_time_label, "Last Update: 0");
    lv_obj_align(update_time_label, LV_ALIGN_BOTTOM_MID, 0, -5);
    lv_obj_set_style_text_font(update_time_label, &lv_font_montserrat_10, 0);
    lv_obj_set_style_text_color(update_time_label, lv_color_black(), 0);
    
    // Create help text
    lv_obj_t *help_label = lv_label_create(ui_temp_humidity_screen);
    lv_label_set_text(help_label, "ENTER: Refresh | ESC: Exit");
    lv_obj_align(help_label, LV_ALIGN_BOTTOM_MID, 0, -20);
    lv_obj_set_style_text_font(help_label, &lv_font_montserrat_10, 0);
    lv_obj_set_style_text_color(help_label, lv_color_black(), 0);
    
    // Initialize sensor data and display
    update_sensor_data();
    update_display();
    
    // Start update timer
    update_timer = lv_timer_create(update_timer_cb, UPDATE_INTERVAL_MS, NULL);
    
    // Add keyboard event handling
    lv_obj_add_event_cb(ui_temp_humidity_screen, keyboard_event_cb, LV_EVENT_KEY, NULL);
    lv_group_add_obj(lv_group_get_default(), ui_temp_humidity_screen);
    lv_group_focus_obj(ui_temp_humidity_screen);
    
    printf("[%s] Temperature & Humidity screen initialized\n", temp_humidity_screen.name);
}

/**
 * @brief Deinitialize the temperature humidity screen
 *
 * This function cleans up the temperature humidity screen by deleting the UI object,
 * timer, removing the event callback, and cleaning up hardware resources.
 */
void temp_humidity_screen_deinit(void)
{
    if (ui_temp_humidity_screen) {
        printf("[%s] Deinitializing temperature & humidity screen\n", temp_humidity_screen.name);
        lv_obj_remove_event_cb(ui_temp_humidity_screen, keyboard_event_cb);
        lv_group_remove_obj(ui_temp_humidity_screen);
    }
    
    if (update_timer) {
        lv_timer_del(update_timer);
        update_timer = NULL;
    }

#ifdef ENABLE_LVGL_HARDWARE
    // Deinitialize hardware
    hardware_deinit();
#endif
    
    // Reset variables
    update_count = 0;
    current_temperature = 0;
    current_humidity = 0;
    
#ifdef ENABLE_LVGL_HARDWARE
    // Reset hardware status variables
    sensor_connected = 0;
#endif
}
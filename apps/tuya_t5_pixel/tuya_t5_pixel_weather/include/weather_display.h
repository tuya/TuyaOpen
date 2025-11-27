/**
 * @file weather_display.h
 * @brief Weather display engine for pixel screen
 */

#ifndef __WEATHER_DISPLAY_H__
#define __WEATHER_DISPLAY_H__

#include "tal_api.h"
#include "board_pixel_api.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize weather display engine
 * @return OPRT_OK on success, error code on failure
 */
OPERATE_RET weather_display_init(void);

/**
 * @brief Start weather display task
 * @return OPRT_OK on success, error code on failure
 */
OPERATE_RET weather_display_start(void);

/**
 * @brief Stop weather display task
 */
void weather_display_stop(void);

/**
 * @brief Update weather data and trigger display refresh
 */
void weather_display_update(void);

/**
 * @brief Notify display that MQTT is connected (can show weather pages)
 */
void weather_display_mqtt_connected(void);

/**
 * @brief Force WiFi status page to be displayed
 */
void weather_display_show_wifi_status(void);

#ifdef __cplusplus
}
#endif

#endif /* __WEATHER_DISPLAY_H__ */

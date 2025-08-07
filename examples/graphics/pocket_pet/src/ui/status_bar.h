/**
 * @file status_bar.h
 * Status Bar Component for AI Pocket Pet
 */

#ifndef STATUS_BAR_H
#define STATUS_BAR_H

#ifdef __cplusplus
extern "C" {
#endif

/*********************
 *      INCLUDES
 *********************/
#include "../lvgl/lvgl.h"
#include <stdbool.h>

/**********************
 * GLOBAL PROTOTYPES
 **********************/

/**
 * Create the status bar with WiFi, network, and battery icons
 * @param parent Parent object to attach the status bar to
 * @return Pointer to the created status bar object
 */
lv_obj_t* status_bar_create(lv_obj_t *parent);

/**
 * Set WiFi signal strength
 * @param strength 0 = off, 1-3 = bars, 4 = find, 5 = add
 */
void status_bar_set_wifi_strength(uint8_t strength);

/**
 * Set cellular signal strength and connection status
 * @param strength 0 = off, 1-3 = bars, 4 = no internet
 * @param connected Whether cellular is connected to internet
 */
void status_bar_set_cellular_status(uint8_t strength, bool connected);

/**
 * Set battery level and charging status
 * @param level Battery level (0-6, where 0 = empty, 5 = 5 bars, 6 = full)
 * @param charging Whether battery is charging
 */
void status_bar_set_battery_status(uint8_t level, bool charging);

/**
 * Get current WiFi signal strength
 * @return Current WiFi signal strength (0-5)
 */
uint8_t status_bar_get_wifi_strength(void);

/**
 * Get current cellular signal strength
 * @return Current cellular signal strength (0-4)
 */
uint8_t status_bar_get_cellular_strength(void);

/**
 * Get current cellular connection status
 * @return Whether cellular is connected to internet
 */
bool status_bar_get_cellular_connected(void);

/**
 * Get current battery level
 * @return Current battery level (0-6)
 */
uint8_t status_bar_get_battery_level(void);

/**
 * Get current battery charging status
 * @return Whether battery is charging
 */
bool status_bar_get_battery_charging(void);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* STATUS_BAR_H */

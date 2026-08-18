/**
 * @file board_config.h
 * @brief NiceMCU-T5-DEV pin map (button + optional I2S audio add-on)
 * @version 0.2
 * @date 2026-08-12
 * @copyright Copyright (c) Tuya Inc. All Rights Reserved.
 *
 * Audio uses ONE I2S port (I2S1 / group1) in duplex — share BCLK/WS:
 *
 *   INMP441 (I2S mic)                 MAX98357 (I2S amp)
 *   ------------------                ------------------
 *   VDD  -> 3V3                       VIN  -> 3V3 or 5V (GND common)
 *   GND  -> GND                       GND  -> GND
 *   SCK  -> P40 (shared BCLK)         BCLK -> P40
 *   WS   -> P41 (shared LRCK)         LRC  -> P41
 *   SD   -> P42 (I2S1_DIN)            DIN  -> P43 (I2S1_DOUT)
 *   L/R  -> GND (left channel)        SD   -> 3V3 or BOARD_SPK_SD_PIN
 *
 * Do NOT wire the mic to P6/P7/P8 anymore: I2S0-only RX never got DMA data
 * on this board/driver path. Beken duplex expects TX+RX on the same I2S id.
 */
#ifndef __BOARD_CONFIG_H__
#define __BOARD_CONFIG_H__

#include "tuya_cloud_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* MAX98357 SD: set to a GPIO to software-mute, or TUYA_GPIO_NUM_MAX if tied to 3V3 */
#define BOARD_SPK_SD_PIN           TUYA_GPIO_NUM_MAX
#define BOARD_SPK_SD_ACTIVE_LV     TUYA_GPIO_LEVEL_HIGH

/* User button on P0, active-low (press to GND) */
#define BOARD_BUTTON_PIN           TUYA_GPIO_NUM_0
#define BOARD_BUTTON_ACTIVE_LV     TUYA_GPIO_LEVEL_LOW

#ifdef __cplusplus
}
#endif

#endif /* __BOARD_CONFIG_H__ */

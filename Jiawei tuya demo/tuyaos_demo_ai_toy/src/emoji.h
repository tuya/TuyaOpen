#include <stdint.h>
#include "lvgl.h"
#include "hal/lv_hal_disp.h"
#ifndef __EMOJI_H__
#define __EMOJI_H__


void ssh1106_write(uint8_t ctrl, const uint8_t *data, uint16_t len);
void ssh1106_refresh(const uint8_t *buf128x64);
void my_flush_cb(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_p);
void emoji_play_frames35(uint8_t play_times);
void emoji_oled_init(void);
extern const uint8_t frames35[][1024];



#endif
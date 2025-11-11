#ifndef _UI_HOMEPAGE_H
#define _UI_HOMEPAGE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "../../ui.h"

typedef struct {
    uint16_t witdh;                 // screen width  
    uint16_t height;                // screen height
    uint8_t container_total_pages;  // total pages of app-container
    uint8_t app_container_index;    // present app-container index
    bool show_dropdown;             // show dropdown or not
    bool scroll_busy;               // avoid scroll too fast, and btns click event happen at the same time
}ui_desktop_data_t;

void ui_HomePage_init(void);
void ui_HomePage_deinit(void);

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif
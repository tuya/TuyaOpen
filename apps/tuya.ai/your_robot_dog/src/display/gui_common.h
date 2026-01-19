/**
 * @file gui_common.h
 * @author www.tuya.com
 * @version 0.1
 * @date 2025-02-07
 *
 * @copyright Copyright (c) tuya.inc 2025
 *
 */

 #ifndef __TUYA_GUI_COMMON__
 #define __TUYA_GUI_COMMON__

#include "lvgl.h"
#include "font_awesome_symbols.h"

#ifdef __cplusplus
extern "C" {
#endif 

#define GUI_SUPPORT_LANG_NUM        2

typedef struct {
    char    *text[GUI_SUPPORT_LANG_NUM];
} gui_lang_desc_t;

typedef struct {
    const void  *source;
    const char  *desc;
} gui_emotion_t;

typedef enum {
    GUI_STAT_IDLE,
    GUI_STAT_LISTEN,
    GUI_STAT_UPLOAD,
    GUI_STAT_THINK,
    GUI_STAT_SPEAK,
    GUI_STAT_PROV,
    GUI_STAT_INIT,
    GUI_STAT_CONN,
    GUI_STAT_MAX,
} gui_stat_t;


void  gui_lang_set(uint8_t lang);
int   gui_lang_get(void);
int   gui_emotion_find(gui_emotion_t *emotion, uint8_t count, char *desc);
OPERATE_RET gui_img_load_psram(char *filename, lv_img_dsc_t *img_dst);

/* gui_status_bar 
---------------------------
mode   [gif]stat   vol cell
---------------------------
*/
char *gui_wifi_level_get(uint8_t net);
char *gui_battery_level_get(uint8_t battery);
int   gui_status_desc_get(uint8_t stat, const char **text, const lv_img_dsc_t **gif);


#ifdef __cplusplus
}
#endif

#endif /* __TUYA_GUI_COMMON__ */


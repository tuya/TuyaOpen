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
#include "tuya_slist.h"

#ifdef __cplusplus
extern "C" {
#endif 

#define GUI_SUPPORT_LANG_NUM        2

typedef struct {
    char    *text[GUI_SUPPORT_LANG_NUM];
} gui_lang_desc_t;


typedef struct {
    SLIST_HEAD  node;
    uint32_t    timeindex;
    char        data[33 + 3];
} gui_text_disp_t;

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
char *gui_volum_level_get(uint8_t volum);
char *gui_battery_level_get(uint8_t battery);
char *gui_mode_desc_get(uint8_t mode);
int   gui_status_desc_get(uint8_t stat, const char **text, const lv_img_dsc_t **gif);

int gui_txet_disp_init(lv_obj_t *obj, void *priv_data, uint16_t width, uint16_t high);
int gui_txet_disp_start(void);
int gui_txet_disp_stop(void);
int gui_text_disp_pop(gui_text_disp_t **text);
int gui_text_disp_push(uint8_t *data, int len);
int gui_text_disp_free(void);
int gui_txet_disp_set_cb(void (*text_disp_cb)(void *obj, char *text, int pos, void *priv_data));
int gui_txet_disp_set_windows(lv_obj_t *obj, void *priv_data, uint16_t width, uint16_t high);


#ifdef __cplusplus
}
#endif

#endif /* __TUYA_GUI_COMMON__ */


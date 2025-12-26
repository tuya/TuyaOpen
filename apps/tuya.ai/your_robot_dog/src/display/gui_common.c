//! Common GUI helpers
#include "gui_common.h"
#include "tuya_cloud_types.h"

#include <stdio.h>

#include "tkl_fs.h"

#define GUI_LOG(...)        bk_printf(__VA_ARGS__)

/* Centralize PSRAM alloc/free declarations to avoid implicit declarations in multiple places */
extern void *tkl_system_psram_malloc(CONST SIZE_T size);
extern void  tkl_system_psram_free(void *ptr);

#ifndef TY_AI_DEFAULT_LANG
#define TY_AI_DEFAULT_LANG 0
#endif
static int s_gui_lang = TY_AI_DEFAULT_LANG;

//! About common GUI handling
/* 
    1. Device status
    2. Chat mode
    3. Emotion lookup
    4. Image loading
    5. Battery icon
    6. Volume icon
    7. Network icon
*/

//! status gif
LV_IMG_DECLARE(Initializing);
LV_IMG_DECLARE(Provisioning);
LV_IMG_DECLARE(Connecting);
LV_IMG_DECLARE(Listening);
LV_IMG_DECLARE(Uploading);
LV_IMG_DECLARE(Thinking);
LV_IMG_DECLARE(Speaking);
LV_IMG_DECLARE(Waiting);

static const gui_lang_desc_t gui_mode_desc[] = {
    { {"长按", "LongPress"} },
    { {"按键", "ShortPress"} },
    { {"唤醒", "Keyword"} },
    { {"随意", "Free"} },
};

static const gui_lang_desc_t gui_stat_desc[] = {
    { {"配网中...",      "Provisioning"} },
    { {"   初始化...",      "Initializing"} },
    { {"连接中...",      "Connecting"} },
    { {"待命",           "Standby"} },
    { {"聆听中...",      "Listening"} },
    { {"上传中...",      "Uploading"} },
    { {"思考中...",      "Thinking"} },
    { {"说话中...",      "Speaking"} },
};

typedef struct {
    gui_stat_t          index[GUI_STAT_MAX];
    const lv_img_dsc_t *gif[GUI_STAT_MAX];
    const gui_lang_desc_t *mode;
    const gui_lang_desc_t *stat;
} gui_status_desc_t;

static const gui_status_desc_t s_gui_status_desc = {
    .gif   = {&Provisioning, &Initializing, &Connecting, &Waiting, &Listening, &Uploading, &Thinking, &Speaking},
    .index = {GUI_STAT_PROV, GUI_STAT_INIT, GUI_STAT_CONN, GUI_STAT_IDLE, GUI_STAT_LISTEN, GUI_STAT_UPLOAD, GUI_STAT_THINK, GUI_STAT_SPEAK},
    .stat  = gui_stat_desc,
    .mode  = gui_mode_desc,
};

char *gui_mode_desc_get(uint8_t mode)
{
    return (char *)s_gui_status_desc.mode[mode].text[s_gui_lang];
}

int gui_status_desc_get(uint8_t stat, const char **text, const lv_img_dsc_t **gif)
{
    int i = 0;

    GUI_LOG("gui stat %d\r\n", stat);

    for (i = 0; i < GUI_STAT_MAX; i++) {
        if (stat == s_gui_status_desc.index[i]) {
            break;
        }
    }

    if (i == GUI_STAT_MAX) {
        return OPRT_NOT_FOUND;
    }
    
    if (text) {
        *text = (const char *)s_gui_status_desc.stat[i].text[s_gui_lang];
    }

    if (gif) {
        *gif  = s_gui_status_desc.gif[i];
    }

    return OPRT_OK;
}



void gui_lang_set(uint8_t lang)
{
    if (lang != s_gui_lang) {
        s_gui_lang = lang;
        GUI_LOG("gui lang set %d", lang);
    }
}


int gui_lang_get(void)
{
    return s_gui_lang;
}


//! Load image into PSRAM (required)
OPERATE_RET gui_img_load_psram(char *filename, lv_img_dsc_t *img_dst)
{
    OPERATE_RET ret = OPRT_COM_ERROR;
    TUYA_FILE file = tkl_fopen(filename, "r");
    if (!file) {
        LV_LOG_ERROR("Failed to open file: %s\n", filename);
        return ret;
    }

    if (0 != tkl_fseek(file, 0, SEEK_END)) {
        LV_LOG_ERROR("Failed to seek file end: %s\n", filename);
        tkl_fclose(file);
        return ret;
    }

    int64_t file_size64 = tkl_ftell(file);
    if (file_size64 <= 0 || file_size64 > (int64_t)UINT32_MAX) {
        LV_LOG_ERROR("Invalid file size: %s size:%lld\n", filename, (long long)file_size64);
        tkl_fclose(file);
        return ret;
    }

    if (0 != tkl_fseek(file, 0, SEEK_SET)) {
        LV_LOG_ERROR("Failed to seek file start: %s\n", filename);
        tkl_fclose(file);
        return ret;
    }

    uint32_t file_size = (uint32_t)file_size64;

    // Allocate memory to store file content
    uint8_t *buffer = tkl_system_psram_malloc(file_size);
    if (buffer == NULL) {
        LV_LOG_ERROR("Memory allocation failed\n");
        tkl_fclose(file);
        return ret;
    }

    int bytes_read = tkl_fread(buffer, (int)file_size, file);
    if (bytes_read != (int)file_size) {
        LV_LOG_ERROR("Failed to read file: %s read:%d expect:%u\n", filename, bytes_read, file_size);
        tkl_system_psram_free(buffer);
        tkl_fclose(file);
        return ret;
    }

    LV_LOG_WARN("gif file '%s' load successful !\r\n", filename);
    img_dst->data = (const uint8_t *)buffer;
    img_dst->data_size = file_size;
    ret = OPRT_OK;

    tkl_fclose(file);
    
    return ret;
}

int gui_emotion_find(gui_emotion_t *emotion, uint8_t count, char *desc)
{
    uint8_t which = 0;

    int i = 0;
    for (i = 0; i < count; i++) {
        if (0 == strcasecmp(emotion[i].desc, desc)) {
            GUI_LOG("find emotion %s\r\n", emotion[i].desc);
            which = i;
            break;
        }
    }

    return which;
}


char *gui_battery_level_get(uint8_t battery)
{
    GUI_LOG("battery_level %d\r\n", battery);

    if (battery >= 100) {
        return FONT_AWESOME_BATTERY_FULL;
    } else if (battery >= 70 && battery < 100) {
        return FONT_AWESOME_BATTERY_3;
    } else if (battery >= 40 && battery < 70) {
        return FONT_AWESOME_BATTERY_2;
    } else if (battery > 10 && battery < 40) {
        return FONT_AWESOME_BATTERY_1;
    }

    return FONT_AWESOME_BATTERY_EMPTY;
}

char *gui_volum_level_get(uint8_t volum)
{
    GUI_LOG("volum_level %d\r\n", volum);

    if (volum >= 70 && volum <= 100) {
        return FONT_AWESOME_VOLUME_HIGH;
    } else if (volum >= 40 && volum < 70) {
        return  FONT_AWESOME_VOLUME_MEDIUM;
    } else if (volum > 10 && volum < 40){
        return FONT_AWESOME_VOLUME_LOW;
    }

    return FONT_AWESOME_VOLUME_MUTE;
}

char *gui_wifi_level_get(uint8_t net)
{
    return net ? FONT_AWESOME_WIFI : FONT_AWESOME_WIFI_OFF;
}

typedef struct {
    uint8_t     init;
    uint8_t     start;
    uint8_t     last_delay;
    uint16_t    max_chars;
    void        *obj;
    void        *priv_data;
    void        (*text_disp_cb)(void *obj, char *text, int pos, void *priv_data);
    SLIST_HEAD  head;
} gui_text_mgr_t;


gui_text_mgr_t *gui_text_disp_mgr_get(void)
{
    static gui_text_mgr_t  mgr;

    return &mgr;
}


static void gui_text_default_disp_cb(void *obj, char *text, int pos, void *priv_data)
{
    if (pos) {
        lv_label_ins_text(obj, pos, text);
    } else {
        lv_label_set_text(obj, text);
    }
}

int gui_text_disp_pop(gui_text_disp_t **text)
{
    gui_text_mgr_t *mgr = gui_text_disp_mgr_get();
    SLIST_HEAD *pos = mgr->head.next;
    if (pos) {
        // bk_printf("gui_text_disp_pop %08x\n", pos);
        tuya_slist_del(&mgr->head, pos);
       *text = (gui_text_disp_t *)pos;
        return OPRT_OK;
    }
 
    return OPRT_NOT_FOUND;
}

static void gui_text_disp_show(gui_text_mgr_t *mgr, gui_text_disp_t *text)
{
    uint32_t offset = _lv_text_get_encoded_length(text->data);
    uint32_t pos    = _lv_text_get_encoded_length(lv_label_get_text(mgr->obj));

    // bk_printf("mgr->obj %08x, offset %d, pos %d,  txt %s\n", mgr->obj, offset, pos, text->data);
    if (mgr->max_chars && (mgr->last_delay || ((pos + offset) > mgr->max_chars))) {
        mgr->last_delay = false;
        pos  = 0;
    } 

    if (mgr->text_disp_cb) {
        mgr->text_disp_cb(mgr->obj, text->data, pos, mgr->priv_data); 
    }
}

//! TODO: interrupt/state display
static void gui_text_disp_timer(struct _lv_timer_t * timer)
{
    gui_text_mgr_t  *mgr  = (gui_text_mgr_t *)timer->user_data;
    gui_text_disp_t *text = NULL;

    if (!mgr->start) {
        lv_timer_del(timer);
        return;
    }

    //! TODO need more
    if (OPRT_OK != gui_text_disp_pop(&text)) {
        lv_timer_set_period(timer, 100);
        return;
    }

    //! TODO: composite display
    gui_text_disp_show(mgr, text);

    if (text->timeindex) {
        lv_timer_set_period(timer, text->timeindex);
    }

    tkl_system_psram_free(text);
}


int gui_text_disp_push(uint8_t *data, int len)
{
    gui_text_mgr_t *mgr = gui_text_disp_mgr_get();

    // TODO: msg discard

    gui_text_disp_t *text = tkl_system_psram_malloc(sizeof(gui_text_disp_t));
    if (NULL == text) {
        return OPRT_MALLOC_FAILED;
    }

    uint16_t text_len =  data[0] << 8 | data[1];
    if (text_len > sizeof(text->data) - 1) {
        bk_printf("txet len has cut %d-%d=%d", text_len, sizeof(text->data) - 1, text_len - sizeof(text->data) - 1);
        text_len = sizeof(text->data) - 1;
    }
    text->timeindex = data[2] << 24 | data[3] << 16 | data[4] << 8 | data[5];
    memcpy(text->data, &data[6], text_len);
    text->data[text_len] = 0;
    tuya_init_slist_node(&text->node);  
    tuya_slist_add_tail(&mgr->head, &text->node);
    // bk_printf("gui_text_disp_push %08x, text %s, timeindex %d\n", text, text->data, text->timeindex);

    return OPRT_OK;
}

static uint16_t gui_txet_disp_area_cacl(lv_obj_t *obj, uint16_t width, uint16_t high) 
{
    const lv_font_t *font = lv_obj_get_style_text_font(obj, LV_PART_MAIN);

    uint32_t unicode_char = 0;
    if (!gui_lang_get()) {
        unicode_char = _lv_text_encoded_next("中", &unicode_char); 
    } else {
        unicode_char = _lv_text_encoded_next("A", &unicode_char); 
    }

    //! Font size
    lv_coord_t font_width  = lv_font_get_glyph_width(font, unicode_char, 0);
    lv_coord_t font_height = lv_font_get_line_height(font);
    font_width = font_width ? font_width : font_height;

    //! Effective area
    width -= (lv_obj_get_style_pad_left(obj, LV_PART_MAIN) + lv_obj_get_style_pad_right(obj, LV_PART_MAIN));
    high  -= (lv_obj_get_style_pad_top(obj, LV_PART_MAIN)  + lv_obj_get_style_pad_bottom(obj, LV_PART_MAIN));
    
    // Calculate max lines (with 90% safety margin)
    uint16_t max_lines = (high * 0.9) / font_height;
    if (max_lines < 1) max_lines = 1;
    
    // Calculate max characters per line
    uint16_t chars_per_line = (width * 0.9) / font_width;
    if(chars_per_line < 1) chars_per_line = 1;
    
    // Calculate max character capacity
    uint16_t max_chars = max_lines * chars_per_line;

    bk_printf("gui text width %d, high %d, font_width %d, font_height %d, max_lines %d, chars_per_line %d, max_chars %d\n", 
        width, high, font_width, font_height, max_lines, chars_per_line, max_chars);
    
    return max_chars;
}

int gui_txet_disp_init(lv_obj_t *obj, void *priv_data, uint16_t width, uint16_t high) 
{
    gui_text_mgr_t *mgr = gui_text_disp_mgr_get();

    if (mgr->init) {
        return OPRT_OK;
    }

    mgr->start        = 0;
    mgr->last_delay   = 0;
    mgr->max_chars    = 0;
    mgr->text_disp_cb = gui_text_default_disp_cb;
    mgr->obj        = obj;
    mgr->priv_data  = priv_data;
    tuya_init_slist_node(&mgr->head);

    if (obj && width && high) {
        mgr->max_chars = gui_txet_disp_area_cacl(obj, width, high);
    }

    mgr->init = true;
    bk_printf("gui text disp init\n");

    return OPRT_OK;
}

int gui_txet_disp_set_cb(void (*text_disp_cb)(void *obj, char *text, int pos, void *priv_data))
{
    gui_text_mgr_t *mgr = gui_text_disp_mgr_get();

    if (NULL == text_disp_cb) {
        return OPRT_INVALID_PARM;
    }

    mgr->text_disp_cb = text_disp_cb;

    return OPRT_OK;
}

int gui_txet_disp_set_windows(lv_obj_t *obj, void *priv_data, uint16_t width, uint16_t high)
{
    gui_text_mgr_t *mgr   = gui_text_disp_mgr_get();
    mgr->obj        = obj;
    mgr->priv_data  = priv_data;

    if (obj && width && high) {
        mgr->max_chars = gui_txet_disp_area_cacl(obj, width, high);
    }

    return OPRT_OK;
}


int gui_txet_disp_start(void)
{
    gui_text_mgr_t  *mgr  = gui_text_disp_mgr_get();
    gui_text_disp_t *text = NULL;

    bk_printf("gui text disp start\n");
    mgr->start      = true;
    //! TODO need more
    if (OPRT_OK != gui_text_disp_pop(&text)) {
        mgr->last_delay = true;
        bk_printf("gui text pop delay\n");
        lv_timer_create(gui_text_disp_timer, 100, mgr);
        return OPRT_NOT_FOUND;
    }

    mgr->last_delay = false;
    if (mgr->text_disp_cb) {
        mgr->text_disp_cb(mgr->obj, text->data, 0, mgr->priv_data);
    }
    lv_timer_create(gui_text_disp_timer, text->timeindex, mgr);
    tkl_system_psram_free(text);

    return OPRT_OK;
}

int gui_text_disp_free(void)
{
    gui_text_mgr_t *mgr = gui_text_disp_mgr_get();

    SLIST_HEAD            *pos   = NULL;
    gui_text_disp_t       *text  = NULL;

    SLIST_FOR_EACH_ENTRY(text, gui_text_disp_t, pos, (&mgr->head), node) {
        tkl_system_psram_free(text);
    }

    tuya_init_slist_node(&mgr->head);

    return OPRT_OK;
}

int gui_txet_disp_stop(void)
{
    gui_text_mgr_t *mgr   = gui_text_disp_mgr_get();
    gui_text_disp_t *text = NULL;

    if (mgr->start) {
        mgr->start = false;
        mgr->last_delay = false;
        //! Flush all remaining items
        bk_printf("gui text disp all\n");
        while (0 == mgr->max_chars && OPRT_OK == gui_text_disp_pop(&text)) {
            gui_text_disp_show(mgr, text);
            tkl_system_psram_free(text);
        }
        gui_text_disp_free();
        bk_printf("gui text disp stop\n");
    }

    return OPRT_OK;
}

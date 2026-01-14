//! Common GUI helpers
#include "gui_common.h"
#include "tuya_cloud_types.h"

#include "lang_config.h"

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

static const gui_lang_desc_t gui_stat_desc[] = {
    { {PROVISIONING,      "Provisioning"} },
    { {INITIALIZING,      "Initializing"} },
    { {CONNECTING,        "Connecting"} },
    { {STANDBY,           "Standby"} },
    { {LISTENING,         "Listening"} },
    { {UPLOADING,         "Uploading"} },
    { {THINKING,          "Thinking"} },
    { {SPEAKING,          "Speaking"} },
};

typedef struct {
    gui_stat_t          index[GUI_STAT_MAX];
    const lv_img_dsc_t *gif[GUI_STAT_MAX];
    const gui_lang_desc_t *stat;
} gui_status_desc_t;

static const gui_status_desc_t s_gui_status_desc = {
    .gif   = {&Provisioning, &Initializing, &Connecting, &Waiting, &Listening, &Uploading, &Thinking, &Speaking},
    .index = {GUI_STAT_PROV, GUI_STAT_INIT, GUI_STAT_CONN, GUI_STAT_IDLE, GUI_STAT_LISTEN, GUI_STAT_UPLOAD, GUI_STAT_THINK, GUI_STAT_SPEAK},
    .stat  = gui_stat_desc,
};

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

char *gui_wifi_level_get(uint8_t net)
{
    return net ? FONT_AWESOME_WIFI : FONT_AWESOME_WIFI_OFF;
}

//！公共部分处理
#include "gui_common.h"

#define GUI_LOG(...)        bk_printf(__VA_ARGS__)

static int s_gui_lang = TY_AI_DEFAULT_LANG;

//！ 关于gui公共部分处理
/* 
    1. 设备状态
    2. 聊天模式
    3. 表情查找
    4. 图片加载
    5. 电量图标
    6. 声音图标
    7. 网络图标
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
    {"长按", "LongPress"},
    {"按键", "ShortPress"},
    {"唤醒", "Keyword"},
    {"随意", "Free"},
};

static const gui_lang_desc_t gui_stat_desc[] = {
    {"配网中...",      "Provisioning"},
    {"初始化...",      "Initializing"},
    {"连接中...",      "Connecting"},
    {"待命",           "Standby"},
    {"聆听中...",      "Listening"},
    {"上传中...",      "Uploading"},
    {"思考中...",      "Thinking"},
    {"说话中...",      "Speaking"},
};

typedef struct {
    gui_stat_t          index[GUI_STAT_MAX];
    lv_img_dsc_t       *gif[GUI_STAT_MAX];
    gui_lang_desc_t    *mode;
    gui_lang_desc_t    *stat;
} gui_status_desc_t;

static const gui_status_desc_t s_gui_status_desc = {
    .gif   = {&Provisioning, &Initializing, &Connecting, &Waiting, &Listening, &Uploading, &Thinking, &Speaking},
    .index = {GUI_STAT_PROV, GUI_STAT_INIT, GUI_STAT_CONN, GUI_STAT_IDLE, GUI_STAT_LISTEN, GUI_STAT_UPLOAD, GUI_STAT_THINK, GUI_STAT_SPEAK},
    .stat  = gui_stat_desc,
    .mode  = gui_mode_desc,
};

char *gui_mode_desc_get(uint8_t mode)
{
    return s_gui_status_desc.mode[mode].text[s_gui_lang];
}

int gui_status_desc_get(uint8_t stat,  void **text, void **gif)
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
        *text = s_gui_status_desc.stat[i].text[s_gui_lang];
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


//! 加载图片到psram，必须加载
OPERATE_RET gui_img_load_psram(char *filename, lv_img_dsc_t *img_dst)
{
    OPERATE_RET ret = OPRT_COM_ERROR;
    lv_fs_file_t file;
    lv_fs_res_t  res;

    res = lv_fs_open(&file, filename, LV_FS_MODE_RD);
    if (res != LV_FS_RES_OK) {
        LV_LOG_ERROR("Failed to open file: %s\n", filename);
        return ret;
    }

    lv_fs_seek(&file, 0, LV_FS_SEEK_END);
    uint32_t file_size;
    lv_fs_tell(&file, &file_size);
    lv_fs_seek(&file, 0, LV_FS_SEEK_SET);

    // 分配内存以存储文件内容
    uint8_t *buffer = tkl_system_psram_malloc(file_size);
    if (buffer == NULL) {
        LV_LOG_ERROR("Memory allocation failed\n");
        lv_fs_close(&file);
        return ret;
    }

    uint32_t bytes_read;
    res = lv_fs_read(&file, buffer, file_size, &bytes_read);
    if (res != LV_FS_RES_OK || bytes_read != file_size) {
        LV_LOG_ERROR("Failed to read file: %s\n", filename);
        tkl_system_psram_free(buffer);
    } else {
        LV_LOG_WARN("gif file '%s' load successful !\r\n", filename);
        img_dst->data = buffer;
        img_dst->data_size = file_size;
        ret = OPRT_OK;
    }
    lv_fs_close(&file);
    
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

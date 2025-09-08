#include "tuya_cloud_types.h"

#if defined(ENABLE_GUI_EMOJI) && (ENABLE_GUI_EMOJI == 1)

#include "ui_display.h"
#include "lvgl.h"

typedef struct {
    const void  *data;
    const char  *text;
} gif_emotion_t;

static lv_obj_t *s_gif = NULL;
static lv_timer_t *s_emmo_timer = NULL;  // Expression switching timer
static uint8_t s_current_index = 0; // Current expression index

#define EMMO_GIF_W           240
#define EMMO_GIF_H           240
#define EMMO_CHANGE_INTERVAL (10*1000) // Expression switching interval

LV_IMG_DECLARE(blink240);

/*TuyaOpen is stuck, I hit a stub, added an expression, added an expression, 
converted gif to c, put it in the emmo directory, and then added it to this array gif_emotionh */
static const gif_emotion_t gif_emotion[] = {
    {&blink240,    "blink" },
};

static uint8_t __emotion_get(char *emotion)
{
    uint8_t which = 0;

    int i = 0;
    for (i = 0; i < sizeof(gif_emotion)/sizeof(gif_emotion[0]); i++) {
        if (0 == strcasecmp(gif_emotion[i].text, emotion)) {
            which = i;
            break;
        }
    }

    return which;
}

static void __emotion_flush(char *emotion)
{
    uint8_t index = 0;

    index = __emotion_get(emotion);

    lv_gif_set_src(s_gif, gif_emotion[index].data);
}

static void __emotion_timer_cb(lv_timer_t *timer)
{
    s_current_index = (s_current_index + 1) % (sizeof(gif_emotion) / sizeof(gif_emotion[0]));
    
    lv_gif_set_src(s_gif, gif_emotion[s_current_index].data);
}

// --- UI Init ---
int ui_init(UI_FONT_T *ui_font)
{
    lv_obj_t * obj = lv_obj_create(lv_scr_act());
    lv_obj_set_size(obj, EMMO_GIF_W, EMMO_GIF_H);
    lv_obj_set_style_pad_all(obj, 0, 0);
    lv_obj_set_style_border_width(obj, 0, 0);

    s_gif = lv_gif_create(obj);
    lv_obj_set_size(s_gif, EMMO_GIF_W, EMMO_GIF_H);

    __emotion_flush("blink");
    
    // Create expression switching timer
    s_emmo_timer = lv_timer_create(__emotion_timer_cb, EMMO_CHANGE_INTERVAL, NULL);
    
    return 0;
}

void ui_set_user_msg(const char *text) {}
void ui_set_assistant_msg(const char *text) {}
void ui_set_system_msg(const char *text) {}
void ui_set_emotion(const char *emotion) {}
void ui_set_status(const char *status) {}
void ui_set_notification(const char *notification) {}
void ui_set_network(char *wifi_icon) {}
void ui_set_chat_mode(const char *chat_mode) {}
void ui_set_status_bar_pad(int32_t value) {}

#endif

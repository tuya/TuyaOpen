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
static bool s_auto_cycle = true; // Auto cycle through expressions

#define EMMO_GIF_W           160
#define EMMO_GIF_H           80
#define EMMO_CHANGE_INTERVAL (5*1000) // Expression switching interval (5 seconds)

// Declare all emoji GIF animations (optimized, faster animations + fun expressions)
LV_IMG_DECLARE(happy);
LV_IMG_DECLARE(sad);
LV_IMG_DECLARE(anger);
LV_IMG_DECLARE(surprise);
LV_IMG_DECLARE(sleep);
LV_IMG_DECLARE(wakeup);
LV_IMG_DECLARE(left);
LV_IMG_DECLARE(right);
LV_IMG_DECLARE(center);
// Fun expressions
LV_IMG_DECLARE(wink);
LV_IMG_DECLARE(heart_eyes);
LV_IMG_DECLARE(rolling);
LV_IMG_DECLARE(zigzag);
LV_IMG_DECLARE(rainbow);

/* Desk-Emoji inspired expressions converted to LVGL GIF animations
   Based on geometric eye system with various emotional states
   Optimized for faster animation speed (50ms duration)
   Includes 5 fun new expressions for enhanced interactivity */
static const gif_emotion_t gif_emotion[] = {
    // Basic emotions
    {&happy,    "happy" },
    {&sad,      "sad" },
    {&anger,    "anger" },
    {&surprise, "surprise" },
    {&sleep,    "sleep" },
    {&wakeup,   "wakeup" },
    {&left,     "left" },
    {&right,    "right" },
    {&center,   "center" },
    // Fun expressions
    {&wink,     "wink" },
    {&heart_eyes, "heart_eyes" },
    {&rolling,  "rolling" },
    {&zigzag,   "zigzag" },
    {&rainbow,  "rainbow" },
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
    
    // Update current index and pause auto cycle temporarily
    s_current_index = index;
    s_auto_cycle = false;
    
    lv_gif_set_src(s_gif, gif_emotion[index].data);
    
    // Resume auto cycle after a delay (restart timer)
    if (s_emmo_timer) {
        lv_timer_reset(s_emmo_timer);
    }
}

static void __emotion_timer_cb(lv_timer_t *timer)
{
    // Re-enable auto cycle after manual switch
    s_auto_cycle = true;
    
    // Switch to next expression
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

    __emotion_flush("happy");
    
    // Create expression switching timer
    s_emmo_timer = lv_timer_create(__emotion_timer_cb, EMMO_CHANGE_INTERVAL, NULL);
    
    return 0;
}

void ui_set_user_msg(const char *text) {}
void ui_set_assistant_msg(const char *text) {}
void ui_set_system_msg(const char *text) {}

void ui_set_emotion(const char *emotion) {
    if (emotion != NULL && s_gif != NULL) {
        __emotion_flush((char*)emotion);
    }
}

// Set emotion by mood type (simplified mapping)
void ui_set_emotion_by_mood(const char *mood) {
    if (mood == NULL) return;
    
    // Direct mapping - use mood string as emotion name if it exists
    // This allows direct use of emotion names like "happy", "sad", etc.
    ui_set_emotion(mood);
}

void ui_set_status(const char *status) {}
void ui_set_notification(const char *notification) {}
void ui_set_network(char *wifi_icon) {}
void ui_set_chat_mode(const char *chat_mode) {}
void ui_set_status_bar_pad(int32_t value) {}

#endif

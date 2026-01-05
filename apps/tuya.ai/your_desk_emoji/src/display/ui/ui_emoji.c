#include "tuya_cloud_types.h"

#if defined(ENABLE_GUI_EMOJI) && (ENABLE_GUI_EMOJI == 1)

#include "ui_display.h"
#include "lvgl.h"
#include "tal_log.h"

typedef struct {
    const void  *data;
    const char  *text;
} gif_emotion_t;

static lv_obj_t *s_gif = NULL;
static lv_obj_t *s_container = NULL;  // Container object for emoji UI
static lv_timer_t *s_emmo_timer = NULL;  // Expression switching timer
static uint8_t s_current_index = 0; // Current expression index
static bool s_auto_cycle = true; // Auto cycle through expressions
static uint8_t s_rotation_count = 0; // Count of rotations completed
static uint8_t s_total_emotions = 0; // Total number of emotions

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

// Cloud emotion to local emotion mapping
static const char* __map_cloud_emotion(const char *cloud_emotion)
{
    if (cloud_emotion == NULL) return "happy";
    
    // Direct mapping for cloud emotions
    if (strcasecmp(cloud_emotion, "HAPPY") == 0) return "happy";
    if (strcasecmp(cloud_emotion, "SAD") == 0) return "sad";
    if (strcasecmp(cloud_emotion, "ANGRY") == 0) return "anger";
    if (strcasecmp(cloud_emotion, "SURPRISE") == 0) return "surprise";
    if (strcasecmp(cloud_emotion, "NEUTRAL") == 0) return "center";
    if (strcasecmp(cloud_emotion, "CONFUSED") == 0) return "surprise";
    if (strcasecmp(cloud_emotion, "THINKING") == 0) return "center";
    if (strcasecmp(cloud_emotion, "TOUCH") == 0) return "heart_eyes";
    if (strcasecmp(cloud_emotion, "FEARFUL") == 0) return "sad";
    if (strcasecmp(cloud_emotion, "DISAPPOINTED") == 0) return "sad";
    if (strcasecmp(cloud_emotion, "ANNOYED") == 0) return "anger";
    if (strcasecmp(cloud_emotion, "SLEEP") == 0) return "sleep";
    if (strcasecmp(cloud_emotion, "WAKEUP") == 0) return "wakeup";
    if (strcasecmp(cloud_emotion, "LEFT") == 0) return "left";
    if (strcasecmp(cloud_emotion, "RIGHT") == 0) return "right";
    if (strcasecmp(cloud_emotion, "WINK") == 0) return "wink";
    
    // If not found, try direct match with local emotions
    for (int i = 0; i < sizeof(gif_emotion)/sizeof(gif_emotion[0]); i++) {
        if (strcasecmp(cloud_emotion, gif_emotion[i].text) == 0) {
            return gif_emotion[i].text;
        }
    }
    
    // Default fallback
    return "happy";
}

static uint8_t __emotion_get(char *emotion)
{
    uint8_t which = 0;

    PR_DEBUG("Looking for emotion: '%s'", emotion ? emotion : "NULL");
    
    // Map cloud emotion to local emotion
    const char *mapped_emotion = __map_cloud_emotion(emotion);
    PR_DEBUG("Mapped emotion: '%s' -> '%s'", emotion ? emotion : "NULL", mapped_emotion);
    
    int i = 0;
    for (i = 0; i < sizeof(gif_emotion)/sizeof(gif_emotion[0]); i++) {
        PR_DEBUG("Comparing with: '%s'", gif_emotion[i].text);
        if (0 == strcasecmp(gif_emotion[i].text, mapped_emotion)) {
            which = i;
            PR_DEBUG("Found emotion '%s' at index %d", mapped_emotion, i);
            break;
        }
    }

    if (which == 0 && (mapped_emotion == NULL || strcasecmp(gif_emotion[0].text, mapped_emotion) != 0)) {
        PR_ERR("Emotion '%s' not found, using default (happy)", mapped_emotion ? mapped_emotion : "NULL");
    }

    return which;
}

static void __emotion_flush(char *emotion)
{
    uint8_t index = 0;

    PR_DEBUG("=== EMOTION FLUSH START ===");
    PR_DEBUG("Input emotion: '%s'", emotion ? emotion : "NULL");
    PR_DEBUG("s_gif pointer: %p", s_gif);
    
    index = __emotion_get(emotion);
    PR_DEBUG("Got emotion index: %d", index);
    
    // Update current index and enable auto cycle for rotation
    s_current_index = index;
    s_auto_cycle = true;  // Enable auto cycle for emoji rotation
    s_rotation_count = 0; // Reset rotation counter
    PR_DEBUG("Set s_current_index to %d, s_auto_cycle to true, s_rotation_count to 0", s_current_index);
    
    if (s_gif != NULL) {
        lv_gif_set_src(s_gif, gif_emotion[index].data);
        PR_DEBUG("Set GIF source to emotion data at index %d", index);
    } else {
        PR_ERR("s_gif is NULL, cannot set emotion!");
    }
    
    // Start/reset the rotation timer
    if (s_emmo_timer) {
        lv_timer_reset(s_emmo_timer);
        PR_DEBUG("Reset emoji timer");
    } else {
        PR_ERR("s_emmo_timer is NULL!");
    }
    
    PR_DEBUG("=== EMOTION FLUSH END ===");
}

static void __emotion_timer_cb(lv_timer_t *timer)
{
    // Only rotate if auto cycle is enabled
    if (s_auto_cycle) {
        // Check if servo is busy to avoid conflicts
        extern bool _s_servo_busy;
        if (_s_servo_busy) {
            PR_DEBUG("Servo busy, delaying emoji rotation");
            return; // Skip this rotation cycle if servo is busy
        }
        
        // Increment rotation counter
        s_rotation_count++;
        PR_DEBUG("Rotation count: %d/%d", s_rotation_count, s_total_emotions - 1);
        
        // Check if we've rotated through all emotions
        if (s_rotation_count >= (s_total_emotions - 1)) {
            PR_DEBUG("All emotions rotated, returning to weather clock");
            s_auto_cycle = false; // Stop rotation
            
            // Send message to return to weather clock
            app_display_send_msg(TY_DISPLAY_TP_WEATHER_CLOCK_SHOW, NULL, 0);
            return;
        }
        
        // Switch to next expression
        s_current_index = (s_current_index + 1) % s_total_emotions;
        lv_gif_set_src(s_gif, gif_emotion[s_current_index].data);
        PR_DEBUG("Emoji rotated to index %d: %s", s_current_index, gif_emotion[s_current_index].text);
    }
}

// --- UI Init ---
int ui_init(UI_FONT_T *ui_font)
{
    PR_DEBUG("=== EMOJI UI INIT START ===");
    
    // Calculate total number of emotions
    s_total_emotions = sizeof(gif_emotion) / sizeof(gif_emotion[0]);
    PR_DEBUG("Total emotions available: %d", s_total_emotions);
    
    s_container = lv_obj_create(lv_scr_act());
    lv_obj_set_size(s_container, EMMO_GIF_W, EMMO_GIF_H);
    lv_obj_set_style_pad_all(s_container, 0, 0);
    lv_obj_set_style_border_width(s_container, 0, 0);
    PR_DEBUG("Created emoji container object: %p", s_container);

    s_gif = lv_gif_create(s_container);
    lv_obj_set_size(s_gif, EMMO_GIF_W, EMMO_GIF_H);
    PR_DEBUG("Created GIF object: %p", s_gif);

    __emotion_flush("happy");
    PR_DEBUG("Set initial emotion to happy");
    
    // Create expression switching timer
    s_emmo_timer = lv_timer_create(__emotion_timer_cb, EMMO_CHANGE_INTERVAL, NULL);
    PR_DEBUG("Created emoji timer: %p", s_emmo_timer);
    
    // Initially hide the emoji UI
    lv_obj_add_flag(s_container, LV_OBJ_FLAG_HIDDEN);
    PR_DEBUG("Emoji UI initially hidden");
    
    PR_DEBUG("=== EMOJI UI INIT COMPLETE ===");
    return 0;
}

void ui_set_user_msg(const char *text) {}
void ui_set_assistant_msg(const char *text) {}
void ui_set_system_msg(const char *text) {}

void ui_set_emotion(const char *emotion) {
    PR_DEBUG("=== UI_SET_EMOTION CALLED ===");
    PR_DEBUG("Input emotion: '%s'", emotion ? emotion : "NULL");
    PR_DEBUG("s_gif pointer: %p", s_gif);
    
    if (emotion != NULL && s_gif != NULL) {
        PR_DEBUG("Calling __emotion_flush with emotion: '%s'", emotion);
        __emotion_flush((char*)emotion);
    } else {
        PR_ERR("Cannot set emotion - emotion=%p, s_gif=%p", emotion, s_gif);
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

// Show emoji UI
void ui_emoji_show(void)
{
    PR_DEBUG("=== SHOWING EMOJI UI ===");
    if (s_container != NULL) {
        lv_obj_clear_flag(s_container, LV_OBJ_FLAG_HIDDEN);
        lv_obj_move_foreground(s_container);
        PR_DEBUG("Emoji UI shown and moved to foreground");
    } else {
        PR_ERR("s_container is NULL, cannot show emoji UI");
    }
}

// Hide emoji UI
void ui_emoji_hide(void)
{
    PR_DEBUG("=== HIDING EMOJI UI ===");
    if (s_container != NULL) {
        lv_obj_add_flag(s_container, LV_OBJ_FLAG_HIDDEN);
        PR_DEBUG("Emoji UI hidden");
    } else {
        PR_ERR("s_container is NULL, cannot hide emoji UI");
    }
}

#endif

#include "tuya_ai_display.h"
#include "gui_common.h"
#include "lvgl.h"

LV_IMG_DECLARE(Nature128);
LV_IMG_DECLARE(Fearful128);
LV_IMG_DECLARE(Delicious128);
LV_IMG_DECLARE(Think128);
LV_IMG_DECLARE(Loving128);
LV_IMG_DECLARE(Confused128);
LV_IMG_DECLARE(Disappointed128);
LV_IMG_DECLARE(Angry);

static lv_obj_t *gif_half_top;
static lv_obj_t *gif_half_bottom;
static lv_obj_t *gif_full;
static int current_gif_index = -1;

//! gif file
static lv_img_dsc_t gif_files[11]; 

// ! do not support
#define ENABLE_DUAL_EYES             0      

#define GIF_EMOTION_FILE_INDEX      7

static const gui_emotion_t gif_emotion[] = {
    {&Nature128,        "neutral"     },
    {&Fearful128,       "fearful"     },
    {&Loving128,        "lovestruck"  },
    {&Think128,         "thinking"    },
    {&Confused128,      "confused"    },
    {&Delicious128,     "delicious"   },
    {&Disappointed128,  "disappointed"},
    /*----------------------------------- */
    {&gif_files[0],     "angry"       },
    {&gif_files[0],     "annoyed"     },
    {&gif_files[1],     "embarrassed" },
    {&gif_files[2],     "surprise"    },
    {&gif_files[3],     "happy"       },
    {&gif_files[4],     "laughing"    },
    {&gif_files[5],     "unamused"    },
    {&gif_files[6],     "sad"         },
    {&gif_files[7],     "relaxed"     },
    {&gif_files[8],     "winking"     },
    {&gif_files[9],     "cool"        },
    {&gif_files[10],    "zany"        },
};


void eyes_gif_load(void)
{
    int i = 0;

    static bool init = false;

    if (init) {
        return;
    }
    
    char *gif_file_path[] = {
        "/Embarrassed.gif",
        "/Shocked.gif",     //! suprise
        "/Suprising.gif",   //! happy
        "/Laughing.gif",
        "/Sleepy.gif",
        "/Crying.gif",
        "/Relaxing.gif",
        "/Winking.gif",
        "/Cool.gif",
        "/Silly.gif"
    };

    gif_files[0] = Angry;

    for (i = 0; i < sizeof(gif_file_path) / sizeof(gif_file_path[0]); i++) {
        gui_img_load_psram(gif_file_path[i], &gif_files[i + 1]);
    }

    init = true;
}

void eyes_emotion_flush(char  *emotion)
{
    uint8_t index = 0;

    index = gui_emotion_find(gif_emotion, CNTSOF(gif_emotion), emotion);

    if (current_gif_index == index) {
        return;
    }

    current_gif_index = index;

#if ENABLE_DUAL_EYES
    if (index <  GIF_EMOTION_FILE_INDEX) {
        lv_obj_add_flag(gif_full, LV_OBJ_FLAG_HIDDEN);

        lv_obj_clear_flag(gif_half_top, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(gif_half_bottom, LV_OBJ_FLAG_HIDDEN);

        lv_gif_set_src(gif_half_top, gif_emotion[index].source);
        lv_gif_set_src(gif_half_bottom, gif_emotion[index].source); // 相同图片
    } else {
        lv_obj_add_flag(gif_half_top, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(gif_half_bottom, LV_OBJ_FLAG_HIDDEN);

        lv_obj_clear_flag(gif_full, LV_OBJ_FLAG_HIDDEN);

        lv_gif_set_src(gif_full, gif_emotion[index].source); // 相同图片
    }
#else
    lv_gif_set_src(gif_full, gif_emotion[index].source); 
#endif

}


void tuya_eyes_init(void)
{
    lv_obj_t * obj = lv_obj_create(lv_scr_act());
    lv_obj_set_size(obj, LCD_WIDTH, LCD_HEIGHT);
    lv_obj_set_style_pad_all(obj, 0, 0);
    lv_obj_set_style_border_width(obj, 0, 0);

#if ENABLE_DUAL_EYES
    gif_half_top = lv_gif_create(obj);
    lv_obj_set_size(gif_half_top, LCD_WIDTH, LCD_HEIGHT / 2);
    lv_obj_set_pos(gif_half_top, 0, 0);

    gif_half_bottom = lv_gif_create(obj);
    lv_obj_set_size(gif_half_bottom, LCD_WIDTH, LCD_HEIGHT / 2);
    lv_obj_set_pos(gif_half_bottom, 0, 128);
#endif

    gif_full = lv_gif_create(obj);
    lv_obj_set_size(gif_full, LCD_WIDTH, LCD_HEIGHT);

    eyes_emotion_flush("neutral");
}


void tuya_eyes_app(TY_DISPLAY_MSG_T *msg)
{
    uint8_t emoji;

    switch (msg->type)
    {
        
    case TY_DISPLAY_TP_EMOJI:
        eyes_emotion_flush(msg->data);
        break;    

    case TY_DISPLAY_TP_CHAT_STAT: {
        if (0 == msg->data[0] || 1 == msg->data[0]) {
            eyes_emotion_flush("neutral");
        }
    } break;

    case TY_DISPLAY_TP_STAT_NET:
        if (msg->data[0]) {
            eyes_gif_load();
        }
        break;    


    default:
        break;
    }
}

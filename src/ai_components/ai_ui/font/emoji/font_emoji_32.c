#include "lvgl.h"

extern const lv_image_dsc_t emoji_1f636_32; // neutral
extern const lv_image_dsc_t emoji_1f642_32; // happy
extern const lv_image_dsc_t emoji_1f606_32; // laughing
extern const lv_image_dsc_t emoji_1f602_32; // funny
extern const lv_image_dsc_t emoji_1f614_32; // sad
extern const lv_image_dsc_t emoji_1f620_32; // angry
extern const lv_image_dsc_t emoji_1f62d_32; // crying
extern const lv_image_dsc_t emoji_1f60d_32; // loving
extern const lv_image_dsc_t emoji_1f633_32; // embarrassed
extern const lv_image_dsc_t emoji_1f62f_32; // surprised
extern const lv_image_dsc_t emoji_1f631_32; // shocked
extern const lv_image_dsc_t emoji_1f914_32; // thinking
extern const lv_image_dsc_t emoji_1f609_32; // winking
extern const lv_image_dsc_t emoji_1f60e_32; // cool
extern const lv_image_dsc_t emoji_1f60c_32; // relaxed
extern const lv_image_dsc_t emoji_1f924_32; // delicious
extern const lv_image_dsc_t emoji_1f618_32; // kissy
extern const lv_image_dsc_t emoji_1f60f_32; // confident
extern const lv_image_dsc_t emoji_1f634_32; // sleepy
extern const lv_image_dsc_t emoji_1f61c_32; // silly
extern const lv_image_dsc_t emoji_1f644_32; // confused

typedef struct emoji_32 {
    const lv_image_dsc_t *emoji;
    uint32_t unicode;
} emoji_32_t;

static const void *get_imgfont_path(const lv_font_t *font, uint32_t unicode, uint32_t unicode_next, int32_t *offset_y,
                                    void *user_data)
{
    static const emoji_32_t emoji_32_table[] = {
        {&emoji_1f636_32, 0x1f636}, // neutral
        {&emoji_1f642_32, 0x1f642}, // happy
        {&emoji_1f606_32, 0x1f606}, // laughing
        {&emoji_1f602_32, 0x1f602}, // funny
        {&emoji_1f614_32, 0x1f614}, // sad
        {&emoji_1f620_32, 0x1f620}, // angry
        {&emoji_1f62d_32, 0x1f62d}, // crying
        {&emoji_1f60d_32, 0x1f60d}, // loving
        {&emoji_1f633_32, 0x1f633}, // embarrassed
        {&emoji_1f62f_32, 0x1f62f}, // surprised
        {&emoji_1f631_32, 0x1f631}, // shocked
        {&emoji_1f914_32, 0x1f914}, // thinking
        {&emoji_1f609_32, 0x1f609}, // winking
        {&emoji_1f60e_32, 0x1f60e}, // cool
        {&emoji_1f60c_32, 0x1f60c}, // relaxed
        {&emoji_1f924_32, 0x1f924}, // delicious
        {&emoji_1f618_32, 0x1f618}, // kissy
        {&emoji_1f60f_32, 0x1f60f}, // confident
        {&emoji_1f634_32, 0x1f634}, // sleepy
        {&emoji_1f61c_32, 0x1f61c}, // silly
        {&emoji_1f644_32, 0x1f644}, // confused
    };

    for (size_t i = 0; i < sizeof(emoji_32_table) / sizeof(emoji_32_table[0]); i++) {
        if (emoji_32_table[i].unicode == unicode) {
            return emoji_32_table[i].emoji;
        }
    }
    return NULL;
}

const lv_font_t *font_emoji_32_init(void)
{
    static lv_font_t *font = NULL;
    if (font == NULL) {
        font = lv_imgfont_create(32, get_imgfont_path, NULL);
        if (font == NULL) {
            LV_LOG_ERROR("Failed to allocate memory for emoji font");
            return NULL;
        }
        font->base_line = 0;
        font->fallback = NULL;
    }
    return font;
}

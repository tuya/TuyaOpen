#include "lvgl.h"

extern const lv_image_dsc_t emoji_1f636_64; // neutral
extern const lv_image_dsc_t emoji_1f642_64; // happy
extern const lv_image_dsc_t emoji_1f606_64; // laughing
extern const lv_image_dsc_t emoji_1f602_64; // funny
extern const lv_image_dsc_t emoji_1f614_64; // sad
extern const lv_image_dsc_t emoji_1f620_64; // angry
extern const lv_image_dsc_t emoji_1f62d_64; // crying
extern const lv_image_dsc_t emoji_1f60d_64; // loving
extern const lv_image_dsc_t emoji_1f633_64; // embarrassed
extern const lv_image_dsc_t emoji_1f62f_64; // surprised
extern const lv_image_dsc_t emoji_1f631_64; // shocked
extern const lv_image_dsc_t emoji_1f914_64; // thinking
extern const lv_image_dsc_t emoji_1f609_64; // winking
extern const lv_image_dsc_t emoji_1f60e_64; // cool
extern const lv_image_dsc_t emoji_1f60c_64; // relaxed
extern const lv_image_dsc_t emoji_1f924_64; // delicious
extern const lv_image_dsc_t emoji_1f618_64; // kissy
extern const lv_image_dsc_t emoji_1f60f_64; // confident
extern const lv_image_dsc_t emoji_1f634_64; // sleepy
extern const lv_image_dsc_t emoji_1f61c_64; // silly
extern const lv_image_dsc_t emoji_1f644_64; // confused

typedef struct emoji_64 {
    const lv_image_dsc_t *emoji;
    uint32_t unicode;
} emoji_64_t;

static const void *get_imgfont_path(const lv_font_t *font, uint32_t unicode, uint32_t unicode_next, int32_t *offset_y,
                                    void *user_data)
{
    static const emoji_64_t emoji_64_table[] = {
        {&emoji_1f636_64, 0x1f636}, // neutral
        {&emoji_1f642_64, 0x1f642}, // happy
        {&emoji_1f606_64, 0x1f606}, // laughing
        {&emoji_1f602_64, 0x1f602}, // funny
        {&emoji_1f614_64, 0x1f614}, // sad
        {&emoji_1f620_64, 0x1f620}, // angry
        {&emoji_1f62d_64, 0x1f62d}, // crying
        {&emoji_1f60d_64, 0x1f60d}, // loving
        {&emoji_1f633_64, 0x1f633}, // embarrassed
        {&emoji_1f62f_64, 0x1f62f}, // surprised
        {&emoji_1f631_64, 0x1f631}, // shocked
        {&emoji_1f914_64, 0x1f914}, // thinking
        {&emoji_1f609_64, 0x1f609}, // winking
        {&emoji_1f60e_64, 0x1f60e}, // cool
        {&emoji_1f60c_64, 0x1f60c}, // relaxed
        {&emoji_1f924_64, 0x1f924}, // delicious
        {&emoji_1f618_64, 0x1f618}, // kissy
        {&emoji_1f60f_64, 0x1f60f}, // confident
        {&emoji_1f634_64, 0x1f634}, // sleepy
        {&emoji_1f61c_64, 0x1f61c}, // silly
        {&emoji_1f644_64, 0x1f644}, // confused
    };

    for (size_t i = 0; i < sizeof(emoji_64_table) / sizeof(emoji_64_table[0]); i++) {
        if (emoji_64_table[i].unicode == unicode) {
            return emoji_64_table[i].emoji;
        }
    }
    return NULL;
}

const lv_font_t *font_emoji_64_init(void)
{
    static lv_font_t *font = NULL;
    if (font == NULL) {
        font = lv_imgfont_create(64, get_imgfont_path, NULL);
        if (font == NULL) {
            LV_LOG_ERROR("Failed to allocate memory for emoji font");
            return NULL;
        }
        font->base_line = 0;
        font->fallback = NULL;
    }
    return font;
}

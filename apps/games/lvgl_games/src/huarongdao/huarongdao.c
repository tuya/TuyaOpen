#include "lvgl.h"
#include "huaorngdao.h"

enum {
    little,
    big,
    hor,
    ver,
} obj_type_enum;

enum {
    up,
    down,
    left,
    right,
} direction_type_enum;

typedef struct {
    lv_obj_t *obj;
    uint8_t x;
    uint8_t y;
    uint8_t obj_type;
} game_obj_type;

const static game_obj_type game_stage_obj_lib[][10] = {
    0, 0, 0, ver,    0, 1, 0, ver,    0, 2, 0, ver,    0, 3, 0, little, 0, 3, 1, little,
    0, 0, 2, hor,    0, 2, 2, hor,    0, 0, 3, little, 0, 2, 3, big,    0, 0, 4, little,

    0, 0, 0, ver,    0, 1, 0, ver,    0, 2, 0, ver,    0, 3, 0, little, 0, 3, 1, little,
    0, 0, 2, hor,    0, 0, 3, hor,    0, 2, 2, big,    0, 2, 4, little, 0, 3, 4, little,

    0, 0, 0, ver,    0, 1, 0, ver,    0, 2, 0, ver,    0, 3, 0, ver,    0, 0, 2, ver,
    0, 1, 2, big,    0, 0, 4, little, 0, 1, 4, little, 0, 2, 4, little, 0, 3, 4, little,

    0, 0, 0, hor,    0, 2, 0, hor,    0, 0, 1, hor,    0, 2, 1, hor,    0, 0, 2, big,
    0, 2, 2, little, 0, 3, 2, little, 0, 2, 3, little, 0, 3, 3, little, 0, 0, 4, hor,

    0, 0, 0, hor,    0, 2, 0, hor,    0, 0, 1, big,    0, 2, 1, hor,    0, 2, 2, hor,
    0, 0, 3, hor,    0, 2, 3, little, 0, 3, 3, little, 0, 0, 4, little, 0, 3, 4, little,

    0, 0, 0, big,    0, 2, 0, hor,    0, 2, 1, hor,    0, 0, 2, hor,    0, 2, 2, hor,
    0, 0, 3, little, 0, 1, 3, little, 0, 2, 3, hor,    0, 0, 4, little, 0, 3, 4, little,

    0, 0, 0, hor,    0, 0, 1, hor,    0, 2, 0, big,    0, 0, 2, hor,    0, 2, 2, hor,
    0, 0, 3, ver,    0, 2, 3, little, 0, 3, 3, little, 0, 2, 4, little, 0, 3, 4, little,

    0, 0, 0, ver,    0, 1, 0, big,    0, 3, 0, ver,    0, 0, 2, ver,    0, 1, 2, little,
    0, 2, 2, little, 0, 3, 2, ver,    0, 1, 3, little, 0, 2, 3, little, 0, 1, 4, hor,

    0, 0, 0, ver,    0, 1, 0, little, 0, 2, 0, little, 0, 3, 0, ver,    0, 1, 1, big,
    0, 0, 2, little, 0, 3, 2, little, 0, 0, 3, ver,    0, 1, 3, hor,    0, 1, 4, hor,

    0, 0, 0, hor,    0, 2, 0, little, 0, 3, 0, little, 0, 0, 1, ver,    0, 1, 1, big,
    0, 3, 1, ver,    0, 0, 3, ver,    0, 1, 3, little, 0, 1, 4, little, 0, 2, 3, ver,

    0, 0, 0, little, 0, 1, 0, hor,    0, 3, 0, ver,    0, 0, 1, ver,    0, 1, 1, hor,
    0, 1, 2, little, 0, 2, 2, big,    0, 0, 3, hor,    0, 2, 4, little, 0, 3, 4, little,

    0, 0, 0, ver,    0, 1, 0, little, 0, 1, 1, little, 0, 2, 0, ver,    0, 3, 0, ver,
    0, 0, 2, ver,    0, 1, 2, little, 0, 1, 3, little, 0, 2, 2, big,    0, 2, 4, hor,

    0, 0, 0, ver,    0, 1, 0, big,    0, 3, 0, ver,    0, 0, 2, ver,    0, 1, 2, hor,
    0, 3, 2, ver,    0, 0, 4, little, 0, 1, 3, little, 0, 2, 3, little, 0, 3, 4, little,

    0, 0, 0, ver,    0, 1, 0, big,    0, 3, 0, ver,    0, 0, 2, hor,    0, 2, 2, hor,
    0, 0, 3, little, 0, 1, 3, hor,    0, 3, 3, little, 0, 0, 4, little, 0, 3, 4, little,

    0, 0, 0, ver,    0, 1, 0, big,    0, 3, 0, little, 0, 3, 1, little, 0, 1, 2, hor,
    0, 0, 3, ver,    0, 1, 3, ver,    0, 2, 3, little, 0, 3, 3, little, 0, 2, 4, hor,

};

static game_obj_type game_stage_obj[10] = {
    0,
};
static uint8_t game_map[5][4] = {
    0,
};
static uint8_t current_stage = 0;
const static uint8_t max_stage = sizeof(game_stage_obj_lib) / sizeof(game_stage_obj);
static lv_obj_t *screen1, *game_window, *exit_btn, *bgmap, *next_btn, *pre_btn, *step_lable;
static void game_stage_init(lv_timer_t *t);
static void clear_map();
static void move_obj_cb(lv_event_t *e);
static void exit_game_cb(lv_event_t *e);
static void next_stage_cb(lv_event_t *e);
static void pre_stage_cb(lv_event_t *e);
static void stage_clear();
static float screen_ratio;
static int step_count;

LV_IMG_DECLARE(bg_img)
LV_IMG_DECLARE(exit_img)
LV_IMG_DECLARE(next_img)
LV_IMG_DECLARE(pre_img)

void huarongdao(void)
{

    screen_ratio = (float)lv_disp_get_hor_res(lv_disp_get_default()) / 480;

    lv_obj_clear_flag(lv_scr_act(), LV_OBJ_FLAG_SCROLLABLE);

    screen1 = lv_tileview_create(lv_scr_act());
    lv_obj_set_style_bg_color(screen1, lv_color_hex(0x000000), LV_PART_MAIN);
    lv_obj_clear_flag(screen1, LV_OBJ_FLAG_SCROLLABLE);

    bgmap = lv_img_create(screen1);
    lv_img_set_src(bgmap, &bg_img);
    lv_img_set_pivot(bgmap, 0, 0);
    lv_img_set_zoom(bgmap, 256 * 2);
    lv_obj_clear_flag(bgmap, LV_OBJ_FLAG_SCROLLABLE);

    game_window = lv_tileview_create(screen1);
    lv_obj_set_style_bg_color(game_window, lv_color_hex(0x000022), LV_PART_MAIN);
    lv_obj_set_style_bg_grad_color(game_window, lv_color_hex(0x000055), 0);
    lv_obj_set_style_bg_grad_dir(game_window, LV_GRAD_DIR_VER, 0);
    lv_obj_set_style_bg_opa(game_window, 255, LV_PART_MAIN);
    lv_obj_clear_flag(game_window, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_outline_width(game_window, 6, LV_PART_MAIN);
    lv_obj_set_style_outline_color(game_window, lv_color_hex(0xbb7700), LV_PART_MAIN);
    lv_obj_center(game_window);
    lv_obj_set_size(game_window, lv_pct(60), lv_pct(60));

    step_lable = lv_label_create(screen1);
    lv_label_set_text_fmt(step_lable, "STEP:%d", step_count);
    lv_obj_set_style_text_color(step_lable, lv_color_hex(0xffffff), 0);
    lv_obj_set_style_text_font(step_lable, &lv_font_montserrat_24, 0);
    lv_obj_align(step_lable, LV_ALIGN_TOP_MID, 0, 10);

    exit_btn = lv_img_create(screen1);
    lv_img_set_src(exit_btn, &exit_img);
    lv_img_set_zoom(exit_btn, 128);
    lv_obj_add_event_cb(exit_btn, exit_game_cb, LV_EVENT_CLICKED, 0);
    lv_obj_align(exit_btn, LV_ALIGN_TOP_LEFT, 10, 10);

    pre_btn = lv_img_create(screen1);
    lv_img_set_src(pre_btn, &pre_img);
    lv_img_set_zoom(pre_btn, 128);
    lv_obj_set_pos(pre_btn, lv_pct(75), lv_pct(30));
    lv_obj_add_event_cb(pre_btn, pre_stage_cb, LV_EVENT_CLICKED, 0);

    next_btn = lv_img_create(screen1);
    lv_img_set_src(next_btn, &next_img);
    lv_img_set_zoom(next_btn, 128);
    lv_obj_set_pos(next_btn, lv_pct(75), lv_pct(60));
    lv_obj_add_event_cb(next_btn, next_stage_cb, LV_EVENT_CLICKED, 0);

    game_stage_init(0);
}

static void pre_stage_cb(lv_event_t *e)
{
    if (current_stage == 0)
        return;
    lv_obj_clean(game_window);
    current_stage--;
    game_stage_init(0);
}

static void next_stage_cb(lv_event_t *e)
{
    if (current_stage == max_stage - 1)
        return;
    lv_obj_clean(game_window);
    current_stage++;
    game_stage_init(0);
}

static void game_stage_init(lv_timer_t *t)
{
    step_count = 0;
    lv_label_set_text_fmt(step_lable, "STEP:%d", step_count);
    clear_map();

    lv_memcpy(game_stage_obj, (void *)((int)game_stage_obj_lib + sizeof(game_stage_obj) * current_stage),
              sizeof(game_stage_obj));

    for (int i = 0; i < 10; i++) {
        game_stage_obj[i].obj = lv_btn_create(game_window);
        lv_obj_set_size(
            game_stage_obj[i].obj,
            game_stage_obj[i].obj_type == little || game_stage_obj[i].obj_type == ver ? lv_pct(25) : lv_pct(50),
            game_stage_obj[i].obj_type == big || game_stage_obj[i].obj_type == ver ? lv_pct(40) : lv_pct(20));
        lv_obj_set_pos(game_stage_obj[i].obj, lv_pct((game_stage_obj[i].x) * 25), lv_pct((game_stage_obj[i].y) * 20));
        lv_obj_set_style_bg_color(game_stage_obj[i].obj,
                                  lv_color_hex(game_stage_obj[i].obj_type == little ? 0xffa200
                                               : game_stage_obj[i].obj_type == big  ? 0xff5555
                                               : game_stage_obj[i].obj_type == ver  ? 0x6666ff
                                                                                    : 0x44bb44),
                                  LV_PART_MAIN);
        lv_obj_set_style_border_color(game_stage_obj[i].obj, lv_color_hex(0x555555), LV_PART_MAIN);
        lv_obj_set_style_border_width(game_stage_obj[i].obj, 2, 0);
        game_stage_obj[i].obj->user_data = &game_stage_obj[i];

        if (game_stage_obj[i].obj_type == little) {
            game_map[game_stage_obj[i].y][game_stage_obj[i].x] = 1;
        }

        if (game_stage_obj[i].obj_type == hor) {
            game_map[game_stage_obj[i].y][game_stage_obj[i].x] = 1;
            game_map[game_stage_obj[i].y][game_stage_obj[i].x + 1] = 1;
        }

        if (game_stage_obj[i].obj_type == ver) {
            game_map[game_stage_obj[i].y][game_stage_obj[i].x] = 1;
            game_map[game_stage_obj[i].y + 1][game_stage_obj[i].x] = 1;
        }

        if (game_stage_obj[i].obj_type == big) {
            game_map[game_stage_obj[i].y][game_stage_obj[i].x] = 1;
            game_map[game_stage_obj[i].y][game_stage_obj[i].x + 1] = 1;
            game_map[game_stage_obj[i].y + 1][game_stage_obj[i].x] = 1;
            game_map[game_stage_obj[i].y + 1][game_stage_obj[i].x + 1] = 1;
        }

        lv_obj_add_event_cb(game_stage_obj[i].obj, move_obj_cb, LV_EVENT_ALL, &game_stage_obj[i]);
    }

    lv_obj_t *lable = lv_label_create(game_window);
    lv_label_set_text_fmt(lable, "STAGE  %d START", current_stage + 1);
    lv_obj_set_style_text_color(lable, lv_color_hex(0xffffff), 0);
    lv_obj_center(lable);
#if LVGL_VERSION_MAJOR == 9
    lv_obj_delete_delayed(lable, 1000);
#else
    lv_obj_del_delayed(lable, 1000);
#endif

    lv_obj_add_flag(pre_btn, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(next_btn, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(exit_btn, LV_OBJ_FLAG_CLICKABLE);
}

static void move_obj_cb(lv_event_t *e)
{
    static lv_point_t click_point1, click_point2;
    int movex, movey, direction;

    game_obj_type *stage_data = (game_obj_type *)e->user_data;

    if (e->code == LV_EVENT_PRESSED) {
        lv_indev_get_point(lv_indev_get_act(), &click_point1);
        return;
    }

    if (e->code == LV_EVENT_RELEASED) {
        lv_indev_get_point(lv_indev_get_act(), &click_point2);
        movex = click_point2.x - click_point1.x;
        movey = click_point2.y - click_point1.y;

        if ((movex == 0 && movey == 0) || (movex == movey) || (movex == -movey))
            return;

        if ((movex < 0 && movey < 0 && movex > movey) || (movex > 0 && movey < 0 && movex < -movey))
            direction = up;
        if ((movex > 0 && movey < 0 && movex > -movey) || (movex > 0 && movey > 0 && movex > movey))
            direction = right;
        if ((movex < 0 && movey < 0 && movex < movey) || (movex < 0 && movey > 0 && movex < -movey))
            direction = left;
        if ((movex < 0 && movey > 0 && movex > -movey) || (movex > 0 && movey > 0 && movex < movey))
            direction = down;

        if (direction == up) {

            if (stage_data->obj_type == little) {
                if (stage_data->y == 0)
                    return;

                if (game_map[stage_data->y - 1][stage_data->x] == 0) {
                    game_map[stage_data->y - 1][stage_data->x] = 1;
                    game_map[stage_data->y][stage_data->x] = 0;
                    stage_data->y--;
                    step_count++;
                    lv_label_set_text_fmt(step_lable, "STEP:%d", step_count);
                    lv_obj_set_pos(stage_data->obj, lv_pct((stage_data->x) * 25), lv_pct((stage_data->y) * 20));
                }
            }

            if (stage_data->obj_type == big) {
                if (stage_data->y == 0)
                    return;

                if (game_map[stage_data->y - 1][stage_data->x] == 0 &&
                    game_map[stage_data->y - 1][(stage_data->x) + 1] == 0) {
                    game_map[stage_data->y - 1][stage_data->x] = 1;
                    game_map[stage_data->y - 1][(stage_data->x) + 1] = 1;
                    game_map[stage_data->y + 1][stage_data->x] = 0;
                    game_map[stage_data->y + 1][(stage_data->x) + 1] = 0;
                    stage_data->y--;
                    step_count++;
                    lv_label_set_text_fmt(step_lable, "STEP:%d", step_count);
                    lv_obj_set_pos(stage_data->obj, lv_pct((stage_data->x) * 25), lv_pct((stage_data->y) * 20));
                }
            }

            if (stage_data->obj_type == hor) {
                if (stage_data->y == 0)
                    return;

                if (game_map[stage_data->y - 1][stage_data->x] == 0 &&
                    game_map[stage_data->y - 1][(stage_data->x) + 1] == 0) {
                    game_map[stage_data->y - 1][stage_data->x] = 1;
                    game_map[stage_data->y - 1][(stage_data->x) + 1] = 1;
                    game_map[stage_data->y][stage_data->x] = 0;
                    game_map[stage_data->y][(stage_data->x) + 1] = 0;
                    stage_data->y--;
                    step_count++;
                    lv_label_set_text_fmt(step_lable, "STEP:%d", step_count);
                    lv_obj_set_pos(stage_data->obj, lv_pct((stage_data->x) * 25), lv_pct((stage_data->y) * 20));
                }
            }

            if (stage_data->obj_type == ver) {
                if (stage_data->y == 0)
                    return;

                if (game_map[stage_data->y - 1][stage_data->x] == 0) {
                    game_map[stage_data->y - 1][stage_data->x] = 1;
                    game_map[stage_data->y + 1][stage_data->x] = 0;
                    stage_data->y--;
                    step_count++;
                    lv_label_set_text_fmt(step_lable, "STEP:%d", step_count);
                    lv_obj_set_pos(stage_data->obj, lv_pct((stage_data->x) * 25), lv_pct((stage_data->y) * 20));
                }
            }
        }

        if (direction == down) {

            if (stage_data->obj_type == little) {
                if (stage_data->y == 4)
                    return;

                if (game_map[stage_data->y + 1][stage_data->x] == 0) {
                    game_map[stage_data->y + 1][stage_data->x] = 1;
                    game_map[stage_data->y][stage_data->x] = 0;
                    stage_data->y++;
                    step_count++;
                    lv_label_set_text_fmt(step_lable, "STEP:%d", step_count);
                    lv_obj_set_pos(stage_data->obj, lv_pct((stage_data->x) * 25), lv_pct((stage_data->y) * 20));
                }
            }

            if (stage_data->obj_type == big) {
                if (stage_data->y == 3)
                    return;

                if (game_map[stage_data->y + 2][stage_data->x] == 0 &&
                    game_map[stage_data->y + 2][(stage_data->x) + 1] == 0) {
                    game_map[stage_data->y + 2][stage_data->x] = 1;
                    game_map[stage_data->y + 2][(stage_data->x) + 1] = 1;
                    game_map[stage_data->y][stage_data->x] = 0;
                    game_map[stage_data->y][(stage_data->x) + 1] = 0;
                    stage_data->y++;
                    step_count++;
                    lv_label_set_text_fmt(step_lable, "STEP:%d", step_count);
                    lv_obj_set_pos(stage_data->obj, lv_pct((stage_data->x) * 25), lv_pct((stage_data->y) * 20));
                }
            }

            if (stage_data->obj_type == hor) {
                if (stage_data->y == 4)
                    return;

                if (game_map[stage_data->y + 1][stage_data->x] == 0 &&
                    game_map[stage_data->y + 1][(stage_data->x) + 1] == 0) {
                    game_map[stage_data->y + 1][stage_data->x] = 1;
                    game_map[stage_data->y + 1][(stage_data->x) + 1] = 1;
                    game_map[stage_data->y][stage_data->x] = 0;
                    game_map[stage_data->y][(stage_data->x) + 1] = 0;
                    stage_data->y++;
                    step_count++;
                    lv_label_set_text_fmt(step_lable, "STEP:%d", step_count);
                    lv_obj_set_pos(stage_data->obj, lv_pct((stage_data->x) * 25), lv_pct((stage_data->y) * 20));
                }
            }

            if (stage_data->obj_type == ver) {
                if (stage_data->y == 3)
                    return;

                if (game_map[stage_data->y + 2][stage_data->x] == 0) {
                    game_map[stage_data->y + 2][stage_data->x] = 1;
                    game_map[stage_data->y][stage_data->x] = 0;
                    stage_data->y++;
                    step_count++;
                    lv_label_set_text_fmt(step_lable, "STEP:%d", step_count);
                    lv_obj_set_pos(stage_data->obj, lv_pct((stage_data->x) * 25), lv_pct((stage_data->y) * 20));
                }
            }
        }

        if (direction == left) {

            if (stage_data->obj_type == little) {
                if (stage_data->x == 0)
                    return;

                if (game_map[stage_data->y][stage_data->x - 1] == 0) {
                    game_map[stage_data->y][stage_data->x - 1] = 1;
                    game_map[stage_data->y][stage_data->x] = 0;
                    stage_data->x--;
                    step_count++;
                    lv_label_set_text_fmt(step_lable, "STEP:%d", step_count);
                    lv_obj_set_pos(stage_data->obj, lv_pct((stage_data->x) * 25), lv_pct((stage_data->y) * 20));
                }
            }

            if (stage_data->obj_type == big) {
                if (stage_data->x == 0)
                    return;

                if (game_map[stage_data->y][stage_data->x - 1] == 0 &&
                    game_map[stage_data->y + 1][(stage_data->x) - 1] == 0) {
                    game_map[stage_data->y][stage_data->x - 1] = 1;
                    game_map[stage_data->y + 1][(stage_data->x) - 1] = 1;
                    game_map[stage_data->y][stage_data->x + 1] = 0;
                    game_map[stage_data->y + 1][(stage_data->x) + 1] = 0;
                    stage_data->x--;
                    step_count++;
                    lv_label_set_text_fmt(step_lable, "STEP:%d", step_count);
                    lv_obj_set_pos(stage_data->obj, lv_pct((stage_data->x) * 25), lv_pct((stage_data->y) * 20));
                }
            }

            if (stage_data->obj_type == hor) {
                if (stage_data->x == 0)
                    return;

                if (game_map[stage_data->y][stage_data->x - 1] == 0) {
                    game_map[stage_data->y][stage_data->x - 1] = 1;
                    game_map[stage_data->y][stage_data->x + 1] = 0;
                    stage_data->x--;
                    step_count++;
                    lv_label_set_text_fmt(step_lable, "STEP:%d", step_count);
                    lv_obj_set_pos(stage_data->obj, lv_pct((stage_data->x) * 25), lv_pct((stage_data->y) * 20));
                }
            }

            if (stage_data->obj_type == ver) {
                if (stage_data->x == 0)
                    return;

                if (game_map[stage_data->y][stage_data->x - 1] == 0 &&
                    game_map[stage_data->y + 1][(stage_data->x) - 1] == 0) {
                    game_map[stage_data->y][stage_data->x - 1] = 1;
                    game_map[stage_data->y + 1][stage_data->x - 1] = 1;
                    game_map[stage_data->y][stage_data->x] = 0;
                    game_map[stage_data->y + 1][stage_data->x] = 0;
                    stage_data->x--;
                    step_count++;
                    lv_label_set_text_fmt(step_lable, "STEP:%d", step_count);
                    lv_obj_set_pos(stage_data->obj, lv_pct((stage_data->x) * 25), lv_pct((stage_data->y) * 20));
                }
            }
        }

        if (direction == right) {

            if (stage_data->obj_type == little) {
                if (stage_data->x == 3)
                    return;

                if (game_map[stage_data->y][stage_data->x + 1] == 0) {
                    game_map[stage_data->y][stage_data->x + 1] = 1;
                    game_map[stage_data->y][stage_data->x] = 0;
                    stage_data->x++;
                    step_count++;
                    lv_label_set_text_fmt(step_lable, "STEP:%d", step_count);
                    lv_obj_set_pos(stage_data->obj, lv_pct((stage_data->x) * 25), lv_pct((stage_data->y) * 20));
                }
            }

            if (stage_data->obj_type == big) {
                if (stage_data->x == 2)
                    return;

                if (game_map[stage_data->y][stage_data->x + 2] == 0 &&
                    game_map[stage_data->y + 1][(stage_data->x) + 2] == 0) {
                    game_map[stage_data->y][stage_data->x + 2] = 1;
                    game_map[stage_data->y + 1][(stage_data->x) + 2] = 1;
                    game_map[stage_data->y][stage_data->x] = 0;
                    game_map[stage_data->y + 1][(stage_data->x)] = 0;
                    stage_data->x++;
                    step_count++;
                    lv_label_set_text_fmt(step_lable, "STEP:%d", step_count);
                    lv_obj_set_pos(stage_data->obj, lv_pct((stage_data->x) * 25), lv_pct((stage_data->y) * 20));
                }
            }

            if (stage_data->obj_type == hor) {
                if (stage_data->x == 2)
                    return;

                if (game_map[stage_data->y][stage_data->x + 2] == 0) {
                    game_map[stage_data->y][stage_data->x + 2] = 1;
                    game_map[stage_data->y][stage_data->x] = 0;
                    stage_data->x++;
                    step_count++;
                    lv_label_set_text_fmt(step_lable, "STEP:%d", step_count);
                    lv_obj_set_pos(stage_data->obj, lv_pct((stage_data->x) * 25), lv_pct((stage_data->y) * 20));
                }
            }

            if (stage_data->obj_type == ver) {
                if (stage_data->x == 3)
                    return;

                if (game_map[stage_data->y][stage_data->x + 1] == 0 &&
                    game_map[stage_data->y + 1][(stage_data->x) + 1] == 0) {
                    game_map[stage_data->y][stage_data->x + 1] = 1;
                    game_map[stage_data->y + 1][stage_data->x + 1] = 1;
                    game_map[stage_data->y][stage_data->x] = 0;
                    game_map[stage_data->y + 1][stage_data->x] = 0;
                    stage_data->x++;
                    step_count++;
                    lv_label_set_text_fmt(step_lable, "STEP:%d", step_count);
                    lv_obj_set_pos(stage_data->obj, lv_pct((stage_data->x) * 25), lv_pct((stage_data->y) * 20));
                }
            }
        }

        if (stage_data->obj_type == big && stage_data->x == 1 && stage_data->y == 3) {
            lv_obj_t *clear_lable = lv_label_create(game_window);
            lv_label_set_text(clear_lable, "STAGE CLEAR");
            lv_obj_set_style_text_color(clear_lable, lv_color_hex(0xffffff), 0);
            lv_obj_center(clear_lable);
            if (current_stage < max_stage - 1) {
                current_stage++;
            }
            stage_clear();
        }
    }
}

static void stage_clear()
{

    lv_obj_clear_flag(pre_btn, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(next_btn, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(exit_btn, LV_OBJ_FLAG_CLICKABLE);

    for (int i = 0; i < 11; i++) {
        lv_obj_t *obj = lv_obj_get_child(game_window, i);
#if LVGL_VERSION_MAJOR == 9
        lv_obj_delete_delayed(obj, 50 * i);
#else
        lv_obj_del_delayed(obj, 50 * i);
#endif
    }
    lv_timer_t *t1 = lv_timer_create(game_stage_init, 600, 0);
    lv_timer_set_repeat_count(t1, 1);
}

static void clear_map()
{
    for (int i = 0; i < 5; i++) {
        for (int j = 0; j < 4; j++) {
            game_map[i][j] = 0;
        }
    }
}

static void exit_game_cb(lv_event_t *e)
{
    clear_map();
    lv_obj_del(screen1);
}

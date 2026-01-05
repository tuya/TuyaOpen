#include "lvgl.h"
#include "stdlib.h"
#include "xiaoxiaole.h"

// 整体的宽度
#define XXL_SIZE_W  440
#define XXL_SIZE_H  440
#define XXL_IMG_W   (XXL_SIZE_W/8)
#define XXL_IMG_H   (XXL_SIZE_H/8)

typedef enum {
    red = 0xff0000,
    green = 0x00ff00,
    blue = 0x0000ff,
    gblue = 0x00ffff,
    yellow = 0xffff00,
    rblue = 0xff00ff,
    orange = 0xff9200,
} color_type_enum;

typedef enum {
    up,
    down,
    left,
    right,
} direction_type_enum;

typedef struct {
    lv_obj_t *obj;
    uint8_t color_index;
    uint8_t x;
    uint8_t y;
    uint8_t alive;

} game_obj_type;

static const unsigned int color_lib[7] = {
    red, green, blue, gblue, yellow, rblue, orange,
};
static game_obj_type game_obj[8][8] = {
    0,
};
static int score;
static lv_obj_t *screen1, *game_window, *refs_btn, *bgmap, *next_btn, *pre_btn, *step_lable, *exit_btn, *coin,
    *score_lable;
static float screen_ratio;
static void game_init();
static void exchange_obj(game_obj_type *obj1, game_obj_type *obj2);
static void move_obj_cb(lv_event_t *e);
static void x_move_cb(void *var, int32_t v);
static void y_move_cb(void *var, int32_t v);
static void exchange_done_cb(lv_anim_t *a);
static void same_color_move_to_coin();
static int same_color_check();
static void obj_move_down();
static void set_obj_userdata();
static void move_deleted_cb(lv_anim_t *a);
static bool map_is_full();
static bool has_same_color();
static void map_del_all();
static void map_refs(lv_event_t *e);
static void exit_game_cb(lv_event_t *e);
static void clear_all_clickable();
static void add_all_clickable();
static void move_to_coin_end_cb(lv_anim_t *a);
static void flash_end_cb(lv_anim_t *a);
static void flash_cb(void *var, int32_t v);
static void same_color_flash();

LV_IMG_DECLARE(xiaoxiaole_bg_img)
LV_IMG_DECLARE(refs_btn_img)
LV_IMG_DECLARE(exit_img)
LV_IMG_DECLARE(coin_img)

void xiaoxiaole(void)
{
    if (lv_disp_get_hor_res(lv_disp_get_default()) >= lv_disp_get_ver_res(lv_disp_get_default())) {
        screen_ratio = (float)lv_disp_get_ver_res(lv_disp_get_default()) / 400;
    } else {
        screen_ratio = (float)lv_disp_get_hor_res(lv_disp_get_default()) / 480;
    }

    lv_obj_clear_flag(lv_scr_act(), LV_OBJ_FLAG_SCROLLABLE);

    screen1 = lv_tileview_create(lv_scr_act());
    lv_obj_set_style_bg_color(screen1, lv_color_hex(0x000000), LV_PART_MAIN);
    lv_obj_clear_flag(screen1, LV_OBJ_FLAG_SCROLLABLE);

    //////////////////////////////////////////////////////////////////////////////////////
    bgmap = lv_img_create(screen1); // 如果很卡的话，把这个背景图片删掉
    lv_img_set_src(bgmap, &xiaoxiaole_bg_img);
    lv_img_set_pivot(bgmap, 0, 0);
    lv_img_set_zoom(bgmap, 256 * 1.5 * 1.2);
    lv_obj_clear_flag(bgmap, LV_OBJ_FLAG_SCROLLABLE);
    ///////////////////////////////////////////////////////////////////////////////////

    game_window = lv_tileview_create(screen1);
    lv_obj_set_style_bg_color(game_window, lv_color_hex(0x000000), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(game_window, 200, LV_PART_MAIN);
    lv_obj_clear_flag(game_window, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_outline_width(game_window, 6, LV_PART_MAIN);
    lv_obj_set_style_outline_color(game_window, lv_color_hex(0xbb7700), LV_PART_MAIN);
    lv_obj_center(game_window);
    lv_obj_set_size(game_window, XXL_SIZE_W * screen_ratio, XXL_SIZE_H * screen_ratio);

    refs_btn = lv_img_create(screen1);
    lv_img_set_src(refs_btn, &refs_btn_img);
    lv_obj_align(refs_btn, LV_ALIGN_TOP_RIGHT, -10, -10);
    lv_obj_add_flag(refs_btn, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(refs_btn, map_refs, LV_EVENT_CLICKED, 0);
    lv_img_set_zoom(refs_btn, 150);

    exit_btn = lv_img_create(screen1);
    lv_img_set_src(exit_btn, &exit_img);
    lv_obj_align(exit_btn, LV_ALIGN_TOP_LEFT, -10, -20);
    lv_obj_add_flag(exit_btn, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(exit_btn, exit_game_cb, LV_EVENT_CLICKED, 0);
    lv_img_set_zoom(exit_btn, 130);

    coin = lv_img_create(screen1);
    lv_img_set_src(coin, &coin_img);
    lv_img_set_zoom(coin, 130);
    lv_obj_align(coin, LV_ALIGN_TOP_MID, 0, -10);

    score = 0;

    score_lable = lv_label_create(screen1);
    lv_label_set_text_fmt(score_lable, "SCORE:%d", score);
    lv_obj_set_style_text_font(score_lable, &lv_font_montserrat_24, 0);
    lv_obj_align(score_lable, LV_ALIGN_TOP_MID, 0, 50);
    lv_obj_set_style_text_color(score_lable, lv_color_hex(0x00695C), LV_PART_MAIN);

    game_init();
}

static void game_init()
{
    int i, j;
    lv_obj_refr_size(game_window);

    for (j = 0; j < 8; j++) {
        for (i = 0; i < 8; i++) {
            game_obj[j][i].x = i;
            game_obj[j][i].y = j;
            game_obj[j][i].alive = 1;
            game_obj[j][i].color_index = rand() % 7;
            game_obj[j][i].obj = lv_btn_create(game_window);
            lv_obj_set_pos(game_obj[j][i].obj, i * XXL_IMG_W * screen_ratio + 1, j * XXL_IMG_H * screen_ratio + 1);
            lv_obj_set_size(game_obj[j][i].obj, XXL_IMG_W * screen_ratio - 2, XXL_IMG_H * screen_ratio - 2);
            lv_obj_set_style_bg_color(game_obj[j][i].obj, lv_color_hex(color_lib[game_obj[j][i].color_index]), 0);
            game_obj[j][i].obj->user_data = &game_obj[j][i];
            lv_obj_add_event_cb(game_obj[j][i].obj, move_obj_cb, LV_EVENT_PRESSING, 0);
            lv_obj_add_event_cb(game_obj[j][i].obj, move_obj_cb, LV_EVENT_RELEASED, 0);
        }
    }
    if (map_is_full() && same_color_check()) {
        same_color_flash();
        lv_obj_clear_flag(refs_btn, LV_OBJ_FLAG_CLICKABLE);
    }
}

static void move_obj_cb(lv_event_t *e)
{
    static lv_point_t click_point1, click_point2;
    int movex, movey, direction;
    static int c;
    static bool touched = false;
    game_obj_type temp;
    game_obj_type *stage_data = (game_obj_type *)lv_obj_get_user_data(e->current_target);
    lv_obj_t *xxx = (lv_obj_t *)e->current_target;
    if (touched == false) {
        touched = true;
        lv_indev_get_point(lv_indev_get_act(), &click_point1);
        return;
    }
    if (e->code == LV_EVENT_RELEASED) {
        touched = false;
        lv_obj_move_foreground(xxx);
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
            if (stage_data->y == 0) {
                return;
            } else {
                exchange_obj(stage_data, &game_obj[stage_data->y - 1][stage_data->x]);
            }
        }

        if (direction == down) {
            if (stage_data->y == 7) {
                return;
            } else {
                exchange_obj(stage_data, &game_obj[stage_data->y + 1][stage_data->x]);
            }
        }

        if (direction == left) {
            if (stage_data->x == 0) {
                return;
            } else {
                exchange_obj(stage_data, &game_obj[stage_data->y][stage_data->x - 1]);
            }
        }

        if (direction == right) {
            if (stage_data->x == 7) {
                return;
            } else {
                exchange_obj(stage_data, &game_obj[stage_data->y][stage_data->x + 1]);
            }
        }
    }
}

static void exchange_obj(game_obj_type *obj1, game_obj_type *obj2)
{
    game_obj_type temp;

    clear_all_clickable();

    temp.color_index = obj1->color_index;
    temp.obj = obj1->obj;

    obj1->color_index = obj2->color_index;
    obj2->color_index = temp.color_index;

    lv_anim_t a1;
    lv_anim_init(&a1);
    lv_anim_set_var(&a1, obj1->obj);
    lv_anim_set_exec_cb(&a1, x_move_cb);
    lv_anim_set_time(&a1, 200);
    if (!has_same_color()) {
        lv_anim_set_playback_time(&a1, 200);
    }
    lv_anim_set_values(&a1, obj1->x * XXL_IMG_W * screen_ratio + 1, obj2->x * XXL_IMG_H * screen_ratio + 1);
    lv_anim_set_path_cb(&a1, lv_anim_path_ease_out);
    lv_anim_start(&a1);

    lv_anim_t a2;
    lv_anim_init(&a2);
    lv_anim_set_var(&a2, obj1->obj);
    lv_anim_set_exec_cb(&a2, y_move_cb);
    lv_anim_set_time(&a2, 200);
    if (!has_same_color()) {
        lv_anim_set_playback_time(&a2, 200);
    }
    lv_anim_set_values(&a2, obj1->y * XXL_IMG_W * screen_ratio + 1, obj2->y * XXL_IMG_H * screen_ratio + 1);
    lv_anim_set_path_cb(&a2, lv_anim_path_ease_out);
    lv_anim_start(&a2);

    lv_anim_t a3;
    lv_anim_init(&a3);
    lv_anim_set_var(&a3, obj2->obj);
    lv_anim_set_exec_cb(&a3, x_move_cb);
    lv_anim_set_time(&a3, 200);
    if (!has_same_color()) {
        lv_anim_set_playback_time(&a3, 200);
    }
    lv_anim_set_values(&a3, obj2->x * XXL_IMG_W * screen_ratio + 1, obj1->x * XXL_IMG_H * screen_ratio + 1);
    lv_anim_set_path_cb(&a3, lv_anim_path_ease_out);
    lv_anim_start(&a3);

    lv_anim_t a4;
    lv_anim_init(&a4);
    lv_anim_set_var(&a4, obj2->obj);
    lv_anim_set_exec_cb(&a4, y_move_cb);
    lv_anim_set_deleted_cb(&a4, exchange_done_cb);
    lv_anim_set_time(&a4, 200);
    if (!has_same_color()) {
        lv_anim_set_playback_time(&a4, 200);
    }
    lv_anim_set_values(&a4, obj2->y * XXL_IMG_W * screen_ratio + 1, obj1->y * XXL_IMG_H * screen_ratio + 1);
    lv_anim_set_path_cb(&a4, lv_anim_path_ease_out);
    lv_anim_start(&a4);

    if (has_same_color()) {
        obj1->obj = obj2->obj;
        obj2->obj = temp.obj;
        obj1->obj->user_data = obj1;
        obj2->obj->user_data = obj2;
    } else {
        obj2->color_index = obj1->color_index;
        obj1->color_index = temp.color_index;
    }
}

static void x_move_cb(void *var, int32_t v)
{
    lv_obj_t *xxx = (lv_obj_t *)var;
    lv_obj_set_x(xxx, v);
}

static void y_move_cb(void *var, int32_t v)
{
    lv_obj_t *xxx = (lv_obj_t *)var;
    lv_obj_set_y(xxx, v);
}

static void flash_cb(void *var, int32_t v)
{
    lv_obj_t *xxx = (lv_obj_t *)var;
    lv_obj_set_style_bg_opa(xxx, 255 * (v % 2), 0);
}

static void exchange_done_cb(lv_anim_t *a)
{
    if (same_color_check()) {
        same_color_flash();
    } else {
        add_all_clickable();
    }
}

static bool has_same_color()
{
    int i, j;
    for (i = 0; i < 8; i++) {
        for (j = 0; j < 6; j++) {
            if (game_obj[j][i].color_index == game_obj[j + 1][i].color_index &&
                game_obj[j][i].color_index == game_obj[j + 2][i].color_index) {
                return true;
            }
        }
    }

    for (j = 0; j < 8; j++) {
        for (i = 0; i < 6; i++) {
            if (game_obj[j][i].color_index == game_obj[j][i + 1].color_index &&
                game_obj[j][i].color_index == game_obj[j][i + 2].color_index) {
                return true;
            }
        }
    }
    return false;
}

static int same_color_check()
{
    int i, j, m;
    m = 0;

    for (i = 0; i < 8; i++) {
        for (j = 0; j < 6; j++) {
            if (game_obj[j][i].color_index == game_obj[j + 1][i].color_index &&
                game_obj[j][i].color_index == game_obj[j + 2][i].color_index) {
                game_obj[j][i].alive = 0;
                game_obj[j + 1][i].alive = 0;
                game_obj[j + 2][i].alive = 0;
                m++;
            }
        }
    }

    for (j = 0; j < 8; j++) {
        for (i = 0; i < 6; i++) {
            if (game_obj[j][i].color_index == game_obj[j][i + 1].color_index &&
                game_obj[j][i].color_index == game_obj[j][i + 2].color_index) {
                game_obj[j][i].alive = 0;
                game_obj[j][i + 1].alive = 0;
                game_obj[j][i + 2].alive = 0;
                m++;
            }
        }
    }
    return m;
}

static void same_color_move_to_coin()
{
    int i, j, k;

    for (j = 0; j < 8; j++) {
        for (i = 0; i < 8; i++) {
            if (game_obj[j][i].alive == 0) {
                lv_obj_set_parent(game_obj[j][i].obj, screen1);
                lv_anim_t a1;
                lv_anim_init(&a1);
                lv_anim_set_var(&a1, game_obj[j][i].obj);
                lv_anim_set_exec_cb(&a1, x_move_cb);
                lv_anim_set_time(&a1, 600);
                lv_anim_set_path_cb(&a1, lv_anim_path_ease_in_out);
                lv_anim_set_values(&a1, (i) * XXL_IMG_W * screen_ratio + 1 + lv_obj_get_x(game_window), 0);
                lv_anim_start(&a1);

                lv_anim_t a2;
                lv_anim_init(&a2);
                lv_anim_set_var(&a2, game_obj[j][i].obj);
                lv_anim_set_exec_cb(&a2, y_move_cb);
                lv_anim_set_time(&a2, 600);
                lv_anim_set_path_cb(&a2, lv_anim_path_ease_in_out);
                lv_anim_set_deleted_cb(&a2, move_to_coin_end_cb);
                lv_anim_set_values(&a2, (j) * XXL_IMG_W * screen_ratio + 1 + lv_obj_get_y(game_window), 0);
                lv_anim_start(&a2);
            }
        }
    }
}

static void same_color_flash()
{
    int i, j, k;

    for (j = 0; j < 8; j++) {
        for (i = 0; i < 8; i++) {
            if (game_obj[j][i].alive == 0) {
                lv_anim_t a2;
                lv_anim_init(&a2);
                lv_anim_set_var(&a2, game_obj[j][i].obj);
                lv_anim_set_exec_cb(&a2, flash_cb);
                lv_anim_set_time(&a2, 600);
                lv_anim_set_deleted_cb(&a2, flash_end_cb);
                lv_anim_set_values(&a2, 1, 7);
                lv_anim_start(&a2);
            }
        }
    }
}

static void obj_move_down()
{
    int i, j, k, m;
    for (i = 0; i < 8; i++) {
        for (j = 7; j > 0; j--) {
            if (game_obj[j][i].alive == 0) {
                for (k = j; k > 0; k--) {
                    game_obj[k][i].alive = game_obj[k - 1][i].alive;
                    game_obj[k][i].obj = game_obj[k - 1][i].obj;
                    game_obj[k][i].color_index = game_obj[k - 1][i].color_index;

                    if (game_obj[k][i].alive) {
                        game_obj[k][i].obj->user_data = &game_obj[k][i];
                        lv_anim_t a1;
                        lv_anim_init(&a1);
                        lv_anim_set_var(&a1, game_obj[k][i].obj);
                        lv_anim_set_exec_cb(&a1, y_move_cb);
                        lv_anim_set_time(&a1, 150);
                        lv_anim_set_deleted_cb(&a1, move_deleted_cb);
                        lv_anim_set_values(&a1, (k - 1) * XXL_IMG_W * screen_ratio + 1, k * XXL_IMG_H * screen_ratio + 1);
                        lv_anim_start(&a1);
                    }
                }

                game_obj[0][i].x = i;
                game_obj[0][i].y = 0;
                game_obj[0][i].alive = 1;
                game_obj[0][i].color_index = rand() % 7;
                game_obj[0][i].obj = lv_btn_create(game_window);
                lv_obj_set_pos(game_obj[0][i].obj, i * XXL_IMG_W * screen_ratio + 1, -1 * XXL_IMG_H * screen_ratio + 1);
                lv_obj_set_size(game_obj[0][i].obj, XXL_IMG_W * screen_ratio - 2, XXL_IMG_H * screen_ratio - 2);
                lv_obj_set_style_bg_color(game_obj[0][i].obj, lv_color_hex(color_lib[game_obj[0][i].color_index]), 0);
                game_obj[0][i].obj->user_data = &game_obj[0][i];
                lv_obj_add_event_cb(game_obj[0][i].obj, move_obj_cb, LV_EVENT_PRESSING, 0);
                lv_obj_add_event_cb(game_obj[0][i].obj, move_obj_cb, LV_EVENT_RELEASED, 0);

                lv_anim_t a;
                lv_anim_init(&a);
                lv_anim_set_var(&a, game_obj[0][i].obj);
                lv_anim_set_exec_cb(&a, y_move_cb);
                lv_anim_set_time(&a, 150);
                lv_anim_set_deleted_cb(&a, move_deleted_cb);
                lv_anim_set_values(&a, (-1) * XXL_IMG_W * screen_ratio + 1, 0 * XXL_IMG_H * screen_ratio + 1);
                lv_anim_start(&a);
                break;
            }
        }

        if (game_obj[0][i].alive == 0) {
            game_obj[0][i].x = i;
            game_obj[0][i].y = 0;
            game_obj[0][i].alive = 1;
            game_obj[0][i].color_index = rand() % 7;
            game_obj[0][i].obj = lv_btn_create(game_window);
            lv_obj_set_pos(game_obj[0][i].obj, i * XXL_IMG_W * screen_ratio + 1, -1 * XXL_IMG_H * screen_ratio + 1);
            lv_obj_set_size(game_obj[0][i].obj, XXL_IMG_W * screen_ratio - 2, XXL_IMG_H * screen_ratio - 2);
            lv_obj_set_style_bg_color(game_obj[0][i].obj, lv_color_hex(color_lib[game_obj[0][i].color_index]), 0);
            game_obj[0][i].obj->user_data = &game_obj[0][i];
            lv_obj_add_event_cb(game_obj[0][i].obj, move_obj_cb, LV_EVENT_PRESSING, 0);
            lv_obj_add_event_cb(game_obj[0][i].obj, move_obj_cb, LV_EVENT_RELEASED, 0);

            lv_anim_t a2;
            lv_anim_init(&a2);
            lv_anim_set_var(&a2, game_obj[0][i].obj);
            lv_anim_set_exec_cb(&a2, y_move_cb);
            lv_anim_set_time(&a2, 150);
            lv_anim_set_deleted_cb(&a2, move_deleted_cb);
            lv_anim_set_values(&a2, (-1) * XXL_IMG_W * screen_ratio + 1, 0 * XXL_IMG_H * screen_ratio + 1);
            lv_anim_start(&a2);
        }
    }
}

static void move_deleted_cb(lv_anim_t *a)
{
    if (lv_anim_count_running() == 0) {
        obj_move_down();
    }

    if (map_is_full()) {
        if (same_color_check()) {
            same_color_flash();
            set_obj_userdata();
        } else {
            lv_obj_add_flag(refs_btn, LV_OBJ_FLAG_CLICKABLE);
            add_all_clickable();
        }
    }
}

static void set_obj_userdata()
{
    int i, j, k;

    for (j = 0; j < 8; j++) {
        for (i = 0; i < 8; i++) {
            if (lv_obj_is_valid(game_obj[j][i].obj))
                game_obj[j][i].obj->user_data = &game_obj[j][i];
        }
    }
}

static bool map_is_full()
{
    int i, j, k;
    for (j = 0; j < 8; j++) {
        for (i = 0; i < 8; i++) {
            if (game_obj[j][i].alive == 0)
                return false;
        }
    }
    return true;
}

static void map_del_all()
{
    int i, j, k;
    for (j = 0; j < 8; j++) {
        for (i = 0; i < 8; i++) {
            if (lv_obj_is_valid(game_obj[j][i].obj))
                lv_obj_del(game_obj[j][i].obj);
        }
    }
}

static void map_refs(lv_event_t *e)
{
    lv_obj_clear_flag(refs_btn, LV_OBJ_FLAG_CLICKABLE);
    map_del_all();
    game_init();
}

static void clear_all_clickable()
{
    int i, j;
    for (j = 0; j < 8; j++) {
        for (i = 0; i < 8; i++) {
            if (lv_obj_is_valid(game_obj[j][i].obj))
                lv_obj_clear_flag(game_obj[j][i].obj, LV_OBJ_FLAG_CLICKABLE);
        }
    }
}

static void add_all_clickable()
{
    int i, j;
    for (j = 0; j < 8; j++) {
        for (i = 0; i < 8; i++) {
            if (lv_obj_is_valid(game_obj[j][i].obj))
                lv_obj_add_flag(game_obj[j][i].obj, LV_OBJ_FLAG_CLICKABLE);
        }
    }
}

static void move_to_coin_end_cb(lv_anim_t *a)
{
    lv_obj_t *xxx = (lv_obj_t *)a->var;

    lv_obj_del(xxx);
    score += 10;

    if (lv_anim_count_running() == 0) {
        lv_label_set_text_fmt(score_lable, "SCORE:%d", score);
        obj_move_down();
    }
}

static void flash_end_cb(lv_anim_t *a)
{
    if (lv_anim_count_running() == 0) {
        same_color_move_to_coin();
    }
}

static void exit_game_cb(lv_event_t *e)
{
    lv_anim_del_all();
    lv_obj_del(screen1);
}
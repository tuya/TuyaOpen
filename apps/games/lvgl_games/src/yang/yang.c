#include "yang.h"
#include "lvgl.h"
#include "stdlib.h"

#define card_img_type_count 15
#define left_count          20
#define right_count         20
#define mid_count           100
#define bottom_count        8
#define max_layer           4
#define layer_row_count     6
#define layer_col_count     6
#define mid_x_start         30
#define mid_y_start         20
#define card_width          40
#define card_height         50
#define left_x_start        0
#define right_x_start       282
#define left_y_start        150
#define right_y_start       150
#define left_y_distance     5
#define right_y_distance    5
#define bottom_start_x      16
#define bottom_start_y      395

LV_IMG_DECLARE(grass2_img)
LV_IMG_DECLARE(grass1_img)
LV_IMG_DECLARE(yang_start_btn_img)
LV_IMG_DECLARE(bottom_dock_img)
LV_IMG_DECLARE(yang_card1_img)
LV_IMG_DECLARE(yang_card2_img)
LV_IMG_DECLARE(yang_card3_img)
LV_IMG_DECLARE(yang_card4_img)
LV_IMG_DECLARE(yang_card5_img)
LV_IMG_DECLARE(yang_card6_img)
LV_IMG_DECLARE(yang_card9_img)
LV_IMG_DECLARE(yang_card7_img)
LV_IMG_DECLARE(yang_card8_img)
LV_IMG_DECLARE(yang_card9_img)
LV_IMG_DECLARE(yang_card10_img)
LV_IMG_DECLARE(yang_card11_img)
LV_IMG_DECLARE(yang_card12_img)
LV_IMG_DECLARE(yang_card13_img)
LV_IMG_DECLARE(yang_card14_img)
LV_IMG_DECLARE(yang_card15_img)

typedef enum {
    left = 0,
    mid,
    right,
    bottom,

} area_enum;

typedef struct {
    lv_obj_t *obj;
    bool alive;
    char x;
    char y;
    area_enum area;
    char layer;
    char img_index;

} card_typedef;

card_typedef left_card[left_count] = {
    0,
};
card_typedef right_card[right_count] = {
    0,
};
card_typedef mid_card[max_layer * layer_col_count * layer_row_count] = {
    0,
};
card_typedef bottom_card[bottom_count] = {
    0,
};

static const lv_img_dsc_t *card_img[card_img_type_count] = {
    &yang_card1_img,  &yang_card2_img,  &yang_card3_img,  &yang_card4_img,  &yang_card5_img,
    &yang_card6_img,  &yang_card7_img,  &yang_card8_img,  &yang_card9_img,  &yang_card10_img,
    &yang_card11_img, &yang_card12_img, &yang_card13_img, &yang_card14_img, &yang_card15_img,
};
static const lv_img_dsc_t *grass_img[2] = {
    &grass1_img,
    &grass2_img,
};
lv_obj_t *screen, *map1, *start_btn, *dock;
static void game_start(lv_event_t *e);
static void cover_test();
static void clicked_cb(lv_event_t *e);
static void left_cover_test();
static void right_cover_test();
static void move_to_right(char index);
static char find_same_card(char img_index);
static void del_same_card();
static void x_move_cb(void *var, int32_t v);
static void y_move_cb(void *var, int32_t v);
static void move_to_left();
static void move_done_cb(lv_anim_t *a);
static void game_over();
static void card_anim_cb(void *var, int32_t v);
static void card_del_cb(lv_anim_t *a);
static void right_x_move_cb(void *var, int32_t v);

void yang_game(void)
{
#if LVGL_VERSION_MAJOR == 9
    memset(left_card, 0, sizeof(left_card));
    memset(right_card, 0, sizeof(right_card));
    memset(mid_card, 0, sizeof(mid_card));
    memset(bottom_card, 0, sizeof(bottom_card));
#else
    lv_memset_00(left_card, sizeof(left_card));
    lv_memset_00(right_card, sizeof(right_card));
    lv_memset_00(mid_card, sizeof(mid_card));
    lv_memset_00(bottom_card, sizeof(bottom_card));
#endif

    screen = lv_tileview_create(lv_scr_act());
    lv_obj_set_size(screen, 320, 480);
    lv_obj_set_style_bg_color(screen, lv_color_hex(0xcdfd8b), 0);
    lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);

    for (int i = 0; i < 20; i++) {
        lv_obj_t *g = lv_img_create(screen);
        lv_img_set_src(g, grass_img[rand() % 2]);
        lv_obj_set_pos(g, rand() % 450, rand() % 450);
    }

    dock = lv_img_create(screen);
    lv_img_set_src(dock, &bottom_dock_img);
    lv_obj_align(dock, LV_ALIGN_BOTTOM_MID, 0, -5);

    start_btn = lv_img_create(screen);
    lv_img_set_src(start_btn, &yang_start_btn_img);
    lv_obj_center(start_btn);
    lv_obj_add_flag(start_btn, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(start_btn, game_start, LV_EVENT_RELEASED, 0);
}

static void game_start(lv_event_t *e)
{
    int i, j;

    lv_obj_del((lv_obj_t *)e->current_target);

    for (i = 0; i < left_count; i++) {
        left_card[i].obj = lv_img_create(screen);
        left_card[i].alive = true;
        left_card[i].y = i;
        left_card[i].area = left;
        left_card[i].layer = i;
        left_card[i].img_index = rand() % card_img_type_count;
        lv_img_set_src(left_card[i].obj, card_img[left_card[i].img_index]);
        lv_obj_set_pos(left_card[i].obj, left_x_start, left_y_start + i * left_y_distance);
        left_card[i].obj->user_data = &left_card[i].obj;
        lv_obj_set_style_img_recolor(left_card[i].obj, lv_color_hex(0x000000), 0);
        lv_obj_set_style_img_recolor_opa(left_card[i].obj, 100, 0);
        lv_obj_clear_flag(left_card[i].obj, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(left_card[i].obj, clicked_cb, LV_EVENT_RELEASED, 0);
    }

    for (i = 0; i < right_count; i++) {
        right_card[i].obj = lv_img_create(screen);
        right_card[i].alive = true;
        right_card[i].y = i;
        right_card[i].area = right;
        right_card[i].layer = i;
        right_card[i].img_index = rand() % card_img_type_count;
        lv_img_set_src(right_card[i].obj, card_img[right_card[i].img_index]);
        lv_obj_set_pos(right_card[i].obj, right_x_start, right_y_start + i * right_y_distance);
        right_card[i].obj->user_data = &right_card[i].obj;
        lv_obj_set_style_img_recolor(right_card[i].obj, lv_color_hex(0x000000), 0);
        lv_obj_set_style_img_recolor_opa(right_card[i].obj, 100, 0);
        lv_obj_clear_flag(right_card[i].obj, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(right_card[i].obj, clicked_cb, LV_EVENT_RELEASED, 0);
    }

    for (j = 0; j < max_layer; j++) {
        for (i = 0; i < layer_row_count * layer_col_count; i++) {
            mid_card[j * layer_row_count * layer_col_count + i].obj = lv_img_create(screen);
            mid_card[j * layer_row_count * layer_col_count + i].alive = true;
            mid_card[j * layer_row_count * layer_col_count + i].x = i % layer_row_count * 2 + j % 2;
            mid_card[j * layer_row_count * layer_col_count + i].y = i / layer_row_count * 2 + j % 2;
            mid_card[j * layer_row_count * layer_col_count + i].area = mid;
            mid_card[j * layer_row_count * layer_col_count + i].layer = j;
            mid_card[j * layer_row_count * layer_col_count + i].img_index = rand() % 10;
            lv_img_set_src(mid_card[j * layer_row_count * layer_col_count + i].obj,
                           card_img[mid_card[j * layer_row_count * layer_col_count + i].img_index]);
            lv_obj_set_pos(mid_card[j * layer_row_count * layer_col_count + i].obj,
                           mid_x_start + mid_card[j * layer_row_count * layer_col_count + i].x * card_width / 2,
                           mid_y_start + mid_card[j * layer_row_count * layer_col_count + i].y * card_height / 2);
            mid_card[j * layer_row_count * layer_col_count + i].obj->user_data =
                &mid_card[j * layer_row_count * layer_col_count + i];
            lv_obj_set_style_img_recolor(mid_card[j * layer_row_count * layer_col_count + i].obj,
                                         lv_color_hex(0x000000), 0);
            lv_obj_set_style_img_recolor_opa(mid_card[j * layer_row_count * layer_col_count + i].obj, 0, 0);
            lv_obj_add_flag(mid_card[j * layer_row_count * layer_col_count + i].obj, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_add_event_cb(mid_card[j * layer_row_count * layer_col_count + i].obj, clicked_cb, LV_EVENT_RELEASED,
                                0);
        }
    }
    cover_test();
    left_cover_test();
    right_cover_test();
}

static void clicked_cb(lv_event_t *e)
{
    int i, j, k, m;
    card_typedef *card = (card_typedef *)lv_obj_get_user_data(e->current_target);
    card->alive = false;

    cover_test();
    left_cover_test();
    right_cover_test();

    m = find_same_card(card->img_index);
    move_to_right(m);

    bottom_card[m].alive = true;
    bottom_card[m].obj = e->current_target;
    bottom_card[m].img_index = card->img_index;
    lv_obj_clear_flag(bottom_card[m].obj, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_move_foreground(bottom_card[m].obj);

    lv_anim_t a1;
    lv_anim_init(&a1);
    lv_anim_set_var(&a1, bottom_card[m].obj);
    lv_anim_set_exec_cb(&a1, x_move_cb);
    lv_anim_set_time(&a1, 300);
    lv_anim_set_path_cb(&a1, lv_anim_path_ease_out);
    lv_anim_set_values(&a1, lv_obj_get_x(bottom_card[m].obj), bottom_start_x + m * (card_width + 2));
    lv_anim_start(&a1);

    lv_anim_t a2;
    lv_anim_init(&a2);
    lv_anim_set_var(&a2, bottom_card[m].obj);
    lv_anim_set_exec_cb(&a2, y_move_cb);
    lv_anim_set_time(&a2, 300);
    lv_anim_set_path_cb(&a2, lv_anim_path_ease_out);
    lv_anim_set_deleted_cb(&a2, move_done_cb);
    lv_anim_set_values(&a2, lv_obj_get_y(bottom_card[m].obj), bottom_start_y);
    lv_anim_start(&a2);

    del_same_card();
    move_to_left();
}

static void cover_test()
{
    int i, j;

    for (i = 0; i < layer_row_count * layer_col_count * (max_layer); i++) {
        if (mid_card[i].alive == true) {
            for (j = i + 1; j < layer_row_count * layer_col_count * max_layer; j++) {
                if (mid_card[j].alive == true && mid_card[j].layer > mid_card[i].layer) {
                    if ((mid_card[j].x - mid_card[i].x) < 2 && (mid_card[i].x - mid_card[j].x) < 2 &&
                        (mid_card[j].y - mid_card[i].y) < 2 && (mid_card[i].y - mid_card[j].y) < 2) {
                        lv_obj_set_style_img_recolor_opa(mid_card[i].obj, 100, 0);
                        lv_obj_clear_flag(mid_card[i].obj, LV_OBJ_FLAG_CLICKABLE);
                        break;
                    }
                }
                lv_obj_set_style_img_recolor_opa(mid_card[i].obj, 0, 0);
                lv_obj_add_flag(mid_card[i].obj, LV_OBJ_FLAG_CLICKABLE);
            }
        }
    }
    if (mid_card[layer_row_count * layer_col_count * max_layer - 1].alive == true)
        lv_obj_add_flag(mid_card[layer_row_count * layer_col_count * max_layer - 1].obj, LV_OBJ_FLAG_CLICKABLE);
}

static void left_cover_test()
{
    int i;

    for (i = left_count - 1; i >= 0; i--) {
        if (left_card[i].alive == true) {
            lv_obj_set_style_img_recolor_opa(left_card[i].obj, 0, 0);
            lv_obj_add_flag(left_card[i].obj, LV_OBJ_FLAG_CLICKABLE);
            return;
        }
    }
}

static void right_cover_test()
{
    int i;

    for (i = right_count - 1; i >= 0; i--) {
        if (right_card[i].alive == true) {
            lv_obj_set_style_img_recolor_opa(right_card[i].obj, 0, 0);
            lv_obj_add_flag(right_card[i].obj, LV_OBJ_FLAG_CLICKABLE);
            return;
        }
    }
}

static void move_to_right(char index)
{
    int i;

    for (i = 6; i > index; i--) {
        bottom_card[i].alive = bottom_card[i - 1].alive;
        bottom_card[i].img_index = bottom_card[i - 1].img_index;
        bottom_card[i].obj = bottom_card[i - 1].obj;
        bottom_card[i - 1].alive = false;

        if (bottom_card[i].alive == true) {
            lv_anim_t a1;
            lv_anim_init(&a1);
            lv_anim_set_var(&a1, bottom_card[i].obj);
            lv_anim_set_exec_cb(&a1, right_x_move_cb);
            lv_anim_set_time(&a1, 200);
            lv_anim_set_values(&a1, bottom_start_x + (i - 1) * (card_width + 2),
                               bottom_start_x + (i) * (card_width + 2));
            lv_anim_start(&a1);
        }
    }
}

static void move_to_left()
{
    int i, j;

    for (j = 0; j < 6; j++) {
        if (bottom_card[j].alive == false) {
            for (i = j + 1; i < 7; i++) {
                if (bottom_card[i].alive == true) {
                    bottom_card[j].alive = bottom_card[i].alive;
                    bottom_card[j].img_index = bottom_card[i].img_index;
                    bottom_card[j].obj = bottom_card[i].obj;
                    bottom_card[i].alive = false;

                    lv_anim_t a1;
                    lv_anim_init(&a1);
                    lv_anim_set_var(&a1, bottom_card[j].obj);
                    lv_anim_set_delay(&a1, 500);
                    lv_anim_set_exec_cb(&a1, x_move_cb);
                    lv_anim_set_time(&a1, 200);
                    lv_anim_set_values(&a1, bottom_start_x + (i) * (card_width + 2),
                                       bottom_start_x + (j) * (card_width + 2));
                    lv_anim_start(&a1);

                    break;
                }
            }
        }
    }
}

static char find_same_card(char img_index)
{
    int i, j, k;

    for (i = 5; i >= 0; i--) {
        if (bottom_card[i].alive && bottom_card[i].img_index == img_index) {
            return i + 1;
        }
    }

    for (i = 5; i >= 0; i--) {
        if (bottom_card[i].alive) {
            return i + 1;
        }
    }
    return 0;
}

static void del_same_card()
{
    int i, j, k;

    for (i = 0; i < 7; i++) {
        if (bottom_card[i].alive == true) {
            for (j = i + 1; j < 7; j++) {
                if (bottom_card[j].alive == true && bottom_card[i].img_index == bottom_card[j].img_index) {
                    for (k = j + 1; k < 7; k++) {
                        if (bottom_card[k].alive == true && bottom_card[j].img_index == bottom_card[k].img_index) {
                            bottom_card[i].alive = false;
                            bottom_card[j].alive = false;
                            bottom_card[k].alive = false;

                            lv_anim_t a1;
                            lv_anim_init(&a1);
                            lv_anim_set_var(&a1, bottom_card[i].obj);
                            lv_anim_set_exec_cb(&a1, card_anim_cb);
                            lv_anim_set_time(&a1, 300);
                            lv_anim_set_delay(&a1, 300);
                            lv_anim_set_deleted_cb(&a1, card_del_cb);
                            lv_anim_set_values(&a1, 256, 1);
                            lv_anim_start(&a1);

                            lv_anim_init(&a1);
                            lv_anim_set_var(&a1, bottom_card[j].obj);
                            lv_anim_set_exec_cb(&a1, card_anim_cb);
                            lv_anim_set_time(&a1, 300);
                            lv_anim_set_delay(&a1, 300);
                            lv_anim_set_deleted_cb(&a1, card_del_cb);
                            lv_anim_set_values(&a1, 256, 1);
                            lv_anim_start(&a1);

                            lv_anim_init(&a1);
                            lv_anim_set_var(&a1, bottom_card[k].obj);
                            lv_anim_set_exec_cb(&a1, card_anim_cb);
                            lv_anim_set_time(&a1, 300);
                            lv_anim_set_delay(&a1, 300);
                            lv_anim_set_deleted_cb(&a1, card_del_cb);
                            lv_anim_set_values(&a1, 256, 1);
                            lv_anim_start(&a1);
                        }
                    }
                }
            }
        }
    }
}

static void card_anim_cb(void *var, int32_t v)
{
    lv_img_set_zoom(var, v);
    lv_obj_set_style_img_opa(var, v - 1, 0);
}

static void card_del_cb(lv_anim_t *a)
{
    lv_obj_del(a->var);
}

static void x_move_cb(void *var, int32_t v)
{
    lv_obj_t *xxx = (lv_obj_t *)var;
    lv_obj_set_x(xxx, v);
}

static void right_x_move_cb(void *var, int32_t v)
{
    lv_obj_t *xxx = (lv_obj_t *)var;
    lv_obj_set_x(xxx, v);
}

static void y_move_cb(void *var, int32_t v)
{
    lv_obj_t *xxx = (lv_obj_t *)var;
    lv_obj_set_y(xxx, v);
}

static void move_done_cb(lv_anim_t *a)
{
    if (bottom_card[6].alive == true) {
        game_over();
    }
}

static void game_over()
{
    lv_anim_del_all();
    lv_obj_del(screen);
    yang_game();
}

#include "lvgl.h"
#include "stdlib.h"

#define max_zb_count             30   // ��ʬ�������
#define max_quantity             15   // ÿ��ֲ���������
#define zb_period                4000 // ��ʬ��������
#define fast_zb_period           1000 // ���ٲ�����ʬ
#define super_fast_zb_period     500  // �����ٲ�����ʬ
#define zidan_speed              7    // �ӵ�ÿ�η�5����
#define shine_period             7000 // ������������
#define plant_stackable          0    // ֲ��ɵ��ӷ��ã�1�ɵ��ӣ�0���ɵ��ӣ�
#define sunflower_add_sun_period 7000 // ���տ�������������ms
#define add_zidan_period         1000 // �ϵ�����ms
#define sunflower_price          50
#define wandou_price             50
#define wogua_price              50
#define lajiao_price             100
#define yingtao_price            100
#define hedan_price              200
#define jiguang_price            200
#define dici_price               150
#define start_money              1000

static int chanzi_btn_select;
static lv_obj_t *jiguangdou_btn, *start_btn, *gameover_btn;
static lv_obj_t *nuclear_btn;
static lv_obj_t *cherry_btn;
static lv_obj_t *wogua_btn;
static lv_obj_t *chanzi_btn;
static lv_obj_t *sunflower_btn;
static lv_obj_t *wandouflower_btn;
static lv_obj_t *botton_exit;
static lv_obj_t *map1;
static lv_obj_t *screen;
static lv_obj_t *lable_sun_score;
static lv_obj_t *lable_right;
static lv_obj_t *lable_down;
static lv_timer_t *timer_newzb, *timer_wogua_test;
static lv_timer_t *timer_movezb;
static lv_timer_t *timer_newshine;
static lv_timer_t *timer_zidan_fly;
static lv_timer_t *timer_hit_test;
static lv_timer_t *timer_car_test;
static lv_timer_t *timer_zidan_refr_pos;
static lv_obj_t *mini_btn;
static lv_obj_t *bar_btn;

static lv_obj_t *panel;
static lv_obj_t *panellable;
static lv_obj_t *btext;
static lv_obj_t *score_lable;
static lv_obj_t *next_lable;
static lv_obj_t *gameover_botton;
static lv_obj_t *gameover_lable;
static lv_obj_t *exit_lable;
static lv_obj_t *map1;
static int sun_score = 0, zb_count = 0;
static int offsetx, offsety, tuoching, win_old_x, win_old_y, win_old_w, win_old_h;
static lv_obj_t *window;
static lv_obj_t *close_btn;

LV_IMG_DECLARE(pvz_map_3)
LV_IMG_DECLARE(zb1)
LV_IMG_DECLARE(jump_zb)
LV_IMG_DECLARE(car_zb_img)
LV_IMG_DECLARE(qiqiu_zb_img)
LV_IMG_DECLARE(sunshine)
LV_IMG_DECLARE(wandou_img)
LV_IMG_DECLARE(sun_img)
LV_IMG_DECLARE(wogua_img)
LV_IMG_DECLARE(zidan_split_img)
LV_IMG_DECLARE(cherry_img)
LV_IMG_DECLARE(cherry_boom_img)
LV_IMG_DECLARE(nuclear_img)
LV_IMG_DECLARE(nuclear_boom_img)
LV_IMG_DECLARE(yingtao_btn_img)
LV_IMG_DECLARE(wogua_btn_img)
LV_IMG_DECLARE(wandou_btn_img)
LV_IMG_DECLARE(sun_btn_img)
LV_IMG_DECLARE(hedan_btn_img)
LV_IMG_DECLARE(lajiao_btn_img)
LV_IMG_DECLARE(huoyan_img)
LV_IMG_DECLARE(lajiao_img)
LV_IMG_DECLARE(jiguangdou_img)
LV_IMG_DECLARE(jiguangdou_btn_img)
LV_IMG_DECLARE(jiguangkou_img)
LV_IMG_DECLARE(wave_zb_come_img)
LV_IMG_DECLARE(little_car_img)
LV_IMG_DECLARE(start_game_img)
LV_IMG_DECLARE(game_over_img)
LV_IMG_DECLARE(dici_img)
LV_IMG_DECLARE(dici_btn_img)

enum {
    Normalzb = 0,
    jumpzb,
    carzb,
    qiqiuzb,
};

typedef enum {
    sunflower_card = 0,
    wandou_card,
    wogua_card,
    lajiao_card,
    yingtao_card,
    hedan_card,
    jiguangdou_card,
    dici_card,
} card_type_enum;

struct _zb_class_type;
struct _zb_type;
struct _card_btn_type;
typedef void (*zb_del_cb_t)(struct _zb_type *);
typedef void (*plant_creat_cb_t)(struct _card_btn_type *, int, int);

typedef struct _card_btn_type {
    lv_obj_t *card_btn;
    void *img_src;
    card_type_enum card_type;
    bool ispushed;
    lv_event_cb_t event_cb;
    unsigned int needscore;
    plant_creat_cb_t plant_creat_cb;
} card_btn_type;

typedef struct _zb_class_type {
    unsigned char zb_kind;
    void *zb_img_src;
    unsigned int max_blood;
    unsigned int move_time;
    lv_anim_exec_xcb_t exec_cb;
    zb_del_cb_t zb_del_cb;
    zb_del_cb_t zb_start_anim_cb;
} zb_class_type;

typedef struct _zb_type {
    lv_obj_t *zb;
    const zb_class_type *zb_class;
    int alive;
    int blood;
    int x;
    int y;
} zb_type;

typedef struct {
    lv_obj_t *zidan;
    int alive;
    int x;
    int y;
} zidan_type;

typedef struct {
    lv_obj_t *plant;
    lv_timer_t *timer;
    int blood;
    int alive;
    int x;
    int y;
} plant_type;

typedef struct {
    lv_obj_t *shine;
    int alive;
    int x;
    int y;
} shine_type;

static zb_type zb_matrix[max_zb_count] = {
    0,
};
static char map_flag[5][9] = {
    0,
};
static plant_type wogua[max_quantity] = {
    0,
};
static plant_type add_zidan_timer[max_quantity] = {
    0,
};
static plant_type sunflower[max_quantity] = {
    0,
};
static plant_type wandouflower[max_quantity] = {
    0,
};
static plant_type jiguangdou[max_quantity] = {
    0,
};
static plant_type dici[max_quantity] = {
    0,
};
static shine_type shine[max_quantity] = {
    0,
};
static zidan_type zidan[max_quantity] = {
    0,
};
static zidan_type little_car[5] = {
    0,
};
static void zb_create_cb(lv_timer_t *t);
static void timer_cb2(lv_timer_t *t);
static void newshine_cb(lv_timer_t *t);
static void shine_del_cb1(lv_event_t *e);
static void anim_cb1(void *var, int32_t v);
static void anim_cb2(void *var, int32_t v);
static void cube_ready();
static void init_all();
static void lv_anim_exec_xcb(void *var, int32_t v);
static void ready_cb(lv_anim_t *var);
static void print_cubematrix();
static void all_clear(lv_event_t *e);
static void gameoverbotton_event_cb(lv_event_t *e);
static void shine_delect_cb(lv_anim_t *a);
static void sunflower_btn_cb(lv_event_t *e);
static void wandouflower_btn_cb(lv_event_t *e);
static void map_click_cb(lv_event_t *e);
static void add_zidan_cb(lv_timer_t *t);
static void timer_car_test_cb(lv_timer_t *t);
static void zidan_move();
static void hit_test(lv_timer_t *t);
static void anim_zb_dead_cb(void *var, int32_t v);
static void anim_zb_delect_cb(lv_anim_t *a);
static void shine_start_cb1(void *var, int32_t v);
static void chanzi_btn_cb(lv_event_t *e);
static void sunflower_creat_shine_cb(lv_timer_t *t);
static void wogua_btn_cb(lv_event_t *e);
static void nuclear_btn_cb(lv_event_t *e);
static void wogua_dead_cb(void *var, int32_t v);
static void wogua_dead_cb2(void *var, int32_t v);
static void wogua_delect_cb(lv_anim_t *a);
static void zb_set_angle_cb(void *var, int32_t v);
static void timer_led_cb(lv_timer_t *t);
static void exit_game_cb(lv_event_t *e);
static void plant_anim_cb(void *var, int32_t v);
static void zb_del_start_cb(lv_anim_t *a);
static void cherry_delete_cb(lv_anim_t *a);
static void cherry_anim_cb(void *var, int32_t v);
static void boom_delete_cb(lv_anim_t *a);
static void boom_anim_cb(void *var, int32_t v);
static void cherry_btn_cb(lv_event_t *e);
static void nuclear_boom_delete_cb(lv_anim_t *a);
static void nuclear_boom_anim_cb(void *var, int32_t v);
static void nuclear_delete_cb(lv_anim_t *a);
static void nuclear_anim_cb(void *var, int32_t v);
static void zb_dead_anim(zb_type *xzb);
static void carzb_boom_anim_cb(void *var, int32_t v);
static void anim_zb_dead_start_cb(lv_anim_t *a);
static void timer_zidan_refr_pos_cb(lv_timer_t *t);
static void jumpzb_jump_cb(void *var, int32_t v);
static void btn_cliked_cb(lv_event_t *e);
static void carzb_del_cb(zb_type *xzb);
static void zb_left_move_cb(void *var, int32_t v);
static void Normalzb_start_anim_cb(zb_type *xzb);
static void jumpzb_start_anim_cb(zb_type *xzb);
static void carzb_start_anim_cb(zb_type *xzb);
static void qiqiuzb_start_anim_cb(zb_type *xzb);
static void sunflower_creat_cb(card_btn_type *card_btn, int x, int y);
static void wandou_creat_cb(card_btn_type *card_btn, int x, int y);
static void wogua_creat_cb(card_btn_type *card_btn, int x, int y);
static void lajiao_creat_cb(card_btn_type *card_btn, int x, int y);
static void cherry_creat_cb(card_btn_type *card_btn, int x, int y);
static void hedan_creat_cb(card_btn_type *card_btn, int x, int y);
static void jiguangdou_creat_cb(card_btn_type *card_btn, int x, int y);
static void add_jiguang_cb(lv_timer_t *t);
static void jiguangdou_anim_cb(void *var, int32_t v);
static void jiguangdou_width_cb(void *var, int32_t v);
static void burn_zb_cb(void *var, int32_t v);
static void car_attack(int j);
static void car_start_move(void *var, int32_t v);
static void game_start(lv_event_t *e);
static void refs_card_btn(void);
static void game_over_cb(void);
static void game_over_img_ready_cb(lv_anim_t *a);
static void obj_set_x_anim(void *var, int32_t v);
static void wogua_delect_zb_cb(lv_anim_t *a);
static void timer_wogua_test_cb(lv_timer_t *t);
static void dici_creat_cb(card_btn_type *card_btn, int x, int y);
static void timer_dici_test_cb(lv_timer_t *t);
static void dici_anim_cb(void *var, int32_t v);

const static zb_class_type zb_class[] = {
    Normalzb, (void *)&zb1,          20, 150, 0, zb_dead_anim, Normalzb_start_anim_cb,
    jumpzb,   (void *)&jump_zb,      20, 100, 0, zb_dead_anim, jumpzb_start_anim_cb,
    carzb,    (void *)&car_zb_img,   30, 200, 0, carzb_del_cb, carzb_start_anim_cb,
    qiqiuzb,  (void *)&qiqiu_zb_img, 10, 50,  0, carzb_del_cb, qiqiuzb_start_anim_cb,
};

static card_btn_type card_btn[] = {
    0, (void *)&sun_btn_img,        sunflower_card,  0, btn_cliked_cb, sunflower_price, sunflower_creat_cb,
    0, (void *)&wandou_btn_img,     wandou_card,     0, btn_cliked_cb, wandou_price,    wandou_creat_cb,
    0, (void *)&wogua_btn_img,      wogua_card,      0, btn_cliked_cb, wogua_price,     wogua_creat_cb,
    0, (void *)&lajiao_btn_img,     lajiao_card,     0, btn_cliked_cb, lajiao_price,    lajiao_creat_cb,
    0, (void *)&yingtao_btn_img,    yingtao_card,    0, btn_cliked_cb, yingtao_price,   cherry_creat_cb,
    0, (void *)&dici_btn_img,       dici_card,       0, btn_cliked_cb, dici_price,      dici_creat_cb,
    0, (void *)&hedan_btn_img,      hedan_card,      0, btn_cliked_cb, hedan_price,     hedan_creat_cb,
    0, (void *)&jiguangdou_btn_img, jiguangdou_card, 0, btn_cliked_cb, jiguang_price,   jiguangdou_creat_cb,

};

void pvz_start(void)
{
#if LVGL_VERSION_MAJOR == 9
    memset(zb_matrix, 0, sizeof(zb_matrix));
    memset(map_flag, 0, sizeof(map_flag));
    memset(wogua, 0, sizeof(wogua));
    memset(add_zidan_timer, 0, sizeof(add_zidan_timer));
    memset(sunflower, 0, sizeof(sunflower));
    memset(wandouflower, 0, sizeof(wandouflower));
    memset(jiguangdou, 0, sizeof(jiguangdou));
    memset(shine, 0, sizeof(shine));
    memset(zidan, 0, sizeof(zidan));
    memset(little_car, 0, sizeof(little_car));
#else
    lv_memset_00(zb_matrix, sizeof(zb_matrix));
    lv_memset_00(map_flag, sizeof(map_flag));
    lv_memset_00(wogua, sizeof(wogua));
    lv_memset_00(add_zidan_timer, sizeof(add_zidan_timer));
    lv_memset_00(sunflower, sizeof(sunflower));
    lv_memset_00(wandouflower, sizeof(wandouflower));
    lv_memset_00(jiguangdou, sizeof(jiguangdou));
    lv_memset_00(shine, sizeof(shine));
    lv_memset_00(zidan, sizeof(zidan));
    lv_memset_00(little_car, sizeof(little_car));
#endif
    screen = lv_tileview_create(lv_scr_act());
    lv_obj_set_size(screen, 480, 320);
    lv_obj_center(screen);
    lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
    map1 = lv_img_create(screen);
    lv_obj_set_size(map1, 480, 320);
    lv_obj_center(screen);
    lv_img_set_src(map1, &pvz_map_3);
    lv_obj_clear_flag(map1, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(map1, map_click_cb, LV_EVENT_RELEASED, 0);

    start_btn = lv_img_create(map1);
    lv_img_set_src(start_btn, &start_game_img);
    lv_obj_center(start_btn);
    lv_obj_add_flag(start_btn, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(start_btn, game_start, LV_EVENT_RELEASED, 0);
}

static void game_start(lv_event_t *e)
{
    int i;

    score_lable = lv_label_create(map1);
    lv_label_set_text_fmt(score_lable, "%d", sun_score);
    lv_obj_set_style_text_color(score_lable, lv_color_hex(0xffffff), LV_PART_MAIN);
    lv_obj_set_pos(score_lable, 10, 30);

    for (i = 0; i < sizeof(card_btn) / sizeof(card_btn_type); i++) {
        card_btn[i].card_btn = lv_img_create(map1);
        lv_obj_set_pos(card_btn[i].card_btn, 52 + (i) * 33, 2);
        lv_img_set_src(card_btn[i].card_btn, card_btn[i].img_src);
        lv_obj_set_style_img_recolor(card_btn[i].card_btn, lv_color_hex(0x000000), LV_PART_MAIN);
        lv_obj_set_style_img_recolor_opa(card_btn[i].card_btn, 200, LV_PART_MAIN);
        lv_obj_set_style_outline_width(card_btn[i].card_btn, 0, LV_PART_MAIN);
        lv_obj_set_style_outline_color(card_btn[i].card_btn, lv_color_hex(0xffffff), LV_PART_MAIN);
        lv_obj_set_style_outline_opa(card_btn[i].card_btn, 255, LV_PART_MAIN);
        card_btn[i].card_btn->user_data = &card_btn[i];
        card_btn[i].ispushed = 0;

        lv_obj_t *price_lable = lv_label_create(card_btn[i].card_btn);
        lv_label_set_text_fmt(price_lable, "%d", card_btn[i].needscore);
        lv_obj_set_style_text_color(price_lable, lv_color_hex(0x555555), LV_PART_MAIN);
        lv_obj_set_align(price_lable, LV_ALIGN_BOTTOM_MID);
        lv_obj_set_style_text_font(price_lable, &lv_font_montserrat_14, 0);

        lv_obj_add_event_cb(card_btn[i].card_btn, card_btn[i].event_cb, LV_EVENT_RELEASED, 0);
    }

    botton_exit = lv_btn_create(map1);
    lv_obj_set_style_bg_color(botton_exit, lv_color_hex(0x6f3011), LV_PART_MAIN);
    lv_obj_set_pos(botton_exit, 400, 0);
    exit_lable = lv_label_create(botton_exit);
    lv_label_set_text(exit_lable, "<EXIT");
    lv_obj_set_style_text_color(exit_lable, lv_color_hex(0xffffff), LV_PART_MAIN);
    lv_obj_add_event_cb(botton_exit, exit_game_cb, LV_EVENT_RELEASED, 0);

    chanzi_btn = lv_btn_create(map1);
    lv_obj_set_pos(chanzi_btn, 350, -1);
    lv_obj_set_size(chanzi_btn, 43, 43);
    lv_obj_set_style_shadow_opa(chanzi_btn, 0, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(chanzi_btn, 0, LV_PART_MAIN);
    lv_obj_set_style_border_width(chanzi_btn, 2, LV_PART_MAIN);
    lv_obj_set_style_border_color(chanzi_btn, lv_color_hex(0xffffff), LV_PART_MAIN);
    lv_obj_set_style_border_opa(chanzi_btn, 0, LV_PART_MAIN);
    lv_obj_add_event_cb(chanzi_btn, chanzi_btn_cb, LV_EVENT_RELEASED, 0);

    sun_score = start_money;
    zb_count = 0;
    lv_label_set_text_fmt(score_lable, "%d", sun_score);
    chanzi_btn_select = 0;
    refs_card_btn();

    lv_obj_del((lv_obj_t *)e->current_target);

    lv_anim_t a;

    for (i = 0; i < 5; i++) {
        little_car[i].alive = 1;
        little_car[i].x = -55;
        little_car[i].y = i;
        little_car[i].zidan = lv_img_create(map1);
        lv_obj_set_pos(little_car[i].zidan, little_car[i].x, i * 55 + 65);
        lv_img_set_src(little_car[i].zidan, &little_car_img);
        little_car[i].zidan->user_data = &little_car[i];

        lv_anim_init(&a);
        lv_anim_set_var(&a, little_car[i].zidan);
        lv_anim_set_exec_cb(&a, obj_set_x_anim);
        lv_anim_set_time(&a, 500);
        lv_anim_set_values(&a, -55, -15);
        lv_anim_start(&a);
    }

    timer_newzb = lv_timer_create(zb_create_cb, zb_period, 0);
    timer_newshine = lv_timer_create(newshine_cb, shine_period, 0);
    timer_zidan_refr_pos = lv_timer_create(timer_zidan_refr_pos_cb, 20, 0);
    timer_car_test = lv_timer_create(timer_car_test_cb, 1000, 0);
    lv_timer_ready(timer_newshine);
}

static void obj_set_x_anim(void *var, int32_t v)
{
    lv_obj_set_x(var, v);
}

static void wogua_dead_cb(void *var, int32_t v)
{
    int x, y;
    lv_obj_t *user = (lv_obj_t *)var;

    x = lv_obj_get_x(user) + 1;
    y = lv_obj_get_y(user) - 1;
    lv_obj_set_pos(user, x, y);
}

static void wogua_dead_cb2(void *var, int32_t v)
{
    int y;
    lv_obj_t *user = (lv_obj_t *)var;

    y = lv_obj_get_y(user) + 1;
    lv_obj_set_y(user, y);
}

static void wogua_dead_cb3(void *var, int32_t v)
{
    lv_obj_t *user = (lv_obj_t *)var;
    lv_obj_set_style_img_opa(user, v * 10, LV_PART_MAIN);
}

static void wogua_delect_cb(lv_anim_t *a)
{

    plant_type *wogua = (plant_type *)(a->user_data);

    wogua->alive = 0;
    lv_obj_del(wogua->plant);
}

static void hit_zb_cb(void *var, int32_t v)
{
    lv_obj_t *user = (lv_obj_t *)var;
    lv_obj_set_style_img_recolor(user, lv_color_hex(0xffffff), 0);
    lv_obj_set_style_img_recolor_opa(user, 120 - v * 120, 0);
}

static void anim_zb_dead_start_cb(lv_anim_t *a)
{
    lv_obj_t *xzb = (lv_obj_t *)a->var;

    lv_obj_set_style_img_recolor(xzb, lv_color_hex(0x000000), 0);
    lv_obj_set_style_img_recolor_opa(xzb, 255, 0);
}

static void carzb_del_cb(zb_type *xzb)
{
    lv_img_set_src(xzb->zb, &cherry_boom_img);
    lv_obj_set_y(xzb->zb, lv_obj_get_y(xzb->zb) + 30);

    lv_anim_del(xzb->zb, jumpzb_jump_cb);

    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, xzb->zb);
    lv_anim_set_exec_cb(&a, carzb_boom_anim_cb);
    lv_anim_set_time(&a, 1000);
    lv_anim_set_values(&a, 0, 15);
    lv_anim_set_user_data(&a, xzb);
    lv_anim_set_ready_cb(&a, anim_zb_delect_cb);
    lv_anim_start(&a);
}

static void zb_dead_anim(zb_type *xzb)
{

    if (xzb->zb_class->zb_kind == Normalzb) {
        lv_anim_del(xzb->zb, zb_set_angle_cb);
    }
    if (xzb->zb_class->zb_kind == jumpzb) {
        lv_anim_del(xzb->zb, jumpzb_jump_cb);
    }

    lv_img_set_pivot(xzb->zb, 15, 59);

    lv_anim_del(xzb->zb, hit_zb_cb);

    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, xzb->zb);
    lv_anim_set_exec_cb(&a, anim_zb_dead_cb);
    lv_anim_set_start_cb(&a, anim_zb_dead_start_cb);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_in);
    lv_anim_set_time(&a, 2000);
    lv_anim_set_values(&a, 0, 200);
    lv_anim_set_user_data(&a, xzb);
    lv_anim_set_ready_cb(&a, anim_zb_delect_cb);
    lv_anim_start(&a);
}

static void nuclear_anim_cb(void *var, int32_t v)
{
    lv_obj_t *nuclear = (lv_obj_t *)var;
    lv_obj_set_y(nuclear, v);
    lv_img_set_zoom(nuclear, 155 - v);
}

static void nuclear_delete_cb(lv_anim_t *a)
{
    int i;
    lv_obj_t *nuclear = (lv_obj_t *)a->var;

    lv_obj_t *nuclear_boom = lv_img_create(map1);
    lv_img_set_src(nuclear_boom, &nuclear_boom_img);
    lv_obj_center(nuclear_boom);

    lv_anim_t a1;
    lv_anim_init(&a1);
    lv_anim_set_var(&a1, nuclear_boom);
    lv_anim_set_exec_cb(&a1, nuclear_boom_anim_cb);
    lv_anim_set_ready_cb(&a1, nuclear_boom_delete_cb);
    lv_anim_set_time(&a1, 1000);
    lv_anim_set_values(&a1, 0, 20);
    lv_anim_start(&a1);

    for (i = 0; i < max_zb_count; i++) {
        if (zb_matrix[i].blood > 0) {
            zb_matrix[i].blood = 0;
            zb_matrix[i].zb_class->zb_del_cb(&zb_matrix[i]);
        }
    }

    lv_obj_del(nuclear);
}

static void carzb_boom_anim_cb(void *var, int32_t v)
{
    lv_obj_t *cherry = (lv_obj_t *)var;

    lv_img_set_zoom(cherry, 255 + v * 15);
    lv_obj_set_style_img_opa(cherry, 255 - v * 15, 0);
}

static void cherry_anim_cb(void *var, int32_t v)
{
    lv_obj_t *cherry = (lv_obj_t *)var;
    lv_img_set_zoom(cherry, 255 + v * 15);
}

static void lajiao_anim_cb(void *var, int32_t v)
{
    lv_obj_t *cherry = (lv_obj_t *)var;

    lv_img_set_zoom(cherry, 255 + v * 15);
}

static void cherry_delete_cb(lv_anim_t *a)
{
    int i;
    lv_obj_t *cherry = (lv_obj_t *)a->var;
    int x = lv_obj_get_x(cherry);
    int y = lv_obj_get_y(cherry);

    lv_obj_t *boom = lv_img_create(map1);
    lv_img_set_src(boom, &cherry_boom_img);
    lv_img_set_zoom(boom, 400);
    lv_obj_set_pos(boom, x - 30, y - 10);

    lv_anim_t a1;
    lv_anim_init(&a1);
    lv_anim_set_var(&a1, boom);
    lv_anim_set_exec_cb(&a1, boom_anim_cb);
    lv_anim_set_ready_cb(&a1, boom_delete_cb);
    lv_anim_set_time(&a1, 500);
    lv_anim_set_values(&a1, 0, 1);
    lv_anim_start(&a1);

    for (i = 0; i < max_zb_count; i++) {
        if (zb_matrix[i].blood > 0 && lv_obj_get_x(zb_matrix[i].zb) - x < 80 &&
            x - lv_obj_get_x(zb_matrix[i].zb) < 80 && lv_obj_get_y(zb_matrix[i].zb) - y < 80 &&
            y - lv_obj_get_y(zb_matrix[i].zb) < 80) {
            zb_matrix[i].blood = 0;
            zb_matrix[i].zb_class->zb_del_cb(&zb_matrix[i]);
        }
    }

    lv_obj_del(cherry);
}

static void lajiao_delete_cb(lv_anim_t *a)
{
    int i;
    lv_obj_t *lajiao = (lv_obj_t *)a->var;

    int y = (lv_obj_get_y(lajiao) - 44) / 54;

    lv_obj_t *huoyan = lv_img_create(map1);
    lv_img_set_src(huoyan, &huoyan_img);
    lv_obj_set_y(huoyan, y * 54 + 60);
    lv_obj_set_size(huoyan, 480, 30);

#if LVGL_VERSION_MAJOR == 9
    lv_obj_delete_delayed(huoyan, 500);
#else
    lv_obj_del_delayed(huoyan, 500);
#endif

    for (i = 0; i < max_zb_count; i++) {
        if (zb_matrix[i].blood > 0 && y == zb_matrix[i].y) {
            zb_matrix[i].blood = 0;
            zb_matrix[i].zb_class->zb_del_cb(&zb_matrix[i]);
        }
    }
    lv_obj_del(lajiao);
}

static void boom_delete_cb(lv_anim_t *a)
{
    lv_obj_t *boom = (lv_obj_t *)a->var;
    lv_obj_del(boom);
}

static void boom_anim_cb(void *var, int32_t v)
{
    return;
}

static void nuclear_boom_anim_cb(void *var, int32_t v)
{
    lv_obj_t *nuclear_boom = (lv_obj_t *)var;
    lv_img_set_zoom(nuclear_boom, 255 + v * 20);
}

static void nuclear_boom_delete_cb(lv_anim_t *a)
{
    lv_obj_t *nuclear_boom = (lv_obj_t *)a->var;

    lv_obj_del(nuclear_boom);
}

static void anim_zb_dead_cb(void *var, int32_t v)
{
    lv_obj_t *user = (lv_obj_t *)var;

    if (v < 80) {
        lv_img_set_angle(user, v * 11);
    } else {
        lv_obj_set_style_img_opa(user, 415 - 2 * v, 0);
    }
}

static void plant_anim_cb(void *var, int32_t v)
{
    plant_type *xxx = (plant_type *)var;

    lv_img_set_angle(xxx->plant, -v * 10);

    if (v >= 0) {
        lv_obj_set_y(xxx->plant, xxx->y * 54 + 44 + v / 2);
    } else {
        lv_obj_set_y(xxx->plant, xxx->y * 54 + 44 - v / 2);
    }
}

static void anim_zb_delect_cb(lv_anim_t *a)
{
    zb_type *user = (zb_type *)a->user_data;
    user->alive = 0;
    user->blood = 0;
    lv_anim_del(user, zb_left_move_cb);
    lv_obj_del(user->zb);
}

static void zb_create_cb(lv_timer_t *t)
{

    int j;
    zb_count++;

    if (zb_count == 10) {
        lv_timer_set_period(timer_newzb, fast_zb_period);
        lv_obj_t *wavelable = lv_img_create(map1);
        lv_obj_center(wavelable);
        lv_img_set_src(wavelable, &wave_zb_come_img);
#if LVGL_VERSION_MAJOR == 9
        lv_obj_delete_delayed(wavelable, 2000);
#else
        lv_obj_del_delayed(wavelable, 2000);
#endif
    }

    if (zb_count == 100) {
        lv_timer_set_period(timer_newzb, super_fast_zb_period);

        lv_obj_t *wavelable = lv_img_create(map1);
        lv_obj_center(wavelable);
        lv_img_set_src(wavelable, &wave_zb_come_img);
#if LVGL_VERSION_MAJOR == 9
        lv_obj_delete_delayed(wavelable, 2000);
#else
        lv_obj_del_delayed(wavelable, 2000);
#endif
    }

    for (j = 0; j < max_zb_count; j++) {
        if (zb_matrix[j].alive == 0) {

            zb_matrix[j].zb_class = &zb_class[rand() % 4];
            zb_matrix[j].alive = 1;
            zb_matrix[j].blood = zb_matrix[j].zb_class->max_blood + zb_count / 100;
            zb_matrix[j].zb = lv_img_create(map1);
            lv_img_set_src(zb_matrix[j].zb, zb_matrix[j].zb_class->zb_img_src);
            zb_matrix[j].y = rand() % 5;
            zb_matrix[j].x = 480;
            lv_obj_set_pos(zb_matrix[j].zb, 480, zb_matrix[j].y * 54 + 35);
            lv_img_set_pivot(zb_matrix[j].zb, 15, 59);

            lv_anim_t a1;
            lv_anim_init(&a1);
            lv_anim_set_var(&a1, &zb_matrix[j]);
            lv_anim_set_exec_cb(&a1, zb_left_move_cb);
            lv_anim_set_time(&a1, zb_matrix[j].zb_class->move_time * 10);
            lv_anim_set_values(&a1, 0, 10);
            lv_anim_set_delay(&a1, zb_matrix[j].zb_class->move_time);
            lv_anim_set_repeat_delay(&a1, zb_matrix[j].zb_class->move_time);
            lv_anim_set_repeat_count(&a1, LV_ANIM_REPEAT_INFINITE);
            lv_anim_start(&a1);

            zb_matrix[j].zb_class->zb_start_anim_cb(&zb_matrix[j]);

            return;
        }
    }
}

static void Normalzb_start_anim_cb(zb_type *xzb)
{
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, xzb->zb);
    lv_anim_set_exec_cb(&a, zb_set_angle_cb);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_in);
    lv_anim_set_time(&a, 4300);
    lv_anim_set_values(&a, -5, 10);
    lv_anim_set_playback_time(&a, 1900);
    lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
    lv_anim_start(&a);
}

static void jumpzb_start_anim_cb(zb_type *xzb)
{
    lv_anim_t a;
    lv_img_set_angle(xzb->zb, -80);
    lv_anim_init(&a);
    lv_anim_set_var(&a, xzb->zb);
    lv_anim_set_exec_cb(&a, jumpzb_jump_cb);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_in);
    lv_anim_set_time(&a, 300);
    lv_anim_set_values(&a, xzb->y * 54 + 35, xzb->y * 54 + 5);
    lv_anim_set_playback_time(&a, 500);
    lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
    lv_anim_start(&a);
}

static void carzb_start_anim_cb(zb_type *xzb)
{
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, xzb->zb);
    lv_anim_set_exec_cb(&a, jumpzb_jump_cb);
    lv_anim_set_time(&a, 100);
    lv_anim_set_values(&a, xzb->y * 54, xzb->y * 54 + 2);
    lv_anim_set_playback_time(&a, 100);
    lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
    lv_anim_start(&a);
}

static void qiqiuzb_start_anim_cb(zb_type *xzb)
{
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, xzb->zb);
    lv_anim_set_exec_cb(&a, jumpzb_jump_cb);
    lv_anim_set_time(&a, 1000);
    lv_anim_set_values(&a, xzb->y * 54, xzb->y * 54 + 20);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_in_out);
    lv_anim_set_playback_time(&a, 1000);
    lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
    lv_anim_start(&a);
}

static void zb_left_move_cb(void *var, int32_t v)
{
    zb_type *zb = (zb_type *)var;

    if (zb->blood > 0) {
        zb->x--;
        lv_obj_set_x(zb->zb, zb->x);
    }
}

static void newshine_cb(lv_timer_t *t)
{
    int i, j;

    for (j = 0; j < max_quantity; j++) {
        if (shine[j].alive == 0) {
            shine[j].alive = 1;
            shine[j].shine = lv_img_create(map1);
            lv_img_set_src(shine[j].shine, &sunshine);
            lv_obj_add_flag(shine[j].shine, LV_OBJ_FLAG_CLICKABLE);
            shine[j].x = rand() % 400 + 30;
            shine[j].y = rand() % 220 + 50;
            lv_obj_set_pos(shine[j].shine, shine[j].x, shine[j].y);
            lv_obj_add_event_cb(shine[j].shine, shine_del_cb1, LV_EVENT_RELEASED, &shine[j]);

            lv_anim_t a1;
            lv_anim_init(&a1);
            lv_anim_set_var(&a1, shine[j].shine);
            lv_anim_set_exec_cb(&a1, shine_start_cb1);
            lv_anim_set_path_cb(&a1, lv_anim_path_ease_out);
            lv_anim_set_time(&a1, 1000);
            lv_anim_set_values(&a1, shine[j].y, shine[j].y + 20);
            lv_anim_set_user_data(&a1, &shine[j]);
            lv_anim_start(&a1);
            return;
        }
    }
}

static void shine_start_cb1(void *var, int32_t v)
{
    lv_obj_t *xxx = (lv_obj_t *)var;
    lv_obj_set_y(xxx, v);
}

static void add_zidan_cb(lv_timer_t *t)
{
    int x, y, i, j;
    plant_type *user = (plant_type *)t->user_data;
    x = user->x;
    y = user->y;

    for (i = 0; i < max_quantity; i++) {
        if (zidan[i].alive == 0) {
            zidan[i].alive = 1;
            zidan[i].x = x * 49 + 70;
            zidan[i].y = y;

            zidan[i].zidan = lv_btn_create(map1);
            lv_obj_set_pos(zidan[i].zidan, zidan[i].x, y * 54 + 46);
            lv_obj_set_size(zidan[i].zidan, 16, 16);
            lv_obj_set_style_bg_color(zidan[i].zidan, lv_color_hex(0x40ff40), LV_PART_MAIN);
            lv_obj_set_style_shadow_color(zidan[i].zidan, lv_color_hex(0x004000), LV_PART_MAIN);
            lv_obj_set_style_radius(zidan[i].zidan, 8, LV_PART_MAIN);
            lv_obj_set_style_border_width(zidan[i].zidan, 2, LV_PART_MAIN);
            lv_obj_set_style_border_color(zidan[i].zidan, lv_color_hex(0x004000), LV_PART_MAIN);
            lv_obj_clear_flag(zidan[i].zidan, LV_OBJ_FLAG_CLICKABLE);
            return;
        }
    }
}

static void timer_zidan_refr_pos_cb(lv_timer_t *t)
{
    int i, j;

    for (i = 0; i < max_quantity; i++) {
        if (zidan[i].alive == 1) {
            zidan[i].x += zidan_speed;

            if (zidan[i].x > 490) {
                zidan[i].alive = 0;
                lv_obj_del(zidan[i].zidan);
                continue;
            } else {
                lv_obj_set_x(zidan[i].zidan, zidan[i].x);
            }

            for (j = 0; j < max_zb_count; j++) {

                if (zb_matrix[j].blood > 0) {
                    if ((zb_matrix[j].y == zidan[i].y)) {
                        if ((zb_matrix[j].x - zidan[i].x < 15) && (zidan[i].x - zb_matrix[j].x < 10)) {
                            zb_matrix[j].blood--;
                            zidan[i].alive = 0;

                            lv_obj_t *zidan_split = lv_img_create(map1);
                            lv_img_set_src(zidan_split, &zidan_split_img);
                            lv_obj_set_pos(zidan_split, lv_obj_get_x(zidan[i].zidan) - 8,
                                           lv_obj_get_y(zidan[i].zidan) - 8);
#if LVGL_VERSION_MAJOR == 9
                            lv_obj_delete_delayed(zidan_split, 500);
#else
                            lv_obj_del_delayed(zidan_split, 500);
#endif
                            lv_obj_del(zidan[i].zidan);

                            if (zb_matrix[j].blood == 0) {
                                zb_matrix[j].zb_class->zb_del_cb(&zb_matrix[j]);
                            } else {

                                lv_img_set_angle(zb_matrix[j].zb, 50);

                                lv_anim_t a1;
                                lv_anim_init(&a1);
                                lv_anim_set_var(&a1, zb_matrix[j].zb);
                                lv_anim_set_exec_cb(&a1, hit_zb_cb);
                                lv_anim_set_time(&a1, 150);
                                lv_anim_set_values(&a1, 0, 1);
                                lv_anim_start(&a1);
                            }

                            break;
                        }
                    }
                }
            }
        }
    }
}

static void timer_wogua_test_cb(lv_timer_t *t)
{
    int i, j;

    plant_type *wogua = (plant_type *)(t->user_data);

    if (wogua->blood > 0) {

        for (i = 0; i < max_zb_count; i++) {
            if (zb_matrix[i].blood > 0) {
                if ((zb_matrix[i].y == wogua->y) && (zb_matrix[i].x - wogua->x * 49 < 80) &&
                    (zb_matrix[i].x - wogua->x * 49 > 50)) {
                    wogua->blood = 0;
                    zb_matrix[i].blood = 0;

#if (plant_stackable != 1)
                    map_flag[wogua->y][wogua->x] = 0;
#endif

                    lv_anim_t a2;
                    lv_anim_init(&a2);
                    lv_anim_set_var(&a2, wogua->plant);
                    lv_anim_set_exec_cb(&a2, wogua_dead_cb);
                    lv_anim_set_path_cb(&a2, lv_anim_path_ease_in);
                    lv_anim_set_time(&a2, 300);
                    lv_anim_set_values(&a2, 0, 80);
                    lv_anim_start(&a2);

                    lv_anim_t a3;
                    lv_anim_init(&a3);
                    lv_anim_set_var(&a3, wogua->plant);
                    lv_anim_set_exec_cb(&a3, wogua_dead_cb2);
                    lv_anim_set_path_cb(&a3, lv_anim_path_ease_in);
                    lv_anim_set_time(&a3, 300);
                    lv_anim_set_delay(&a3, 600);
                    lv_anim_set_values(&a3, 0, 80);
                    lv_anim_set_user_data(&a3, &zb_matrix[i]);
                    lv_anim_set_ready_cb(&a3, wogua_delect_zb_cb);
                    lv_anim_start(&a3);

                    lv_anim_t a4;
                    lv_anim_init(&a4);
                    lv_anim_set_var(&a4, wogua->plant);
                    lv_anim_set_exec_cb(&a4, wogua_dead_cb3);
                    lv_anim_set_path_cb(&a4, lv_anim_path_ease_in);
                    lv_anim_set_time(&a4, 1000);
                    lv_anim_set_delay(&a4, 700);
                    lv_anim_set_values(&a4, 25, 0);
                    lv_anim_set_user_data(&a4, wogua);
                    lv_anim_set_ready_cb(&a4, wogua_delect_cb);
                    lv_anim_start(&a4);

                    return;
                }
            }
        }
    }
}

static void timer_dici_test_cb(lv_timer_t *t)
{
    int i;

    plant_type *dici = (plant_type *)(t->user_data);

    if (dici->blood > 0) {

        for (i = 0; i < max_zb_count; i++) {
            if (zb_matrix[i].blood > 0 &&
                (zb_matrix[i].zb_class->zb_kind == Normalzb || zb_matrix[i].zb_class->zb_kind == carzb)) {
                if ((zb_matrix[i].y == dici->y) && (zb_matrix[i].x - dici->x * 49 < 50) &&
                    (zb_matrix[i].x - dici->x * 49 > -50)) {

                    zb_matrix[i].blood--;
                    dici->blood--;

                    lv_anim_t a1;
                    lv_anim_init(&a1);
                    lv_anim_set_var(&a1, zb_matrix[i].zb);
                    lv_anim_set_exec_cb(&a1, hit_zb_cb);
                    lv_anim_set_time(&a1, 150);
                    lv_anim_set_values(&a1, 0, 1);
                    lv_anim_start(&a1);

                    if (zb_matrix[i].blood == 0) {
                        zb_matrix[i].zb_class->zb_del_cb(&zb_matrix[i]);
                    }

                    if (dici->blood == 0) {
                        dici->alive = 0;
                        lv_timer_del(dici->timer);
                        lv_obj_del(dici->plant);
#if (plant_stackable != 1)
                        map_flag[dici->y][dici->x] = 0;
#endif
                        return;

                    } else {
                        lv_anim_t a2;
                        lv_anim_init(&a2);
                        lv_anim_set_var(&a2, dici->plant);
                        lv_anim_set_exec_cb(&a2, dici_anim_cb);
                        lv_anim_set_time(&a2, 200);
                        lv_anim_set_values(&a2, 10, 0);
                        lv_anim_start(&a2);
                    }
                }
            }
        }
    }
}

static void dici_anim_cb(void *var, int32_t v)
{

    lv_img_set_zoom(var, 256 + 3 * v);
}

static void wogua_delect_zb_cb(lv_anim_t *a)
{
    zb_type *zb = (zb_type *)(a->user_data);
    zb->zb_class->zb_del_cb(zb);
}

static void timer_car_test_cb(lv_timer_t *t)
{
    int i, j;

    for (j = 0; j < 5; j++) {
        if (little_car[j].alive == 1) {
            for (i = 0; i < max_zb_count; i++) {
                if (zb_matrix[i].blood > 0 && zb_matrix[i].y == j && zb_matrix[i].x < 20) {
                    little_car[j].alive = 0;
                    lv_anim_t a2;
                    lv_anim_init(&a2);
                    lv_anim_set_var(&a2, little_car[j].zidan);
                    lv_anim_set_exec_cb(&a2, car_start_move);
                    lv_anim_set_time(&a2, 2000);
                    lv_anim_set_values(&a2, -3, lv_disp_get_hor_res(lv_disp_get_default()) / 5);
                    lv_anim_set_deleted_cb(&a2, boom_delete_cb);
                    lv_anim_start(&a2);
                    break;
                }
            }
        }
    }

    for (i = 0; i < max_zb_count; i++) {
        if (zb_matrix[i].blood > 0 && zb_matrix[i].x < -10) {
            game_over_cb();
            return;
        }
    }
}

static void game_over_cb(void)
{

    gameover_btn = lv_img_create(map1);
    lv_img_set_src(gameover_btn, &game_over_img);
    lv_obj_center(gameover_btn);

    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, gameover_btn);
    lv_anim_set_exec_cb(&a, NULL);
    lv_anim_set_time(&a, 1);
    lv_anim_set_delay(&a, 2000);
    lv_anim_set_ready_cb(&a, game_over_img_ready_cb);
    lv_anim_start(&a);
}

static void game_over_img_ready_cb(lv_anim_t *a)
{
    exit_game_cb(0);
    pvz_start();
}

static void car_start_move(void *var, int32_t v)
{
    int i, j;

    j = ((zidan_type *)(((lv_obj_t *)var)->user_data))->y;

    lv_obj_set_x((lv_obj_t *)var, v * 5);

    for (i = 0; i < max_zb_count; i++) {
        if (zb_matrix[i].blood > 0) {
            if (zb_matrix[i].y == j) {
                if (zb_matrix[i].x < v * 5) {
                    zb_matrix[i].blood = 0;
                    zb_matrix[i].zb_class->zb_del_cb(&zb_matrix[i]);
                }
            }
        }
    }
}

static void zidan_move(lv_timer_t *t)
{
    // int i,j;
    //
    //	for(i=0;i<max_quantity;i++)
    //	{
    //		if(zidan[i].alive==1)
    //		{
    //			zidan[i].x+=zidan_speed;
    //		}
    //	}
}

static void zb_del_start_cb(lv_anim_t *a)
{
    lv_obj_t *xxx = (lv_obj_t *)a->var;
    lv_obj_set_style_img_recolor(xxx, lv_color_hex(0x000000), 0);
    lv_obj_set_style_img_recolor_opa(xxx, 255, 0);
}

static void shine_del_cb1(lv_event_t *e)
{
    shine_type *user = (shine_type *)lv_event_get_user_data(e);
    lv_anim_del(user->shine, shine_start_cb1);
    lv_obj_clear_flag(user->shine, LV_OBJ_FLAG_CLICKABLE);

    lv_anim_t a1;
    lv_anim_init(&a1);
    lv_anim_set_var(&a1, user->shine);
    lv_anim_set_exec_cb(&a1, anim_cb1);
    lv_anim_set_path_cb(&a1, lv_anim_path_ease_out);
    lv_anim_set_time(&a1, 500);
    lv_anim_set_values(&a1, user->x, 10);
    lv_anim_start(&a1);

    lv_anim_t a2;
    lv_anim_init(&a2);
    lv_anim_set_var(&a2, user->shine);
    lv_anim_set_exec_cb(&a2, anim_cb2);
    lv_anim_set_path_cb(&a2, lv_anim_path_ease_out);
    lv_anim_set_time(&a2, 500);
    lv_anim_set_values(&a2, lv_obj_get_y(user->shine), 0);
    lv_anim_set_user_data(&a2, user);
    lv_anim_set_ready_cb(&a2, shine_delect_cb);
    lv_anim_start(&a2);
}

static void zb_set_angle_cb(void *var, int32_t v)
{
    lv_obj_t *xxx = (lv_obj_t *)var;
    lv_img_set_angle(xxx, v * 10);
}

static void jumpzb_jump_cb(void *var, int32_t v)
{
    lv_obj_t *xxx = (lv_obj_t *)var;
    lv_obj_set_y(xxx, v);
    lv_img_set_angle(xxx, 0);
}

static void anim_cb1(void *var, int32_t v)
{
    lv_obj_t *xxx = (lv_obj_t *)var;
    lv_obj_set_x(xxx, v);
}
void anim_cb2(void *var, int32_t v)
{
    lv_obj_t *xxx = (lv_obj_t *)var;
    lv_obj_set_y(xxx, v);
}

static void shine_delect_cb(lv_anim_t *a)
{
    shine_type *user = (shine_type *)a->user_data;
    user->alive = 0;
    lv_obj_del(user->shine);
    sun_score += 50;
    lv_label_set_text_fmt(score_lable, "%d", sun_score);
    refs_card_btn();
}

static void refs_card_btn()
{
    for (int i = 0; i < sizeof(card_btn) / sizeof(card_btn_type); i++) {
        if (sun_score >= card_btn[i].needscore) {
            lv_obj_set_style_img_recolor_opa(card_btn[i].card_btn, 0, LV_PART_MAIN);
            lv_obj_add_flag(card_btn[i].card_btn, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_set_style_text_color(lv_obj_get_child(card_btn[i].card_btn, 0), lv_color_hex(0xffffff),
                                        LV_PART_MAIN);
        } else {
            lv_obj_set_style_img_recolor_opa(card_btn[i].card_btn, 200, LV_PART_MAIN);
            lv_obj_clear_flag(card_btn[i].card_btn, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_set_style_text_color(lv_obj_get_child(card_btn[i].card_btn, 0), lv_color_hex(0x555555),
                                        LV_PART_MAIN);
        }
    }
}

static void btn_cliked_cb(lv_event_t *e)
{

    for (int i = 0; i < sizeof(card_btn) / sizeof(card_btn_type); i++) {
        if (card_btn[i].card_btn == e->current_target) {
            card_btn[i].ispushed = 1;
            lv_obj_set_style_outline_width(card_btn[i].card_btn, 3, LV_PART_MAIN);
        } else {
            card_btn[i].ispushed = 0;
            lv_obj_set_style_outline_width(card_btn[i].card_btn, 0, LV_PART_MAIN);
        }
    }
    lv_obj_set_style_border_opa(chanzi_btn, 0, LV_PART_MAIN);
    chanzi_btn_select = 0;
    lv_obj_add_flag(map1, LV_OBJ_FLAG_CLICKABLE);
}

static void chanzi_btn_cb(lv_event_t *e)
{
    if (chanzi_btn_select == 0) {
        for (int i = 0; i < sizeof(card_btn) / sizeof(card_btn_type); i++) {
            card_btn[i].ispushed = 0;
            lv_obj_set_style_outline_width(card_btn[i].card_btn, 0, LV_PART_MAIN);
        }

        lv_obj_set_style_border_opa(chanzi_btn, 255, LV_PART_MAIN);
        lv_obj_add_flag(map1, LV_OBJ_FLAG_CLICKABLE);
        chanzi_btn_select = 1;
    } else {
        lv_obj_set_style_border_opa(chanzi_btn, 0, LV_PART_MAIN);
        chanzi_btn_select = 0;
        lv_obj_clear_flag(map1, LV_OBJ_FLAG_CLICKABLE);
    }
}

static void map_click_cb(lv_event_t *e)
{
    int x, y, i, j;
    lv_point_t click_point;
    lv_indev_get_point(lv_indev_get_act(), &click_point);

    x = (click_point.x - 17) / 50;
    y = (click_point.y - 46) / 53;

    if (x < 0 || y < 0 || x > 8 || y > 4) {
        return;
    }

    if (chanzi_btn_select) {
        for (j = 0; j < max_quantity; j++) {
            if (wandouflower[j].alive == 1 && wandouflower[j].x == x && wandouflower[j].y == y) {
                map_flag[y][x] = 0;
                wandouflower[j].alive = 0;
                wandouflower[j].x = 0;
                wandouflower[j].y = 0;
                lv_anim_del(&wandouflower[j], plant_anim_cb);
                lv_obj_del(wandouflower[j].plant);
                lv_timer_del(wandouflower[j].timer);
                chanzi_btn_select = 0;
                lv_obj_set_style_border_opa(chanzi_btn, 0, LV_PART_MAIN);
                lv_obj_clear_flag(map1, LV_OBJ_FLAG_CLICKABLE);
                break;
            }

            if (sunflower[j].alive == 1 && sunflower[j].x == x && sunflower[j].y == y) {
                map_flag[y][x] = 0;
                sunflower[j].alive = 0;
                sunflower[j].x = 0;
                sunflower[j].y = 0;
                lv_anim_del(&sunflower[j], plant_anim_cb);
                lv_obj_del(sunflower[j].plant);
                lv_timer_del(sunflower[j].timer);
                chanzi_btn_select = 0;
                lv_obj_set_style_border_opa(chanzi_btn, 0, LV_PART_MAIN);
                lv_obj_clear_flag(map1, LV_OBJ_FLAG_CLICKABLE);
                break;
            }

            if (wogua[j].alive == 1 && wogua[j].x == x && wogua[j].y == y) {
                map_flag[y][x] = 0;
                wogua[j].alive = 0;
                wogua[j].blood = 0;
                lv_timer_del(wogua[j].timer);
                lv_obj_del(wogua[j].plant);
                chanzi_btn_select = 0;
                lv_obj_set_style_border_opa(chanzi_btn, 0, LV_PART_MAIN);
                lv_obj_clear_flag(map1, LV_OBJ_FLAG_CLICKABLE);
                break;
            }

            if (jiguangdou[j].alive == 1 && jiguangdou[j].x == x && jiguangdou[j].y == y) {
                map_flag[y][x] = 0;
                jiguangdou[j].alive = 0;
                jiguangdou[j].x = 0;
                jiguangdou[j].y = 0;
                lv_obj_del(jiguangdou[j].plant);
                lv_anim_del(&jiguangdou[j], jiguangdou_anim_cb);
                lv_timer_del(jiguangdou[j].timer);
                chanzi_btn_select = 0;
                lv_obj_set_style_border_opa(chanzi_btn, 0, LV_PART_MAIN);
                lv_obj_clear_flag(map1, LV_OBJ_FLAG_CLICKABLE);
                break;
            }

            if (dici[j].alive == 1 && dici[j].x == x && dici[j].y == y) {
                map_flag[y][x] = 0;
                dici[j].alive = 0;
                dici[j].x = 0;
                dici[j].y = 0;
                lv_anim_del(dici[j].plant, dici_anim_cb);
                lv_obj_del(dici[j].plant);
                lv_timer_del(dici[j].timer);
                chanzi_btn_select = 0;
                lv_obj_set_style_border_opa(chanzi_btn, 0, LV_PART_MAIN);
                lv_obj_clear_flag(map1, LV_OBJ_FLAG_CLICKABLE);
                break;
            }
        }
        return;
    }

    for (i = 0; i < sizeof(card_btn) / sizeof(card_btn_type); i++) {
        if (card_btn[i].ispushed == 1) {
            break;
        }
    }
    card_btn[i].plant_creat_cb(&card_btn[i], x, y);

    refs_card_btn();
}

static void sunflower_creat_cb(card_btn_type *card_btn, int x, int y)
{
    int j;

#if (plant_stackable != 1)
    if (map_flag[y][x] == 1) {
        return;
    }
#endif

    for (j = 0; j < max_quantity; j++) {
        if (sunflower[j].alive == 0) {
            sunflower[j].alive = 1;
            sunflower[j].x = x;
            sunflower[j].y = y;
            sunflower[j].plant = lv_img_create(map1);
            lv_img_set_src(sunflower[j].plant, &sun_img);
            lv_obj_set_pos(sunflower[j].plant, x * 49 + 20, y * 54 + 44);
            lv_img_set_pivot(sunflower[j].plant, 23, 55);

#if (plant_stackable != 1)
            map_flag[y][x] = 1;
#endif

            lv_anim_t a;
            lv_anim_init(&a);
            lv_anim_set_var(&a, &sunflower[j]);
            lv_anim_set_exec_cb(&a, plant_anim_cb);
            lv_anim_set_path_cb(&a, lv_anim_path_ease_in_out);
            lv_anim_set_time(&a, 1000);
            lv_anim_set_values(&a, -5, 5);
            lv_anim_set_playback_time(&a, 1000);
            lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
            lv_anim_start(&a);

            sun_score -= card_btn->needscore;
            lv_label_set_text_fmt(score_lable, "%d", sun_score);
            lv_obj_clear_flag(map1, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_set_style_outline_width(card_btn->card_btn, 0, LV_PART_MAIN);
            sunflower[j].timer = lv_timer_create(sunflower_creat_shine_cb, sunflower_add_sun_period, &sunflower[j]);
            card_btn->ispushed = 0;
            return;
        }
    }
}

static void wandou_creat_cb(card_btn_type *card_btn, int x, int y)
{
    int j;

#if (plant_stackable != 1)
    if (map_flag[y][x] == 1) {
        return;
    }
#endif

    for (j = 0; j < max_quantity; j++) {
        if (wandouflower[j].alive == 0) {
            wandouflower[j].alive = 1;
            wandouflower[j].x = x;
            wandouflower[j].y = y;
            wandouflower[j].plant = lv_img_create(map1);
            lv_img_set_src(wandouflower[j].plant, &wandou_img);
            lv_obj_set_pos(wandouflower[j].plant, x * 49 + 18, y * 54 + 44);
            lv_img_set_pivot(wandouflower[j].plant, 27, 50);

#if (plant_stackable != 1)
            map_flag[y][x] = 1;
#endif

            lv_anim_t a;
            lv_anim_init(&a);
            lv_anim_set_var(&a, &wandouflower[j]);
            lv_anim_set_exec_cb(&a, plant_anim_cb);
            lv_anim_set_path_cb(&a, lv_anim_path_ease_in_out);
            lv_anim_set_time(&a, 1000);
            lv_anim_set_values(&a, -5, 5);
            lv_anim_set_playback_time(&a, 1000);
            lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
            lv_anim_start(&a);

            wandouflower[j].timer = lv_timer_create(add_zidan_cb, add_zidan_period, &wandouflower[j]);

            sun_score -= card_btn->needscore;
            lv_label_set_text_fmt(score_lable, "%d", sun_score);
            lv_obj_clear_flag(map1, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_set_style_outline_width(card_btn->card_btn, 0, LV_PART_MAIN);
            card_btn->ispushed = 0;
            return;
        }
    }
}

static void jiguangdou_creat_cb(card_btn_type *card_btn, int x, int y)
{
    int j;

#if (plant_stackable != 1)
    if (map_flag[y][x] == 1) {
        return;
    }
#endif

    for (j = 0; j < max_quantity; j++) {
        if (jiguangdou[j].alive == 0) {
            jiguangdou[j].alive = 1;
            jiguangdou[j].x = x;
            jiguangdou[j].y = y;
            jiguangdou[j].plant = lv_img_create(map1);
            lv_img_set_src(jiguangdou[j].plant, &jiguangdou_img);
            lv_obj_set_pos(jiguangdou[j].plant, x * 49 + 18, y * 54 + 30);
            lv_img_set_pivot(jiguangdou[j].plant, 27, 50);

#if (plant_stackable != 1)
            map_flag[y][x] = 1;
#endif

            jiguangdou[j].timer = lv_timer_create(add_jiguang_cb, 2500, &jiguangdou[j]);
            add_jiguang_cb(jiguangdou[j].timer);

            sun_score -= card_btn->needscore;
            lv_label_set_text_fmt(score_lable, "%d", sun_score);
            lv_obj_clear_flag(map1, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_set_style_outline_width(card_btn->card_btn, 0, LV_PART_MAIN);
            card_btn->ispushed = 0;
            return;
        }
    }
}

static void jiguangdou_anim_cb(void *var, int32_t v)
{
    plant_type *xxx = (plant_type *)var;
    lv_img_set_angle(xxx->plant, v * 10);
}

static void add_jiguang_cb(lv_timer_t *t)
{
    static lv_point_t jiguang_point[max_quantity][2] = {
        0,
    };
    static int point_count = 0;
    int x, y, i, j;
    plant_type *user = (plant_type *)t->user_data;
    x = user->x;
    y = user->y;

    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, &user->plant);
    lv_anim_set_exec_cb(&a, jiguangdou_anim_cb);
    lv_anim_set_time(&a, 1600);
    lv_anim_set_delay(&a, 500);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_in);
    lv_anim_set_values(&a, 20, -20);
    lv_anim_set_playback_time(&a, 400);
    lv_anim_start(&a);

    jiguang_point[point_count][0].x = x * 49 + 68;
    jiguang_point[point_count][0].y = y * 54 + 64;
    jiguang_point[point_count][1].x = lv_disp_get_hor_res(lv_disp_get_default());
    jiguang_point[point_count][1].y = y * 54 + 64;

    lv_obj_t *jiguangkou = lv_img_create(map1);
    lv_obj_set_pos(jiguangkou, jiguang_point[point_count][0].x - 15, jiguang_point[point_count][0].y - 7);
    lv_img_set_src(jiguangkou, &jiguangkou_img);
    lv_img_set_zoom(jiguangkou, 512);
    lv_obj_move_foreground(jiguangkou);
#if LVGL_VERSION_MAJOR == 9
    lv_obj_delete_delayed(jiguangkou, 500);
#else
    lv_obj_del_delayed(jiguangkou, 500);
#endif

    lv_obj_t *jiguang_track1 = lv_line_create(map1);
    lv_obj_set_style_line_width(jiguang_track1, 5, 0);
    lv_obj_set_style_line_color(jiguang_track1, lv_color_hex(0x0000ff), 0);
    lv_line_set_points(jiguang_track1, jiguang_point[point_count], 2);
    lv_obj_move_foreground(jiguang_track1);
#if LVGL_VERSION_MAJOR == 9
    lv_obj_delete_delayed(jiguang_track1, 550);
#else
    lv_obj_del_delayed(jiguang_track1, 550);
#endif

    lv_obj_t *jiguang_track2 = lv_line_create(map1);
    lv_obj_set_style_line_width(jiguang_track2, 1, 0);
    lv_obj_set_style_line_color(jiguang_track2, lv_color_hex(0xffffff), 0);
    lv_line_set_points(jiguang_track2, jiguang_point[point_count], 2);
    lv_obj_move_foreground(jiguang_track2);
#if LVGL_VERSION_MAJOR == 9
    lv_obj_delete_delayed(jiguang_track2, 550);
#else
    lv_obj_del_delayed(jiguang_track2, 550);
#endif
    jiguang_track2->user_data = jiguang_track1;

    point_count++;
    if (point_count == max_quantity)
        point_count = 0;

    lv_anim_t a1;
    lv_anim_init(&a1);
    lv_anim_set_var(&a1, jiguang_track2);
    lv_anim_set_exec_cb(&a1, jiguangdou_width_cb);
    lv_anim_set_time(&a1, 250);
    lv_anim_set_values(&a1, 1, 6);
    lv_anim_set_playback_time(&a1, 250);
    lv_anim_start(&a1);

    for (i = 0; i < max_zb_count; i++) {
        if (zb_matrix[i].blood > 0 && y == zb_matrix[i].y) {
            zb_matrix[i].blood--;
            if (zb_matrix[i].blood == 0) {
                zb_matrix[i].zb_class->zb_del_cb(&zb_matrix[i]);
            } else {
                lv_anim_t a1;
                lv_anim_init(&a1);
                lv_anim_set_var(&a1, zb_matrix[i].zb);
                lv_anim_set_exec_cb(&a1, burn_zb_cb);
                lv_anim_set_path_cb(&a1, lv_anim_path_linear);
                lv_anim_set_time(&a1, 300);
                lv_anim_set_values(&a1, 0, 1);
                lv_anim_start(&a1);
            }
        }
    }
}

static void burn_zb_cb(void *var, int32_t v)
{
    lv_obj_t *user = (lv_obj_t *)var;

    // lv_obj_set_style_img_opa(user,120+v*135,0);
    lv_obj_set_style_img_recolor(user, lv_color_hex(0xff0000), LV_PART_MAIN);
    lv_obj_set_style_img_recolor_opa(user, (1 - v) * 100, 0);
}

static void jiguangdou_width_cb(void *var, int32_t v)
{
    lv_obj_t *xxx = (lv_obj_t *)var;
    lv_obj_t *yyy = (lv_obj_t *)(xxx->user_data);
    lv_obj_set_style_line_width(xxx, v, 0);
    lv_obj_set_style_line_width(yyy, v * 2 + 4, 0);
}

static void wogua_creat_cb(card_btn_type *card_btn, int x, int y)
{
    int j;

#if (plant_stackable != 1)
    if (map_flag[y][x] == 1) {
        return;
    }
#endif

    for (j = 0; j < max_quantity; j++) {
        if (wogua[j].alive == 0) {
            wogua[j].alive = 1;
            wogua[j].blood = 1;
            wogua[j].x = x;
            wogua[j].y = y;
            wogua[j].plant = lv_img_create(map1);
            lv_img_set_src(wogua[j].plant, &wogua_img);
            lv_obj_clear_flag(wogua[j].plant, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_set_pos(wogua[j].plant, x * 49 + 18, y * 54 + 44);
            wogua[j].plant->user_data = &wogua[j];
            wogua[j].timer = lv_timer_create(timer_wogua_test_cb, 200, &wogua[j]);

#if (plant_stackable != 1)
            map_flag[y][x] = 1;
#endif

            sun_score -= card_btn->needscore;
            lv_label_set_text_fmt(score_lable, "%d", sun_score);
            lv_obj_clear_flag(map1, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_set_style_outline_width(card_btn->card_btn, 0, LV_PART_MAIN);
            card_btn->ispushed = 0;
            return;
        }
    }
}

static void dici_creat_cb(card_btn_type *card_btn, int x, int y)
{
    int j;

#if (plant_stackable != 1)
    if (map_flag[y][x] == 1) {
        return;
    }
#endif

    for (j = 0; j < max_quantity; j++) {
        if (dici[j].alive == 0) {
            dici[j].alive = 1;
            dici[j].blood = 100;
            dici[j].x = x;
            dici[j].y = y;
            dici[j].plant = lv_img_create(map1);
            lv_img_set_src(dici[j].plant, &dici_img);
            lv_obj_clear_flag(dici[j].plant, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_set_pos(dici[j].plant, x * 49 + 18, y * 54 + 74);
            lv_obj_move_background(dici[j].plant);
            dici[j].plant->user_data = &dici[j];
            dici[j].timer = lv_timer_create(timer_dici_test_cb, 500, &dici[j]);

#if (plant_stackable != 1)
            map_flag[y][x] = 1;
#endif

            sun_score -= card_btn->needscore;
            lv_label_set_text_fmt(score_lable, "%d", sun_score);
            lv_obj_clear_flag(map1, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_set_style_outline_width(card_btn->card_btn, 0, LV_PART_MAIN);
            card_btn->ispushed = 0;
            return;
        }
    }
}

static void lajiao_creat_cb(card_btn_type *card_btn, int x, int y)
{
    lv_obj_t *lajiao = lv_img_create(map1);
    lv_img_set_src(lajiao, &lajiao_img);
    lv_obj_set_pos(lajiao, x * 49 + 20, y * 54 + 44);

    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, lajiao);
    lv_anim_set_exec_cb(&a, lajiao_anim_cb);
    lv_anim_set_ready_cb(&a, lajiao_delete_cb);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
    lv_anim_set_time(&a, 1000);
    lv_anim_set_values(&a, 0, 10);
    lv_anim_start(&a);

    sun_score -= card_btn->needscore;
    lv_label_set_text_fmt(score_lable, "%d", sun_score);
    lv_obj_clear_flag(map1, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_outline_width(card_btn->card_btn, 0, LV_PART_MAIN);

    card_btn->ispushed = 0;
}

static void cherry_creat_cb(card_btn_type *card_btn, int x, int y)
{
    lv_obj_t *cherry = lv_img_create(map1);
    lv_img_set_src(cherry, &cherry_img);
    lv_obj_set_pos(cherry, x * 49 + 20, y * 54 + 44);

    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, cherry);
    lv_anim_set_exec_cb(&a, cherry_anim_cb);
    lv_anim_set_ready_cb(&a, cherry_delete_cb);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
    lv_anim_set_time(&a, 1000);
    lv_anim_set_values(&a, 0, 10);
    lv_anim_start(&a);

    sun_score -= card_btn->needscore;
    lv_label_set_text_fmt(score_lable, "%d", sun_score);
    lv_obj_clear_flag(map1, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_outline_width(card_btn->card_btn, 0, LV_PART_MAIN);
    card_btn->ispushed = 0;
}

static void hedan_creat_cb(card_btn_type *card_btn, int x, int y)
{
    lv_obj_t *nuclear = lv_img_create(map1);
    lv_img_set_src(nuclear, &nuclear_img);
    lv_obj_clear_flag(nuclear, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_pos(nuclear, 225, -100);

    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, nuclear);
    lv_anim_set_exec_cb(&a, nuclear_anim_cb);
    lv_anim_set_ready_cb(&a, nuclear_delete_cb);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
    lv_anim_set_time(&a, 2000);
    lv_anim_set_values(&a, -100, 120);
    lv_anim_start(&a);

    sun_score -= card_btn->needscore;
    lv_label_set_text_fmt(score_lable, "%d", sun_score);
    lv_obj_clear_flag(map1, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_outline_width(card_btn->card_btn, 0, LV_PART_MAIN);
    card_btn->ispushed = 0;
}

static void sunflower_creat_shine_cb(lv_timer_t *t)
{
    int x, y, i, j;
    plant_type *user = (plant_type *)t->user_data;
    x = user->x;
    y = user->y;

    for (j = 0; j < max_quantity; j++) {
        if (shine[j].alive == 0) {
            shine[j].alive = 1;
            shine[j].shine = lv_img_create(map1);
            lv_img_set_src(shine[j].shine, &sunshine);
            lv_obj_add_flag(shine[j].shine, LV_OBJ_FLAG_CLICKABLE);
            shine[j].x = x * 49 + 20;
            shine[j].y = y * 54 + 44;
            lv_obj_set_pos(shine[j].shine, shine[j].x, shine[j].y);
            lv_obj_add_event_cb(shine[j].shine, shine_del_cb1, LV_EVENT_SHORT_CLICKED, &shine[j]);

            lv_anim_t a1;
            lv_anim_init(&a1);
            lv_anim_set_var(&a1, shine[j].shine);
            lv_anim_set_exec_cb(&a1, shine_start_cb1);
            lv_anim_set_path_cb(&a1, lv_anim_path_ease_out);
            lv_anim_set_time(&a1, 1000);
            lv_anim_set_values(&a1, shine[j].y, shine[j].y + 20);
            lv_anim_set_user_data(&a1, &shine[j]);
            lv_anim_start(&a1);

            return;
        }
    }
}

static void init_all()
{
    int i, j;

    for (j = 0; j < max_quantity; j++) {
        shine[j].alive = 0;
        zidan[j].alive = 0;

        if (wandouflower[j].alive == 1) {
            wandouflower[j].alive = 0;
            lv_timer_del(wandouflower[j].timer);
        }

        if (sunflower[j].alive == 1) {
            sunflower[j].alive = 0;
            lv_timer_del(sunflower[j].timer);
        }

        if (wogua[j].alive == 1) {
            wogua[j].alive = 0;
            lv_timer_del(wogua[j].timer);
        }

        if (jiguangdou[j].alive == 1) {
            jiguangdou[j].alive = 0;
            lv_timer_del(jiguangdou[j].timer);
        }

        if (dici[j].alive == 1) {
            dici[j].alive = 0;
            lv_timer_del(dici[j].timer);
        }
    }

#if (plant_stackable != 1)
    for (i = 0; i < 9; i++) {
        for (j = 0; j < 5; j++) {
            map_flag[j][i] = 0;
        }
    }
#endif
}

static void exit_game_cb(lv_event_t *e)
{
    lv_anim_del_all();
    lv_timer_del(timer_newzb);
    lv_timer_del(timer_newshine);
    lv_timer_del(timer_zidan_refr_pos);
    lv_timer_del(timer_car_test);
    init_all();
    lv_obj_del(screen);
}

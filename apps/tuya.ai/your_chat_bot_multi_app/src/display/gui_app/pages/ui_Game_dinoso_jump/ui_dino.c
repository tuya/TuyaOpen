/**
 * @file ui_dino.c
 * @brief Chrome Dino game clone for LVGL
 */

#include <stdint.h>
#include <stdbool.h>
#include "lvgl.h"
#include "ui_display.h"


#define DINO_JUMP_VELOCITY -15  // 恐龙跳跃速度
#define GRAVITY 0.9   // 重力加速度
#define GROUND_LEVEL 240  // 地面高度
#define BASE_OBSTACLE_SPEED 5  // 障碍物速度
#define BASE_OBSTACLE_SPAWN_INTERVAL 70  // 障碍物生成间隔
#define SPEED_INCREASE_INTERVAL 100  // 速度增加间隔

// 障碍物类型枚举
typedef enum {
    OBSTACLE_CACTUS_SMALL = 0,
    OBSTACLE_CACTUS_LARGE,
    OBSTACLE_BIRD,
    OBSTACLE_TYPE_COUNT
} OBSTACLE_TYPE_T;

// 障碍物定义结构体
typedef struct {
    int16_t width;
    int16_t height;
    int16_t y_offset;
    lv_color_t color;
} OBSTACLE_DEF_T;

static const OBSTACLE_DEF_T obstacle_defs[OBSTACLE_TYPE_COUNT] = {
    [OBSTACLE_CACTUS_SMALL] = {20, 30, 0, LV_COLOR_MAKE(0x22, 0x8B, 0x22)},
    [OBSTACLE_CACTUS_LARGE] = {30, 40, 0, LV_COLOR_MAKE(0x00, 0x64, 0x00)},
    [OBSTACLE_BIRD] = {25, 20, -50, LV_COLOR_MAKE(0x8B, 0x45, 0x13)}
};

typedef struct {
    lv_obj_t *screen;
    lv_obj_t *dino;
    lv_obj_t *ground;
    lv_obj_t *score_label;
    lv_obj_t *btn_back;
    lv_obj_t *btn_restart;
    lv_obj_t *game_over_label;
    lv_obj_t *prev_screen;
    
    lv_anim_t dino_anim;
    lv_obj_t *obstacles[10];
    OBSTACLE_TYPE_T obstacle_types[10];
    
    float dino_y;
    float dino_velocity;
    uint32_t score;
    uint32_t high_score;
    uint32_t obstacle_timer;
    uint8_t obstacle_count;
    bool is_jumping;
    bool game_over;
    
    // 新增游戏难度变量
    float current_speed;
    uint32_t current_spawn_interval;
} UI_DINO_T;

static UI_DINO_T g_dino;
static lv_timer_t *loop_timer = NULL;

// LV_IMG_DECLARE(dinoso_icon);
/* -------------------- Game logic -------------------- */
static void spawn_obstacle(void)
{
    if (g_dino.obstacle_count >= 10) return;
    
    // 随机选择障碍物类型
    OBSTACLE_TYPE_T type = rand() % OBSTACLE_TYPE_COUNT;
    const OBSTACLE_DEF_T *def = &obstacle_defs[type];
    
    lv_obj_t *obstacle = lv_obj_create(g_dino.screen);
    lv_obj_set_size(obstacle, def->width, def->height);
    lv_obj_set_style_bg_color(obstacle, def->color, 0);
    lv_obj_set_style_bg_opa(obstacle, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(obstacle, 4, 0);
    
    // 根据类型设置Y位置
    int16_t y_pos = GROUND_LEVEL - def->height + def->y_offset;
    lv_obj_set_pos(obstacle, LV_HOR_RES, y_pos);  // 从屏幕右边缘开始
    
    g_dino.obstacles[g_dino.obstacle_count] = obstacle;
    g_dino.obstacle_types[g_dino.obstacle_count] = type;
    g_dino.obstacle_count++;
}

static void update_obstacles(void)
{
    for (int i = 0; i < g_dino.obstacle_count; i++) {
        if (!g_dino.obstacles[i]) continue;
        
        lv_coord_t x = lv_obj_get_x(g_dino.obstacles[i]);
        x -= g_dino.current_speed;  // 使用当前速度
        
        if (x < -50) {
            lv_obj_del(g_dino.obstacles[i]);
            g_dino.obstacles[i] = NULL;
            continue;
        }
        
        lv_obj_set_x(g_dino.obstacles[i], x);
        
        // 碰撞检测
        lv_area_t dino_area;
        lv_obj_get_coords(g_dino.dino, &dino_area);
        
        lv_area_t obstacle_area;
        lv_obj_get_coords(g_dino.obstacles[i], &obstacle_area);
        
        if (_lv_area_is_on(&dino_area, &obstacle_area)) {
            g_dino.game_over = true;
            lv_obj_clear_flag(g_dino.game_over_label, LV_OBJ_FLAG_HIDDEN);
            return;
        }
    }
    
    // 重新整理障碍物数组，移除NULL指针
    int j = 0;
    for (int i = 0; i < g_dino.obstacle_count; i++) {
        if (g_dino.obstacles[i]) {
            if (j != i) {
                g_dino.obstacles[j] = g_dino.obstacles[i];
                g_dino.obstacle_types[j] = g_dino.obstacle_types[i];
            }
            j++;
        }
    }
    g_dino.obstacle_count = j;
}

static void cleanup_obstacles(void)
{
    for (int i = 0; i < g_dino.obstacle_count; i++) {
        if (g_dino.obstacles[i]) {
            lv_obj_del(g_dino.obstacles[i]);
        }
    }
    g_dino.obstacle_count = 0;
}

static void dino_jump(void)
{
    if (!g_dino.is_jumping && !g_dino.game_over) {
        g_dino.is_jumping = true;
        g_dino.dino_velocity = DINO_JUMP_VELOCITY;
    }
}

static void update_dino(void)
{
    if (g_dino.is_jumping) {
        g_dino.dino_y += g_dino.dino_velocity;
        g_dino.dino_velocity += GRAVITY;
        
        if (g_dino.dino_y >= GROUND_LEVEL - 40) {
            g_dino.dino_y = GROUND_LEVEL - 40;
            g_dino.is_jumping = false;
            g_dino.dino_velocity = 0;
        }
        
        lv_obj_set_y(g_dino.dino, (lv_coord_t)g_dino.dino_y);
    }
}

static void update_score(void)
{
    if (!g_dino.game_over) {
        g_dino.score++;
        lv_label_set_text_fmt(g_dino.score_label, "Score: %u", (unsigned)g_dino.score);
        
        if (g_dino.score > g_dino.high_score) {
            g_dino.high_score = g_dino.score;
        }
        
        // 每100分增加难度
        if (g_dino.score % SPEED_INCREASE_INTERVAL == 0) {
            g_dino.current_speed += 0.5;
            g_dino.current_spawn_interval = LV_MAX(30, BASE_OBSTACLE_SPAWN_INTERVAL - (g_dino.score / 100) * 10);
        }
    }
}

/* -------------------- Event handlers -------------------- */
static void on_back_to_back(lv_event_t *e)
{
    (void)e;
    
    lv_lib_pm_OpenPrePage(&page_manager);
}

static void on_restart_game(lv_event_t *e)
{
    (void)e;
    
    // Reset game state
    g_dino.score = 0;
    g_dino.game_over = false;
    g_dino.is_jumping = false;
    g_dino.dino_y = GROUND_LEVEL - 40;
    g_dino.dino_velocity = 0;
    g_dino.obstacle_timer = 0;
    
    // 重置游戏难度
    g_dino.current_speed = BASE_OBSTACLE_SPEED;
    g_dino.current_spawn_interval = BASE_OBSTACLE_SPAWN_INTERVAL;
    
    // Clean up obstacles
    cleanup_obstacles();
    
    // Reset dino position
    lv_obj_set_pos(g_dino.dino, LV_HOR_RES * 0.2, GROUND_LEVEL - 40);
    
    // Hide game over label
    lv_obj_add_flag(g_dino.game_over_label, LV_OBJ_FLAG_HIDDEN);
    
    // Update score display
    lv_label_set_text_fmt(g_dino.score_label, "Score: %u", (unsigned)g_dino.score);
}

static void on_screen_click(lv_event_t *e)
{
    (void)e;
    dino_jump();
}

static void game_loop_cb(lv_timer_t *timer)
{
    (void)timer;
    
    if (g_dino.game_over) return;
    
    update_dino();
    update_obstacles();
    update_score();
    
    // Spawn obstacles
    g_dino.obstacle_timer++;
    if (g_dino.obstacle_timer >= g_dino.current_spawn_interval) {
        spawn_obstacle();
        g_dino.obstacle_timer = 0;
    }
}

/* -------------------- UI construction -------------------- */
static void ui_dino_build_screen(void)
{
    
    // 初始化随机种子
    srand(lv_tick_get());
    
    g_dino.screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(g_dino.screen, lv_color_hex(0x87CEEB), 0);
    // 使用屏幕实际尺寸，支持横向显示
    lv_obj_set_size(g_dino.screen, LV_HOR_RES, LV_VER_RES);
    
    // Add click event to entire screen for jumping
    lv_obj_add_event_cb(g_dino.screen, on_screen_click, LV_EVENT_CLICKED, NULL);
    
    // Top bar
    lv_obj_t *top_bar = lv_obj_create(g_dino.screen);
    lv_obj_set_size(top_bar, LV_HOR_RES, 40);
    lv_obj_set_style_bg_opa(top_bar, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(top_bar, 0, 0);
    
    // Back button
    g_dino.btn_back = lv_btn_create(top_bar);
    lv_obj_align(g_dino.btn_back, LV_ALIGN_LEFT_MID, 8, 0);
    lv_obj_t *lbl_back = lv_label_create(g_dino.btn_back);
    lv_label_set_text(lbl_back, "Back");
    lv_obj_center(lbl_back);
    lv_obj_add_event_cb(g_dino.btn_back, on_back_to_back, LV_EVENT_CLICKED, NULL);
    
    // Restart button
    g_dino.btn_restart = lv_btn_create(top_bar);
    lv_obj_align(g_dino.btn_restart, LV_ALIGN_RIGHT_MID, -8, 0);
    lv_obj_t *lbl_restart = lv_label_create(g_dino.btn_restart);
    lv_label_set_text(lbl_restart, "Restart");
    lv_obj_center(lbl_restart);
    lv_obj_add_event_cb(g_dino.btn_restart, on_restart_game, LV_EVENT_CLICKED, NULL);
    
    // Score label
    g_dino.score_label = lv_label_create(top_bar);
    lv_obj_align(g_dino.score_label, LV_ALIGN_CENTER, 0, 0);
    lv_label_set_text(g_dino.score_label, "Score: 0");
    
    // Ground
    g_dino.ground = lv_obj_create(g_dino.screen);
    lv_obj_set_size(g_dino.ground, LV_HOR_RES, 80);
    lv_obj_set_style_bg_color(g_dino.ground, lv_color_hex(0x8B4513), 0);
    lv_obj_align(g_dino.ground, LV_ALIGN_BOTTOM_MID, 0, 0);
    
    // Dino - 使用图片
    g_dino.dino = lv_img_create(g_dino.screen);
    lv_img_set_src(g_dino.dino, &dinoso_icon);
    // 恐龙位置：距离左边缘20%的屏幕宽度，为横向显示留出更多空间
    lv_obj_set_pos(g_dino.dino, LV_HOR_RES * 0.2, GROUND_LEVEL - 40);
    g_dino.dino_y = GROUND_LEVEL - 40;
    
    // Game over label (initially hidden)
    g_dino.game_over_label = lv_label_create(g_dino.screen);
    lv_label_set_text(g_dino.game_over_label, "GAME OVER\nTap to restart");
    lv_obj_set_style_text_align(g_dino.game_over_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(g_dino.game_over_label, LV_ALIGN_CENTER, 0, 0);
    lv_obj_add_flag(g_dino.game_over_label, LV_OBJ_FLAG_HIDDEN);
    
    // Start game loop timer
    if(loop_timer == NULL) {
        loop_timer = lv_timer_create(game_loop_cb, 16, NULL); // ~60 FPS
    }
}

/* -------------------- Public API -------------------- */
void ui_dino_init()
{

    ui_dino_build_screen();
    on_restart_game(NULL); // Initialize game state
    lv_scr_load_anim(g_dino.screen, LV_SCR_LOAD_ANIM_MOVE_LEFT, 200, 0, false);
}

void ui_dino_deinit(void)
{

    cleanup_obstacles();
    if (loop_timer) {
        PR_DEBUG("ui_dino_deinit: delete loop_timer");
        lv_timer_pause(loop_timer);
        lv_timer_del(loop_timer);
        loop_timer = NULL;
    }
    

}

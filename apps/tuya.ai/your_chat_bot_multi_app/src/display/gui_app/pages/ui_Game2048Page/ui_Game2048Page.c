/**
 * @file ui_2048.c
 * @brief Lightweight 2048 game UI for LVGL v8 (320x480)
 */

 #include <stdint.h>
 #include "lvgl.h"
 #include "ui_Game2048Page.h"
 
 typedef struct {
     lv_obj_t *screen;
     lv_obj_t *top_bar;
     lv_obj_t *bottom_bar;
     lv_obj_t *board;
     lv_obj_t *tiles[4][4];
     lv_obj_t *labels[4][4];
     lv_obj_t *btn_back;
     lv_obj_t *btn_new;
     lv_obj_t *score_label;
     lv_obj_t *best_label;
     lv_obj_t *prev_screen; // for returning to chat
     uint32_t score;
     uint32_t best;
     uint16_t grid[4][4]; // store values (0 for empty, others are powers of 2)
 } UI2048_T;
 
 static UI2048_T g_2048;
 
 /* -------------------- Simple PRNG -------------------- */
 static uint32_t rng_state = 0x12345678u;
 static uint32_t rng_next(void) {
     uint32_t x = rng_state;
     x ^= x << 13; x ^= x >> 17; x ^= x << 5; rng_state = x; return x;
 }
 
 /* -------------------- Game core helpers -------------------- */
 static void game_spawn(void)
 {
     int empties[16]; int n = 0;
     for (int r = 0; r < 4; r++) for (int c = 0; c < 4; c++) if (g_2048.grid[r][c] == 0) empties[n++] = r * 4 + c;
     if (n == 0) return;
     int pick = rng_next() % n; int idx = empties[pick];
     int rr = idx / 4, cc = idx % 4;
     g_2048.grid[rr][cc] = (rng_next() % 10 == 0) ? 4 : 2;
 }
 
 static int compact_line(uint16_t a[4], uint32_t *score_delta)
 {
     uint16_t b[4] = {0}; int j = 0; int moved = 0;
     for (int i = 0; i < 4; i++) if (a[i] != 0) b[j++] = a[i];
     for (int i = 0; i < 3; i++) {
         if (b[i] != 0 && b[i] == b[i+1]) { b[i] *= 2; *score_delta += b[i]; b[i+1] = 0; i++; }
     }
     uint16_t c[4] = {0}; j = 0; for (int i = 0; i < 4; i++) if (b[i] != 0) c[j++] = b[i];
     for (int i = 0; i < 4; i++) { if (a[i] != c[i]) moved = 1; a[i] = c[i]; }
     return moved;
 }
 
 static int game_move_dir(lv_dir_t dir)
 {
     int moved = 0; uint32_t delta = 0;
     if (dir == LV_DIR_LEFT) {
         for (int r = 0; r < 4; r++) moved |= compact_line(g_2048.grid[r], &delta);
     } else if (dir == LV_DIR_RIGHT) {
         for (int r = 0; r < 4; r++) {
             uint16_t line[4]; for (int c = 0; c < 4; c++) line[c] = g_2048.grid[r][3-c];
             moved |= compact_line(line, &delta);
             for (int c = 0; c < 4; c++) g_2048.grid[r][3-c] = line[c];
         }
     } else if (dir == LV_DIR_TOP) {
         for (int c = 0; c < 4; c++) {
             uint16_t line[4]; for (int r = 0; r < 4; r++) line[r] = g_2048.grid[r][c];
             moved |= compact_line(line, &delta);
             for (int r = 0; r < 4; r++) g_2048.grid[r][c] = line[r];
         }
     } else if (dir == LV_DIR_BOTTOM) {
         for (int c = 0; c < 4; c++) {
             uint16_t line[4]; for (int r = 0; r < 4; r++) line[r] = g_2048.grid[3-r][c];
             moved |= compact_line(line, &delta);
             for (int r = 0; r < 4; r++) g_2048.grid[3-r][c] = line[r];
         }
     }
     if (moved) { g_2048.score += delta; if (g_2048.score > g_2048.best) g_2048.best = g_2048.score; game_spawn(); }
     return moved;
 }
 
 static void ui2048_draw_cell_style(lv_obj_t *cell, uint32_t val)
 {
     lv_color_t bg = lv_color_hex(0xCDC1B4); // empty
     lv_color_t txt = lv_color_hex(0x776E65);
     switch (val) {
     case 0: bg = lv_color_hex(0xCDC1B4); break;
     case 2: bg = lv_color_hex(0xEEE4DA); break;
     case 4: bg = lv_color_hex(0xEDE0C8); break;
     case 8: bg = lv_color_hex(0xF2B179); txt = lv_color_white(); break;
     case 16: bg = lv_color_hex(0xF59563); txt = lv_color_white(); break;
     case 32: bg = lv_color_hex(0xF67C5F); txt = lv_color_white(); break;
     case 64: bg = lv_color_hex(0xF65E3B); txt = lv_color_white(); break;
     case 128: bg = lv_color_hex(0xEDCF72); txt = lv_color_white(); break;
     case 256: bg = lv_color_hex(0xEDCC61); txt = lv_color_white(); break;
     case 512: bg = lv_color_hex(0xEDC850); txt = lv_color_white(); break;
     case 1024: bg = lv_color_hex(0xEDC53F); txt = lv_color_white(); break;
     default: bg = lv_color_hex(0xEDC22E); txt = lv_color_white(); break;
     }
     lv_obj_set_style_bg_color(cell, bg, 0);
     lv_obj_set_style_bg_opa(cell, LV_OPA_COVER, 0);
     lv_obj_set_style_radius(cell, 8, 0);
     // label color handled by caller
     (void)txt;
 }
 
 static void ui2048_update_board(void)
 {
     for (int r = 0; r < 4; r++) {
         for (int c = 0; c < 4; c++) {
             uint32_t v = g_2048.grid[r][c];
             ui2048_draw_cell_style(g_2048.tiles[r][c], v);
             if (v == 0) lv_label_set_text(g_2048.labels[r][c], "");
             else lv_label_set_text_fmt(g_2048.labels[r][c], "%u", v);
         }
     }
     lv_label_set_text_fmt(g_2048.score_label, "Score:%u", (unsigned)g_2048.score);
     lv_label_set_text_fmt(g_2048.best_label, "Best:%u", (unsigned)g_2048.best);
 }
 
 static void ui2048_reset_board(void)
 {
     // clear grid
     for (int r = 0; r < 4; r++) for (int c = 0; c < 4; c++) g_2048.grid[r][c] = 0;
     g_2048.score = 0;
     // spawn two tiles
     game_spawn(); game_spawn();
     ui2048_update_board();
 }
 
 static void on_back_to_chat(lv_event_t *e)
 {
    lv_lib_pm_OpenPrePage(&page_manager);
 }
 
 static void on_new_game(lv_event_t *e)
 {
     (void)e;
     ui2048_reset_board();
 }
 
 static void on_gesture(lv_event_t *e)
 {
     lv_dir_t d = lv_indev_get_gesture_dir(lv_event_get_indev(e));
     if (d == LV_DIR_NONE) return;
     if (game_move_dir(d)) ui2048_update_board();
 }
 
 static void ui2048_build_screen(void)
 {
    //  if (g_2048.screen) return;
     g_2048.screen = lv_obj_create(NULL);
     lv_obj_set_style_bg_color(g_2048.screen, lv_color_hex(0xFAF8EF), 0);
 
     // top bar
     g_2048.top_bar = lv_obj_create(g_2048.screen);
     lv_obj_set_size(g_2048.top_bar, LV_HOR_RES, 48);
     lv_obj_set_style_bg_opa(g_2048.top_bar, LV_OPA_TRANSP, 0);
     lv_obj_set_style_border_width(g_2048.top_bar, 0, 0);
 
     // Back button
     g_2048.btn_back = lv_btn_create(g_2048.top_bar);
     lv_obj_align(g_2048.btn_back, LV_ALIGN_LEFT_MID, 8, 0);
     lv_obj_t *lbl_back = lv_label_create(g_2048.btn_back);
     lv_label_set_text(lbl_back, "back");
     lv_obj_center(lbl_back);
     lv_obj_add_event_cb(g_2048.btn_back, on_back_to_chat, LV_EVENT_CLICKED, NULL);
 
     // New Game
     g_2048.btn_new = lv_btn_create(g_2048.top_bar);
     lv_obj_align(g_2048.btn_new, LV_ALIGN_RIGHT_MID, -8, 0);
     lv_obj_t *lbl_new = lv_label_create(g_2048.btn_new);
     lv_label_set_text(lbl_new, "New");
     lv_obj_center(lbl_new);
     lv_obj_add_event_cb(g_2048.btn_new, on_new_game, LV_EVENT_CLICKED, NULL);
 
     // bottom bar (Score & Best at bottom)
    //  g_2048.bottom_bar = lv_obj_create(g_2048.screen);
    //  lv_obj_set_size(g_2048.bottom_bar, LV_HOR_RES, 48);
    //  lv_obj_align(g_2048.bottom_bar, LV_ALIGN_BOTTOM_MID, 0, 0);
    //  lv_obj_set_style_bg_opa(g_2048.bottom_bar, LV_OPA_TRANSP, 0);
    //  lv_obj_set_style_border_width(g_2048.bottom_bar, 0, 0);
 
     g_2048.score_label = lv_label_create(g_2048.top_bar);
     lv_obj_align(g_2048.score_label, LV_ALIGN_CENTER, 0, 0);
     lv_label_set_text(g_2048.score_label, "Score:0");
 
     g_2048.best_label = lv_label_create(g_2048.top_bar);
     lv_obj_align(g_2048.best_label, LV_ALIGN_CENTER, 0, 0);
     lv_label_set_text(g_2048.best_label, "Best:0");
 
     // board (use grid layout 4x4)
     const int pad = 10;
     const int top = 60;      // top bar area
     const int bottom = 0;   // bottom bar area
     const int avail_h = LV_VER_RES - top - bottom - pad;
     const int board_size = (LV_HOR_RES - 2 * pad) < avail_h ? (LV_HOR_RES - 2 * pad) : avail_h;
     g_2048.board = lv_obj_create(g_2048.screen);
     lv_obj_set_size(g_2048.board, board_size, board_size);
     lv_obj_align(g_2048.board, LV_ALIGN_TOP_MID, 0, top);
     lv_obj_set_style_bg_color(g_2048.board, lv_color_hex(0xbbada0), 0);
     lv_obj_set_style_radius(g_2048.board, 10, 0);
     lv_obj_set_style_pad_all(g_2048.board, 6, 0);
     lv_obj_set_style_pad_row(g_2048.board, 6, 0);
     lv_obj_set_style_pad_column(g_2048.board, 6, 0);
 
     static lv_coord_t col_dsc[] = {LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
     static lv_coord_t row_dsc[] = {LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
     lv_obj_set_grid_dsc_array(g_2048.board, col_dsc, row_dsc);
     lv_obj_set_layout(g_2048.board, LV_LAYOUT_GRID);
 
     for (int r = 0; r < 4; r++) {
         for (int c = 0; c < 4; c++) {
             lv_obj_t *tile = lv_obj_create(g_2048.board);
             lv_obj_set_grid_cell(tile, LV_GRID_ALIGN_STRETCH, c, 1, LV_GRID_ALIGN_STRETCH, r, 1);
             lv_obj_set_style_border_width(tile, 0, 0);
             lv_obj_t *lbl = lv_label_create(tile);
             lv_obj_center(lbl);
             lv_obj_set_style_text_color(lbl, lv_color_hex(0x776E65), 0);
             g_2048.tiles[r][c] = tile;
             g_2048.labels[r][c] = lbl;
         }
     }
 
     ui2048_reset_board();
 
     // gestures on screen
     lv_obj_add_event_cb(g_2048.screen, on_gesture, LV_EVENT_GESTURE, NULL);
 }
 
 static void ui_2048_load_cb(void *unused)
 {
     ui2048_build_screen();
     lv_scr_load_anim(g_2048.screen, LV_SCR_LOAD_ANIM_MOVE_LEFT, 200, 0, false);
 }
 
 void ui_Game2048Page_init()
 {
     // load asynchronously to avoid re-entrancy
     lv_async_call(ui_2048_load_cb, NULL);
 }
 void ui_Game2048Page_deinit(void)
 {
    //  deinit
 

 }
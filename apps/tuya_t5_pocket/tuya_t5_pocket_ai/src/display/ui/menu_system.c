/**
 * @file menu_system.c
 * Menu System Component for AI Pocket Pet
 */

/*********************
 *      INCLUDES
 *********************/
#include "menu_system.h"
#include "dino_game.h"
#include "snake_game.h"
#include "level_indicator.h"
#include "peripherals_scan.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
/*********************
 *      DEFINES
 *********************/
#define BOTTOM_MENU_HEIGHT 26
#define SUB_MENU_PADDING 1
#define SUB_MENU_TITLE_OFFSET 10
#define SUB_MENU_LIST_OFFSET 40
#define STAT_CONTAINER_HEIGHT 30
#define STAT_CONTAINER_WIDTH (AI_PET_SCREEN_WIDTH - 40)
#define SEPARATOR_HEIGHT 2

// UI Constants
#define MENU_BUTTON_COUNT 7
#define MENU_BUTTON_SIZE 24
#define MENU_BUTTON_SPACING 30
#define MENU_BUTTON_START_X (AI_PET_SCREEN_WIDTH - 195)

// Pet stats constants
#define MAX_STAT_VALUE 100
#define MIN_WEIGHT_KG 1.0f
#define MAX_WEIGHT_KG 5.0f
#define WEIGHT_INCREMENT 0.1f

// Dino game tuning constants
#define DINO_JUMP_VY   12  /* positive = upward velocity magnitude */
#define DINO_GRAVITY    1  /* per-tick gravity (subtracts from vy) */
// Horizontal velocity applied when jumping (pixels per tick)
#define DINO_HORZ_VX    0

/**********************
 *      TYPEDEFS
 **********************/
typedef struct {
    lv_obj_t *bottom_menu;
    lv_obj_t *menu_buttons[MENU_BUTTON_COUNT];
    lv_obj_t *sub_menu;
    lv_obj_t *sub_menu_list;

    ai_pet_menu_t current_menu;
    uint8_t selected_button;
    uint8_t sub_menu_selection;

    pet_stats_t pet_stats;

    // Pet event callback system
    pet_event_callback_t pet_event_callback;
    void *pet_event_user_data;
} menu_system_data_t;

/**********************
 *  STATIC PROTOTYPES
 **********************/
static void menu_button_event_cb(lv_event_t *e);
static void keyboard_callback(keyboard_result_t result, const char *text, void *user_data);
static void show_info_menu(menu_system_data_t *data);
static void show_food_menu(menu_system_data_t *data);
static void show_bath_menu(menu_system_data_t *data);
static void show_health_menu(menu_system_data_t *data);
static void show_sleep_menu(menu_system_data_t *data);
static void show_video_menu(menu_system_data_t *data);
static void show_scan_menu(menu_system_data_t *data);
static void show_keyboard_for_pet_name(menu_system_data_t *data);
static void update_button_selection(uint8_t old_selection, uint8_t new_selection);
static void update_sub_menu_selection(uint8_t old_selection, uint8_t new_selection);
static void create_pet_name_display(menu_system_data_t *data);
static void create_pet_stats_displays(menu_system_data_t *data);
static void create_separator(void);
static void create_actions_section(void);
static void create_stat_display_item(lv_obj_t *parent, const char *label, const char *value);
static void highlight_first_sub_menu_item(menu_system_data_t *data);
// static void create_sub_menu_with_items(menu_system_data_t *data, const char *title, const char *symbols[], const char *items[], uint8_t item_count);
static uint32_t find_action_items_start(void);
// static void menu_game_stop_and_cleanup(void);
// static void menu_game_timer_cb(lv_timer_t *tmr);
// static void menu_game_event_cb(lv_event_t *e);

// Forward declaration for functions from main app
extern void lv_demo_ai_pocket_pet_show_toast(const char *message, uint32_t delay_ms);
extern lv_obj_t* lv_demo_ai_pocket_pet_get_main_screen(void);

/**********************
 *  EXTERNAL VARIABLES
 **********************/
// LV_IMG_DECLARE(scan_icon);
LV_IMG_DECLARE(info_icon);
LV_IMG_DECLARE(eat_icon);
LV_IMG_DECLARE(sick_icon);
LV_IMG_DECLARE(sleep_icon);
LV_IMG_DECLARE(toilet_icon);
LV_IMG_DECLARE(camera_icon);
LV_IMG_DECLARE(family_star);
LV_IMG_DECLARE(scan_icon);
LV_IMG_DECLARE(ducky_game);
LV_IMG_DECLARE(ducky_walk);
/**********************
 *  STATIC VARIABLES
 **********************/
static menu_system_data_t g_menu_system_data;

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

lv_obj_t* menu_system_create_bottom_menu(lv_obj_t *parent)
{
    menu_system_data_t *data = &g_menu_system_data;

    data->bottom_menu = lv_obj_create(parent);
    lv_obj_set_size(data->bottom_menu, AI_PET_SCREEN_WIDTH, BOTTOM_MENU_HEIGHT);
    lv_obj_align(data->bottom_menu, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_opa(data->bottom_menu, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(data->bottom_menu, 0, 0);
    lv_obj_set_style_pad_all(data->bottom_menu, 2, 0);

    // Custom menu icons - using image objects instead of symbols
    const lv_img_dsc_t *menu_icons[] = {
        &info_icon,
        &eat_icon,
        &toilet_icon,
        &sick_icon,
        &sleep_icon,
        &camera_icon,
        &scan_icon
    };

    for(int i = 0; i < MENU_BUTTON_COUNT; i++) {
        data->menu_buttons[i] = lv_btn_create(data->bottom_menu);
        lv_obj_set_size(data->menu_buttons[i], MENU_BUTTON_SIZE, MENU_BUTTON_SIZE);
        lv_obj_align(data->menu_buttons[i], LV_ALIGN_BOTTOM_RIGHT,
                     -(MENU_BUTTON_START_X - i * MENU_BUTTON_SPACING), 0);

        // Set default button style
        lv_obj_set_style_bg_color(data->menu_buttons[i], lv_color_white(), 0);
        lv_obj_set_style_bg_opa(data->menu_buttons[i], LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(data->menu_buttons[i], 0, 0);
        lv_obj_set_style_radius(data->menu_buttons[i], 3, 0);
        lv_obj_set_style_shadow_width(data->menu_buttons[i], 0, 0);
        lv_obj_set_style_shadow_opa(data->menu_buttons[i], LV_OPA_TRANSP, 0);

        // Custom icon buttons
        lv_obj_t *img = lv_img_create(data->menu_buttons[i]);
        lv_img_set_src(img, menu_icons[i]);
        lv_obj_align(img, LV_ALIGN_CENTER, 0, 0);

        lv_obj_add_event_cb(data->menu_buttons[i], menu_button_event_cb, LV_EVENT_CLICKED, data);
    }

    // Initialize menu state
    data->current_menu = AI_PET_MENU_MAIN;
    data->selected_button = 0;
    data->sub_menu_selection = 0;

    // Highlight first button
    update_button_selection(0, 0);

    return data->bottom_menu;
}

lv_obj_t* menu_system_create_sub_menu(lv_obj_t *parent)
{
    menu_system_data_t *data = &g_menu_system_data;

    data->sub_menu = lv_obj_create(parent);
    lv_obj_set_size(data->sub_menu, AI_PET_SCREEN_WIDTH, AI_PET_SCREEN_HEIGHT);
    lv_obj_align(data->sub_menu, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_set_style_bg_color(data->sub_menu, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(data->sub_menu, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(data->sub_menu, 0, 0);
    lv_obj_set_style_pad_all(data->sub_menu, SUB_MENU_PADDING, 0);

    // Title at the top
    lv_obj_t *title = lv_label_create(data->sub_menu);
    lv_label_set_text(title, "Menu");
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, SUB_MENU_TITLE_OFFSET);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(title, lv_color_black(), 0);

    // List for sub menu items
    data->sub_menu_list = lv_list_create(data->sub_menu);
    lv_obj_set_size(data->sub_menu_list, AI_PET_SCREEN_WIDTH - 20, AI_PET_SCREEN_HEIGHT - 60);
    lv_obj_align(data->sub_menu_list, LV_ALIGN_TOP_MID, 0, SUB_MENU_LIST_OFFSET);
    lv_obj_add_flag(data->sub_menu_list, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(data->sub_menu_list, LV_DIR_VER);

    lv_obj_set_style_border_color(data->sub_menu_list, lv_color_black(), 0);
    lv_obj_set_style_border_width(data->sub_menu_list, 2, 0);

    // Initially hide sub menu
    lv_obj_add_flag(data->sub_menu, LV_OBJ_FLAG_HIDDEN);

    return data->sub_menu;
}

void menu_system_handle_main_navigation(uint32_t key)
{
    menu_system_data_t *data = &g_menu_system_data;

    uint8_t old_selection = data->selected_button;
    uint8_t new_selection = old_selection;

    switch(key) {
        case KEY_UP: // KEY_UP
        case KEY_LEFT: // KEY_LEFT
            if(data->selected_button > 0) {
                new_selection = data->selected_button - 1;
            }
            break;

        case KEY_DOWN: // KEY_DOWN
        case KEY_RIGHT: // KEY_RIGHT
            if(data->selected_button < MENU_BUTTON_COUNT - 1) {
                new_selection = data->selected_button + 1;
            }
            break;
    }

    if(new_selection != old_selection) {
        data->selected_button = new_selection;
        update_button_selection(old_selection, new_selection);
    }
}

void menu_system_handle_sub_navigation(uint32_t key)
{
    menu_system_data_t *data = &g_menu_system_data;

    uint32_t child_count = lv_obj_get_child_cnt(data->sub_menu_list);
    if(child_count == 0) return;

    uint8_t old_selection = data->sub_menu_selection;
    uint8_t new_selection = old_selection;

    switch(key) {
        case 17: // KEY_UP
            if(data->sub_menu_selection > 0) {
                new_selection = data->sub_menu_selection - 1;
            }
            break;

        case 18: // KEY_DOWN
            if(data->sub_menu_selection < child_count - 1) {
                new_selection = data->sub_menu_selection + 1;
            }
            break;
    }

    if(new_selection != old_selection) {
        update_sub_menu_selection(old_selection, new_selection);
        data->sub_menu_selection = new_selection;
    }
}

void menu_system_handle_main_selection(void)
{
    menu_system_data_t *data = &g_menu_system_data;

    switch(data->selected_button) {
        case 0: // Info
            show_info_menu(data);
            break;
        case 1: // Food
            show_food_menu(data);
            break;
        case 2: // Bath
            show_bath_menu(data);
            break;
        case 3: // Health
            show_health_menu(data);
            break;
        case 4: // Sleep
            show_sleep_menu(data);
            break;
        case 5: // Video
            show_video_menu(data);
            break;
        case 6: // Scan (includes games)
            show_scan_menu(data);
            break;
    }
}

void menu_system_handle_sub_selection(void)
{

    int return_to_main_menu = 0;

    menu_system_data_t *data = &g_menu_system_data;

    if(data->current_menu == AI_PET_MENU_INFO) {
        uint32_t action_items_start = find_action_items_start();
        uint32_t action_index = data->sub_menu_selection - action_items_start;

        if (action_index == 0) {
            show_keyboard_for_pet_name(data);
        }
        if (action_index == 1) {
            lv_demo_ai_pocket_pet_show_toast("Not supported yet", 500);
        }
        if (action_index == 2) {
            lv_demo_ai_pocket_pet_show_toast("WIFI SETUP: Not supported yet", 1500);
        }
        if (action_index == 3) {
            if (data->pet_event_callback) {
                data->pet_event_callback(PET_STAT_RANDOMIZE, data->pet_event_user_data);
            }
            return_to_main_menu = 1;
        }
    } else if(data->current_menu == AI_PET_MENU_FOOD) {
        // Handle food menu selection based on sub_menu_selection index
        switch(data->sub_menu_selection) {
            case 0: // "Feed Hamburger"
                if (data->pet_event_callback) {
                    data->pet_event_callback(PET_EVENT_FEED_HAMBURGER, data->pet_event_user_data);
                }
                return_to_main_menu = 1;
                break;
            case 1: // "Drink Water"
                if (data->pet_event_callback) {
                    data->pet_event_callback(PET_EVENT_DRINK_WATER, data->pet_event_user_data);
                }
                return_to_main_menu = 1;
                break;
            case 2: // "Feed Pizza"
                if (data->pet_event_callback) {
                    data->pet_event_callback(PET_EVENT_FEED_PIZZA, data->pet_event_user_data);
                }
                lv_demo_ai_pocket_pet_show_toast("Need higher level", 1500);
                break;
            case 3: // "Feed Apple"
                if (data->pet_event_callback) {
                    data->pet_event_callback(PET_EVENT_FEED_APPLE, data->pet_event_user_data);
                }
                lv_demo_ai_pocket_pet_show_toast("Need higher level", 1500);
                break;
            case 4: // "Feed Fish"
                if (data->pet_event_callback) {
                    data->pet_event_callback(PET_EVENT_FEED_FISH, data->pet_event_user_data);
                }
                lv_demo_ai_pocket_pet_show_toast("Need higher level", 1500);
                break;
            case 5: // "Feed Carrot"
                if (data->pet_event_callback) {
                    data->pet_event_callback(PET_EVENT_FEED_CARROT, data->pet_event_user_data);
                }
                lv_demo_ai_pocket_pet_show_toast("Need higher level", 1500);
                break;
            case 6: // "Feed Ice Cream"
                if (data->pet_event_callback) {
                    data->pet_event_callback(PET_EVENT_FEED_ICE_CREAM, data->pet_event_user_data);
                }
                lv_demo_ai_pocket_pet_show_toast("Need higher level", 1500);
                break;
            case 7: // "Feed Cookie"
                if (data->pet_event_callback) {
                    data->pet_event_callback(PET_EVENT_FEED_COOKIE, data->pet_event_user_data);
                }
                lv_demo_ai_pocket_pet_show_toast("Need higher level", 1500);
                break;
        }
    } else if(data->current_menu == AI_PET_MENU_BATH) {
        // Handle bath menu selection
        switch(data->sub_menu_selection) {
            case 0: // "Toilet"
                if (data->pet_event_callback) {
                    data->pet_event_callback(PET_EVENT_TOILET, data->pet_event_user_data);
                }
                return_to_main_menu = 1;
                break;
            case 1: // "Take Bath"
                if (data->pet_event_callback) {
                    data->pet_event_callback(PET_EVENT_TAKE_BATH, data->pet_event_user_data);
                }
                return_to_main_menu = 1;
                break;
        }
    } else if(data->current_menu == AI_PET_MENU_HEALTH) {
        // Handle health menu selection
        switch(data->sub_menu_selection) {
            case 0: // "See Doctor"
                if (data->pet_event_callback) {
                    data->pet_event_callback(PET_EVENT_SEE_DOCTOR, data->pet_event_user_data);
                }
                return_to_main_menu = 1;
                break;
        }
    } else if(data->current_menu == AI_PET_MENU_SLEEP) {
        // Handle sleep menu selection
        switch(data->sub_menu_selection) {
            case 0: // "Sleep"
                if (data->pet_event_callback) {
                    data->pet_event_callback(PET_EVENT_SLEEP, data->pet_event_user_data);
                }
                return_to_main_menu = 1;
                break;
            case 1: // "Wake Up"
                if (data->pet_event_callback) {
                    data->pet_event_callback(PET_EVENT_WAKE_UP, data->pet_event_user_data);
                }
                return_to_main_menu = 1;
                break;
        }
    } else if(data->current_menu == AI_PET_MENU_SCAN) {
        // Handle scan menu selection (not implemented)

        switch(data->sub_menu_selection) {
            case 0:
                // lv_demo_ai_pocket_pet_show_toast("WiFi Scan: Not implemented", 1000);
                wifi_scan_show();
                if (data->pet_event_callback) {
                    // data->pet_event_callback(PET_EVENT_WIFI_SCAN, data->pet_event_user_data);
                }
                // return_to_main_menu = 1;
                break;
            case 1: {
                // lv_demo_ai_pocket_pet_show_toast("I2C Scan: Not implemented", 1000);
                i2c_scan_show(0);
                if (data->pet_event_callback) {
                    // data->pet_event_callback(PET_EVENT_I2C_SCAN, data->pet_event_user_data);
                }
                // return_to_main_menu = 1;
                break;
            }
            case 2:
                dino_game_show();
                if (data->pet_event_callback) {
                    // data->pet_event_callback(PET_EVENT_BLE_SCAN, data->pet_event_user_data);
                }
                // return_to_main_menu = 1;
                break;
            case 3:
                snake_game_show();
                // return_to_main_menu = 1;
                break;
            case 4:
                level_indicator_show();
                // return_to_main_menu = 1;
                break;
        }
    }

    if (return_to_main_menu)
    {
        data->current_menu = AI_PET_MENU_MAIN;
        lv_obj_add_flag(data->sub_menu, LV_OBJ_FLAG_HIDDEN);
    }
}

void menu_system_hide_sub_menu(void)
{
    menu_system_data_t *data = &g_menu_system_data;

    data->current_menu = AI_PET_MENU_MAIN;
    lv_obj_add_flag(data->sub_menu, LV_OBJ_FLAG_HIDDEN);
}

ai_pet_menu_t menu_system_get_current_menu(void)
{
    return g_menu_system_data.current_menu;
}

uint8_t menu_system_get_selected_button(void)
{
    return g_menu_system_data.selected_button;
}

void menu_system_init_pet_stats(pet_stats_t *stats)
{
    stats->health = 85;
    stats->hungry = 60;
    stats->clean = 70;
    stats->happy = 90;
    stats->age_days = 15;
    stats->weight_kg = 1.2f;
    strcpy(stats->name, "Ducky");

    // Also initialize in our data
    g_menu_system_data.pet_stats = *stats;
}

uint8_t menu_system_update_pet_stats(pet_stats_t *stats)
{
    if (NULL == stats) {
        return 1;
    }

    if (stats->health <= 100) {
        g_menu_system_data.pet_stats.health = stats->health;
    }
    if (stats->hungry <= 100) {
        g_menu_system_data.pet_stats.hungry = stats->hungry;
    }
    if (stats->clean <= 100) {
        g_menu_system_data.pet_stats.clean = stats->clean;
    }
    if (stats->happy <= 100) {
        g_menu_system_data.pet_stats.happy = stats->happy;
    }
    if (stats->age_days <= 999) {
        g_menu_system_data.pet_stats.age_days = stats->age_days;
    }
    if (stats->weight_kg <= 999.9) {
        g_menu_system_data.pet_stats.weight_kg = stats->weight_kg;
    }

    return 0;
}

pet_stats_t* menu_system_get_pet_stats(void)
{
    return &g_menu_system_data.pet_stats;
}

void menu_system_update_pet_stats_for_testing(void)
{
    menu_system_data_t *data = &g_menu_system_data;

    // Update pet stats for testing
    data->pet_stats.health = (data->pet_stats.health + 5) % (MAX_STAT_VALUE + 1);
    data->pet_stats.hungry = (data->pet_stats.hungry + 10) % (MAX_STAT_VALUE + 1);
    data->pet_stats.happy = (data->pet_stats.happy + 3) % (MAX_STAT_VALUE + 1);
    data->pet_stats.age_days++;
    data->pet_stats.weight_kg += WEIGHT_INCREMENT;

    if(data->pet_stats.weight_kg > MAX_WEIGHT_KG) {
        data->pet_stats.weight_kg = MIN_WEIGHT_KG;
    }

    printf("Pet stats updated - Health: %d, Hungry: %d, Happy: %d, Age: %d days, Weight: %.1f kg\n",
           data->pet_stats.health, data->pet_stats.hungry, data->pet_stats.happy,
           data->pet_stats.age_days, data->pet_stats.weight_kg);
}

void menu_system_register_pet_event_callback(pet_event_callback_t callback, void *user_data)
{
    menu_system_data_t *data = &g_menu_system_data;
    data->pet_event_callback = callback;
    data->pet_event_user_data = user_data;
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

static void menu_button_event_cb(lv_event_t *e)
{
    menu_system_data_t *data = (menu_system_data_t *)lv_event_get_user_data(e);
    lv_obj_t *btn = lv_event_get_target(e);

    // Find which button was clicked
    for(int i = 0; i < MENU_BUTTON_COUNT; i++) {
        if(data->menu_buttons[i] == btn) {
            data->selected_button = i;
            break;
        }
    }
}

static void keyboard_callback(keyboard_result_t result, const char *text, void *user_data)
{
    menu_system_data_t *data = (menu_system_data_t *)user_data;

    switch (result) {
        case KEYBOARD_RESULT_OK:
            if (text && strlen(text) > 0) {
                strncpy(data->pet_stats.name, text, sizeof(data->pet_stats.name) - 1);
                data->pet_stats.name[sizeof(data->pet_stats.name) - 1] = '\0';
                printf("Pet name updated to: %s\n", data->pet_stats.name);

                if (data->current_menu == AI_PET_MENU_INFO) {
                    show_info_menu(data);
                }

                // Show toast message confirming name change
                lv_demo_ai_pocket_pet_show_toast("Pet name updated successfully!", 1500);
            }
            break;

        case KEYBOARD_RESULT_CANCEL:
            printf("Keyboard input cancelled\n");
            break;

        case KEYBOARD_RESULT_MENU:
            printf("Menu key pressed in keyboard\n");
            break;
    }

    // Restore the main screen after keyboard operation
    lv_obj_t *main_screen = lv_demo_ai_pocket_pet_get_main_screen();
    if (main_screen) {
        lv_screen_load(main_screen);
        // Now it's safe to cleanup the keyboard
        keyboard_cleanup();
    }

    highlight_first_sub_menu_item(data);
}

static void show_keyboard_for_pet_name(menu_system_data_t *data)
{
    keyboard_show(data->pet_stats.name, keyboard_callback, data);
}

static void update_button_selection(uint8_t old_selection, uint8_t new_selection)
{
    menu_system_data_t *data = &g_menu_system_data;

    // Reset old button style
    lv_obj_set_style_bg_color(data->menu_buttons[old_selection], lv_color_white(), 0);
    lv_obj_set_style_border_width(data->menu_buttons[old_selection], 0, 0);
    lv_obj_set_style_shadow_width(data->menu_buttons[old_selection], 0, 0);

    // Handle old button content styling
    lv_obj_t *old_child = lv_obj_get_child(data->menu_buttons[old_selection], 0);
    if (old_child) {
        if (lv_obj_check_type(old_child, &lv_label_class)) {
            lv_obj_set_style_text_color(old_child, lv_color_black(), 0);
        } else if (lv_obj_check_type(old_child, &lv_image_class)) {
            // Reset image styling for unselected state
            lv_obj_set_style_img_recolor_opa(old_child, LV_OPA_TRANSP, 0);
            lv_obj_set_style_img_recolor(old_child, lv_color_black(), 0);
            lv_obj_set_style_img_opa(old_child, LV_OPA_COVER, 0);
        }
    }

    // Set new button style
    lv_obj_set_style_bg_color(data->menu_buttons[new_selection], lv_color_black(), 0);
    lv_obj_set_style_border_color(data->menu_buttons[new_selection], lv_color_black(), 0);
    lv_obj_set_style_border_width(data->menu_buttons[new_selection], 2, 0);
    lv_obj_set_style_shadow_width(data->menu_buttons[new_selection], 0, 0);

    // Handle new button content styling
    lv_obj_t *new_child = lv_obj_get_child(data->menu_buttons[new_selection], 0);
    if (new_child) {
        if (lv_obj_check_type(new_child, &lv_label_class)) {
            lv_obj_set_style_text_color(new_child, lv_color_white(), 0);
        } else if (lv_obj_check_type(new_child, &lv_image_class)) {
            // Invert the image colors for selected state - black becomes white, white becomes black
            lv_obj_set_style_img_recolor_opa(new_child, LV_OPA_COVER, 0);
            lv_obj_set_style_img_recolor(new_child, lv_color_white(), 0);
            lv_obj_set_style_img_opa(new_child, LV_OPA_COVER, 0);
        }
    }
}

static void update_sub_menu_selection(uint8_t old_selection, uint8_t new_selection)
{
    menu_system_data_t *data = &g_menu_system_data;
    uint32_t child_count = lv_obj_get_child_cnt(data->sub_menu_list);

    if(old_selection < child_count) {
        lv_obj_set_style_bg_color(lv_obj_get_child(data->sub_menu_list, old_selection), lv_color_white(), 0);
        lv_obj_set_style_text_color(lv_obj_get_child(data->sub_menu_list, old_selection), lv_color_black(), 0);
    }

    if(new_selection < child_count) {
        lv_obj_set_style_bg_color(lv_obj_get_child(data->sub_menu_list, new_selection), lv_color_black(), 0);
        lv_obj_set_style_text_color(lv_obj_get_child(data->sub_menu_list, new_selection), lv_color_white(), 0);
        lv_obj_scroll_to_view(lv_obj_get_child(data->sub_menu_list, new_selection), LV_ANIM_ON);
    }
}

static void create_pet_name_display(menu_system_data_t *data)
{
    lv_obj_t *name_container = lv_obj_create(data->sub_menu_list);
    lv_obj_set_size(name_container, STAT_CONTAINER_WIDTH, 40);
    lv_obj_set_style_bg_opa(name_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(name_container, 0, 0);
    lv_obj_set_style_pad_all(name_container, 2, 0);

    lv_obj_t *name_label = lv_label_create(name_container);
    lv_label_set_text_fmt(name_label, "Name: %s", data->pet_stats.name);
    lv_obj_align(name_label, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_text_color(name_label, lv_color_black(), 0);
    lv_obj_set_style_text_font(name_label, &lv_font_montserrat_14, 0);
}

static void create_pet_stats_displays(menu_system_data_t *data)
{
    // Helper function to create a 5-icon quantized stat bar using the family_star icon
    void create_stat_icon_bar(lv_obj_t *parent, const char *label, int value)
    {
        lv_obj_t *container = lv_obj_create(parent);
        lv_obj_set_size(container, STAT_CONTAINER_WIDTH, STAT_CONTAINER_HEIGHT);
        lv_obj_set_style_bg_opa(container, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(container, 0, 0);
        lv_obj_set_style_pad_all(container, 2, 0);

        lv_obj_t *label_obj = lv_label_create(container);
        lv_label_set_text(label_obj, label);
        lv_obj_align(label_obj, LV_ALIGN_LEFT_MID, 5, 0);
        lv_obj_set_style_text_color(label_obj, lv_color_black(), 0);

        // Quantize value to 0-5
        int filled = (value + 9) / 20; // 0-19:0, 20-39:1, ..., 100:5
        if (filled > 5) filled = 5;
        if (filled < 0) filled = 0;

        // Draw 5 icons using the family_star image
        int i;
        for (i = 0; i < 5; ++i) {
            if (i < filled) {
                lv_obj_t *icon = lv_img_create(container);
                lv_img_set_src(icon, &family_star);
                lv_obj_set_size(icon, 18, 18); // Adjust size as needed
                lv_obj_set_style_img_recolor_opa(icon, LV_OPA_TRANSP, 0); // No recolor, show as normal
                lv_obj_align(icon, LV_ALIGN_LEFT_MID, 80 + i * 22, 0);
            }
            // else: do not create an icon for empty slot (leave empty)
        }

        // Add x/5 text after the icons
        char stat_text[8];
        snprintf(stat_text, sizeof(stat_text), "%d/5", filled);
        lv_obj_t *stat_label = lv_label_create(container);
        lv_label_set_text(stat_label, stat_text);
        // Place the text after the last icon
        lv_obj_align(stat_label, LV_ALIGN_LEFT_MID, 80 + 5 * 22 + 8, 0);
        lv_obj_set_style_text_color(stat_label, lv_color_black(), 0);
    }

    create_stat_icon_bar(data->sub_menu_list, "Health:", data->pet_stats.health);
    create_stat_icon_bar(data->sub_menu_list, "Hungry:", data->pet_stats.hungry);
    create_stat_icon_bar(data->sub_menu_list, "Clean:", data->pet_stats.happy);
    create_stat_icon_bar(data->sub_menu_list, "Happy:", data->pet_stats.happy);

    char value_str[16];
    snprintf(value_str, sizeof(value_str), "%d days", data->pet_stats.age_days);
    create_stat_display_item(data->sub_menu_list, "Age:", value_str);

    snprintf(value_str, sizeof(value_str), "%.1f kg", data->pet_stats.weight_kg);
    create_stat_display_item(data->sub_menu_list, "Weight:", value_str);
}

static void create_separator(void)
{
    menu_system_data_t *data = &g_menu_system_data;

    lv_obj_t *separator = lv_obj_create(data->sub_menu_list);
    lv_obj_set_size(separator, STAT_CONTAINER_WIDTH, SEPARATOR_HEIGHT);
    lv_obj_set_style_bg_color(separator, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(separator, LV_OPA_50, 0);
}

static void create_actions_section(void)
{
    menu_system_data_t *data = &g_menu_system_data;

    // Add actions subtitle
    lv_obj_t *action_title = lv_label_create(data->sub_menu_list);
    lv_label_set_text(action_title, "Actions:");
    lv_obj_align(action_title, LV_ALIGN_LEFT_MID, 10, 0);
    lv_obj_set_style_text_color(action_title, lv_color_black(), 0);
    lv_obj_set_style_text_font(action_title, &lv_font_montserrat_14, 0);
    lv_obj_add_flag(action_title, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(action_title, LV_OBJ_FLAG_CLICK_FOCUSABLE);

    // Add action buttons
    lv_list_add_btn(data->sub_menu_list, LV_SYMBOL_EDIT, "Edit Pet Name");
    lv_list_add_btn(data->sub_menu_list, LV_SYMBOL_SETTINGS, "View Statistics");
    lv_list_add_btn(data->sub_menu_list, LV_SYMBOL_EDIT, "WIFI Settings");
    lv_list_add_btn(data->sub_menu_list, LV_SYMBOL_EDIT, "DEV:Randomize Pet Data");
}

static void create_stat_display_item(lv_obj_t *parent, const char *label, const char *value)
{
    lv_obj_t *container = lv_obj_create(parent);
    lv_obj_set_size(container, STAT_CONTAINER_WIDTH, STAT_CONTAINER_HEIGHT);
    lv_obj_set_style_bg_opa(container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(container, 0, 0);
    lv_obj_set_style_pad_all(container, 2, 0);

    lv_obj_t *label_obj = lv_label_create(container);
    lv_label_set_text(label_obj, label);
    lv_obj_align(label_obj, LV_ALIGN_LEFT_MID, 5, 0);
    lv_obj_set_style_text_color(label_obj, lv_color_black(), 0);

    lv_obj_t *value_obj = lv_label_create(container);
    lv_label_set_text(value_obj, value);
    lv_obj_align(value_obj, LV_ALIGN_RIGHT_MID, -5, 0);
    lv_obj_set_style_text_color(value_obj, lv_color_black(), 0);
}

static void highlight_first_sub_menu_item(menu_system_data_t *data)
{
    data->sub_menu_selection = 0;
    if(lv_obj_get_child_cnt(data->sub_menu_list) > 0) {
        lv_obj_set_style_bg_color(lv_obj_get_child(data->sub_menu_list, 0), lv_color_black(), 0);
        lv_obj_set_style_text_color(lv_obj_get_child(data->sub_menu_list, 0), lv_color_white(), 0);
        lv_obj_scroll_to_view(lv_obj_get_child(data->sub_menu_list, 0), LV_ANIM_ON);
    }
}

// static void create_sub_menu_with_items(menu_system_data_t *data, const char *title,
//                                      const char *symbols[], const char *items[], uint8_t item_count)
// {
//     data->current_menu = AI_PET_MENU_INFO; // This will be overridden by specific menu functions
//     lv_obj_clear_flag(data->sub_menu, LV_OBJ_FLAG_HIDDEN);

//     // Update title
//     lv_obj_t *title_obj = lv_obj_get_child(data->sub_menu, 0);
//     lv_label_set_text(title_obj, title);

//     lv_obj_clean(data->sub_menu_list);

//     // Add menu items
//     for(uint8_t i = 0; i < item_count; i++) {
//         lv_list_add_btn(data->sub_menu_list, symbols[i], items[i]);
//     }

//     highlight_first_sub_menu_item(data);
// }

static uint32_t find_action_items_start(void)
{
    menu_system_data_t *data = &g_menu_system_data;
    uint32_t child_count = lv_obj_get_child_cnt(data->sub_menu_list);

    for (uint32_t i = 0; i < child_count; i++) {
        lv_obj_t *child = lv_obj_get_child(data->sub_menu_list, i);

        // Check if this child is a label (Actions: label)
        if (lv_obj_check_type(child, &lv_label_class)) {
            const char *text = lv_label_get_text(child);
            if (text && strstr(text, "Actions:") != NULL) {
                return i + 1;
            }
        }

        // Also check if this child has a label child (for containers)
        lv_obj_t *label = lv_obj_get_child(child, 0);
        if (label && lv_obj_check_type(label, &lv_label_class)) {
            const char *text = lv_label_get_text(label);
            if (text && strstr(text, "Actions:") != NULL) {
                return i + 1;
            }
        }
    }

    return 0;
}

static void show_info_menu(menu_system_data_t *data)
{
    data->current_menu = AI_PET_MENU_INFO;
    lv_obj_clear_flag(data->sub_menu, LV_OBJ_FLAG_HIDDEN);

    // Update title
    lv_obj_t *title = lv_obj_get_child(data->sub_menu, 0);
    lv_label_set_text(title, "Pet Information");

    // Clear existing items
    lv_obj_clean(data->sub_menu_list);

    // Create all UI components
    create_pet_name_display(data);
    create_pet_stats_displays(data);
    create_separator();
    create_actions_section();

    highlight_first_sub_menu_item(data);
}

static void show_food_menu(menu_system_data_t *data)
{
    data->current_menu = AI_PET_MENU_FOOD;
    lv_obj_clear_flag(data->sub_menu, LV_OBJ_FLAG_HIDDEN);

    // Update title
    lv_obj_t *title = lv_obj_get_child(data->sub_menu, 0);
    lv_label_set_text(title, "Food & Nutrition");

    // Clear existing items
    lv_obj_clean(data->sub_menu_list);

    // Add food menu items
    lv_list_add_btn(data->sub_menu_list, LV_SYMBOL_DUMMY, "Feed Hamburger");
    lv_list_add_btn(data->sub_menu_list, LV_SYMBOL_DUMMY, "Drink Water");
    lv_list_add_btn(data->sub_menu_list, LV_SYMBOL_DUMMY, "Feed Pizza");
    lv_list_add_btn(data->sub_menu_list, LV_SYMBOL_DUMMY, "Feed Apple");
    lv_list_add_btn(data->sub_menu_list, LV_SYMBOL_DUMMY, "Feed Fish");
    lv_list_add_btn(data->sub_menu_list, LV_SYMBOL_DUMMY, "Feed Carrot");
    lv_list_add_btn(data->sub_menu_list, LV_SYMBOL_DUMMY, "Feed Ice Cream");
    lv_list_add_btn(data->sub_menu_list, LV_SYMBOL_DUMMY, "Feed Cookie");

    highlight_first_sub_menu_item(data);
}

static void show_bath_menu(menu_system_data_t *data)
{
    data->current_menu = AI_PET_MENU_BATH;
    lv_obj_clear_flag(data->sub_menu, LV_OBJ_FLAG_HIDDEN);

    // Update title
    lv_obj_t *title = lv_obj_get_child(data->sub_menu, 0);
    lv_label_set_text(title, "Toilet & Bath");

    // Clear existing items
    lv_obj_clean(data->sub_menu_list);

    // Add toilet/bath menu items
    lv_list_add_btn(data->sub_menu_list, LV_SYMBOL_DUMMY, "Toilet");
    lv_list_add_btn(data->sub_menu_list, LV_SYMBOL_DUMMY, "Take Bath");

    highlight_first_sub_menu_item(data);
}

static void show_health_menu(menu_system_data_t *data)
{
    data->current_menu = AI_PET_MENU_HEALTH;
    lv_obj_clear_flag(data->sub_menu, LV_OBJ_FLAG_HIDDEN);

    // Update title
    lv_obj_t *title = lv_obj_get_child(data->sub_menu, 0);
    lv_label_set_text(title, "Health & Wellness");

    // Clear existing items
    lv_obj_clean(data->sub_menu_list);

    // Add health menu items
    lv_list_add_btn(data->sub_menu_list, LV_SYMBOL_DUMMY, "See Doctor");

    highlight_first_sub_menu_item(data);
}

static void show_sleep_menu(menu_system_data_t *data)
{
    data->current_menu = AI_PET_MENU_SLEEP;
    lv_obj_clear_flag(data->sub_menu, LV_OBJ_FLAG_HIDDEN);

    // Update title
    lv_obj_t *title = lv_obj_get_child(data->sub_menu, 0);
    lv_label_set_text(title, "Sleep & Rest");

    // Clear existing items
    lv_obj_clean(data->sub_menu_list);

    // Add sleep menu items
    lv_list_add_btn(data->sub_menu_list, LV_SYMBOL_DUMMY, "Sleep");
    lv_list_add_btn(data->sub_menu_list, LV_SYMBOL_DUMMY, "Wake Up");

    highlight_first_sub_menu_item(data);
}

static void show_video_menu(menu_system_data_t *data)
{
    (void)data;
    lv_demo_ai_pocket_pet_show_toast("Video AI: Coming Soon...", 2000);
    // TODO: Add video stream for multimodal feature
}

static void show_scan_menu(menu_system_data_t *data)
{
    (void)data;
    // lv_demo_ai_pocket_pet_show_toast("Scan: Not implemented", 1000);
    data->current_menu = AI_PET_MENU_SCAN;
    lv_obj_clear_flag(data->sub_menu, LV_OBJ_FLAG_HIDDEN);

    // Update title
    lv_obj_t *title = lv_obj_get_child(data->sub_menu, 0);
    lv_label_set_text(title, "Device Scan");

    // Clear existing items
    lv_obj_clean(data->sub_menu_list);

    // Add demo items
    lv_list_add_btn(data->sub_menu_list, LV_SYMBOL_WIFI, "WIFI scan demo");
    lv_list_add_btn(data->sub_menu_list, LV_SYMBOL_SETTINGS, "I2C device scan demo");
    lv_list_add_btn(data->sub_menu_list, LV_SYMBOL_PLAY, "Dino Game");
    lv_list_add_btn(data->sub_menu_list, LV_SYMBOL_SHUFFLE, "Snake Game");
    lv_list_add_btn(data->sub_menu_list, LV_SYMBOL_EYE_OPEN, "Level Indicator");

    highlight_first_sub_menu_item(data);
}
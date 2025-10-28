/**
 * @file menu_food_screen.c
 * @brief Implementation of the food menu screen for the application
 *
 * This file contains the implementation of the food menu screen which displays
 * food and nutrition options for the pet.
 *
 * The food menu includes:
 * - Food item selection with visual feedback
 * - Level-based food unlocking system
 * - Pet feeding functionality
 * - Keyboard event handling for navigation
 *
 * @copyright Copyright (c) 2024 LVGL PC Simulator Project
 */

#include "menu_food_screen.h"
#include "screen_manager.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/***********************************************************
***********************variable define**********************
***********************************************************/

static lv_obj_t *ui_menu_food_screen_screen;
static lv_obj_t *menu_food_screen_list;
static lv_timer_t *timer;
static uint8_t selected_item = 0;
static uint8_t pet_level = 1;
static food_event_callback_t food_callback = NULL;
static void *food_callback_user_data = NULL;

Screen_t menu_food_screen = {
    .init = menu_food_screen_init,
    .deinit = menu_food_screen_deinit,
    .screen_obj = &ui_menu_food_screen_screen,
    .name = "food_menu",
};

// Food items configuration
static food_item_t food_items[] = {
    {"Feed Hamburger", LV_SYMBOL_PLUS, 1, 30, 5, true},
    {"Drink Water", LV_SYMBOL_REFRESH, 1, 10, 2, true},
    {"Feed Pizza", LV_SYMBOL_PLUS, 2, 40, 8, false},
    {"Feed Apple", LV_SYMBOL_PLUS, 3, 25, 10, false},
    {"Feed Fish", LV_SYMBOL_PLUS, 4, 35, 12, false},
    {"Feed Carrot", LV_SYMBOL_PLUS, 3, 20, 8, false},
    {"Feed Ice Cream", LV_SYMBOL_PLUS, 5, 15, 15, false},
    {"Feed Cookie", LV_SYMBOL_PLUS, 4, 20, 12, false},
};

#define FOOD_ITEMS_COUNT (sizeof(food_items) / sizeof(food_items[0]))

/***********************************************************
********************function declaration********************
***********************************************************/

static void menu_food_screen_timer_cb(lv_timer_t *timer);
static void keyboard_event_cb(lv_event_t *e);
static void create_food_items(void);
static void create_food_item(food_item_t *item, uint8_t index);
static void update_selection(uint8_t old_selection, uint8_t new_selection);
static void handle_food_selection(void);
static void update_food_availability(void);

/***********************************************************
***********************function define**********************
***********************************************************/

/**
 * @brief Timer callback for the food menu screen
 *
 * This function is called when the food menu timer expires.
 * It can be used for periodic updates.
 *
 * @param timer The timer object
 */
static void menu_food_screen_timer_cb(lv_timer_t *timer)
{
    printf("[%s] food menu timer callback\n", menu_food_screen.name);
    // Add any periodic update logic here
}

/**
 * @brief Keyboard event callback
 *
 * This function handles keyboard events for the food menu screen.
 *
 * @param e The event object
 */
static void keyboard_event_cb(lv_event_t *e)
{
    uint32_t key = lv_event_get_key(e);
    printf("[%s] Keyboard event received: key = %d\n", menu_food_screen.name, key);

    uint32_t child_count = lv_obj_get_child_cnt(menu_food_screen_list);
    if (child_count == 0) return;

    uint8_t old_selection = selected_item;
    uint8_t new_selection = old_selection;

    switch (key) {
        case KEY_UP:
            if (selected_item > 0) {
                new_selection = selected_item - 1;
            }
            break;
        case KEY_DOWN:
            if (selected_item < child_count - 1) {
                new_selection = selected_item + 1;
            }
            break;
        case KEY_ENTER:
            handle_food_selection();
            break;
        case KEY_ESC:
            printf("ESC key pressed - returning to main menu\n");
            screen_back();
            break;
            break;
        default:
            printf("Key %d pressed\n", key);
            break;
    }

    if (new_selection != old_selection) {
        update_selection(old_selection, new_selection);
        selected_item = new_selection;
    }
}

/**
 * @brief Create all food items in the list
 */
static void create_food_items(void)
{
    for (uint8_t i = 0; i < FOOD_ITEMS_COUNT; i++) {
        create_food_item(&food_items[i], i);
    }
}

/**
 * @brief Create a single food item with proper styling
 */
static void create_food_item(food_item_t *item, uint8_t index)
{
    lv_obj_t *btn = lv_list_add_btn(menu_food_screen_list, item->icon, item->name);

    // Style based on availability
    if (!item->available) {
        lv_obj_set_style_text_color(btn, lv_color_make(128, 128, 128), 0);
        lv_obj_set_style_bg_color(btn, lv_color_make(240, 240, 240), 0);

        // Add level requirement text
        lv_obj_t *level_label = lv_label_create(btn);
        char level_text[16];
        snprintf(level_text, sizeof(level_text), "Lv.%d", item->required_level);
        lv_label_set_text(level_label, level_text);
        lv_obj_align(level_label, LV_ALIGN_RIGHT_MID, -5, 0);
        lv_obj_set_style_text_color(level_label, lv_color_make(255, 0, 0), 0);
        lv_obj_set_style_text_font(level_label, &lv_font_montserrat_10, 0);
    } else {
        // Add nutrition info for available items
        lv_obj_t *nutrition_label = lv_label_create(btn);
        char nutrition_text[32];
        snprintf(nutrition_text, sizeof(nutrition_text), "H:%d +%d",
                item->hunger_restore, item->happiness_bonus);
        lv_label_set_text(nutrition_label, nutrition_text);
        lv_obj_align(nutrition_label, LV_ALIGN_RIGHT_MID, -5, 0);
        lv_obj_set_style_text_color(nutrition_label, lv_color_make(0, 128, 0), 0);
        lv_obj_set_style_text_font(nutrition_label, &lv_font_montserrat_10, 0);
    }
}

/**
 * @brief Update visual selection highlighting
 */
static void update_selection(uint8_t old_selection, uint8_t new_selection)
{
    uint32_t child_count = lv_obj_get_child_cnt(menu_food_screen_list);

    if (old_selection < child_count) {
        lv_obj_set_style_bg_color(lv_obj_get_child(menu_food_screen_list, old_selection), lv_color_white(), 0);
        lv_obj_set_style_text_color(lv_obj_get_child(menu_food_screen_list, old_selection), lv_color_black(), 0);
    }

    if (new_selection < child_count) {
        lv_obj_set_style_bg_color(lv_obj_get_child(menu_food_screen_list, new_selection), lv_color_black(), 0);
        lv_obj_set_style_text_color(lv_obj_get_child(menu_food_screen_list, new_selection), lv_color_white(), 0);
        lv_obj_scroll_to_view(lv_obj_get_child(menu_food_screen_list, new_selection), LV_ANIM_ON);
    }
}

/**
 * @brief Handle food selection and trigger callback
 */
static void handle_food_selection(void)
{
    if (selected_item >= FOOD_ITEMS_COUNT) return;

    food_item_t *selected_food = &food_items[selected_item];

    if (!selected_food->available) {
        printf("Food item '%s' is not available (requires level %d)\n",
               selected_food->name, selected_food->required_level);
        return;
    }

    printf("Selected food: %s (H:+%d, Happy:+%d)\n",
           selected_food->name, selected_food->hunger_restore, selected_food->happiness_bonus);

    // Trigger callback based on selection
    if (food_callback) {
        food_event_t event = (food_event_t)selected_item;
        food_callback(event, food_callback_user_data);
    }
}

/**
 * @brief Update food availability based on pet level
 */
static void update_food_availability(void)
{
    for (uint8_t i = 0; i < FOOD_ITEMS_COUNT; i++) {
        food_items[i].available = (pet_level >= food_items[i].required_level);
    }
}

/**
 * @brief Initialize the food menu screen
 *
 * This function creates the food menu UI with food item selection
 * and nutritional information display.
 */
void menu_food_screen_init(void)
{
    ui_menu_food_screen_screen = lv_obj_create(NULL);
    lv_obj_set_size(ui_menu_food_screen_screen, 384, 168);
    lv_obj_set_style_bg_color(ui_menu_food_screen_screen, lv_color_white(), 0);

    // Title at the top
    lv_obj_t *title = lv_label_create(ui_menu_food_screen_screen);
    lv_label_set_text(title, "Food & Nutrition");
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 10);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(title, lv_color_black(), 0);

    // Pet level indicator
    lv_obj_t *level_indicator = lv_label_create(ui_menu_food_screen_screen);
    char level_text[32];
    snprintf(level_text, sizeof(level_text), "Pet Level: %d", pet_level);
    lv_label_set_text(level_indicator, level_text);
    lv_obj_align(level_indicator, LV_ALIGN_TOP_RIGHT, -10, 10);
    lv_obj_set_style_text_font(level_indicator, &lv_font_montserrat_10, 0);
    lv_obj_set_style_text_color(level_indicator, lv_color_make(0, 0, 255), 0);

    // List for food menu items
    menu_food_screen_list = lv_list_create(ui_menu_food_screen_screen);
    lv_obj_set_size(menu_food_screen_list, 364, 128);
    lv_obj_align(menu_food_screen_list, LV_ALIGN_TOP_MID, 0, 40);
    lv_obj_add_flag(menu_food_screen_list, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(menu_food_screen_list, LV_DIR_VER);

    lv_obj_set_style_border_color(menu_food_screen_list, lv_color_black(), 0);
    lv_obj_set_style_border_width(menu_food_screen_list, 2, 0);

    // Update food availability and create items
    update_food_availability();
    create_food_items();

    // Highlight first item
    selected_item = 0;
    if (lv_obj_get_child_cnt(menu_food_screen_list) > 0) {
        update_selection(0, 0);
    }

    timer = lv_timer_create(menu_food_screen_timer_cb, 1000, NULL);
    lv_obj_add_event_cb(ui_menu_food_screen_screen, keyboard_event_cb, LV_EVENT_KEY, NULL);
    lv_group_add_obj(lv_group_get_default(), ui_menu_food_screen_screen);
    lv_group_focus_obj(ui_menu_food_screen_screen);
}

/**
 * @brief Deinitialize the food menu screen
 *
 * This function cleans up the food menu by removing event callbacks
 * and freeing resources.
 */
void menu_food_screen_deinit(void)
{
    if (ui_menu_food_screen_screen) {
        printf("deinit food menu screen\n");
        lv_obj_remove_event_cb(ui_menu_food_screen_screen, keyboard_event_cb);
        lv_group_remove_obj(ui_menu_food_screen_screen);
    }
    if (timer) {
        lv_timer_del(timer);
        timer = NULL;
    }
}

/**
 * @brief Set pet level for food unlocking
 *
 * @param level Current pet level
 */
void menu_food_screen_set_pet_level(uint8_t level)
{
    pet_level = level;
    update_food_availability();
}

/**
 * @brief Register food event callback
 *
 * @param callback Callback function for food events
 * @param user_data User data passed to callback
 */
void menu_food_screen_register_callback(food_event_callback_t callback, void *user_data)
{
    food_callback = callback;
    food_callback_user_data = user_data;
}

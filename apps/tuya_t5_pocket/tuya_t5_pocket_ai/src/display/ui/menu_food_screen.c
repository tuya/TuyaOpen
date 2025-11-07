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
#include "main_screen.h"
#include "toast_screen.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/***********************************************************
***********************variable define**********************
***********************************************************/

static lv_obj_t *ui_menu_food_screen_screen;
static lv_obj_t *menu_food_screen_list;
static lv_timer_t *timer;
static lv_timer_t *pet_state_timer;  // Timer for eat animation
static uint8_t selected_item = 0;
static uint8_t last_selected_item = 0;
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
static void pet_state_timer_cb(lv_timer_t *timer);
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
 * @brief Timer callback for eat animation
 *
 * This function is called 3 seconds after starting the eat animation
 * to switch back to normal state.
 *
 * @param timer The timer object
 */
static void pet_state_timer_cb(lv_timer_t *timer)
{
    printf("[%s] eat animation timer callback - switching to normal state\n", menu_food_screen.name);

    // Switch pet back to normal state
    main_screen_set_pet_animation_state(AI_PET_STATE_NORMAL);

    // Clean up the timer
    if (pet_state_timer) {
        lv_timer_del(pet_state_timer);
        pet_state_timer = NULL;
    }
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
            last_selected_item = 0;
            screen_back();
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

    lv_obj_set_style_text_color(btn, lv_color_black(), 0);
    lv_obj_set_style_bg_color(btn, lv_color_white(), 0);

    // Add level requirement text
    lv_obj_t *level_label = lv_label_create(btn);
    char level_text[16];
    snprintf(level_text, sizeof(level_text), "Lv.%d", item->required_level);
    lv_label_set_text(level_label, level_text);
    lv_obj_align(level_label, LV_ALIGN_RIGHT_MID, -5, 0);
    lv_obj_set_style_text_color(level_label, lv_color_black(), 0);
    lv_obj_set_style_text_font(level_label, &lv_font_montserrat_10, 0);
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

    last_selected_item = selected_item;

    food_item_t *selected_food = &food_items[selected_item];

    printf("Selected food: %s (index: %d)\n", selected_food->name, selected_item);

    // Handle different food items with specific logic
    switch (selected_item) {
        case 0:  // Feed Hamburger
            if (selected_food->available) {
                printf("Feeding hamburger - returning to main screen and playing eat animation\n");
                // Return to main screen and play eating animation
                screen_back();
                // Trigger eating animation on main screen
                main_screen_set_pet_animation_state(AI_PET_STATE_EAT);

                // Start timer to switch back to normal state after 3 seconds
                if (pet_state_timer) {
                    lv_timer_del(pet_state_timer);  // Clean up existing timer
                }
                pet_state_timer = lv_timer_create(pet_state_timer_cb, 3000, NULL);
                // lv_timer_set_repeat_count(pet_state_timer, 1);  // Run only once

                printf("Started eat animation timer for 3 seconds\n");
            } else {
                printf("Hamburger not available (requires level %d)\n", selected_food->required_level);
                toast_screen_show("Unlock at Higher Level", 2000);
            }
            break;

        case 1:  // Drink Water
            printf("Water selected - showing toast\n");
            if (selected_food->available) {
                printf("Feeding water - returning to main screen and playing drink animation\n");
                // Return to main screen and play drinking animation
                screen_back();
                // Trigger drinking animation on main screen
                main_screen_set_pet_animation_state(AI_PET_STATE_DANCE);

                // Start timer to switch back to normal state after 3 seconds
                if (pet_state_timer) {
                    lv_timer_del(pet_state_timer);  // Clean up existing timer
                }
                pet_state_timer = lv_timer_create(pet_state_timer_cb, 2000, NULL);
                // lv_timer_set_repeat_count(pet_state_timer, 1);  // Run only once

                printf("Started eat animation timer for 3 seconds\n");
            } else {
                printf("Hamburger not available (requires level %d)\n", selected_food->required_level);
                toast_screen_show("Unlock at Higher Level", 2000);
            }
            break;

        case 2:  // Feed Pizza
            printf("Pizza selected - showing toast\n");
            if (selected_food->available) {
                toast_screen_show("Coming Soon: Pizza Feature", 2000);
            } else {
                toast_screen_show("Unlock at Higher Level", 2000);
            }
            break;

        case 3:  // Feed Apple
            printf("Apple selected - showing toast\n");
            if (selected_food->available) {
                toast_screen_show("Coming Soon: Apple Feature", 2000);
            } else {
                toast_screen_show("Unlock at Higher Level", 2000);
            }
            break;

        case 4:  // Feed Fish
            printf("Fish selected - showing toast\n");
            if (selected_food->available) {
                toast_screen_show("Coming Soon: Fish Feature", 2000);
            } else {
                toast_screen_show("Unlock at Higher Level", 2000);
            }
            break;

        case 5:  // Feed Carrot
            printf("Carrot selected - showing toast\n");
            if (selected_food->available) {
                toast_screen_show("Coming Soon: Carrot Feature", 2000);
            } else {
                toast_screen_show("Unlock at Higher Level", 2000);
            }
            break;

        case 6:  // Feed Ice Cream
            printf("Ice Cream selected - showing toast\n");
            if (selected_food->available) {
                toast_screen_show("Coming Soon: Ice Cream Feature", 2000);
            } else {
                toast_screen_show("Unlock at Higher Level", 2000);
            }
            break;

        case 7:  // Feed Cookie
            printf("Cookie selected - showing toast\n");
            if (selected_food->available) {
                toast_screen_show("Coming Soon: Cookie Feature", 2000);
            } else {
                toast_screen_show("Unlock at Higher Level", 2000);
            }
            break;

        default:
            printf("Unknown food item selected: %d\n", selected_item);
            toast_screen_show("Unknown Food Item", 2000);
            break;
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
    lv_obj_set_size(ui_menu_food_screen_screen, AI_PET_SCREEN_WIDTH, AI_PET_SCREEN_HEIGHT);
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

    selected_item = last_selected_item;
    uint32_t child_count = lv_obj_get_child_cnt(menu_food_screen_list);

    if (selected_item >= child_count) {
        selected_item = 0;
        last_selected_item = 0;
    }

    if (child_count > 0) {
        update_selection(0, selected_item);
        printf("[%s] Restored selection to item %d\n", menu_food_screen.name, selected_item);
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
        lv_obj_remove_event_cb(ui_menu_food_screen_screen, NULL);
        lv_group_remove_obj(ui_menu_food_screen_screen);
    }
    if (timer) {
        lv_timer_del(timer);
        timer = NULL;
    }
    if (pet_state_timer) {
        lv_timer_del(pet_state_timer);
        pet_state_timer = NULL;
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

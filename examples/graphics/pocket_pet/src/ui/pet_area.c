/**
 * @file pet_area.c
 * Pet Display Area Component for AI Pocket Pet
 */

/*********************
 *      INCLUDES
 *********************/
#include "pet_area.h"
#include <time.h>
#include <stdlib.h>
#include <stdio.h>

// Pet animation
LV_IMG_DECLARE(ducky_walk);
LV_IMG_DECLARE(ducky_walk_to_left);
LV_IMG_DECLARE(ducky_blink);
LV_IMG_DECLARE(ducky_stand_still);

// Special state animation declarations
LV_IMG_DECLARE(ducky_sleep);
LV_IMG_DECLARE(ducky_dance);
LV_IMG_DECLARE(ducky_eat);
LV_IMG_DECLARE(ducky_bath);
LV_IMG_DECLARE(ducky_toilet);
LV_IMG_DECLARE(ducky_sick);
LV_IMG_DECLARE(ducky_emotion_happy);
LV_IMG_DECLARE(ducky_emotion_angry);
LV_IMG_DECLARE(ducky_emotion_cry);

/*********************
 *      DEFINES
 *********************/
#define STATUS_BAR_HEIGHT 24
#define BOTTOM_MENU_HEIGHT 26
#define PET_AREA_HEIGHT (AI_PET_SCREEN_HEIGHT - STATUS_BAR_HEIGHT - BOTTOM_MENU_HEIGHT)

// Pet animation constants
#define PET_ANIMATION_INTERVAL 20
#define PET_MOVEMENT_INTERVAL 50   // Natural movement timing
#define PET_MOVEMENT_STEP 2        // Smooth movement step
#define PET_MOVEMENT_LIMIT 80      // Movement boundaries
#define PET_WALK_DURATION_MIN 2000 // Minimum walk duration (ms)
#define PET_WALK_DURATION_MAX 8000 // Maximum walk duration (ms)
#define PET_IDLE_DURATION_MIN 3000 // Minimum idle duration (ms)
#define PET_IDLE_DURATION_MAX 10000 // Maximum idle duration (ms)
// Stand longer in general: increase minimum and maximum time before switching idle animations
#define PET_IDLE_ANIMATION_SWITCH_MIN 4000 // Minimum time before switching idle animations (ms)
#define PET_IDLE_ANIMATION_SWITCH_MAX 12000 // Maximum time before switching idle animations (ms)

/**********************
 *      TYPEDEFS
 **********************/
typedef struct {
    lv_obj_t *pet_area;

    // Normal state animation objects (for walking behavior)
    lv_obj_t *pet_image_walk;
    lv_obj_t *pet_image_walk_left;
    lv_obj_t *pet_image_blink;
    lv_obj_t *pet_image_stand;
    lv_obj_t *current_normal_image; // Points to the currently active normal state image

    // Single special animation slot (simple and reliable)
    lv_obj_t *pet_special_animation;
    ai_pet_state_t current_animation_state;

    lv_timer_t *pet_animation_timer;
    lv_timer_t *pet_movement_timer;

    // Pet movement state (only active in normal state)
    int16_t pet_x_pos;
    int8_t pet_direction;  // 1 = right, -1 = left
    uint32_t pet_state_timer;
    uint32_t pet_state_duration;
    bool pet_is_walking;
    uint8_t idle_animation_state; // 0 = blink, 1 = stand
    uint32_t idle_animation_timer;
    uint32_t idle_animation_duration;
    ai_pet_state_t pet_state; // Legacy support
} pet_area_data_t;

/**********************
 *  STATIC PROTOTYPES
 **********************/
static void pet_animation_cb(lv_timer_t *timer);
static void pet_movement_cb(lv_timer_t *timer);
static void switch_pet_animation(lv_obj_t *new_animation);
static void switch_to_special_animation(ai_pet_state_t state);
static void switch_to_normal_animation(void);
static const lv_img_dsc_t* get_animation_source(ai_pet_state_t state);

/**********************
 *  STATIC VARIABLES
 **********************/
static pet_area_data_t g_pet_area_data;

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

lv_obj_t* pet_area_create(lv_obj_t *parent)
{
    pet_area_data_t *data = &g_pet_area_data;

    data->pet_area = lv_obj_create(parent);
    // Use full screen height minus status bar and bottom menu
    lv_obj_set_size(data->pet_area, AI_PET_SCREEN_WIDTH, AI_PET_SCREEN_HEIGHT - STATUS_BAR_HEIGHT - BOTTOM_MENU_HEIGHT);
    lv_obj_align(data->pet_area, LV_ALIGN_TOP_MID, 0, STATUS_BAR_HEIGHT); // Move 10 px higher
    lv_obj_set_style_bg_opa(data->pet_area, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(data->pet_area, 0, 0);
    lv_obj_set_style_pad_all(data->pet_area, 0, 0); // Remove padding to maximize space

    // Disable scrolling for pet area
    lv_obj_clear_flag(data->pet_area, LV_OBJ_FLAG_SCROLLABLE);

    // Create a container for the GIF widgets to constrain rendering area
    lv_obj_t *gif_container = lv_obj_create(data->pet_area);
    lv_obj_set_size(gif_container, 170+10, 170+10); // Optimized size for performance
    lv_obj_align(gif_container, LV_ALIGN_CENTER, 0, -5); // Center with slight upward offset
    lv_obj_set_style_bg_opa(gif_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(gif_container, 0, 0);
    lv_obj_set_style_pad_all(gif_container, 0, 0);
    lv_obj_clear_flag(gif_container, LV_OBJ_FLAG_SCROLLABLE);

    // Create four separate GIF widgets for smooth animation transitions
    // This prevents black flashing by avoiding source switching

    // Walk right animation
    data->pet_image_walk = lv_gif_create(gif_container);
    lv_gif_set_src(data->pet_image_walk, &ducky_walk);
    lv_obj_align(data->pet_image_walk, LV_ALIGN_CENTER, 0, 0);
    lv_obj_clear_flag(data->pet_image_walk, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(data->pet_image_walk, 159, 164);
    lv_obj_set_style_bg_opa(data->pet_image_walk, LV_OPA_TRANSP, 0);

    // Walk left animation
    data->pet_image_walk_left = lv_gif_create(gif_container);
    lv_gif_set_src(data->pet_image_walk_left, &ducky_walk_to_left);
    lv_obj_align(data->pet_image_walk_left, LV_ALIGN_CENTER, 0, 0);
    lv_obj_clear_flag(data->pet_image_walk_left, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(data->pet_image_walk_left, 159, 164);
    lv_obj_set_style_bg_opa(data->pet_image_walk_left, LV_OPA_TRANSP, 0);

    // Blink animation
    data->pet_image_blink = lv_gif_create(gif_container);
    lv_gif_set_src(data->pet_image_blink, &ducky_blink);
    lv_obj_align(data->pet_image_blink, LV_ALIGN_CENTER, 0, 0);
    lv_obj_clear_flag(data->pet_image_blink, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(data->pet_image_blink, 159, 164);
    lv_obj_set_style_bg_opa(data->pet_image_blink, LV_OPA_TRANSP, 0);

    // Stand animation
    data->pet_image_stand = lv_gif_create(gif_container);
    lv_gif_set_src(data->pet_image_stand, &ducky_stand_still);
    lv_obj_align(data->pet_image_stand, LV_ALIGN_CENTER, 0, 0);
    lv_obj_clear_flag(data->pet_image_stand, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(data->pet_image_stand, 159, 164);
    lv_obj_set_style_bg_opa(data->pet_image_stand, LV_OPA_TRANSP, 0);

    // Create single special animation slot (simple approach)
    data->pet_special_animation = lv_gif_create(gif_container);
    lv_obj_align(data->pet_special_animation, LV_ALIGN_CENTER, 0, -5); // Move upward by 5 px
    lv_obj_clear_flag(data->pet_special_animation, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(data->pet_special_animation, 159, 164); // Same size as normal animations
    lv_obj_set_style_bg_opa(data->pet_special_animation, LV_OPA_TRANSP, 0);

    // Ensure special animation is on top layer
    lv_obj_move_foreground(data->pet_special_animation);

    lv_obj_add_flag(data->pet_special_animation, LV_OBJ_FLAG_HIDDEN);

    // No additional initialization needed

    // Set initial active image and hide others
    data->current_normal_image = data->pet_image_stand; // Default to standing
    lv_obj_add_flag(data->pet_image_blink, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(data->pet_image_walk, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(data->pet_image_walk_left, LV_OBJ_FLAG_HIDDEN);

    // Initialize pet state
    data->current_animation_state = AI_PET_STATE_NORMAL;
    data->pet_state = AI_PET_STATE_IDLE; // Legacy support
    data->pet_x_pos = 0;
    data->pet_direction = 1;  // Start facing right
    data->pet_is_walking = false;
    data->idle_animation_state = 1; // Start with stand animation
    data->pet_state_timer = 0;
    data->pet_state_duration = PET_IDLE_DURATION_MIN + (rand() % (PET_IDLE_DURATION_MAX - PET_IDLE_DURATION_MIN));
    data->idle_animation_timer = 0;
    data->idle_animation_duration = PET_IDLE_ANIMATION_SWITCH_MIN + (rand() % (PET_IDLE_ANIMATION_SWITCH_MAX - PET_IDLE_ANIMATION_SWITCH_MIN));

    return data->pet_area;
}

void pet_area_start_animation(void)
{
    pet_area_data_t *data = &g_pet_area_data;

    data->pet_animation_timer = lv_timer_create(pet_animation_cb, PET_ANIMATION_INTERVAL, data);
    data->pet_movement_timer = lv_timer_create(pet_movement_cb, PET_MOVEMENT_INTERVAL, data);
}

void pet_area_stop_animation(void)
{
    pet_area_data_t *data = &g_pet_area_data;

    if (data->pet_animation_timer) {
        lv_timer_del(data->pet_animation_timer);
        data->pet_animation_timer = NULL;
    }

    if (data->pet_movement_timer) {
        lv_timer_del(data->pet_movement_timer);
        data->pet_movement_timer = NULL;
    }
}

ai_pet_state_t pet_area_get_state(void)
{
    return g_pet_area_data.pet_state;
}

void pet_area_set_animation(ai_pet_state_t state)
{
    pet_area_data_t *data = &g_pet_area_data;

        // Check if we're already in the target state
    if (data->current_animation_state == state) {
        return;
    }

    // Switch animations
    if (state == AI_PET_STATE_NORMAL) {
        switch_to_normal_animation();
    } else {
        switch_to_special_animation(state);
    }
}

ai_pet_state_t pet_area_get_animation(void)
{
    return g_pet_area_data.current_animation_state;
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

static void pet_animation_cb(lv_timer_t *timer)
{
    pet_area_data_t *data = (pet_area_data_t *)lv_timer_get_user_data(timer);

    // Only run normal animations if we're in normal state
    if (data->current_animation_state != AI_PET_STATE_NORMAL) {
        return;
    }

    // Ensure current normal animation is always visible
    lv_obj_clear_flag(data->current_normal_image, LV_OBJ_FLAG_HIDDEN);

    // Update legacy pet state based on movement system
    if (data->pet_is_walking) {
        data->pet_state = AI_PET_STATE_WALKING;
    } else {
        data->pet_state = AI_PET_STATE_IDLE;
    }
}

static void pet_movement_cb(lv_timer_t *timer)
{
    pet_area_data_t *data = (pet_area_data_t *)lv_timer_get_user_data(timer);

    // Only handle movement and state changes in normal state
    if (data->current_animation_state != AI_PET_STATE_NORMAL) {
        return;
    }

    // Update state timer
    data->pet_state_timer += PET_MOVEMENT_INTERVAL;

    // Handle idle animation switching when not walking
    if (!data->pet_is_walking) {
        data->idle_animation_timer += PET_MOVEMENT_INTERVAL;

        // Check if it's time to switch idle animations
        if (data->idle_animation_timer >= data->idle_animation_duration) {
            // Toggle between blink and stand animations
            data->idle_animation_state = 1 - data->idle_animation_state; // Toggle between 0 and 1

            if (data->idle_animation_state == 0) {
                switch_pet_animation(data->pet_image_blink);
                data->current_normal_image = data->pet_image_blink;
            } else {
                switch_pet_animation(data->pet_image_stand);
                data->current_normal_image = data->pet_image_stand;
            }

            // Reset idle animation timer and set new duration
            data->idle_animation_timer = 0;
            data->idle_animation_duration = PET_IDLE_ANIMATION_SWITCH_MIN + (rand() % (PET_IDLE_ANIMATION_SWITCH_MAX - PET_IDLE_ANIMATION_SWITCH_MIN));
        }
    }

    // Check if it's time to change state (walking vs idle)
    if (data->pet_state_timer >= data->pet_state_duration) {
        // Switch between walking and idle
        data->pet_is_walking = !data->pet_is_walking;

        if (data->pet_is_walking) {
            // Start walking - choose random direction and duration
            data->pet_direction = (rand() % 2) ? 1 : -1;
            data->pet_state_duration = PET_WALK_DURATION_MIN + (rand() % (PET_WALK_DURATION_MAX - PET_WALK_DURATION_MIN));

            // Set appropriate animation based on direction
            if (data->pet_direction == 1) {
                switch_pet_animation(data->pet_image_walk);
                data->current_normal_image = data->pet_image_walk;
            } else {
                switch_pet_animation(data->pet_image_walk_left);
                data->current_normal_image = data->pet_image_walk_left;
            }
        } else {
            // Start idle - choose random duration and return to current idle animation
            data->pet_state_duration = PET_IDLE_DURATION_MIN + (rand() % (PET_IDLE_DURATION_MAX - PET_IDLE_DURATION_MIN));

            // Reset idle animation timers when starting new idle period
            data->idle_animation_timer = 0;
            data->idle_animation_duration = PET_IDLE_ANIMATION_SWITCH_MIN + (rand() % (PET_IDLE_ANIMATION_SWITCH_MAX - PET_IDLE_ANIMATION_SWITCH_MIN));

            // Start with current idle animation state
            if (data->idle_animation_state == 0) {
                switch_pet_animation(data->pet_image_blink);
                data->current_normal_image = data->pet_image_blink;
            } else {
                switch_pet_animation(data->pet_image_stand);
                data->current_normal_image = data->pet_image_stand;
            }
        }

        data->pet_state_timer = 0;
    }

    // Move pet if walking
    if (data->pet_is_walking) {
        data->pet_x_pos += data->pet_direction * PET_MOVEMENT_STEP;

        // Bounce off boundaries
        if (data->pet_x_pos > PET_MOVEMENT_LIMIT) {
            data->pet_x_pos = PET_MOVEMENT_LIMIT;
            data->pet_direction = -1;
            switch_pet_animation(data->pet_image_walk_left);
            data->current_normal_image = data->pet_image_walk_left;
        } else if (data->pet_x_pos < -PET_MOVEMENT_LIMIT) {
            data->pet_x_pos = -PET_MOVEMENT_LIMIT;
            data->pet_direction = 1;
            switch_pet_animation(data->pet_image_walk);
            data->current_normal_image = data->pet_image_walk;
        }
    } else {
        // Pet is idle - stays at current position
        // Animation is handled by the blinking logic above
    }

            // Update pet position - move the container that holds the GIF widgets
    lv_obj_t *gif_container = NULL;
    if (data->current_animation_state == AI_PET_STATE_NORMAL) {
        gif_container = lv_obj_get_parent(data->current_normal_image);
    } else {
        gif_container = lv_obj_get_parent(data->pet_special_animation);
    }

    if (gif_container) {
        lv_obj_set_x(gif_container, data->pet_x_pos);
    }
}

static void switch_pet_animation(lv_obj_t *new_animation)
{
    pet_area_data_t *data = &g_pet_area_data;

    // Safety check
    if (new_animation == NULL) {
        return;
    }

    // Hide all normal animations first
    lv_obj_add_flag(data->pet_image_walk, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(data->pet_image_walk_left, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(data->pet_image_blink, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(data->pet_image_stand, LV_OBJ_FLAG_HIDDEN);

    // Show new animation
    lv_obj_clear_flag(new_animation, LV_OBJ_FLAG_HIDDEN);
}

static void switch_to_special_animation(ai_pet_state_t state)
{
    pet_area_data_t *data = &g_pet_area_data;

    // Get the animation source for this state
    const lv_img_dsc_t *anim_src = get_animation_source(state);
    if (anim_src == NULL) {
        return; // Invalid state, stay in current animation
    }

    // Hide all normal animations
    lv_obj_add_flag(data->pet_image_walk, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(data->pet_image_walk_left, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(data->pet_image_blink, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(data->pet_image_stand, LV_OBJ_FLAG_HIDDEN);

        // Load and show special animation immediately
    if (data->pet_special_animation == NULL) {
        return; // Fail silently if object is invalid
    }

    // Set the special animation source
    lv_gif_set_src(data->pet_special_animation, anim_src);
    lv_refr_now(NULL);
    // Bring to front and show (no forced refresh to avoid slowdown)
    lv_obj_move_foreground(data->pet_special_animation);
    lv_obj_clear_flag(data->pet_special_animation, LV_OBJ_FLAG_HIDDEN);

    // Update state
    data->current_animation_state = state;
}

static void switch_to_normal_animation(void)
{
    pet_area_data_t *data = &g_pet_area_data;

    // Hide special animation
    lv_obj_add_flag(data->pet_special_animation, LV_OBJ_FLAG_HIDDEN);

    // Show the appropriate normal animation based on current state
    lv_obj_clear_flag(data->current_normal_image, LV_OBJ_FLAG_HIDDEN);

    // Update state
    data->current_animation_state = AI_PET_STATE_NORMAL;
}

static const lv_img_dsc_t* get_animation_source(ai_pet_state_t state)
{
    switch (state) {
        case AI_PET_STATE_SLEEP:
            return &ducky_sleep;
        case AI_PET_STATE_DANCE:
            return &ducky_dance;
        case AI_PET_STATE_EAT:
            return &ducky_eat;
        case AI_PET_STATE_BATH:
            return &ducky_bath;
        case AI_PET_STATE_TOILET:
            return &ducky_toilet;
        case AI_PET_STATE_SICK:
            return &ducky_sick;
        case AI_PET_STATE_HAPPY:
            return &ducky_emotion_happy;
        case AI_PET_STATE_ANGRY:
            return &ducky_emotion_angry;
        case AI_PET_STATE_CRY:
            return &ducky_emotion_cry;
        default:
            return NULL; // Invalid or normal state
    }
}

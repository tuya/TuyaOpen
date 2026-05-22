/**
 * @file otto_robot_main.c
 * @brief Otto robot main control module for Tuya IoT projects
 *
 * This file implements the main control logic for the Otto humanoid robot, including movement control,
 * data point processing, cloud communication, and thread management. It provides comprehensive robot
 * control functionality including walking, dancing, gesture recognition, and audio mode management.
 * The module handles various robot actions such as forward/backward movement, turning, swinging,
 * jumping, and complex dance sequences through cloud-based commands and local control interfaces.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 *
 */

#include "tuya_cloud_types.h"
#include "tuya_kconfig.h"
#include "tal_api.h"
#include "tkl_output.h"
#include "tkl_pwm.h"
#include "oscillator.h"
#include "otto_movements.h"
#include "otto_motion_ctrl.h"
#include "otto_robot_dp_profile.h"
#include "app_chat_bot.h"
#include "tuya_iot_dp.h"
#include "tuya_iot.h"
#include <string.h>
#include <stdint.h>
#include <stdbool.h>



/***********************************************************
*************************micro define***********************
***********************************************************/

// Forward declarations
void otto_robot_dp_proc_thread(uint32_t move_type);
void otto_init_all_trims(void);
void otto_set_trims_from_kv(void);
#if !defined(OTTO_NO_SERVO_TRIM) || (OTTO_NO_SERVO_TRIM != 1)
void otto_trim_calibration_dp_proc(dp_obj_t *dp);
#endif

// Otto motion speed definitions
#define SPEED_SLOW       1500
#define SPEED_NORMAL     1200
#define SPEED_FAST       800

// Audio mode definitions (matching app_chat_bot.c)
#define AUDIO_MODE_KEY_PRESS_HOLD_SINGLE    0  // Press and hold button to start a single conversation
#define AUDIO_MODE_KEY_TRIG_VAD_FREE        1  // Press the button once to start or stop the free conversation
#define AUDIO_MODE_ASR_WAKEUP_SINGLE        2  // Say the wake-up word to start a single conversation
#define AUDIO_MODE_ASR_WAKEUP_FREE          3  // Say the wake-up word for free conversation

#if !defined(OTTO_NO_SERVO_TRIM) || (OTTO_NO_SERVO_TRIM != 1)
// KV storage keys for Otto robot calibration
#define OTTO_LEFT_LEG_TRIM_KEY "otto_left_leg_trim"
#define OTTO_RIGHT_LEG_TRIM_KEY "otto_right_leg_trim"
#define OTTO_LEFT_FOOT_TRIM_KEY "otto_left_foot_trim"
#define OTTO_RIGHT_FOOT_TRIM_KEY "otto_right_foot_trim"
#define OTTO_LEFT_HAND_TRIM_KEY "otto_left_hand_trim"
#define OTTO_RIGHT_HAND_TRIM_KEY "otto_right_hand_trim"
#define OTTO_TRIM_INIT_FLAG_KEY "otto_trim_init_flag"
#endif

// Otto robot servo pin definitions based on board type
#ifdef OTTO_BOARD_DEFAULT_TXP
// board default txp Otto robot servo pin define
#define PIN_LEFT_LEG   TUYA_PWM_NUM_0
#define PIN_RIGHT_LEG  TUYA_PWM_NUM_1
#define PIN_LEFT_FOOT  TUYA_PWM_NUM_2
#define PIN_RIGHT_FOOT TUYA_PWM_NUM_3
#define PIN_LEFT_HAND  TUYA_PWM_NUM_4
#define PIN_RIGHT_HAND TUYA_PWM_NUM_7

#elif defined(OTTO_BOARD_DREAM)
// board dream Otto robot servo pin define
#define OTTO_GPIO_NUM_18   TUYA_PWM_NUM_0
#define OTTO_GPIO_NUM_24   TUYA_PWM_NUM_1
#define OTTO_GPIO_NUM_32   TUYA_PWM_NUM_2
#define OTTO_GPIO_NUM_34   TUYA_PWM_NUM_3
#define OTTO_GPIO_NUM_36   TUYA_PWM_NUM_4
#define OTTO_GPIO_NUM_9    TUYA_PWM_NUM_7

#define PIN_LEFT_LEG   OTTO_GPIO_NUM_18
#define PIN_RIGHT_LEG  OTTO_GPIO_NUM_9
#define PIN_LEFT_FOOT  OTTO_GPIO_NUM_34
#define PIN_RIGHT_FOOT OTTO_GPIO_NUM_24
#define PIN_LEFT_HAND  OTTO_GPIO_NUM_32
#define PIN_RIGHT_HAND OTTO_GPIO_NUM_36

#else
#error "Please select a board type in Kconfig"
#endif

#define TASK_PWM_PRIORITY THREAD_PRIO_2
#define TASK_PWM_SIZE     4096

/***********************************************************
***********************typedef define***********************
***********************************************************/

/***********************************************************
***********************variable define**********************
***********************************************************/

// External reference to global Otto robot structure
extern Otto_t g_otto;

// Otto motion steps
static uint32_t OTTO_STEP = 2; // otto steps, default is 2

// Otto speed
static uint32_t OTTO_SPEED = SPEED_NORMAL;

// Thread running flag
static bool is_otto_robot_dp_proc_running = false;

// Thread handle
static THREAD_HANDLE robot_app_thread = NULL;

#define OTTO_SHOW_STEPS           2
#define OTTO_SHOW_GAP_MS          150

/***********************************************************
***********************function define**********************
***********************************************************/

/**
 * @brief Set Otto steps
 * @param steps Number of steps
 */
void set_otto_steps(uint32_t steps)
{ 
    OTTO_STEP = steps;
    PR_DEBUG("set otto steps:%d", OTTO_STEP);
}

/**
 * @brief Get Otto steps
 * @return Current number of steps
 */
uint32_t get_otto_steps(void)
{
    return OTTO_STEP;
}

/**
 * @brief Report Otto steps to cloud
 * @param steps Number of steps
 * @return Operation result
 */
static OPERATE_RET _report_otto_step(uint32_t steps)
{
    PR_DEBUG("report otto step dp to cloud");
    tuya_iot_client_t *client = tuya_iot_client_get();
    dp_obj_t dp_obj = {
        .id = DPID_OTTO_STEP,
        .type = PROP_VALUE,
        .value.dp_value = steps,
    };
    return tuya_iot_dp_obj_report(client, client->activate.devid, &dp_obj, 1, 0);
}

/**
 * @brief Process Otto step DP
 * @param steps Number of steps
 */
void otto_robot_step_dp_proc(uint32_t steps)
{
    _report_otto_step(steps);
    set_otto_steps(steps);
    PR_DEBUG("otto_robot_step_dp_proc:%d", steps);
}

/**
 * @brief Set Otto speed
 * @param speed Speed value
 */
void set_otto_speed(uint32_t speed)
{
    OTTO_SPEED = speed;
    PR_DEBUG("set otto speed:%d", OTTO_SPEED);
}

/**
 * @brief Get Otto speed
 * @return Current speed
 */
uint32_t get_otto_speed(void)
{
    return OTTO_SPEED;
}

/**
 * @brief Report Otto speed to cloud
 * @param speed_type Speed type
 * @return Operation result
 */
static OPERATE_RET _report_otto_speed(uint32_t speed_type)
{
    PR_DEBUG("report otto speed dp to cloud");
    tuya_iot_client_t *client = tuya_iot_client_get();
    dp_obj_t dp_obj = {
        .id = DPID_OTTO_SPEED,
        .type = PROP_ENUM,
        .value.dp_enum = speed_type,
    };
    return tuya_iot_dp_obj_report(client, client->activate.devid, &dp_obj, 1, 0);
}

/**
 * @brief Process Otto speed DP
 * @param speed_type Speed type (0:slow ,1:normal, 2:fast)
 */
void otto_robot_speed_dp_proc(uint32_t speed_type)
{
    _report_otto_speed(speed_type);
    switch (speed_type)
    {
        case 0:
            OTTO_SPEED = SPEED_SLOW;
            break;
        case 1:
            OTTO_SPEED = SPEED_NORMAL;
            break;
        case 2:
            OTTO_SPEED = SPEED_FAST;
            break;
        default:
            break;
    }
    set_otto_speed(OTTO_SPEED);
    PR_DEBUG("otto_robot_speed_dp_proc");
}

/**
 * @brief Report Otto audio mode to cloud
 * @param mode Audio mode
 * @return Operation result
 */
static OPERATE_RET _report_otto_audio_mode(uint32_t mode)
{
    PR_DEBUG("report otto audio mode dp to cloud");
    tuya_iot_client_t *client = tuya_iot_client_get();
    dp_obj_t dp_obj = {
        .id = DPID_OTTO_AUDIO,
        .type = PROP_ENUM,
        .value.dp_enum = mode,
    };
    return tuya_iot_dp_obj_report(client, client->activate.devid, &dp_obj, 1, 0);
}

/**
 * @brief Process Otto audio mode DP
 * @param mode Audio mode (0:Key Press Hold Single, 1:Key Trigger VAD Free, 2:ASR Wakeup Single, 3:ASR Wakeup Free)
 */
void otto_robot_audio_mode_dp_proc(uint32_t mode)
{ 
    _report_otto_audio_mode(mode);
    
    // Switch chat mode based on mode parameter
    switch (mode) {
        case AUDIO_MODE_KEY_PRESS_HOLD_SINGLE:
            PR_DEBUG("Switching to Key Press Hold Single mode");
            // This mode is handled by Kconfig ENABLE_CHAT_MODE_KEY_PRESS_HOLD_SINGEL
            // The mode is set at compile time, so we just log the request
            break;
            
        case AUDIO_MODE_KEY_TRIG_VAD_FREE:
            PR_DEBUG("Switching to Key Trigger VAD Free mode");
            // This mode is handled by Kconfig ENABLE_CHAT_MODE_KEY_TRIG_VAD_FREE
            // The mode is set at compile time, so we just log the request
            break;
            
        case AUDIO_MODE_ASR_WAKEUP_SINGLE:
            PR_DEBUG("Switching to ASR Wakeup Single mode");
            // This mode is handled by Kconfig ENABLE_CHAT_MODE_ASR_WAKEUP_SINGEL
            // The mode is set at compile time, so we just log the request
            break;
            
        case AUDIO_MODE_ASR_WAKEUP_FREE:
            PR_DEBUG("Switching to ASR Wakeup Free mode");
            // This mode is handled by Kconfig ENABLE_CHAT_MODE_ASR_WAKEUP_FREE
            // The mode is set at compile time, so we just log the request
            break;
            
        default:
            PR_DEBUG("Unknown audio mode: %d", mode);
            break;
    }
    
    PR_DEBUG("otto_robot_audio_mode_dp_proc completed for mode:%d", mode);
}

bool is_otto_power_on = false;

/**
 * @brief Initialize hand servos only, without affecting leg servos
 */
void otto_init_hands_only_wrapper()
{
    PR_DEBUG("Initializing hands only...");
    
    // Call otto_init_hands_only function from otto_movements.c
    otto_init_hands_only(PIN_LEFT_HAND, PIN_RIGHT_HAND);
    
    PR_DEBUG("Hands initialized successfully.");
}

void otto_power_on()
{
    if (is_otto_power_on) {
        PR_DEBUG("Otto already power on");
        return;
    }
    is_otto_power_on = true;
    PR_DEBUG("Otto initializing...");

#if !defined(OTTO_NO_SERVO_TRIM) || (OTTO_NO_SERVO_TRIM != 1)
    otto_init_all_trims();
#endif

    PR_DEBUG("Initializing legs and feet...");
    otto_init(PIN_LEFT_LEG, PIN_RIGHT_LEG, PIN_LEFT_FOOT, PIN_RIGHT_FOOT, -1, -1);
    otto_enable_servo_limit(SERVO_LIMIT_DEFAULT);
    otto_home(false);
    tal_system_sleep(200);

#if !defined(OTTO_NO_ARMS) || (OTTO_NO_ARMS != 1)
    PR_DEBUG("Initializing hands...");
    otto_init_hands_only_wrapper();
#if !defined(OTTO_NO_SERVO_TRIM) || (OTTO_NO_SERVO_TRIM != 1)
    otto_set_trims_from_kv();
#endif
    otto_home(true);
    PR_DEBUG("Otto initialized completely (with arms).");
#else
    PR_DEBUG("Otto initialized completely (4 servos, no arms).");
#endif
}
/**
 * @brief otto_Show
 *
 * @return none
 */
/**
 * @brief Run one show segment if not aborted
 * @param[in] fn Segment runner (calls one motion primitive)
 * @return true if completed, false if aborted
 */
typedef void (*otto_show_step_fn_t)(void);

static bool __otto_show_step(otto_show_step_fn_t fn)
{
    if (otto_motion_should_abort()) {
        return false;
    }
    fn();
    if (otto_motion_should_abort()) {
        return false;
    }
    otto_motion_sleep_ms(OTTO_SHOW_GAP_MS);
    return !otto_motion_should_abort();
}

static void __show_walk_f(void)
{
    otto_walk(OTTO_SHOW_STEPS, OTTO_SPEED, FORWARD, 20);
}

static void __show_turn_l(void)
{
    otto_turn(OTTO_SHOW_STEPS, OTTO_SPEED, LEFT, 25);
}

static void __show_swing(void)
{
    otto_swing(OTTO_SHOW_STEPS, OTTO_SPEED, 20);
}

static void __show_up_down(void)
{
    otto_up_down(OTTO_SHOW_STEPS, OTTO_SPEED, 20);
}

static void __show_bend(void)
{
    otto_bend(1, OTTO_SPEED, LEFT);
}

static void __show_jitter(void)
{
    otto_jitter(OTTO_SHOW_STEPS, OTTO_SPEED, 20);
}

static void __show_moonwalker(void)
{
    otto_moonwalker(OTTO_SHOW_STEPS, OTTO_SPEED, 20, LEFT);
}

static void __show_jump(void)
{
    otto_jump(1, OTTO_SPEED);
}

#if !defined(OTTO_NO_ARMS) || (OTTO_NO_ARMS != 1)
static void __show_hand_wave(void)
{
    otto_hand_wave(OTTO_SPEED, 0);
}
#endif

/**
 * @brief Compact show sequence; abortable between segments
 * @return none
 */
static void otto_Show(void)
{
    PR_DEBUG("otto_Show start");

    if (!__otto_show_step(__show_walk_f)) {
        return;
    }
    if (!__otto_show_step(__show_turn_l)) {
        return;
    }
    if (!__otto_show_step(__show_swing)) {
        return;
    }
    if (!__otto_show_step(__show_up_down)) {
        return;
    }
    if (!__otto_show_step(__show_bend)) {
        return;
    }
    if (!__otto_show_step(__show_jitter)) {
        return;
    }
    if (!__otto_show_step(__show_moonwalker)) {
        return;
    }
    if (!__otto_show_step(__show_jump)) {
        return;
    }
#if !defined(OTTO_NO_ARMS) || (OTTO_NO_ARMS != 1)
    if (!__otto_show_step(__show_hand_wave)) {
        return;
    }
#endif
    PR_DEBUG("otto_Show complete");
}

enum ActionType {
    ACTION_WALK_F = 0,
    ACTION_WALK_B,
    ACTION_WALK_L,
    ACTION_WALK_R,
    ACTION_NONE,
    ACTION_SWING = 5,
    ACTION_UP_DOWN = 6,
    ACTION_BEND = 7,
    ACTION_JITTER = 8,
    ACTION_MOONWALKER = 9,
    ACTION_JUMP = 10,
    ACTION_SHOW = 11,
    ACTION_HAND_WAVE = 12,
};

/**
 * @brief Thread parameter structure for passing action type
 */
typedef struct {
    uint32_t move_type; ///< Action type, refer to enum ActionType
} OttoMotionThreadParams;

/**
 * @brief Execute a specific Otto robot action
 * 
 * @param move_type The action type to execute
 */
void otto_robot_execute_action(uint32_t move_type)
{
    PR_DEBUG("Executing action: %d", move_type);
    
    switch (move_type) {
    case ACTION_WALK_F:
        PR_DEBUG("Walking forward");
        otto_walk(OTTO_STEP, OTTO_SPEED, FORWARD, 20);
        break;

    case ACTION_WALK_B:
        PR_DEBUG("Walking backward");
        otto_walk(OTTO_STEP, OTTO_SPEED, BACKWARD, 15);
        break;

    case ACTION_WALK_L:
        PR_DEBUG("Walking left");
        otto_turn(OTTO_STEP, OTTO_SPEED, LEFT, 25);
        break;

    case ACTION_WALK_R:
        PR_DEBUG("Walking right");
        otto_turn(OTTO_STEP, OTTO_SPEED, RIGHT, 25);
        break;

    case ACTION_SWING:
        PR_DEBUG("Swinging");
        otto_swing(OTTO_STEP, OTTO_SPEED, 20);
        break;

    case ACTION_UP_DOWN:
        PR_DEBUG("Moving up and down");
        otto_up_down(OTTO_STEP, OTTO_SPEED, 20);
        break;

    case ACTION_BEND:
        PR_DEBUG("Bending");
        otto_bend(OTTO_STEP, OTTO_SPEED, LEFT);
        break;

    case ACTION_JITTER:
        PR_DEBUG("Jittering");
        otto_jitter(OTTO_STEP, OTTO_SPEED, 20);
        break;

    case ACTION_MOONWALKER:
        PR_DEBUG("Performing moonwalker");
        otto_moonwalker(OTTO_STEP, OTTO_SPEED, 20, LEFT);
        break;

    case ACTION_JUMP:
        PR_DEBUG("Jumping");
        otto_jump(OTTO_STEP, OTTO_SPEED);
        break;

    case ACTION_SHOW:
        PR_DEBUG("Performing Show");
        otto_Show();
        break;
        
    case ACTION_HAND_WAVE:
        PR_DEBUG("otto hand wave");
        otto_hand_wave(OTTO_SPEED, 0);
        break;

    case ACTION_NONE:    
        PR_DEBUG("Returning to home position");
        //otto_set_trims(0, 0, 0, 0, 0, 0);
        otto_home(true);
        break;

    default:
        PR_DEBUG("Invalid action type: %d", move_type);
        //otto_set_trims(0, 0, 0, 0, 0, 0);
        otto_home(true);
        break;
    }
    
#if !defined(OTTO_NO_SERVO_TRIM) || (OTTO_NO_SERVO_TRIM != 1)
    otto_set_trims_from_kv();
#endif
#if defined(OTTO_NO_ARMS) && (OTTO_NO_ARMS == 1)
    otto_home(false);
#else
    otto_home(true);
#endif
    PR_DEBUG("Action completed, returned to home position");
}

#if defined(OTTO_PRODUCT_ST7789_V1) && (OTTO_PRODUCT_ST7789_V1 == 1)

/**
 * @brief Map V1 speed enum index to legacy speed type (0:slow, 1:normal, 2:fast)
 * @param[in] speed_enum V1 cloud enum index
 * @return Legacy speed type for otto_robot_speed_dp_proc
 */
static uint32_t __otto_v1_speed_enum_to_legacy(uint32_t speed_enum)
{
    switch (speed_enum) {
    case 0:
        return 1;
    case 1:
        return 0;
    case 2:
        return 2;
    default:
        return 1;
    }
}

/**
 * @brief Map V1 speed enum string to legacy speed type
 * @param[in] speed_str V1 cloud enum string
 * @return Legacy speed type for otto_robot_speed_dp_proc
 */
static uint32_t __otto_v1_speed_str_to_legacy(const char *speed_str)
{
    if (speed_str == NULL) {
        return 1;
    }
    if (strcmp(speed_str, "SPEED_SLOW") == 0) {
        return 0;
    }
    if (strcmp(speed_str, "SPEED_FAST") == 0) {
        return 2;
    }
    return 1;
}

/**
 * @brief Map V1 ptz_control string to internal action type
 * @param[in] ptz_str Cloud enum string
 * @param[out] action_out Internal ActionType value
 * @return true if mapped, false if unknown
 */
static bool __otto_v1_ptz_str_to_action(const char *ptz_str, uint32_t *action_out)
{
    if (ptz_str == NULL || action_out == NULL) {
        return false;
    }
    if (strcmp(ptz_str, "front") == 0) {
        *action_out = ACTION_WALK_F;
    } else if (strcmp(ptz_str, "back") == 0) {
        *action_out = ACTION_WALK_B;
    } else if (strcmp(ptz_str, "left") == 0) {
        *action_out = ACTION_WALK_L;
    } else if (strcmp(ptz_str, "right") == 0) {
        *action_out = ACTION_WALK_R;
    } else if (strcmp(ptz_str, "none") == 0) {
        *action_out = ACTION_NONE;
    } else if (strcmp(ptz_str, "swing") == 0) {
        *action_out = ACTION_SWING;
    } else if (strcmp(ptz_str, "up_down") == 0) {
        *action_out = ACTION_UP_DOWN;
    } else if (strcmp(ptz_str, "bend") == 0) {
        *action_out = ACTION_BEND;
    } else if (strcmp(ptz_str, "jitter") == 0) {
        *action_out = ACTION_JITTER;
    } else if (strcmp(ptz_str, "moonwalker") == 0) {
        *action_out = ACTION_MOONWALKER;
    } else if (strcmp(ptz_str, "jump") == 0) {
        *action_out = ACTION_JUMP;
    } else if (strcmp(ptz_str, "show") == 0) {
        *action_out = ACTION_SHOW;
    } else if (strcmp(ptz_str, "hand") == 0) {
        *action_out = ACTION_HAND_WAVE;
    } else {
        return false;
    }
    return true;
}

/**
 * @brief Process V1 ptz_control DP (DP5)
 * @param[in] dp Data point object
 * @return none
 */
static void __otto_v1_ptz_dp_proc(dp_obj_t *dp)
{
    uint32_t action = ACTION_NONE;

    if (dp->type == PROP_ENUM) {
        action = dp->value.dp_enum;
        if (action > ACTION_HAND_WAVE) {
            PR_DEBUG("Unknown ptz_control enum: %d", action);
            return;
        }
        PR_DEBUG("ptz_control enum index: %d", action);
        otto_robot_dp_proc_thread(action);
        return;
    }
    if (dp->type == PROP_STR) {
        if (!__otto_v1_ptz_str_to_action(dp->value.dp_str, &action)) {
            PR_DEBUG("Unknown ptz_control string: %s", dp->value.dp_str);
            return;
        }
        PR_DEBUG("ptz_control string: %s -> action %d", dp->value.dp_str, action);
        otto_robot_dp_proc_thread(action);
        return;
    }
    PR_DEBUG("Unsupported ptz_control DP type: %d", dp->type);
}

/**
 * @brief Process data points for ST7789 V1 product profile
 * @param[in] dpobj Received DP object bundle
 * @return none
 */
static void __otto_robot_dp_proc_v1(dp_obj_recv_t *dpobj)
{
    for (uint32_t i = 0; i < dpobj->dpscnt; i++) {
        dp_obj_t *dp = dpobj->dps + i;
        PR_DEBUG("V1 DP[%d]: id=%d type=%d", i, dp->id, dp->type);
        switch (dp->id) {
        case DPID_OTTO_PTZ_CONTROL:
            __otto_v1_ptz_dp_proc(dp);
            break;
        case DPID_OTTO_SPEED:
            if (dp->type == PROP_ENUM) {
                otto_robot_speed_dp_proc(__otto_v1_speed_enum_to_legacy(dp->value.dp_enum));
            } else if (dp->type == PROP_STR) {
                otto_robot_speed_dp_proc(__otto_v1_speed_str_to_legacy(dp->value.dp_str));
            }
            break;
        case DPID_OTTO_STEP:
            if (dp->type == PROP_VALUE) {
                uint32_t steps = dp->value.dp_value;
                if (steps < OTTO_STEP_MIN) {
                    steps = OTTO_STEP_MIN;
                }
                if (steps > OTTO_STEP_MAX) {
                    steps = OTTO_STEP_MAX;
                }
                otto_robot_step_dp_proc(steps);
            }
            break;
        case DPID_OTTO_AUDIO:
            if (dp->type == PROP_ENUM) {
                otto_robot_audio_mode_dp_proc(dp->value.dp_enum);
            } else if (dp->type == PROP_STR) {
                if (strcmp(dp->value.dp_str, "hold") == 0) {
                    otto_robot_audio_mode_dp_proc(0);
                } else if (strcmp(dp->value.dp_str, "key") == 0) {
                    otto_robot_audio_mode_dp_proc(1);
                } else if (strcmp(dp->value.dp_str, "weakup") == 0) {
                    otto_robot_audio_mode_dp_proc(2);
                } else if (strcmp(dp->value.dp_str, "free") == 0) {
                    otto_robot_audio_mode_dp_proc(3);
                } else {
                    PR_DEBUG("Unknown audio mode string: %s", dp->value.dp_str);
                }
            }
            break;
        default:
            PR_DEBUG("Unknown V1 DP ID: %d", dp->id);
            break;
        }
    }
}

#endif /* OTTO_PRODUCT_ST7789_V1 */

/**
 * @brief Process data point objects for Otto robot control
 * 
 * @param dpobj Pointer to the data point object structure
 */
void otto_robot_dp_proc(dp_obj_recv_t *dpobj)
{
    PR_DEBUG("=== Otto Robot DP Processing Started ===");
    
    if (dpobj == NULL || dpobj->dpscnt == 0) {
        PR_DEBUG("Invalid dpobj or no dps to process");
        return;
    }

#if defined(OTTO_PRODUCT_ST7789_V1) && (OTTO_PRODUCT_ST7789_V1 == 1)
    __otto_robot_dp_proc_v1(dpobj);
    PR_DEBUG("=== Otto Robot DP Processing Completed (V1) ===");
    return;
#else

    PR_DEBUG("Processing %d data points", dpobj->dpscnt);
    
    // Process each dp in the dpobj
    for (uint32_t i = 0; i < dpobj->dpscnt; i++) {
        dp_obj_t *dp = dpobj->dps + i;
        PR_DEBUG("Processing dp idx:%d dpid:%d type:%d", i, dp->id, dp->type);
        PR_DEBUG("DP[%d]: ID=%d, Type=%d", i, dp->id, dp->type);

        // Process different DP types
        switch (dp->id) {
            // Otto robot WALK_DIRECTION (DP4)
            case DPID_OTTO_WALK_DIRECTION:
                if (dp->type == PROP_VALUE) {
                    PR_DEBUG("otto Rev DP Obj Cmd dpid:%d type:%d value:%d", dp->id, dp->type, dp->value.dp_value);
                    PR_DEBUG("=== Otto Robot Direction Control ===");
                    PR_DEBUG("Direction value: %d degrees", dp->value.dp_value);
                    switch(dp->value.dp_value){
                        case 0:
                            PR_DEBUG("Executing: Walk Forward (0 degrees)");
                            otto_robot_dp_proc_thread(ACTION_WALK_F);
                            break;
                        case 90:
                            PR_DEBUG("Executing: Walk Right (90 degrees)");
                            otto_robot_dp_proc_thread(ACTION_WALK_R);
                            break;
                        case 180:
                            PR_DEBUG("Executing: Walk Backward (180 degrees)");
                            otto_robot_dp_proc_thread(ACTION_WALK_B);
                            break;
                        case 270:
                            PR_DEBUG("Executing: Walk Left (270 degrees)");
                            otto_robot_dp_proc_thread(ACTION_WALK_L);
                            break;
                        default:
                            PR_DEBUG("Unknown walk direction: %d degrees", dp->value.dp_value);
                            break;
                    }
                    PR_DEBUG("=== Direction Control Complete ===");
                }
                break;

            // Otto robot SPEED (DP5)
            case DPID_OTTO_SPEED:
                if (dp->type == PROP_ENUM) {
                    PR_DEBUG("otto Rev DP Obj Cmd dpid:%d type:%d value:%d", dp->id, dp->type, dp->value.dp_enum);
                    PR_DEBUG("=== Otto Robot Speed Control ===");
                    PR_DEBUG("Speed type: %d", dp->value.dp_enum);
                    switch(dp->value.dp_enum) {
                        case 0:
                            PR_DEBUG("Setting speed: Normal (700ms)");
                            break;
                        case 1:
                            PR_DEBUG("Setting speed: Slow (1000ms)");
                            break;
                        case 2:
                            PR_DEBUG("Setting speed: Fast (400ms)");
                            break;
                        default:
                            PR_DEBUG("Unknown speed type: %d", dp->value.dp_enum);
                            break;
                    }
                    otto_robot_speed_dp_proc(dp->value.dp_enum);
                    PR_DEBUG("=== Speed Control Complete ===");
                }
                break;

            // Otto robot ACTION (DP10)
            case DPID_OTTO_ACTION:
                if (dp->type == PROP_ENUM) {
                    PR_DEBUG("otto Rev DP Obj Cmd dpid:%d type:%d value:%d", dp->id, dp->type, dp->value.dp_enum);
                    PR_DEBUG("=== Otto Robot Action Control ===");
                    PR_DEBUG("Action type: %d", dp->value.dp_enum);
                    switch(dp->value.dp_enum){
                        case 0:
                            PR_DEBUG("Executing: Return to Home Position");
                            otto_robot_dp_proc_thread(ACTION_NONE);
                            break;
                        case 1:
                            PR_DEBUG("Executing: Swing Movement");
                            otto_robot_dp_proc_thread(ACTION_SWING);
                            break;
                        case 2:
                            PR_DEBUG("Executing: Up and Down Movement");
                            otto_robot_dp_proc_thread(ACTION_UP_DOWN);
                            break;
                        case 3:
                            PR_DEBUG("Executing: Bend Movement");
                            otto_robot_dp_proc_thread(ACTION_BEND);
                            break;
                        case 4:
                            PR_DEBUG("Executing: Jitter Movement");
                            otto_robot_dp_proc_thread(ACTION_JITTER);
                            break;
                        case 5:
                            PR_DEBUG("Executing: Moonwalker Movement");
                            otto_robot_dp_proc_thread(ACTION_MOONWALKER);
                            break;
                        case 6:
                            PR_DEBUG("Executing: Jump Movement");
                            otto_robot_dp_proc_thread(ACTION_JUMP);
                            break;
                        case 7:
                            PR_DEBUG("Executing: Show Sequence (Multiple Actions)");
                            otto_robot_dp_proc_thread(ACTION_SHOW);
                            break;
                        case 8:
                            PR_DEBUG("Executing: Hand Wave Movement");
                            otto_robot_dp_proc_thread(ACTION_HAND_WAVE);
                            break;
                        default:
                            PR_DEBUG("Unknown action type: %d", dp->value.dp_enum);
                            break;
                    }
                    PR_DEBUG("=== Action Control Complete ===");
                } else if (dp->type == PROP_STR) {
                    PR_DEBUG("otto Rev DP Obj Cmd dpid:%d type:%d value:%s", dp->id, dp->type, dp->value.dp_str);
                    PR_DEBUG("=== Otto Robot Action Control (String) ===");
                    PR_DEBUG("Action string: %s", dp->value.dp_str);
                    
                    // Handle string-based action commands
                    if (strcmp(dp->value.dp_str, "swing") == 0) {
                        PR_DEBUG("Executing: Swing Movement (from string)");
                        otto_robot_dp_proc_thread(ACTION_SWING);
                    } else if (strcmp(dp->value.dp_str, "up_down") == 0) {
                        PR_DEBUG("Executing: Up and Down Movement (from string)");
                        otto_robot_dp_proc_thread(ACTION_UP_DOWN);
                    } else if (strcmp(dp->value.dp_str, "bend") == 0) {
                        PR_DEBUG("Executing: Bend Movement (from string)");
                        otto_robot_dp_proc_thread(ACTION_BEND);
                    } else if (strcmp(dp->value.dp_str, "jitter") == 0) {
                        PR_DEBUG("Executing: Jitter Movement (from string)");
                        otto_robot_dp_proc_thread(ACTION_JITTER);
                    } else if (strcmp(dp->value.dp_str, "moonwalker") == 0) {
                        PR_DEBUG("Executing: Moonwalker Movement (from string)");
                        otto_robot_dp_proc_thread(ACTION_MOONWALKER);
                    } else if (strcmp(dp->value.dp_str, "jump") == 0) {
                        PR_DEBUG("Executing: Jump Movement (from string)");
                        otto_robot_dp_proc_thread(ACTION_JUMP);
                    } else if (strcmp(dp->value.dp_str, "show") == 0) {
                        PR_DEBUG("Executing: Show Sequence (from string)");
                        otto_robot_dp_proc_thread(ACTION_SHOW);
                    } else if (strcmp(dp->value.dp_str, "hand_wave") == 0) {
                        PR_DEBUG("Executing: Hand Wave Movement (from string)");
                        otto_robot_dp_proc_thread(ACTION_HAND_WAVE);
                    } else if (strcmp(dp->value.dp_str, "home") == 0) {
                        PR_DEBUG("Executing: Return to Home Position (from string)");
                        otto_robot_dp_proc_thread(ACTION_NONE);
                    } else {
                        PR_DEBUG("Unknown action string: %s", dp->value.dp_str);
                    }
                    PR_DEBUG("=== Action Control Complete ===");
                } else {
                    PR_DEBUG("Unsupported DP type: %d for DP ID: %d", dp->type, dp->id);
                }
                break;

            // Otto robot STEP (DP11)
            case DPID_OTTO_STEP:
                if (dp->type == PROP_VALUE) {
                    PR_DEBUG("otto Rev DP Obj Cmd dpid:%d type:%d value:%d", dp->id, dp->type, dp->value.dp_value);
                    PR_DEBUG("=== Otto Robot Step Control ===");
                    PR_DEBUG("Setting steps: %d", dp->value.dp_value);
                    PR_DEBUG("Current speed: %dms", OTTO_SPEED);
                    otto_robot_step_dp_proc(dp->value.dp_value);
                    PR_DEBUG("=== Step Control Complete ===");
                }
                break;

            // Otto robot AUDIO (DP9)
            case DPID_OTTO_AUDIO:
                if (dp->type == PROP_ENUM) {
                    PR_DEBUG("otto Rev DP Obj Cmd dpid:%d type:%d value:%d", dp->id, dp->type, dp->value.dp_enum);
                    PR_DEBUG("=== Otto Robot Audio Mode Control ===");
                    PR_DEBUG("Audio mode: %d", dp->value.dp_enum);
                    switch(dp->value.dp_enum) {
                        case 0:
                            PR_DEBUG("Setting mode: Key Press Hold Single");
                            break;
                        case 1:
                            PR_DEBUG("Setting mode: Key Trigger VAD Free");
                            break;
                        case 2:
                            PR_DEBUG("Setting mode: ASR Wakeup Single");
                            break;
                        case 3:
                            PR_DEBUG("Setting mode: ASR Wakeup Free");
                            break;
                        default:
                            PR_DEBUG("Unknown audio mode: %d", dp->value.dp_enum);
                            break;
                    }
                    otto_robot_audio_mode_dp_proc(dp->value.dp_enum);
                    PR_DEBUG("=== Audio Mode Control Complete ===");
                }
                break;

#if !defined(OTTO_NO_SERVO_TRIM) || (OTTO_NO_SERVO_TRIM != 1)
            case DPID_OTTO_LEFT_LEG_TRIM:
            case DPID_OTTO_RIGHT_LEG_TRIM:
            case DPID_OTTO_LEFT_FOOT_TRIM:
            case DPID_OTTO_RIGHT_FOOT_TRIM:
            case DPID_OTTO_LEFT_HAND_TRIM:
            case DPID_OTTO_RIGHT_HAND_TRIM:
                PR_DEBUG("otto Rev DP Obj Cmd dpid:%d type:%d value:%d", dp->id, dp->type, dp->value.dp_value);
                PR_DEBUG("=== Otto Robot Trim Calibration ===");
                otto_trim_calibration_dp_proc(dp);
                otto_home(true);
                PR_DEBUG("=== Trim Calibration Complete ===");
                break;
#endif

            default:
                PR_DEBUG("Unknown DP ID: %d", dp->id);
                break;
        }
    }

    PR_DEBUG("=== Otto Robot DP Processing Completed ===");
#endif /* !OTTO_PRODUCT_ST7789_V1 */
}

/**
 * @brief Otto robot motion control thread processing function
 *
 * Execute corresponding actions based on the passed action type, such as forward, backward, left/right turn, etc.
 * Automatically call otto_home() to return to initial position after execution.
 *
 * @param arg Thread parameter pointer (OttoMotionThreadParams*)
 */
static void __otto_motion_thread_process(void *arg)
{
    PR_DEBUG("=== Otto Motion Thread Started ===");
    
    // Get thread parameters
    OttoMotionThreadParams *params = (OttoMotionThreadParams *)arg;
    uint32_t move_type = params->move_type;

    otto_motion_set_current(move_type);
    PR_DEBUG("Thread received motion type: %d", move_type);
    PR_DEBUG("Current Otto settings - Steps: %d, Speed: %dms", OTTO_STEP, OTTO_SPEED);

    // Execute corresponding actions based on action type
    switch(move_type){
        case ACTION_WALK_F:
            PR_DEBUG("Walking forward");
            otto_walk(OTTO_STEP, OTTO_SPEED, FORWARD, 20);
            break;

        case ACTION_WALK_B:
            PR_DEBUG("Walking backward");
            otto_walk(OTTO_STEP, OTTO_SPEED, BACKWARD, 15);
            break;

        case ACTION_WALK_L:
            PR_DEBUG("Walking left");
            otto_turn(OTTO_STEP, OTTO_SPEED, LEFT, 25);
            break;

        case ACTION_WALK_R:
            PR_DEBUG("Walking right");
            otto_turn(OTTO_STEP, OTTO_SPEED, RIGHT, 25);
            break;

        case ACTION_SWING:
            PR_DEBUG("Swinging");
            otto_swing(OTTO_STEP, OTTO_SPEED, 20);
            break;

        case ACTION_UP_DOWN:
            PR_DEBUG("Moving up and down");
            otto_up_down(OTTO_STEP, OTTO_SPEED, 20);
            break;

        case ACTION_BEND:
            PR_DEBUG("Bending");
            otto_bend(OTTO_STEP, OTTO_SPEED, LEFT);
            break;

        case ACTION_JITTER:
            PR_DEBUG("Jittering");
            otto_jitter(OTTO_STEP, OTTO_SPEED, 20);
            break;

        case ACTION_MOONWALKER:
            PR_DEBUG("Performing moonwalker");
            otto_moonwalker(OTTO_STEP, OTTO_SPEED, 20, LEFT);
            break;

        case ACTION_JUMP:
            PR_DEBUG("Jumping");
            otto_jump(OTTO_STEP, OTTO_SPEED);
            break;

        case ACTION_SHOW:
            PR_DEBUG("Performing Show");
            otto_Show();
            break;

        case ACTION_HAND_WAVE:
            PR_DEBUG("otto hand wave");
            otto_hand_wave(OTTO_SPEED, 0);
            break;

        case ACTION_NONE:
            PR_DEBUG("otto_home");
#if !defined(OTTO_NO_SERVO_TRIM) || (OTTO_NO_SERVO_TRIM != 1)
            otto_set_trims_from_kv();
#endif
#if defined(OTTO_NO_ARMS) && (OTTO_NO_ARMS == 1)
            otto_home(false);
#else
            otto_home(true);
#endif
            break;

        default:
            PR_DEBUG("Unknown action type: %d", move_type);
            break;
    }

    {
        bool aborted = otto_motion_should_abort();
        uint32_t pending_type = 0;
        uint32_t queued_type = 0;
        bool has_pending = otto_motion_take_pending(&pending_type);
        bool has_queued = otto_motion_dequeue(&queued_type);

        if (!aborted || !has_pending) {
            PR_DEBUG("Returning to home position...");
#if defined(OTTO_NO_ARMS) && (OTTO_NO_ARMS == 1)
            otto_home(false);
#else
            otto_home(true);
#endif
        } else {
            PR_DEBUG("Show preempted, skip home, next action %d", pending_type);
        }

        tal_free(params);
        is_otto_robot_dp_proc_running = false;
        tal_thread_delete(robot_app_thread);
        robot_app_thread = NULL;

        if (has_pending) {
            otto_robot_dp_proc_thread(pending_type);
        } else if (has_queued) {
            otto_robot_dp_proc_thread(queued_type);
        }
    }
    PR_DEBUG("=== Otto Motion Thread Completed ===");
}

/**
 * @brief Otto robot DP command processing function (thread version)
 *
 * Originally executed actions directly, now changed to execute actions asynchronously in a new thread to avoid blocking the main thread.
 *
 * @param move_type Action type (ACTION_XXX)
 */
void otto_robot_dp_proc_thread(uint32_t move_type)
{
    PR_DEBUG("=== Starting Otto Robot Motion Thread ===");
    PR_DEBUG("Motion type: %d", move_type);
    
    if (is_otto_robot_dp_proc_running) {
        otto_motion_on_busy(move_type);
        return;
    }

    otto_motion_begin();
    is_otto_robot_dp_proc_running = true;
    PR_DEBUG("Motion thread flag set, creating new thread...");

    // Allocate thread parameter memory
    OttoMotionThreadParams *params = (OttoMotionThreadParams *)tal_malloc(sizeof(OttoMotionThreadParams));
    if (!params) {
        PR_DEBUG("Failed to allocate memory for thread parameters");
        is_otto_robot_dp_proc_running = false;
        return;
    }
    params->move_type = move_type;
    PR_DEBUG("Thread parameters allocated successfully");

    // Configure thread parameters
    THREAD_CFG_T thrd_param = {
        .thrdname = "OttoMotionThread",
        .priority = THREAD_PRIO_2,
        .stackDepth = 4096,
    };
    PR_DEBUG("Thread configuration: name=%s, priority=%d, stack=%d", 
            thrd_param.thrdname, thrd_param.priority, thrd_param.stackDepth);

    // Create and start thread
    OPERATE_RET rt = tal_thread_create_and_start(&robot_app_thread, NULL, NULL, __otto_motion_thread_process, (void *)params, &thrd_param);
    if (rt != OPRT_OK) {
        PR_DEBUG("Failed to create motion thread, error: %d", rt);
        tal_free(params);
        is_otto_robot_dp_proc_running = false;
    } else {
        PR_DEBUG("Motion thread created and started successfully");
    }
}

#if !defined(OTTO_NO_SERVO_TRIM) || (OTTO_NO_SERVO_TRIM != 1)

/**
 * @brief Sets the left leg trim value and saves it to KV storage.
 * @param trim_value The trim value to set for the left leg.
 * @return OPERATE_RET - OPRT_OK if the trim value is set successfully, otherwise an error code.
 */
OPERATE_RET otto_set_left_leg_trim(int trim_value)
{
    OPERATE_RET rt = OPRT_OK;

    // Save to KV storage
    TUYA_CALL_ERR_LOG(tal_kv_set(OTTO_LEFT_LEG_TRIM_KEY, (const uint8_t *)&trim_value, sizeof(trim_value)));

    // Update the global trim value
    g_otto.servo_trim[LEFT_LEG] = trim_value;

    // Apply trim to oscillator if it exists
    if (g_otto.oscillator_indices[LEFT_LEG] != -1) {
        oscillator_set_trim(g_otto.oscillator_indices[LEFT_LEG], trim_value);
    }

    PR_DEBUG("set left leg trim: %d", trim_value);

    return rt;
}

/**
 * @brief Retrieves the current left leg trim value from KV storage.
 * @param None
 * @return int - The current left leg trim value.
 */
int otto_get_left_leg_trim(void)
{
    OPERATE_RET rt = OPRT_OK;

    int trim_value = 0;
    uint8_t *value = NULL;
    size_t read_len = 0;

    // Read from KV storage
    TUYA_CALL_ERR_LOG(tal_kv_get(OTTO_LEFT_LEG_TRIM_KEY, &value, &read_len));
    if (OPRT_OK != rt || NULL == value) {
        PR_ERR("read left leg trim failed");
        trim_value = 0;  // Default trim value
    } else {
        trim_value = *(int *)value;
    }

    PR_DEBUG("get left leg trim: %d", trim_value);

    if (value) {
        tal_kv_free(value);
        value = NULL;
    }

    return trim_value;
}

/**
 * @brief Sets the right leg trim value and saves it to KV storage.
 * @param trim_value The trim value to set for the right leg.
 * @return OPERATE_RET - OPRT_OK if the trim value is set successfully, otherwise an error code.
 */
OPERATE_RET otto_set_right_leg_trim(int trim_value)
{
    OPERATE_RET rt = OPRT_OK;

    // Save to KV storage
    TUYA_CALL_ERR_LOG(tal_kv_set(OTTO_RIGHT_LEG_TRIM_KEY, (const uint8_t *)&trim_value, sizeof(trim_value)));

    // Update the global trim value
    g_otto.servo_trim[RIGHT_LEG] = trim_value;

    // Apply trim to oscillator if it exists
    if (g_otto.oscillator_indices[RIGHT_LEG] != -1) {
        oscillator_set_trim(g_otto.oscillator_indices[RIGHT_LEG], trim_value);
    }

    PR_DEBUG("set right leg trim: %d", trim_value);

    return rt;
}

/**
 * @brief Retrieves the current right leg trim value from KV storage.
 * @param None
 * @return int - The current right leg trim value.
 */
int otto_get_right_leg_trim(void)
{
    OPERATE_RET rt = OPRT_OK;

    int trim_value = 0;
    uint8_t *value = NULL;
    size_t read_len = 0;

    // Read from KV storage
    TUYA_CALL_ERR_LOG(tal_kv_get(OTTO_RIGHT_LEG_TRIM_KEY, &value, &read_len));
    if (OPRT_OK != rt || NULL == value) {
        PR_ERR("read right leg trim failed");
        trim_value = 0;  // Default trim value
    } else {
        trim_value = *(int *)value;
    }

    PR_DEBUG("get right leg trim: %d", trim_value);

    if (value) {
        tal_kv_free(value);
        value = NULL;
    }

    return trim_value;
}

/**
 * @brief Sets the left foot trim value and saves it to KV storage.
 * @param trim_value The trim value to set for the left foot.
 * @return OPERATE_RET - OPRT_OK if the trim value is set successfully, otherwise an error code.
 */
OPERATE_RET otto_set_left_foot_trim(int trim_value)
{
    OPERATE_RET rt = OPRT_OK;

    // Save to KV storage
    TUYA_CALL_ERR_LOG(tal_kv_set(OTTO_LEFT_FOOT_TRIM_KEY, (const uint8_t *)&trim_value, sizeof(trim_value)));

    // Update the global trim value
    g_otto.servo_trim[LEFT_FOOT] = trim_value;

    // Apply trim to oscillator if it exists
    if (g_otto.oscillator_indices[LEFT_FOOT] != -1) {
        oscillator_set_trim(g_otto.oscillator_indices[LEFT_FOOT], trim_value);
    }

    PR_DEBUG("set left foot trim: %d", trim_value);

    return rt;
}

/**
 * @brief Retrieves the current left foot trim value from KV storage.
 * @param None
 * @return int - The current left foot trim value.
 */
int otto_get_left_foot_trim(void)
{
    OPERATE_RET rt = OPRT_OK;

    int trim_value = 0;
    uint8_t *value = NULL;
    size_t read_len = 0;

    // Read from KV storage
    TUYA_CALL_ERR_LOG(tal_kv_get(OTTO_LEFT_FOOT_TRIM_KEY, &value, &read_len));
    if (OPRT_OK != rt || NULL == value) {
        PR_ERR("read left foot trim failed");
        trim_value = 0;  // Default trim value
    } else {
        trim_value = *(int *)value;
    }

    PR_DEBUG("get left foot trim: %d", trim_value);

    if (value) {
        tal_kv_free(value);
        value = NULL;
    }

    return trim_value;
}

/**
 * @brief Sets the right foot trim value and saves it to KV storage.
 * @param trim_value The trim value to set for the right foot.
 * @return OPERATE_RET - OPRT_OK if the trim value is set successfully, otherwise an error code.
 */
OPERATE_RET otto_set_right_foot_trim(int trim_value)
{
    OPERATE_RET rt = OPRT_OK;

    // Save to KV storage
    TUYA_CALL_ERR_LOG(tal_kv_set(OTTO_RIGHT_FOOT_TRIM_KEY, (const uint8_t *)&trim_value, sizeof(trim_value)));

    // Update the global trim value
    g_otto.servo_trim[RIGHT_FOOT] = trim_value;

    // Apply trim to oscillator if it exists
    if (g_otto.oscillator_indices[RIGHT_FOOT] != -1) {
        oscillator_set_trim(g_otto.oscillator_indices[RIGHT_FOOT], trim_value);
    }

    PR_DEBUG("set right foot trim: %d", trim_value);

    return rt;
}

/**
 * @brief Retrieves the current right foot trim value from KV storage.
 * @param None
 * @return int - The current right foot trim value.
 */
int otto_get_right_foot_trim(void)
{
    OPERATE_RET rt = OPRT_OK;

    int trim_value = 0;
    uint8_t *value = NULL;
    size_t read_len = 0;

    // Read from KV storage
    TUYA_CALL_ERR_LOG(tal_kv_get(OTTO_RIGHT_FOOT_TRIM_KEY, &value, &read_len));
    if (OPRT_OK != rt || NULL == value) {
        PR_ERR("read right foot trim failed");
        trim_value = 0;  // Default trim value
    } else {
        trim_value = *(int *)value;
    }

    PR_DEBUG("get right foot trim: %d", trim_value);

    if (value) {
        tal_kv_free(value);
        value = NULL;
    }

    return trim_value;
}

/**
 * @brief Sets the left hand trim value and saves it to KV storage.
 * @param trim_value The trim value to set for the left hand.
 * @return OPERATE_RET - OPRT_OK if the trim value is set successfully, otherwise an error code.
 */
OPERATE_RET otto_set_left_hand_trim(int trim_value)
{
    OPERATE_RET rt = OPRT_OK;

    // Save to KV storage
    TUYA_CALL_ERR_LOG(tal_kv_set(OTTO_LEFT_HAND_TRIM_KEY, (const uint8_t *)&trim_value, sizeof(trim_value)));

    // Update the global trim value
    g_otto.servo_trim[LEFT_HAND] = trim_value;

    // Apply trim to oscillator if it exists
    if (g_otto.oscillator_indices[LEFT_HAND] != -1) {
        oscillator_set_trim(g_otto.oscillator_indices[LEFT_HAND], trim_value);
    }

    PR_DEBUG("set left hand trim: %d", trim_value);

    return rt;
}

/**
 * @brief Retrieves the current left hand trim value from KV storage.
 * @param None
 * @return int - The current left hand trim value.
 */
int otto_get_left_hand_trim(void)
{
    OPERATE_RET rt = OPRT_OK;

    int trim_value = 0;
    uint8_t *value = NULL;
    size_t read_len = 0;

    // Read from KV storage
    TUYA_CALL_ERR_LOG(tal_kv_get(OTTO_LEFT_HAND_TRIM_KEY, &value, &read_len));
    if (OPRT_OK != rt || NULL == value) {
        PR_ERR("read left hand trim failed");
        trim_value = 0;  // Default trim value
    } else {
        trim_value = *(int *)value;
    }

    PR_DEBUG("get left hand trim: %d", trim_value);

    if (value) {
        tal_kv_free(value);
        value = NULL;
    }

    return trim_value;
}

/**
 * @brief Sets the right hand trim value and saves it to KV storage.
 * @param trim_value The trim value to set for the right hand.
 * @return OPERATE_RET - OPRT_OK if the trim value is set successfully, otherwise an error code.
 */
OPERATE_RET otto_set_right_hand_trim(int trim_value)
{
    OPERATE_RET rt = OPRT_OK;

    // Save to KV storage
    TUYA_CALL_ERR_LOG(tal_kv_set(OTTO_RIGHT_HAND_TRIM_KEY, (const uint8_t *)&trim_value, sizeof(trim_value)));

    // Update the global trim value
    g_otto.servo_trim[RIGHT_HAND] = trim_value;

    // Apply trim to oscillator if it exists
    if (g_otto.oscillator_indices[RIGHT_HAND] != -1) {
        oscillator_set_trim(g_otto.oscillator_indices[RIGHT_HAND], trim_value);
    }

    PR_DEBUG("set right hand trim: %d", trim_value);

    return rt;
}

/**
 * @brief Retrieves the current right hand trim value from KV storage.
 * @param None
 * @return int - The current right hand trim value.
 */
int otto_get_right_hand_trim(void)
{
    OPERATE_RET rt = OPRT_OK;

    int trim_value = 0;
    uint8_t *value = NULL;
    size_t read_len = 0;

    // Read from KV storage
    TUYA_CALL_ERR_LOG(tal_kv_get(OTTO_RIGHT_HAND_TRIM_KEY, &value, &read_len));
    if (OPRT_OK != rt || NULL == value) {
        PR_ERR("read right hand trim failed");
        trim_value = 0;  // Default trim value
    } else {
        trim_value = *(int *)value;
    }

    PR_DEBUG("get right hand trim: %d", trim_value);

    if (value) {
        tal_kv_free(value);
        value = NULL;
    }

    return trim_value;
}

/**
 * @brief Checks if all servo trims have been initialized
 * @param None
 * @return bool - true if initialized, false otherwise
 */
bool otto_is_trim_initialized(void)
{
    OPERATE_RET rt = OPRT_OK;

    uint8_t init_flag = 0;
    uint8_t *value = NULL;
    size_t read_len = 0;

    // Read initialization flag from KV storage
    TUYA_CALL_ERR_LOG(tal_kv_get(OTTO_TRIM_INIT_FLAG_KEY, &value, &read_len));
    if (OPRT_OK != rt || NULL == value) {
        PR_DEBUG("trim not initialized yet");
        init_flag = 0;  // Not initialized
    } else {
        init_flag = *value;
    }

    if (value) {
        tal_kv_free(value);
        value = NULL;
    }

    return (init_flag == 1);
}

/**
 * @brief Sets the trim initialization flag
 * @param None
 * @return None
 */
void otto_set_trim_init_flag(void)
{
    uint8_t init_flag = 1;
    OPERATE_RET rt = OPRT_OK;
    TUYA_CALL_ERR_LOG(tal_kv_set(OTTO_TRIM_INIT_FLAG_KEY, &init_flag, sizeof(init_flag)));
    PR_DEBUG("trim initialization flag set");
}

/**
 * @brief Initializes all servo trim values to 0 (only once)
 * @param None
 * @return None
 */
void otto_init_all_trims(void)
{
    // Check if already initialized
    if (otto_is_trim_initialized()) {
        PR_DEBUG("trims already initialized, skipping");
        return;
    }

    PR_DEBUG("initializing all servo trims to 0");

    // Initialize all servo trim values to 0
    otto_set_left_leg_trim(0);
    otto_set_right_leg_trim(0);
    otto_set_left_foot_trim(0);
    otto_set_right_foot_trim(0);
    otto_set_left_hand_trim(0);
    otto_set_right_hand_trim(0);

    // Set initialization flag
    otto_set_trim_init_flag();

    PR_DEBUG("all servo trims initialized successfully");
}

/**
 * @brief Sets all servo trim values from KV storage data
 * @param None
 * @return None
 */
void otto_set_trims_from_kv(void)
{
    // Get trim values from KV storage for all servos
    int left_leg_trim = otto_get_left_leg_trim();
    int right_leg_trim = otto_get_right_leg_trim();
    int left_foot_trim = otto_get_left_foot_trim();
    int right_foot_trim = otto_get_right_foot_trim();
    int left_hand_trim = otto_get_left_hand_trim();
    int right_hand_trim = otto_get_right_hand_trim();

    // Apply trim values to all servos
    otto_set_trims(left_leg_trim, right_leg_trim, left_foot_trim, right_foot_trim, left_hand_trim, right_hand_trim);

    PR_DEBUG("Applied trim values from KV: LL=%d, RL=%d, LF=%d, RF=%d, LH=%d, RH=%d",
             left_leg_trim, right_leg_trim, left_foot_trim, right_foot_trim, left_hand_trim, right_hand_trim);
}

/**
 * @brief Process servo trim calibration DP data
 * @param dp Pointer to the data point structure
 * @return None
 */
void otto_trim_calibration_dp_proc(dp_obj_t *dp)
{
    if (dp == NULL) {
        PR_ERR("dp is NULL");
        return;
    }

    int trim_value = 0;
    OPERATE_RET rt = OPRT_OK;

    // Extract trim value from DP data
    if (dp->type == PROP_VALUE) {
        trim_value = dp->value.dp_value;
    } else {
        PR_ERR("Invalid DP type for trim calibration: %d", dp->type);
        return;
    }

    // Validate trim value range (-50 to +50 degrees)
    if (trim_value < -50 || trim_value > 50) {
        PR_ERR("Trim value out of range: %d (valid range: -30 to +30)", trim_value);
        return;
    }

    // Process different trim calibration DPs
    switch (dp->id) {
        case DPID_OTTO_LEFT_LEG_TRIM:
            PR_DEBUG("Setting left leg trim: %d", trim_value);
            rt = otto_set_left_leg_trim(trim_value);
            break;

        case DPID_OTTO_RIGHT_LEG_TRIM:
            PR_DEBUG("Setting right leg trim: %d", trim_value);
            rt = otto_set_right_leg_trim(trim_value);
            break;

        case DPID_OTTO_LEFT_FOOT_TRIM:
            PR_DEBUG("Setting left foot trim: %d", trim_value);
            rt = otto_set_left_foot_trim(trim_value);
            break;

        case DPID_OTTO_RIGHT_FOOT_TRIM:
            PR_DEBUG("Setting right foot trim: %d", trim_value);
            rt = otto_set_right_foot_trim(trim_value);
            break;

        case DPID_OTTO_LEFT_HAND_TRIM:
            PR_DEBUG("Setting left hand trim: %d", trim_value);
            rt = otto_set_left_hand_trim(trim_value);
            break;

        case DPID_OTTO_RIGHT_HAND_TRIM:
            PR_DEBUG("Setting right hand trim: %d", trim_value);
            rt = otto_set_right_hand_trim(trim_value);
            break;

        default:
            PR_ERR("Unknown trim calibration DPID: %d", dp->id);
            return;
    }

    if (rt != OPRT_OK) {
        PR_ERR("Failed to set trim value: %d, error: %d", trim_value, rt);
        return;
    }

    // Apply the new trim values to all servos
    otto_set_trims_from_kv();

    PR_DEBUG("Trim calibration applied successfully: DPID=%d, Value=%d", dp->id, trim_value);
}

#endif /* OTTO_NO_SERVO_TRIM */

#if defined(OTTO_NO_SERVO_TRIM) && (OTTO_NO_SERVO_TRIM == 1)

/**
 * @brief Stub when servo trim is disabled
 * @return none
 */
void otto_init_all_trims(void)
{
}

/**
 * @brief Stub when servo trim is disabled
 * @return none
 */
void otto_set_trims_from_kv(void)
{
}

#endif /* OTTO_NO_SERVO_TRIM */

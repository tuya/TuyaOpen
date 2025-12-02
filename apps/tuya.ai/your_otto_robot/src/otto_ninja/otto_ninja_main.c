/**
 * @file otto_ninja_main.c
 * @brief OttoNinja Robot Main Control Module
 *
 * This file implements the main control logic for the OttoNinja robot, including:
 * - Joystick input processing (X-axis, Y-axis)
 * - Button input processing (A, B, X, Y buttons)
 * - Motion mode switching (walk mode/roll mode)
 * - Data point (DP) message processing
 * - Robot control task creation and management
 *
 * Main functions:
 * 1. Receive and process control commands from the cloud (joystick, buttons, mode switching)
 * 2. Convert control commands to robot motion control function calls
 * 3. Create independent task to run robot main control loop
 * 4. Manage motion mode state (walk mode/roll mode)
 *
 * @note This module depends on otto_ninja_app_servo module for actual servo control
 * @note Supports enabling/disabling arm and head servo functions via ARM_HEAD_ENABLE macro
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 *
 */


#include "tkl_pwm.h"

// Include servo control header files
#include "otto_ninja_app_servo.h"
#include "otto_ninja_main.h"

 
 /***********************************************************
 *************************micro define***********************
 ***********************************************************/

 #define TASK_OTTO_NINJA_PRIORITY THREAD_PRIO_2
 #define TASK_OTTO_NINJA_SIZE     4096
 
 /***********************************************************
 ***********************typedef define***********************
 ***********************************************************/

 
 /***********************************************************
 ***********************variable define**********************
 ***********************************************************/
 static THREAD_HANDLE sg_otto_ninja_handle;
 
 /***********************************************************
 ***********************function define**********************
 ***********************************************************/
// These values are assumed to be obtained from remote control or sensors
/**
 * @brief Set joystick X-axis value
 * @param value: Joystick X-axis value
 * @return none
 */
int8_t joystick_x = 0;
void set_joystick_x(int8_t value){
    joystick_x = value;
}
/**
  * @brief Get joystick X-axis value
  * @return Joystick X-axis value
  */
int8_t get_joystick_x(void){
    return joystick_x;
}
/**
  * @brief Set joystick Y-axis value
  * @param value: Joystick Y-axis value
  * @return none
  */
int8_t joystick_y = 0;
void set_joystick_y(int8_t value){
    joystick_y = value;
}
/**
  * @brief Get joystick Y-axis value
  * @return Joystick Y-axis value
  */
int8_t get_joystick_y(void){
    return joystick_y;
}



bool sport_mode_change = false;
/**
  * @brief Set sport mode change flag
  * @param value: Sport mode change flag value (true means mode switch is needed)
  * @return none
  */
void set_sport_mode_change(bool value){
    sport_mode_change = value;
}
/**
  * @brief Get sport mode change flag
  * @return Sport mode change flag value
  */
bool get_sport_mode_change(void){
    return sport_mode_change;
}


static int mode_counter = 0;  // 0=walk mode, 1=roll mode
void set_mode_counter(int value){
    mode_counter = value;
}
int get_mode_counter(void){
    return mode_counter;
}

#define DPID_JOYSTIC 101
#define SPORT_MODE 102 // roll or walk mode

#define DPID_JOYSTICK_X 103 //joystick_x
#define DPID_JOYSTICK_Y 104 //joystick_y
OPERATE_RET otto_ninja_dp_obj_proc(dp_obj_recv_t *dpobj)
{
    uint32_t index = 0;
    for (index = 0; index < dpobj->dpscnt; index++) {
        dp_obj_t *dp = dpobj->dps + index;
        PR_DEBUG("idx:%d dpid:%d type:%d ts:%u", index, dp->id, dp->type, dp->time_stamp);

        switch (dp->id) {

        case SPORT_MODE:{

            set_sport_mode_change(true);//Trigger mode switch

            if(dp->value.dp_bool){

                set_mode_counter(1);
                PR_DEBUG("robot_set_roll");
            }else{

                set_mode_counter(0);
                PR_DEBUG("robot_set_walk");
            }
            
            break;
        }




        case DPID_JOYSTICK_X:{
            int8_t joystick_x = dp->value.dp_value;
            set_joystick_x(joystick_x);
            PR_DEBUG("joystick_x:%d", joystick_x);
            break;
        }

        case DPID_JOYSTICK_Y:{
            int8_t joystick_y = dp->value.dp_value;
            set_joystick_y(joystick_y);
            PR_DEBUG("joystick_y:%d", joystick_y);
            break;
        }


        default:
            break;
        }
    }

    return OPRT_OK;
}


/**
 * @brief OttoNinja robot control task
 *
 * This task is responsible for:
 * 1. Initializing robot control system
 * 2. Running robot main control loop
 * 3. Processing joystick input and controlling robot motion
 * 4. Executing different motion control logic based on mode switching
 *
 * @param[in] param: Task parameters (unused)
 * @return none
 */
static void __example_otto_ninja_task(void *param)
{
    PR_NOTICE("=== OttoNinja Servo Control Task Start ===");

    main_init();

    robot_set_walk();
    while (1) {
       

      main_loop();
      tal_system_sleep(10);  // Wait 10ms before restarting

    }

    PR_NOTICE("OttoNinja task is finished, will delete");
    tal_thread_delete(sg_otto_ninja_handle);
    return;
}
 
/**
 * @brief Start OttoNinja robot control module
 *
 * Create and start the OttoNinja robot control task, which will:
 * - Initialize robot control system
 * - Enter main control loop, continuously processing joystick input and mode switching
 * - Execute corresponding motion control based on current mode
 *
 * @return none
 */
 void otto_ninja_main(void)
 {
     OPERATE_RET rt = OPRT_OK;
 

     /* a otto ninja thread */
     THREAD_CFG_T otto_ninja_param = {.priority = TASK_OTTO_NINJA_PRIORITY, .stackDepth = TASK_OTTO_NINJA_SIZE, .thrdname = "otto_ninja_task"};
     TUYA_CALL_ERR_LOG(tal_thread_create_and_start(&sg_otto_ninja_handle, NULL, NULL, __example_otto_ninja_task, NULL, &otto_ninja_param));
 
     return;
 }
 

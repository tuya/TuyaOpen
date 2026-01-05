/**
 * @file servo_ctrl.c
 * @brief 
 * @author your name
 * @version 1.0
 * @date 2025-04-21 16:48:10
 * 
 * Copyright (c) tuya.inc 2014-2021.
 * 
 */
#include "tkl_pwm.h"
#include "tkl_system.h"
#include "tal_log.h"
#include "tal_thread.h"
#include "tal_system.h"

#include "servo_ctrl.h"
#include "tal_mutex.h"
#include "tal_memory.h"

#define SERVO_ACTION_TEST        0      


/***********************************************************
*************************micro define***********************
***********************************************************/
/* Servo common parameters */
#define SERVO_PWM_FREQUENCY        50      // Servo PWM frequency is fixed at 50 Hz
#define SERVO_MIN_DUTY             250     // 0.5 ms pulse width (2.5% duty cycle)
#define SERVO_DUTY_60_DEGREE             583     // 0.5 ms pulse width (2.5% duty cycle)
#define SERVO_DUTY_70_DEGREE             639     // 0.5 ms pulse width (2.5% duty cycle)
#define SERVO_MID_DUTY                   750   // 90° (1.5 ms) <- mid value
#define SERVO_DUTY_110_DEGREE             861     // 0.5 ms pulse width (2.5% duty cycle)
#define SERVO_DUTY_120_DEGREE             917     // 0.5 ms pulse width (2.5% duty cycle)
#define SERVO_STANDBY_FRONT_DUTY            583   // 60°
#define SERVO_STANDBY_REAR_DUTY             917   // 120°
#define SERVO_MAX_DUTY             1250    // 2.5 ms pulse width (12.5% duty cycle)
// #define SERVO_MIN_DUTY             400     // 0.5 ms pulse width (2.5% duty cycle)
// #define SERVO_MID_DUTY              750   // 90° (1.5 ms) <- mid value
// #define SERVO_MAX_DUTY             1100    // 2.5 ms pulse width (12.5% duty cycle)
#define SERVO_STANDBY_FRONT_ANGLE            70
#define SERVO_STANDBY_REAR_ANGLE             110


#define TASK_PWM_SIZE              4096*4 //1024
#define TASK_PWM_PRIORITY          THREAD_PRIO_2

// Servo configuration struct
typedef struct {
    TUYA_PWM_NUM_E pwm_id;    // PWM channel (e.g., TUYA_PWM_NUM_0)
    uint32_t min_duty;        // Minimum duty (maps to 0°)
    uint32_t max_duty;        // Maximum duty (maps to 180°)
    uint32_t current_duty;    // Current duty (for state tracking)
    BOOL_T reverse_polarity;  // Whether to reverse polarity (some servos may require this)
} SERVO_CFG_T;

// Planar coordinates
typedef struct {
    float x;
    float y;
} COORDINATE_T;
// Angle coordinates
typedef struct {
    float angle_l; // left side
    float angle_r; // right side
} ANGLE_T;

// Global servo configuration array (adjust according to actual hardware wiring)
static SERVO_CFG_T sg_servo_cfg[SERVO_NUM] = {

    // Due to assembly, some servos may be mounted in reverse; adjust as needed.
    {TUYA_PWM_NUM_1, SERVO_MIN_DUTY, SERVO_MAX_DUTY, SERVO_DUTY_70_DEGREE, TRUE}, // 1 front-left servo  <PIN24  ID4
    {TUYA_PWM_NUM_4, SERVO_MIN_DUTY, SERVO_MAX_DUTY, SERVO_DUTY_70_DEGREE, TRUE}, // 3 rear-right servo  <PIN34  ID8
    {TUYA_PWM_NUM_2, SERVO_MIN_DUTY, SERVO_MAX_DUTY, SERVO_DUTY_110_DEGREE, TRUE}, // 2 front-right servo <PIN32  ID6
    {TUYA_PWM_NUM_3, SERVO_MIN_DUTY, SERVO_MAX_DUTY, SERVO_DUTY_110_DEGREE, TRUE}, // 4 rear-left servo  <PIN36  ID10


    // {TUYA_PWM_NUM_1, SERVO_MIN_DUTY, SERVO_MAX_DUTY, SERVO_STANDBY_FRONT_DUTY, TRUE}, // 1 front-left servo  <PIN24  ID4
    // {TUYA_PWM_NUM_3, SERVO_MIN_DUTY, SERVO_MAX_DUTY, SERVO_STANDBY_FRONT_DUTY, TRUE}, // 3 rear-right servo  <PIN34  ID8
    // {TUYA_PWM_NUM_2, SERVO_MIN_DUTY, SERVO_MAX_DUTY, SERVO_STANDBY_REAR_DUTY, TRUE}, // 2 front-right servo <PIN32  ID6
    // {TUYA_PWM_NUM_4, SERVO_MIN_DUTY, SERVO_MAX_DUTY, SERVO_STANDBY_REAR_DUTY, TRUE}, // 4 rear-left servo  <PIN36  ID10

    // {TUYA_PWM_NUM_0, SERVO_MIN_DUTY, SERVO_MAX_DUTY, SERVO_MID_DUTY, TRUE}, // 1 front-left servo  <PIN18
    // {TUYA_PWM_NUM_2, SERVO_MIN_DUTY, SERVO_MAX_DUTY, (SERVO_MAX_DUTY + SERVO_MIN_DUTY) - SERVO_STANDBY_DUTY, TRUE}, // 3 rear-right servo  <PIN32
    // {TUYA_PWM_NUM_1, SERVO_MIN_DUTY, SERVO_MAX_DUTY, SERVO_MID_DUTY, TRUE}, // 2 front-right servo <PIN24
    // {TUYA_PWM_NUM_3, SERVO_MIN_DUTY, SERVO_MAX_DUTY, SERVO_STANDBY_DUTY, TRUE}, // 4 rear-left servo  <PIN34
};

static THREAD_HANDLE sg_pwm_handle;

// int servo_calibration[SERVO_NUM] = {-15, -15, -15, -15};
int servo_calibration[SERVO_NUM] = {0, 0, 0, 0};

#define FL_ANGLE_STEP_FORWARD       (SERVO_STANDBY_FRONT_ANGLE - 20)
#define FL_ANGLE_STEP_BACKWARD      (SERVO_STANDBY_FRONT_ANGLE + 20)

#define FR_ANGLE_STEP_FORWARD       (SERVO_STANDBY_FRONT_ANGLE - 20)
#define FR_ANGLE_STEP_BACKWARD      (SERVO_STANDBY_FRONT_ANGLE + 20)

#define BL_ANGLE_STEP_FORWARD       (SERVO_STANDBY_REAR_ANGLE - 20)
#define BL_ANGLE_STEP_BACKWARD      (SERVO_STANDBY_REAR_ANGLE + 20)

#define BR_ANGLE_STEP_FORWARD       (SERVO_STANDBY_REAR_ANGLE - 20)
#define BR_ANGLE_STEP_BACKWARD      (SERVO_STANDBY_REAR_ANGLE + 20)

#define STEP_OFFSET      (5)


int action_map_forward[][SERVO_NUM] = {
    // 1——2
    // |  |
    // 4——3
    //{<1>, <3>, <2>, <4>}, 

    {90, 90, 90,  90}, // stage 15
#if 1
    {90-SWING_FORWARD, 90-SWING_FORWARD,-1,  -1}, // stage 9
    {-1, -1, 90+SWING_BACKWARD,  90+SWING_BACKWARD}, // stage 10
    {90, 90, -1,  -1}, // stage 10
    {-1, -1, 90, 90}, // stage 11
    {-1, -1, 90-SWING_FORWARD,  90-SWING_FORWARD}, // stage 12
    {90+SWING_BACKWARD, 90+SWING_BACKWARD, -1,  -1}, // stage 13
    {-1, -1, 90, 90}, // stage 14
    {90, 90, -1,  -1}, // stage 15
#else
    {90-SWING_FORWARD, 90-SWING_FORWARD,-1,  -1}, // stage 9
    {-1, -1, 90+SWING_BACKWARD,  90+SWING_BACKWARD}, // stage 10l
    {90+SWING_BACKWARD, 90+SWING_BACKWARD,90-SWING_FORWARD,  90-SWING_FORWARD}, // stage 9
    {-1, -1, 90, 90}, // stage 11
    {-1, -1, 90-SWING_FORWARD,  90-SWING_FORWARD}, // stage 12
    {90+SWING_BACKWARD, 90+SWING_BACKWARD, -1,  -1}, // stage 13
    {-1, -1, 90, 90}, // stage 14
    {90, 90, -1,  -1}, // stage 15
    {90-SWING_FORWARD, 90-SWING_FORWARD, 90+SWING_BACKWARD,  90+SWING_BACKWARD}, // stage 9
    {90+SWING_BACKWARD, 90+SWING_BACKWARD, 90-SWING_FORWARD,  90-SWING_FORWARD}, // stage 10
#endif

    {SERVO_STANDBY_FRONT_ANGLE, SERVO_STANDBY_REAR_ANGLE, SERVO_STANDBY_FRONT_ANGLE,  SERVO_STANDBY_REAR_ANGLE}, // stage 15
};

int action_map_backward[][SERVO_NUM] = {

#if 0
    {120, 120, 120,  120}, // stage 9

    {120+SWING_FORWARD, 120+SWING_FORWARD, -1, -1}, // stage 1
    {-1,  -1, 120-SWING_BACKWARD, 120-SWING_BACKWARD}, // stage 2
    {120, 120, -1, -1}, // stage 3
    {-1, -1, 120, 120}, // stage 4
    {-1, -1, 120+SWING_FORWARD, 120+SWING_FORWARD},  // stage 5
    {120-SWING_BACKWARD, 120-SWING_BACKWARD, -1, -1}, // stage 6
    {-1, -1, 120, 120}, // stage 7
    {120, 120, -1, -1}, // stage 8

    {90, 135, 90, 135},
#else
    {90, 90, 90,  90}, // stage 9

    {90+SWING_FORWARD, 90+SWING_FORWARD, -1, -1}, // stage 1
    {-1,  -1, 90-SWING_BACKWARD, 90-SWING_BACKWARD}, // stage 2
    {90, 90, -1, -1}, // stage 3
    {-1, -1, 90, 90}, // stage 4
    {-1, -1, 90+SWING_FORWARD, 90+SWING_FORWARD},  // stage 5
    {90-SWING_BACKWARD, 90-SWING_BACKWARD, -1, -1}, // stage 6
    {-1, -1, 90, 90}, // stage 7
    {90, 90, -1, -1}, // stage 8

    {SERVO_STANDBY_FRONT_ANGLE, SERVO_STANDBY_REAR_ANGLE, SERVO_STANDBY_FRONT_ANGLE,  SERVO_STANDBY_REAR_ANGLE}, // stage 15

#endif
};

int action_map_jump[][SERVO_NUM] = {
    {SERVO_STANDBY_FRONT_ANGLE, SERVO_STANDBY_REAR_ANGLE, SERVO_STANDBY_FRONT_ANGLE,  SERVO_STANDBY_REAR_ANGLE}, // stage 15
    {90+SWING_ANGLE_S, 90-SWING_ANGLE_L, 90+SWING_ANGLE_S, 90-SWING_ANGLE_L}, // stage 1
    {-1, SERVO_STANDBY_REAR_ANGLE, -1, SERVO_STANDBY_REAR_ANGLE},
    {SERVO_STANDBY_FRONT_ANGLE, -1, SERVO_STANDBY_FRONT_ANGLE, -1},
};

// Clockwise: turn right
int action_map_spin_clockwise[][SERVO_NUM] = {
#if 1
    {90, 90, 90,  90}, // stage 15

    {90-SWING_ANGLE_M, 90+SWING_ANGLE_M,-1,  -1}, // stage 9
    {-1, -1, 90-SWING_ANGLE_M,  90+SWING_ANGLE_M}, // stage 10
    {90, 90, -1,  -1}, // stage 10
    {-1, -1, 90, 90}, // stage 11

    {SERVO_STANDBY_FRONT_ANGLE, SERVO_STANDBY_REAR_ANGLE, SERVO_STANDBY_FRONT_ANGLE,  SERVO_STANDBY_REAR_ANGLE}, // stage 15
#else
    {90, 90, 90,  90}, // stage 9

    {90+SWING_ANGLE_L, -1, -1,  90+SWING_ANGLE_L}, // stage 9
    {-1,  90-SWING_ANGLE_S,  90-SWING_ANGLE_S, -1}, // stage 10
    {90,  -1,  -1, 90}, // stage 10
    {-1, 90, 90, -1}, // stage 11

    {90, 135, 90,  135}, // stage 15
#endif
};

// Counterclockwise: turn left
int action_map_spin_anticlockwise[][SERVO_NUM] = {
    {90, 90, 90,  90}, // stage 15

    {-1, -1, 90-SWING_ANGLE_M,  90+SWING_ANGLE_M}, // stage 10
    {90-SWING_ANGLE_M, 90+SWING_ANGLE_M,-1,  -1}, // stage 9
    {-1, -1, 90, 90}, // stage 11
    {90, 90, -1,  -1}, // stage 10


    {SERVO_STANDBY_FRONT_ANGLE, SERVO_STANDBY_REAR_ANGLE, SERVO_STANDBY_FRONT_ANGLE,  SERVO_STANDBY_REAR_ANGLE}, // stage 15
};

int action_map_dance[][SERVO_NUM] = {
    {SERVO_STANDBY_FRONT_ANGLE, SERVO_STANDBY_REAR_ANGLE, SERVO_STANDBY_FRONT_ANGLE,  SERVO_STANDBY_REAR_ANGLE}, // stage 15
};

int action_map_handshake[][SERVO_NUM] = {
    // 1——2
    // |  |
    // 4——3
    //{<1>, <3>, <2>, <4>}, 
    {SERVO_STANDBY_FRONT_ANGLE, SERVO_STANDBY_REAR_ANGLE, SERVO_STANDBY_FRONT_ANGLE,  SERVO_STANDBY_REAR_ANGLE}, // stage 15
    {-1,  -1, 0, -1}, // stage 2
    {-1,  -1, SWING_ANGLE_S, -1}, // stage 2
    {SERVO_STANDBY_FRONT_ANGLE, SERVO_STANDBY_REAR_ANGLE, SERVO_STANDBY_FRONT_ANGLE,  SERVO_STANDBY_REAR_ANGLE}, // stage 15
};

int action_map_dragonboat[][SERVO_NUM] = {
    {90,  90, 90, 90}, // stage 2
    {SERVO_STANDBY_FRONT_ANGLE, SERVO_STANDBY_REAR_ANGLE, SERVO_STANDBY_FRONT_ANGLE,  SERVO_STANDBY_REAR_ANGLE}, // stage 15
};

int action_map_stretch[][SERVO_NUM] = {
    {SERVO_STANDBY_FRONT_ANGLE, SERVO_STANDBY_REAR_ANGLE, SERVO_STANDBY_FRONT_ANGLE,  SERVO_STANDBY_REAR_ANGLE}, // stage 15
    {-1,  140, -1, 140}, // stage 2
    {-1,  140, -1,  165}, // stage 2
    {-1,  165, -1,  140}, // stage 2
    {SERVO_STANDBY_FRONT_ANGLE, SERVO_STANDBY_REAR_ANGLE, SERVO_STANDBY_FRONT_ANGLE,  SERVO_STANDBY_REAR_ANGLE}, // stage 15
};

/**
 * @brief Set servo PWM value
 * @param[in/out] servo_id: 
 * @param[in/out] pwm: 
 * @return STATIC: 0 on success; otherwise an error code
 */

static __attribute__((unused)) void servo_pwm_set(uint32_t pwm0, uint32_t pwm1, uint32_t pwm2)
{
    OPERATE_RET rt = OPRT_OK;

    for (int i = 0; i < SERVO_NUM; i++) {
        SERVO_CFG_T* servo = &sg_servo_cfg[i];

        if(servo->current_duty < 600) {
            servo->current_duty = 900;
        } else {
            servo->current_duty = 600;
        }

        // Clamp to allowed range
        if (servo->current_duty < servo->min_duty) {
            servo->current_duty = servo->min_duty;
        } else if (servo->current_duty > servo->max_duty) {
            servo->current_duty = servo->max_duty;
        }
        
    
        TUYA_PWM_BASE_CFG_T update_cfg = {
            .duty = servo->current_duty,
            .frequency = SERVO_PWM_FREQUENCY,
            .polarity = servo->reverse_polarity ? 
                      TUYA_PWM_NEGATIVE : TUYA_PWM_POSITIVE,
        };
        TUYA_CALL_ERR_LOG(tkl_pwm_info_set(servo->pwm_id, &update_cfg));
        // TUYA_CALL_ERR_LOG(tkl_pwm_start(servo->pwm_id));
        PR_DEBUG("Servo[%d] set duty: %d", i, servo->current_duty);
    }
}



// /**
//  * @brief Mapping function: map input range to output range
//  * @param[in/out] x: 
//  * @param[in/out] in_min: 
//  * @param[in/out] in_max: 
//  * @param[in/out] out_min: 
//  * @param[in/out] out_max: 
//  * @return float: mapped value
//  */
// float map(float x, float in_min, float in_max, float out_min, float out_max) 
// {
//     return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
// }


// Set servo angle (0-180 degrees)
OPERATE_RET servo_set_angle(int servo_id, float angle) {
    SERVO_CFG_T *servo = &sg_servo_cfg[servo_id];
    
    // Clamp angle range
    angle = (angle < 0) ? 0 : (angle > 180) ? 180 : angle;
    
    // Compute duty cycle
    uint32_t duty = servo->min_duty + (uint32_t)((angle / 180.0f) * 
                      (servo->max_duty - servo->min_duty));

    // Update PWM configuration
    TUYA_PWM_BASE_CFG_T cfg = {
        .polarity = servo->reverse_polarity ? TUYA_PWM_NEGATIVE : TUYA_PWM_POSITIVE,
        .duty = duty,
        .cycle = 20000, // 20 ms period
        .frequency = 50
    };

    OPERATE_RET ret = tkl_pwm_info_set(servo->pwm_id, &cfg);
    if (OPRT_OK != ret) {
        PR_ERR("Servo[%d] set duty %d failed! Err:%d", 
                  servo_id, duty, ret);
        return ret;
    }

    // Restart PWM to apply the new configuration
    return tkl_pwm_start(servo->pwm_id);
}

// Set angles for multiple servos (0-180 degrees)
OPERATE_RET servos_set_angles_sync(int angle[SERVO_NUM])
{
    OPERATE_RET ret = OPRT_OK;
    TUYA_PWM_BASE_CFG_T cfgs[SERVO_NUM] = {0};
    int real_angle[SERVO_NUM] = {0};
    memcpy(real_angle, angle, sizeof(real_angle));
    // Compute duty cycle for each servo
    for (int i = 0; i < SERVO_NUM; i++) {
        if(real_angle[i] == -1)
        { 
            continue; // If angle is -1, skip this servo
        }
        SERVO_CFG_T *servo = &sg_servo_cfg[i];

        if(i == 1 || i == 2)
        {
            real_angle[i] = 180 - real_angle[i];
        }

        if (real_angle[i] < 0) 
        {
            real_angle[i] = 0;
        }
        if (real_angle[i] > 180) 
        {
            real_angle[i] = 180;
        }
        
        //PR_ERR("[%s] Servo[%d] set real_angle :%d", __func__, i, real_angle[i]);

        real_angle[i] = real_angle[i] + servo_calibration[i]; 
        // Compute duty cycle
        uint32_t duty = servo->min_duty + (uint32_t)((real_angle[i] / 180.0f) * (servo->max_duty - servo->min_duty));

        cfgs[i].polarity = servo->reverse_polarity ? TUYA_PWM_NEGATIVE : TUYA_PWM_POSITIVE;
        cfgs[i].duty = duty;
        cfgs[i].cycle = 20000;
        cfgs[i].frequency = 50; 

        ret = tkl_pwm_info_set(sg_servo_cfg[i].pwm_id, &cfgs[i]);
        if (OPRT_OK != ret) {
            PR_ERR("Servo[%d] set duty failed! Err:%d", i, ret);
            return ret;
        }
        tkl_pwm_start(sg_servo_cfg[i].pwm_id);
    }

    return OPRT_OK;
}

int servo_action_forward_set(void)
{
#if 1
        int angle[SERVO_NUM] = {0};
        int cnt = 6;
        angle[0] = SERVO_STANDBY_FRONT_ANGLE;
        angle[1] = SERVO_STANDBY_REAR_ANGLE;
        angle[2] = SERVO_STANDBY_FRONT_ANGLE;
        angle[3] = SERVO_STANDBY_REAR_ANGLE;
        servos_set_angles_sync(angle);
        tal_system_sleep(10);

        for (int step = 0; step < cnt; step++) 
        {
            for (int i = 1; i <= 40; i++) 
            {
                angle[0] = 90  - i;
                angle[1] = 130 - i;
                angle[2] = 50 + i + STEP_OFFSET;
                angle[3] = 90 + i - STEP_OFFSET;
                servos_set_angles_sync(angle);
                tal_system_sleep(10);
            }
            tal_system_sleep(10);

            for (int i = 1; i <= 40; i++) 
            {
                angle[0] = 50 + i;
                angle[1] = 90 + i;
                angle[2] = 90 - i - STEP_OFFSET;
                angle[3] = 130 - i + STEP_OFFSET;
                servos_set_angles_sync(angle);
                tal_system_sleep(10);
            }
            tal_system_sleep(10);
        }

        for (int i = 1; i <= 20; i++) 
        {
            angle[0] = FL_ANGLE_STEP_BACKWARD - i;
            angle[1] = BR_ANGLE_STEP_BACKWARD - i;
            angle[2] = FR_ANGLE_STEP_FORWARD + i + STEP_OFFSET;
            angle[3] = BL_ANGLE_STEP_FORWARD + i - STEP_OFFSET;
            servos_set_angles_sync(angle);
            tal_system_sleep(10);
        }
#endif
    return 0;
}
int servo_action_backward_set(void)
{
    int angle[SERVO_NUM] = {0};
    int cnt = 6;
    angle[0] = SERVO_STANDBY_FRONT_ANGLE;
    angle[1] = SERVO_STANDBY_REAR_ANGLE;
    angle[2] = SERVO_STANDBY_FRONT_ANGLE;
    angle[3] = SERVO_STANDBY_REAR_ANGLE;
    servos_set_angles_sync(angle);
    tal_system_sleep(10);
    for (int step = 0; step < cnt; step++) 
    {
        for (int i = 1; i <= 40; i++) 
        {
            angle[0] = FL_ANGLE_STEP_BACKWARD - i + STEP_OFFSET;
            angle[1] = BR_ANGLE_STEP_BACKWARD - i - STEP_OFFSET;
            angle[2] = FR_ANGLE_STEP_FORWARD + i;
            angle[3] = BL_ANGLE_STEP_FORWARD + i;
            servos_set_angles_sync(angle);
            tal_system_sleep(10);
        }
        tal_system_sleep(10);

        for (int i = 1; i <= 40; i++) 
        {
            angle[0] = FL_ANGLE_STEP_FORWARD + i - STEP_OFFSET;
            angle[1] = BR_ANGLE_STEP_FORWARD + i + STEP_OFFSET;
            angle[2] = FR_ANGLE_STEP_BACKWARD - i;
            angle[3] = BL_ANGLE_STEP_BACKWARD - i;
            servos_set_angles_sync(angle);
            tal_system_sleep(10);
        }
        tal_system_sleep(10);
    }

    for (int i = 1; i <= 20; i++) 
    {
        angle[0] = FL_ANGLE_STEP_BACKWARD - i + STEP_OFFSET;
        angle[1] = BR_ANGLE_STEP_BACKWARD - i - STEP_OFFSET;
        angle[2] = FR_ANGLE_STEP_FORWARD + i;
        angle[3] = BL_ANGLE_STEP_FORWARD + i;
        servos_set_angles_sync(angle);
        tal_system_sleep(10);
    }
        
    return 0;
}

int servo_action_spin_clockwise_set(int cnt)
{
    PR_NOTICE("[%s] enter", __FUNCTION__);
    int angle[SERVO_NUM] = {0};

    servos_set_angles_sync(action_map_spin_clockwise[0]);
    tal_system_sleep(ACTION_SPEED_SLOW_FAST);

    for (int step = 0; step < cnt; step++) 
    {
        for (int i = 1; i <= SWING_ANGLE_M; i++) 
        {
            angle[0] = 90 - i;;
            angle[1] = 90 + i;
            angle[2] = -1;
            angle[3] = -1;
            servos_set_angles_sync(angle);
            tal_system_sleep(12);
        }
        tal_system_sleep(15);

        for (int i = 1; i <= SWING_ANGLE_M; i++) 
        {
            angle[0] = -1;
            angle[1] = -1;
            angle[2] = 90 - i;
            angle[3] = 90 + i;
            servos_set_angles_sync(angle);
            tal_system_sleep(12);
        }
        tal_system_sleep(15);

        for (int i = 1; i <= SWING_ANGLE_M; i++) 
        {
            angle[0] = 90 - SWING_ANGLE_M + i;;
            angle[1] = 90 + SWING_ANGLE_M - i;
            angle[2] = -1;
            angle[3] = -1;
            servos_set_angles_sync(angle);
            tal_system_sleep(12);
        }
        tal_system_sleep(15);

        for (int i = 1; i <= SWING_ANGLE_M; i++) 
        {
            angle[0] = -1;
            angle[1] = -1;
            angle[2] = 90 - SWING_ANGLE_M + i;
            angle[3] = 90 + SWING_ANGLE_M - i;
            servos_set_angles_sync(angle);
            tal_system_sleep(12);
        }
        tal_system_sleep(15);

    }
    servos_set_angles_sync(action_map_spin_clockwise[5]);
    tal_system_sleep(ACTION_SPEED_SLOW_FAST); 

    return 0;
}

int servo_action_spin_anticlockwise_set(int cnt)
{
    PR_NOTICE("[%s] enter", __FUNCTION__);

    int angle[SERVO_NUM] = {0};
    servos_set_angles_sync(action_map_spin_anticlockwise[0]);
    tal_system_sleep(ACTION_SPEED_SLOW_FAST);
    for (int step = 0; step < cnt; step++) 
    {
        for (int i = 1; i <= SWING_ANGLE_M; i++) 
        {
            angle[0] = -1;
            angle[1] = -1;
            angle[2] = 90 - i;
            angle[3] = 90 + i;
            servos_set_angles_sync(angle);
            tal_system_sleep(12);
        }
        tal_system_sleep(15);

        for (int i = 1; i <= SWING_ANGLE_M; i++) 
        {
            angle[0] = 90 - i;;
            angle[1] = 90 + i;
            angle[2] = -1;
            angle[3] = -1;
            servos_set_angles_sync(angle);
            tal_system_sleep(12);
        }
        tal_system_sleep(15);

        for (int i = 1; i <= SWING_ANGLE_M; i++) 
        {
            angle[0] = -1;
            angle[1] = -1;
            angle[2] = 90 - SWING_ANGLE_M + i;
            angle[3] = 90 + SWING_ANGLE_M - i;
            servos_set_angles_sync(angle);
            tal_system_sleep(12);
        }
        tal_system_sleep(15);

        for (int i = 1; i <= SWING_ANGLE_M; i++) 
        {
            angle[0] = 90 - SWING_ANGLE_M + i;;
            angle[1] = 90 + SWING_ANGLE_M - i;
            angle[2] = -1;
            angle[3] = -1;
            servos_set_angles_sync(angle);
            tal_system_sleep(12);
        }
        tal_system_sleep(15);

    }


    servos_set_angles_sync(action_map_spin_anticlockwise[5]);
    tal_system_sleep(ACTION_SPEED_SLOW_FAST);
    return 0;
}

int servo_action_dance_set(void)
{
    PR_NOTICE("[%s] enter", __FUNCTION__);
    int cnt = 8, i = 0, j = 0;(void)j;
    int angle[SERVO_NUM] = {0};
    servos_set_angles_sync(action_map_dance[0]);
    tal_system_sleep(ACTION_SPEED_FAST); 

    while(1)
    {
        for(i = 0; i < 15; i+=3)
        {
            angle[0] = SERVO_STANDBY_FRONT_ANGLE+i;
            angle[1] = SERVO_STANDBY_REAR_ANGLE-i;
            angle[2] = SERVO_STANDBY_FRONT_ANGLE-i;
            angle[3] = SERVO_STANDBY_REAR_ANGLE+i;
            servos_set_angles_sync(angle);
            tal_system_sleep(50);
        }
        for(i = 0; i < 30; i+=3)
        {
            angle[0] = SERVO_STANDBY_FRONT_ANGLE + 15 -i;
            angle[1] = SERVO_STANDBY_REAR_ANGLE - 15 +i;
            angle[2] = SERVO_STANDBY_FRONT_ANGLE - 15 +i;
            angle[3] = SERVO_STANDBY_REAR_ANGLE + 15 -i;
            servos_set_angles_sync(angle);
            tal_system_sleep(50);
        }
        for(i = 0; i < 15; i+=3)
        {
            angle[0] = SERVO_STANDBY_FRONT_ANGLE + 15 - 30 + i;
            angle[1] = SERVO_STANDBY_REAR_ANGLE - 15 + 30 -i;
            angle[2] = SERVO_STANDBY_FRONT_ANGLE - 15 + 30 -i;
            angle[3] = SERVO_STANDBY_REAR_ANGLE + 15 - 30 +i;
            servos_set_angles_sync(angle);
            tal_system_sleep(50);
        }
        // You can add other logic here to decide when to stop
        if (cnt-- <= 0) {
            break;
        }
    }

    return 0;
}

int servo_action_handshake_set(void)
{
    PR_NOTICE("[%s] enter", __FUNCTION__);

    int cnt = 10;
    int angle[SERVO_NUM] = {0};

    servos_set_angles_sync(action_map_handshake[0]);
    tal_system_sleep(ACTION_SPEED_FAST); 

    for (int i = 1; i <= 40; i++) 
    {
        angle[0] = -1;
        angle[1] = SERVO_STANDBY_REAR_ANGLE - i;
        angle[2] = -1;
        angle[3] = SERVO_STANDBY_REAR_ANGLE - i;
        servos_set_angles_sync(angle);
        tal_system_sleep(12);
    }
    tal_system_sleep(ACTION_SPEED_MID);

    while(1)
    {
        servos_set_angles_sync(action_map_handshake[1]);
        tal_system_sleep(200); 
        servos_set_angles_sync(action_map_handshake[2]);
        tal_system_sleep(200); 
        // You can add other logic here to decide when to stop
        if (cnt-- <= 0) {
            break;
        }
    }

    angle[0] = -1;
    angle[1] = -1;
    angle[2] = SERVO_STANDBY_FRONT_ANGLE;
    angle[3] = -1;
    servos_set_angles_sync(angle);
    tal_system_sleep(20);

    for (int i = 1; i <= 40; i++) 
    {
        angle[0] = -1;
        angle[1] = SERVO_STANDBY_FRONT_ANGLE + i;
        angle[2] = -1;
        angle[3] = SERVO_STANDBY_FRONT_ANGLE + i;
        servos_set_angles_sync(angle);
        tal_system_sleep(12);
    }

    return 0;
}
int servo_action_dragonboat_set(void)
{
    PR_NOTICE("[%s] enter", __FUNCTION__);
    int cnt = 8, i = 0, j = 0;(void)j;
    int angle[SERVO_NUM] = {0};
    servos_set_angles_sync(action_map_dragonboat[0]);
    tal_system_sleep(ACTION_SPEED_FAST);

    while(1)
    {
        for(i = 0; i < 45; i+=3)
        {
            angle[0] = 90-i;
            angle[1] = 90+i;
            angle[2] = 90-i;
            angle[3] = 90+i;
            servos_set_angles_sync(angle);
            tal_system_sleep(50);
        }
        for(i = 0; i < 90; i+=3)
        {
            angle[0] = 45+i;
            angle[1] = 135-i;
            angle[2] = 45+i;
            angle[3] = 135-i;
            servos_set_angles_sync(angle);
            tal_system_sleep(50);
        }
        for(i = 0; i < 45; i+=3)
        {
            angle[0] = 135-i;
            angle[1] = 45+i;
            angle[2] = 135-i;
            angle[3] = 45+i;
            servos_set_angles_sync(angle);
            tal_system_sleep(50);
        }
        // You can add other logic here to decide when to stop
        if (cnt-- <= 0) {
            break;
        }
    }

    servos_set_angles_sync(action_map_dragonboat[1]);
    tal_system_sleep(ACTION_SPEED_FAST);

    return 0;
}

int servo_action_stretch_set(void)
{
    PR_NOTICE("[%s] enter", __FUNCTION__);
    int cnt = 5;

    servos_set_angles_sync(action_map_stretch[0]);
    tal_system_sleep(ACTION_SPEED_MID); 
    servos_set_angles_sync(action_map_stretch[1]);
    tal_system_sleep(ACTION_SPEED_MID); 

    while(1)
    {
        servos_set_angles_sync(action_map_stretch[2]);
        tal_system_sleep(ACTION_SPEED_FAST); 
        servos_set_angles_sync(action_map_stretch[3]);
        tal_system_sleep(ACTION_SPEED_FAST); 
        // You can add other logic here to decide when to stop
        if (cnt-- <= 0) {
            break;
        }
    }
    servos_set_angles_sync(action_map_stretch[4]);
    tal_system_sleep(ACTION_SPEED_MID); 
 
    return 0;
}

int servo_action_jump_set(void)
{    
    PR_NOTICE("[%s] enter", __FUNCTION__);
    servos_set_angles_sync(action_map_jump[0]);
    tal_system_sleep(ACTION_SPEED_FAST); 
    servos_set_angles_sync(action_map_jump[1]);
    tal_system_sleep(ACTION_SPEED_SLOW); 
    servos_set_angles_sync(action_map_jump[2]);
    tal_system_sleep(10); 
    servos_set_angles_sync(action_map_jump[3]);
    tal_system_sleep(ACTION_SPEED_SLOW); 

    return 0;
}

int servo_action_stand_set(void)
{    
    PR_NOTICE("[%s] enter", __FUNCTION__);

    int action_map_stand[][SERVO_NUM] = {
        {SERVO_STANDBY_FRONT_ANGLE, SERVO_STANDBY_REAR_ANGLE , SERVO_STANDBY_FRONT_ANGLE, SERVO_STANDBY_REAR_ANGLE },
        {SERVO_STANDBY_FRONT_ANGLE, SERVO_STANDBY_REAR_ANGLE,      SERVO_STANDBY_FRONT_ANGLE, SERVO_STANDBY_REAR_ANGLE},
    };
    servos_set_angles_sync(action_map_stand[0]);
    tal_system_sleep(ACTION_SPEED_MID);
    servos_set_angles_sync(action_map_stand[1]);
    tal_system_sleep(ACTION_SPEED_SLOW);
    return 0;
}

int servo_action_sit_set(void)
{
    PR_NOTICE("[%s] enter", __FUNCTION__);

    int action_map_sit[][SERVO_NUM] = {
        {110, 40, 110, 40},
    };
    servos_set_angles_sync(action_map_sit[0]);
    tal_system_sleep(ACTION_SPEED_SLOW); 
    return 0;
}

int servo_action_get_down_set(void)
{
    PR_NOTICE("[%s] enter", __FUNCTION__);
    int action_map_get_down[][SERVO_NUM] = {
        {50, 130, 50, 130},
    };
    servos_set_angles_sync(action_map_get_down[0]);
    tal_system_sleep(ACTION_SPEED_SLOW); 
    return 0;
}

int servo_action_test_set(int angle)
{
    int action_map_test[][SERVO_NUM] = {
        {angle, angle, angle, angle}, // stage 1
    };

    servos_set_angles_sync(action_map_test[0]);

    PR_NOTICE("Servo action test completed");

    return 0;
}

int servo_action_map_set(TUYA_ROBOT_ACTION_E action)
{
    PR_NOTICE("Setting servo action: %d", action);
    int ret = 0;

    switch (action) {
        case ROBOT_ACTION_FORWARD:
            ret = servo_action_forward_set(); // Example angle for forward
            //ret = servo_action_forward_set_1();
            break;
        case ROBOT_ACTION_BACKWARD:
            ret = servo_action_backward_set(); // Example angle for backward
            break;
        case ROBOT_ACTION_LEFT:
            ret = servo_action_spin_anticlockwise_set(4); // Example angle for left turn
            break;
        case ROBOT_ACTION_RIGHT:
            ret = servo_action_spin_clockwise_set(4); // Example angle for right turn
            break;
        case ROBOT_ACTION_SPIN:
            ret = servo_action_spin_clockwise_set(16); // Example angle for spin
            break;
        case ROBOT_ACTION_DANCE:
            ret = servo_action_dance_set(); // Example angle for dance
            break;
        case ROBOT_ACTION_HANDSHAKE:
            ret = servo_action_handshake_set(); // Example angle for handshake
            break;
        case ROBOT_ACTION_JUMP:
            ret = servo_action_jump_set(); // Example angle for jump
            break;
        case ROBOT_ACTION_DRAGONBOAT:
            ret = servo_action_dragonboat_set(); // Example angle for dragonboat
            break;
        case ROBOT_ACTION_STAND:
            ret = servo_action_stand_set(); // Example angle for stand
            break;
        case ROBOT_ACTION_SIT:
            ret = servo_action_sit_set(); // Example angle for sit
            break;
        case ROBOT_ACTION_GETDOWN:
            ret = servo_action_get_down_set(); // Example angle for get down
            break;
        case ROBOT_ACTION_STRETCH:
            ret = servo_action_stretch_set(); // Example angle for stretch
            break;
        default:
            ret = -1; // Invalid action
            break;
    }

    return ret;
}

/**
 * @brief Servo initialization
 * @return OPERATE_RET: 0 on success, otherwise an error code
 */
OPERATE_RET servo_hardware_init(VOID)
{
    PR_NOTICE("Servo hardware init start");
    OPERATE_RET rt = OPRT_OK;
    static uint32_t count = 0;(void)count;
    for (int i = 0; i < SERVO_NUM; i++) {

            TUYA_PWM_BASE_CFG_T pwm_cfg = {
                .cycle = 20000, // 20 ms period
                .count_mode = TUYA_PWM_CNT_UP,
                .duty = sg_servo_cfg[i].current_duty,
                .frequency = SERVO_PWM_FREQUENCY,
                .polarity = sg_servo_cfg[i].reverse_polarity ? 
                            TUYA_PWM_NEGATIVE : TUYA_PWM_POSITIVE,
        };

        
        TUYA_CALL_ERR_GOTO(tkl_pwm_init(sg_servo_cfg[i].pwm_id, &pwm_cfg), __error);
        TUYA_CALL_ERR_GOTO(tkl_pwm_start(sg_servo_cfg[i].pwm_id), __error);
    }
    PR_NOTICE("All servos initialized");

    //robot_param_init(); 
    //home_xy();

    return rt;

__error:
    for (int i = 0; i < SERVO_NUM; i++) {
        tkl_pwm_stop(sg_servo_cfg[i].pwm_id);
    }
    tal_thread_delete(sg_pwm_handle);
    PR_NOTICE("Servo task exited");

    return rt;
}

// Initialize linked list
static void robot_action_list_init(robot_action_list_t* list) {
    list->head = NULL;
    list->tail = NULL;
}

// Check whether the list is empty
static bool robot_action_list_is_empty(const robot_action_list_t* list) {
    return list->head == NULL;
}

// Append a node to the tail
static OPERATE_RET robot_action_list_add_tail(robot_action_list_t* list, TUYA_ROBOT_ACTION_E action) {
    robot_action_node_t* node = (robot_action_node_t*)tal_malloc(sizeof(robot_action_node_t));
    if (!node) 
    {
        PR_ERR("Failed to allocate memory for action node");
        return -1; // Memory allocation failed
    }

    node->action = action;
    node->next = NULL;
    if (list->tail) {
        list->tail->next = node;
        list->tail = node;
    } else {
        list->head = node;
        list->tail = node;
    }
    return 0;
}

// Remove the first node that equals the given action
static bool robot_action_list_remove(robot_action_list_t* list, TUYA_ROBOT_ACTION_E action) {
    robot_action_node_t* prev = NULL;
    robot_action_node_t* curr = list->head;
    while (curr) {
        if (curr->action == action) {
            if (prev) {
                prev->next = curr->next;
            } else {
                list->head = curr->next;
            }
            if (curr == list->tail) {
                list->tail = prev;
            }
            tal_free(curr);
            return true;
        }
        prev = curr;
        curr = curr->next;
    }
    return false;
}

// Free the linked list
static void robot_action_list_clear(robot_action_list_t* list) {
    robot_action_node_t* curr = list->head;
    while (curr) {
        robot_action_node_t* tmp = curr;
        curr = curr->next;
        tal_free(tmp);
    }
    list->head = NULL;
    list->tail = NULL;
}

static robot_action_list_t g_action_list;
static MUTEX_HANDLE g_action_list_mutex;
static THREAD_HANDLE action_task_handle;
/* Test thread handle, only used when SERVO_ACTION_TEST is enabled */
#if defined(SERVO_ACTION_TEST) && (SERVO_ACTION_TEST == 1)
static THREAD_HANDLE test_task_handle;
#endif
static int g_thread_running = 1;

// Thread function
static void robot_action_thread_func(void* arg) {
    PR_NOTICE("Robot action thread started");
    while (g_thread_running) {
        tal_mutex_lock(g_action_list_mutex);
        if (!robot_action_list_is_empty(&g_action_list)) {
            // Pop the first action and remove it from the list
            TUYA_ROBOT_ACTION_E action = g_action_list.head->action;
            robot_action_list_remove(&g_action_list, action);
            tal_mutex_unlock(g_action_list_mutex);

            // Execute action
            PR_NOTICE("执行动作: %d", action);
            servo_action_map_set(action);

        } else {
            PR_TRACE("list empty");
            tal_mutex_unlock(g_action_list_mutex);
            tkl_system_sleep(100); // Sleep 100 ms when there is no action
        }
    }
}

/* Test thread function, only compiled when SERVO_ACTION_TEST is enabled */
#if defined(SERVO_ACTION_TEST) && (SERVO_ACTION_TEST == 1)
static void* robot_test_thread_func(void* arg) {
    PR_NOTICE("Robot test thread started");
    int cnt = 50;
    while(cnt > 0)
    {
        tkl_system_sleep(100); // Sleep 100 ms when there is no action
        cnt--;
    }
    //tkl_system_sleep(200); // Sleep 100 ms when there is no action

    robot_action_add_action(ROBOT_ACTION_FORWARD);

    tkl_system_sleep(2 * 1000); // Sleep 100 ms when there is no action

    robot_action_add_action(ROBOT_ACTION_BACKWARD);

    tkl_system_sleep(2 * 1000); // Sleep 100 ms when there is no action

    robot_action_add_action(ROBOT_ACTION_LEFT);

    tkl_system_sleep(2 * 1000); // Sleep 100 ms when there is no action

    robot_action_add_action(ROBOT_ACTION_STAND);

    robot_action_add_action(ROBOT_ACTION_RIGHT);

    tkl_system_sleep(2 * 1000); // Sleep 100 ms when there is no action

    robot_action_add_action(ROBOT_ACTION_STAND);

    robot_action_add_action(ROBOT_ACTION_HANDSHAKE);

    robot_action_add_action(ROBOT_ACTION_STAND);

    tkl_system_sleep(2 * 1000); // Sleep 100 ms when there is no action

    robot_action_add_action(ROBOT_ACTION_SIT);

    tkl_system_sleep(2 * 1000); // Sleep 100 ms when there is no action

    robot_action_add_action(ROBOT_ACTION_STAND);

    tkl_system_sleep(2 * 1000); // Sleep 100 ms when there is no action

    robot_action_add_action(ROBOT_ACTION_GETDOWN);

    tkl_system_sleep(2 * 1000); // Sleep 100 ms when there is no action

    robot_action_add_action(ROBOT_ACTION_STAND);

    robot_action_add_action(ROBOT_ACTION_DANCE);

    tkl_system_sleep(2 * 1000); // Sleep 100 ms when there is no action

    robot_action_add_action(ROBOT_ACTION_STAND);

    tkl_system_sleep(2 * 1000); // Sleep 100 ms when there is no action

    robot_action_add_action(ROBOT_ACTION_STRETCH);

    tkl_system_sleep(2 * 1000); // Sleep 100 ms when there is no action

    robot_action_add_action(ROBOT_ACTION_STAND);

    tkl_system_sleep(2 * 1000); // Sleep 100 ms when there is no action

    robot_action_add_action(ROBOT_ACTION_JUMP);

    return NULL;
}
#endif


// Start thread
OPERATE_RET robot_action_thread_init(void) 
{
    PR_NOTICE("Starting robot action thread...");
    servo_hardware_init();

    tal_mutex_create_init(&g_action_list_mutex);
    robot_action_list_init(&g_action_list);

    THREAD_CFG_T thread_cfg = {
        .thrdname = "action_task",
        .priority = THREAD_PRIO_2,
        .stackDepth = 2048*2
    };
    tal_thread_create_and_start(&action_task_handle, NULL, NULL, robot_action_thread_func, NULL, &thread_cfg);

#if defined(SERVO_ACTION_TEST) && (SERVO_ACTION_TEST == 1)
    THREAD_CFG_T test_thread_cfg = {
        .thrdname = "test_task",
        .priority = THREAD_PRIO_2,
        .stackDepth = 2048
    };
    tal_thread_create_and_start(&test_task_handle, NULL, NULL, robot_test_thread_func, NULL, &test_thread_cfg);
#endif
    return 0;
}

// Stop thread
void robot_action_thread_stop(void) {
    g_thread_running = 0;
    tal_thread_delete(action_task_handle);
    robot_action_list_clear(&g_action_list);
    tal_mutex_release(g_action_list_mutex);
}

// Add action (thread-safe)
OPERATE_RET robot_action_add_action(TUYA_ROBOT_ACTION_E action) {
    int ret = 0;
    tal_mutex_lock(g_action_list_mutex);
    ret = robot_action_list_add_tail(&g_action_list, action);
    tal_mutex_unlock(g_action_list_mutex);

    return ret;
}




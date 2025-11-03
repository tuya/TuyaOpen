//--------------------------------------------------------------
//-- Oscillator.c
//-- Generate sinusoidal oscillations in the servos
//--------------------------------------------------------------
//-- Original work (c) Juan Gonzalez-Gomez (Obijuan), Dec 2011
//-- GPL license
//-- Ported to Tuya AI development board by [txp666], 2025
//-- Extended with arm support - Otto with 6 servos
//--------------------------------------------------------------

#ifndef __OTTO_MOVEMENTS_H__
#define __OTTO_MOVEMENTS_H__

#include "tuya_cloud_types.h"
#include "tal_api.h"
#include "tkl_output.h"
#include "tkl_pwm.h"
#include "oscillator.h"

//-- Movement direction constants
#define FORWARD 1      /**< Forward direction */
#define BACKWARD -1    /**< Backward direction */
#define LEFT 1         /**< Left direction */
#define RIGHT -1       /**< Right direction */
#define BOTH 0         /**< Both sides simultaneously */
#define SMALL 5        /**< Small movement amplitude */
#define MEDIUM 15      /**< Medium movement amplitude */
#define BIG 30         /**< Large movement amplitude */

// -- Servo speed limit default (degrees per second)
#define SERVO_LIMIT_DEFAULT 240

// -- Servo indexes for easy access 
#define LEFT_LEG 0     /**< Left leg servo index */
#define RIGHT_LEG 1    /**< Right leg servo index */
#define LEFT_FOOT 2    /**< Left foot servo index */
#define RIGHT_FOOT 3   /**< Right foot servo index */
#define LEFT_HAND 4    /**< Left hand servo index */
#define RIGHT_HAND 5   /**< Right hand servo index */
#define SERVO_COUNT 6  /**< Total number of servos */

/**< Default hand home position angle */
#define HAND_HOME_POSITION 45


/**
 * @brief Otto robot structure containing all servo and state information
 */
typedef struct {
    int oscillator_indices[SERVO_COUNT];  /**< Array of oscillator indices for each servo */
    int servo_pins[SERVO_COUNT];         /**< Array of servo pin assignments */
    int servo_trim[SERVO_COUNT];         /**< Array of servo trim values for calibration */

    unsigned long final_time;            /**< Final time for movement completion */
    unsigned long partial_time;          /**< Partial time for movement steps */
    float increment[SERVO_COUNT];        /**< Array of servo position increments */

    bool is_otto_resting;                /**< Flag indicating if Otto is in rest position */
    bool has_hands;                      /**< Flag indicating if Otto has hands/arms */
} Otto_t;


/**
 * @brief Initialize Otto robot with servo pin assignments
 * @param left_leg Left leg servo pin number
 * @param right_leg Right leg servo pin number
 * @param left_foot Left foot servo pin number
 * @param right_foot Right foot servo pin number
 * @param left_hand Left hand servo pin number (-1 if no hands)
 * @param right_hand Right hand servo pin number (-1 if no hands)
 */
void otto_init(int left_leg, int right_leg, int left_foot, int right_foot, int left_hand, int right_hand);

/**
 * @brief Initialize hand servos only, without affecting leg servos
 * @param left_hand Left hand servo pin number
 * @param right_hand Right hand servo pin number
 */
void otto_init_hands_only(int left_hand, int right_hand);

/**
 * @brief Attach all servos to their respective pins
 */
void otto_attach_servos(void);

/**
 * @brief Detach all servos from their pins
 */
void otto_detach_servos(void);

/**
 * @brief Set trim values for all servos for calibration
 * @param left_leg Left leg trim value in degrees
 * @param right_leg Right leg trim value in degrees
 * @param left_foot Left foot trim value in degrees
 * @param right_foot Right foot trim value in degrees
 * @param left_hand Left hand trim value in degrees
 * @param right_hand Right hand trim value in degrees
 */
void otto_set_trims(int left_leg, int right_leg, int left_foot, int right_foot, int left_hand, int right_hand);

/**
 * @brief Move all servos to target positions over specified time
 * @param time Movement duration in milliseconds
 * @param servo_target Array of target positions for all servos (0-180 degrees)
 */
void otto_move_servos(int time, int servo_target[]);

/**
 * @brief Move a single servo to specified position
 * @param position Target position in degrees (0-180)
 * @param servo_number Servo index (0-5)
 */
void otto_move_single(int position, int servo_number);

/**
 * @brief Oscillate servos with specified parameters
 * @param amplitude Array of amplitudes for each servo in degrees
 * @param offset Array of offsets for each servo in degrees
 * @param period Oscillation period in milliseconds
 * @param phase_diff Array of phase differences for each servo in radians
 * @param cycle Number of oscillation cycles
 */
void otto_oscillate_servos(int amplitude[SERVO_COUNT], int offset[SERVO_COUNT], int period, double phase_diff[SERVO_COUNT], float cycle);

/**
 * @brief Move Otto to home/rest position
 * @param hands_down Whether to lower hands to rest position
 */
void otto_home(bool hands_down);

/**
 * @brief Get current rest state of Otto
 * @return true if Otto is in rest position, false otherwise
 */
bool otto_get_rest_state(void);

/**
 * @brief Set rest state of Otto
 * @param state Rest state to set
 */
void otto_set_rest_state(bool state);


/**
 * @brief Make Otto perform a jump movement
 * @param steps Number of jump steps
 * @param period Jump period in milliseconds
 */
void otto_jump(float steps, int period);

/**
 * @brief Make Otto walk forward or backward
 * @param steps Number of walking steps
 * @param period Walking period in milliseconds
 * @param dir Direction (FORWARD/BACKWARD)
 * @param amount Hand movement amount (0 if no hands)
 */
void otto_walk(float steps, int period, int dir, int amount);

/**
 * @brief Make Otto turn left or right
 * @param steps Number of turning steps
 * @param period Turning period in milliseconds
 * @param dir Direction (LEFT/RIGHT)
 * @param amount Hand movement amount (0 if no hands)
 */
void otto_turn(float steps, int period, int dir, int amount);

/**
 * @brief Make Otto bend to one side
 * @param steps Number of bend steps
 * @param period Bend period in milliseconds
 * @param dir Direction (LEFT/RIGHT)
 */
void otto_bend(int steps, int period, int dir);

/**
 * @brief Make Otto shake one leg
 * @param steps Number of shake steps
 * @param period Shake period in milliseconds
 * @param dir Direction (LEFT/RIGHT leg)
 */
void otto_shake_leg(int steps, int period, int dir);

/**
 * @brief Make Otto move up and down
 * @param steps Number of up-down cycles
 * @param period Movement period in milliseconds
 * @param height Movement height in degrees
 */
void otto_up_down(float steps, int period, int height);

/**
 * @brief Make Otto swing side to side
 * @param steps Number of swing steps
 * @param period Swing period in milliseconds
 * @param height Swing height in degrees
 */
void otto_swing(float steps, int period, int height);

/**
 * @brief Make Otto swing on tiptoes
 * @param steps Number of swing steps
 * @param period Swing period in milliseconds
 * @param height Swing height in degrees
 */
void otto_tiptoe_swing(float steps, int period, int height);

/**
 * @brief Make Otto jitter (rapid up-down movement)
 * @param steps Number of jitter steps
 * @param period Jitter period in milliseconds
 * @param height Jitter height in degrees
 */
void otto_jitter(float steps, int period, int height);

/**
 * @brief Make Otto perform ascending turn
 * @param steps Number of turn steps
 * @param period Turn period in milliseconds
 * @param height Turn height in degrees
 */
void otto_ascending_turn(float steps, int period, int height);

/**
 * @brief Make Otto perform moonwalker dance
 * @param steps Number of moonwalker steps
 * @param period Movement period in milliseconds
 * @param height Movement height in degrees
 * @param dir Direction (LEFT/RIGHT)
 */
void otto_moonwalker(float steps, int period, int height, int dir);

/**
 * @brief Make Otto perform crusaito dance
 * @param steps Number of crusaito steps
 * @param period Movement period in milliseconds
 * @param height Movement height in degrees
 * @param dir Direction (LEFT/RIGHT)
 */
void otto_crusaito(float steps, int period, int height, int dir);

/**
 * @brief Make Otto perform flapping movement
 * @param steps Number of flapping steps
 * @param period Flapping period in milliseconds
 * @param height Flapping height in degrees
 * @param dir Direction (FORWARD/BACKWARD)
 */
void otto_flapping(float steps, int period, int height, int dir);


/**
 * @brief Raise Otto's hands up
 * @param period Movement period in milliseconds
 * @param dir Direction (LEFT/RIGHT/BOTH)
 */
void otto_hands_up(int period, int dir);

/**
 * @brief Lower Otto's hands down
 * @param period Movement period in milliseconds
 * @param dir Direction (LEFT/RIGHT/BOTH)
 */
void otto_hands_down(int period, int dir);

/**
 * @brief Make Otto wave with hands
 * @param period Wave period in milliseconds
 * @param dir Direction (LEFT/RIGHT/BOTH)
 */
void otto_hand_wave(int period, int dir);

/**
 * @brief Wave with left hand only
 * @param period Wave period in milliseconds
 */
#define otto_wave_left(period)          otto_hand_wave(period, LEFT)

/**
 * @brief Wave with right hand only
 * @param period Wave period in milliseconds
 */
#define otto_wave_right(period)         otto_hand_wave(period, RIGHT)

/**
 * @brief Wave with both hands
 * @param period Wave period in milliseconds
 */
#define otto_wave_both(period)          otto_hand_wave(period, BOTH)

/**
 * @brief Enable servo speed limiting
 * @param speed_limit_degree_per_sec Maximum degrees per second for servo movement
 */
void otto_enable_servo_limit(int speed_limit_degree_per_sec);

/**
 * @brief Disable servo speed limiting
 */
void otto_disable_servo_limit(void);

/**
 * @brief Execute complex servo movement with oscillation parameters
 * @param amplitude Array of amplitudes for each servo in degrees
 * @param offset Array of offsets for each servo in degrees
 * @param period Oscillation period in milliseconds
 * @param phase_diff Array of phase differences for each servo in radians
 * @param steps Number of movement steps
 */
void otto_execute(int amplitude[SERVO_COUNT], int offset[SERVO_COUNT], int period, double phase_diff[SERVO_COUNT], float steps);


#endif  // __OTTO_MOVEMENTS_H__ 
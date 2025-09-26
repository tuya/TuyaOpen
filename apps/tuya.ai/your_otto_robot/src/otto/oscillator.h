//--------------------------------------------------------------
//-- Oscillator.c
//-- Generate sinusoidal oscillations in the servos
//--------------------------------------------------------------
//-- Original work (c) Juan Gonzalez-Gomez (Obijuan), Dec 2011
//-- GPL license
//-- Ported to Tuya AI development board by [txp666], 2025
//--------------------------------------------------------------
#ifndef __OSCILLATOR_H__
#define __OSCILLATOR_H__

#include "tuya_cloud_types.h"
#include "tal_api.h"
#include "tkl_output.h"
#include "tkl_pwm.h"

/**< Mathematical constant Pi */
#define M_PI 3.14159265358979323846

/**< Convert degrees to radians */
#ifndef DEG2RAD
#define DEG2RAD(g) ((g) * M_PI) / 180
#endif

/**< Minimum servo pulse width in microseconds */
#define SERVO_MIN_PULSEWIDTH_US 500           
/**< Maximum servo pulse width in microseconds */
#define SERVO_MAX_PULSEWIDTH_US 2500          
/**< Minimum servo angle in degrees */
#define SERVO_MIN_DEGREE -90                  
/**< Maximum servo angle in degrees */
#define SERVO_MAX_DEGREE 90                   
/**< Servo timebase resolution in Hz */
#define SERVO_TIMEBASE_RESOLUTION_HZ 1000000  
/**< Servo timebase period in microseconds */
#define SERVO_TIMEBASE_PERIOD 20000          

/**< Maximum number of oscillators */
#define MAX_OSCILLATORS 8


/**
 * @brief Oscillator structure for servo control
 * 
 * Contains all parameters and state information for a single servo oscillator
 */
typedef struct {
    bool is_attached;              /**< Whether oscillator is attached to a pin */

    //-- Oscillator parameters
    unsigned int amplitude;        /**< Amplitude in degrees */
    int offset;                     /**< Offset in degrees */
    unsigned int period;            /**< Period in milliseconds */
    double phase0;                  /**< Initial phase in radians */

    //-- Internal variables
    int pos;                        /**< Current servo position */
    int pin;                        /**< Pin where the servo is connected */
    int trim;                       /**< Calibration offset */
    double phase;                   /**< Current phase */
    double inc;                     /**< Phase increment */
    double number_samples;          /**< Number of samples */
    unsigned int sampling_period;   /**< Sampling period in milliseconds */

    unsigned long previous_millis; /**< Previous time in milliseconds */
    unsigned long current_millis;   /**< Current time in milliseconds */

    //-- Oscillation mode. If true, the servo is stopped
    bool stop;                      /**< Stop oscillation flag */

    //-- Reverse mode
    bool rev;                       /**< Reverse direction flag */

    int diff_limit;                 /**< Speed limit in degrees per second */
    unsigned long previous_servo_command_millis; /**< Previous servo command time */

    TUYA_PWM_NUM_E pwm_channel;     /**< PWM channel for servo control */
} Oscillator_t;

// Oscillator function declarations

/**
 * @brief Create a new oscillator
 * @param trim Trim value for the oscillator
 * @return Oscillator index on success, -1 on failure
 */
int oscillator_create(int trim);

/**
 * @brief Destroy an oscillator
 * @param idx Oscillator index
 */
void oscillator_destroy(int idx);

/**
 * @brief Attach oscillator to a pin
 * @param idx Oscillator index
 * @param pin Pin number
 * @param rev Reverse direction flag
 */
void oscillator_attach(int idx, int pin, bool rev);

/**
 * @brief Detach oscillator from pin
 * @param idx Oscillator index
 */
void oscillator_detach(int idx);

/**
 * @brief Set oscillator amplitude
 * @param idx Oscillator index
 * @param amplitude Amplitude in degrees
 */
void oscillator_set_a(int idx, unsigned int amplitude);

/**
 * @brief Set oscillator offset
 * @param idx Oscillator index
 * @param offset Offset in degrees
 */
void oscillator_set_o(int idx, int offset);

/**
 * @brief Set oscillator phase
 * @param idx Oscillator index
 * @param Ph Phase in radians
 */
void oscillator_set_ph(int idx, double Ph);

/**
 * @brief Set oscillator period
 * @param idx Oscillator index
 * @param period Period in milliseconds
 */
void oscillator_set_t(int idx, unsigned int period);

/**
 * @brief Set oscillator trim value
 * @param idx Oscillator index
 * @param trim Trim value in degrees
 */
void oscillator_set_trim(int idx, int trim);

/**
 * @brief Set oscillator speed limiter
 * @param idx Oscillator index
 * @param diff_limit Speed limit in degrees per second
 */
void oscillator_set_limiter(int idx, int diff_limit);

/**
 * @brief Disable oscillator speed limiter
 * @param idx Oscillator index
 */
void oscillator_disable_limiter(int idx);

/**
 * @brief Get oscillator trim value
 * @param idx Oscillator index
 * @return Trim value in degrees
 */
int oscillator_get_trim(int idx);

/**
 * @brief Set oscillator position
 * @param idx Oscillator index
 * @param position Target position in degrees
 */
void oscillator_set_position(int idx, int position);

/**
 * @brief Stop oscillator
 * @param idx Oscillator index
 */
void oscillator_stop(int idx);

/**
 * @brief Start oscillator
 * @param idx Oscillator index
 */
void oscillator_play(int idx);

/**
 * @brief Reset oscillator phase
 * @param idx Oscillator index
 */
void oscillator_reset(int idx);

/**
 * @brief Refresh oscillator (update position)
 * @param idx Oscillator index
 */
void oscillator_refresh(int idx);

/**
 * @brief Get current oscillator position
 * @param idx Oscillator index
 * @return Current position in degrees
 */
int oscillator_get_position(int idx);

/**
 * @brief Convert angle to PWM compare value
 * @param angle Angle in degrees
 * @return PWM compare value
 */
uint32_t oscillator_angle_to_compare(int angle);

/**
 * @brief Check if next sample should be processed
 * @param idx Oscillator index
 * @return true if next sample should be processed
 */
bool oscillator_next_sample(int idx);

/**
 * @brief Write position to servo
 * @param idx Oscillator index
 * @param position Position in degrees
 */
void oscillator_write(int idx, int position);

#endif  // __OSCILLATOR_H__ 
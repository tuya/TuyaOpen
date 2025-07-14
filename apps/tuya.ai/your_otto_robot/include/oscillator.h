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

#define M_PI 3.14159265358979323846

#ifndef DEG2RAD
#define DEG2RAD(g) ((g) * M_PI) / 180
#endif

#define SERVO_MIN_PULSEWIDTH_US 500           
#define SERVO_MAX_PULSEWIDTH_US 2500          
#define SERVO_MIN_DEGREE -90                  
#define SERVO_MAX_DEGREE 90                   
#define SERVO_TIMEBASE_RESOLUTION_HZ 1000000  
#define SERVO_TIMEBASE_PERIOD 20000           


#define MAX_OSCILLATORS 8


typedef struct {
    bool is_attached;

    //-- Oscillators parameters
    unsigned int amplitude;  //-- Amplitude (degrees)
    int offset;              //-- Offset (degrees)
    unsigned int period;     //-- Period (miliseconds)
    double phase0;           //-- Phase (radians)

    //-- Internal variables
    int pos;                       //-- Current servo pos
    int pin;                       //-- Pin where the servo is connected
    int trim;                      //-- Calibration offset
    double phase;                  //-- Current phase
    double inc;                    //-- Increment of phase
    double number_samples;         //-- Number of samples
    unsigned int sampling_period;  //-- sampling period (ms)

    unsigned long previous_millis;
    unsigned long current_millis;

    //-- Oscillation mode. If true, the servo is stopped
    bool stop;

    //-- Reverse mode
    bool rev;

    int diff_limit;
    unsigned long previous_servo_command_millis;

    TUYA_PWM_NUM_E pwm_channel;
} Oscillator_t;

// Oscillator
int oscillator_create(int trim);
void oscillator_destroy(int idx);
void oscillator_attach(int idx, int pin, bool rev);
void oscillator_detach(int idx);
void oscillator_set_a(int idx, unsigned int amplitude);
void oscillator_set_o(int idx, int offset);
void oscillator_set_ph(int idx, double Ph);
void oscillator_set_t(int idx, unsigned int period);
void oscillator_set_trim(int idx, int trim);
void oscillator_set_limiter(int idx, int diff_limit);
void oscillator_disable_limiter(int idx);
int oscillator_get_trim(int idx);
void oscillator_set_position(int idx, int position);
void oscillator_stop(int idx);
void oscillator_play(int idx);
void oscillator_reset(int idx);
void oscillator_refresh(int idx);
int oscillator_get_position(int idx);
uint32_t oscillator_angle_to_compare(int angle);
bool oscillator_next_sample(int idx);
void oscillator_write(int idx, int position);

#endif  // __OSCILLATOR_H__ 
//--------------------------------------------------------------
//-- Oscillator.c
//-- Generate sinusoidal oscillations in the servos
//--------------------------------------------------------------
//-- Original work (c) Juan Gonzalez-Gomez (Obijuan), Dec 2011
//-- GPL license
//-- Ported to Tuya AI development board by [txp666], 2025
//-- Extended with arm support - Otto with 6 servos
//--------------------------------------------------------------

#include "otto_movements.h"
#include "oscillator.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>


Otto_t g_otto;

static unsigned long millis()
{
    return tal_system_get_millisecond();
}

/**
 * @brief 重置所有振荡器相位到初始状态
 * 解决相位累积导致的不协调问题
 */
static void otto_reset_all_oscillators(void)
{
    for (int i = 0; i < SERVO_COUNT; i++) {
        if (g_otto.oscillator_indices[i] != -1) {
            oscillator_reset(g_otto.oscillator_indices[i]);
        }
    }
}


void otto_init(int left_leg, int right_leg, int left_foot, int right_foot, int left_hand, int right_hand)
{
    g_otto.servo_pins[LEFT_LEG] = left_leg;
    g_otto.servo_pins[RIGHT_LEG] = right_leg;
    g_otto.servo_pins[LEFT_FOOT] = left_foot;
    g_otto.servo_pins[RIGHT_FOOT] = right_foot;
    g_otto.servo_pins[LEFT_HAND] = left_hand;
    g_otto.servo_pins[RIGHT_HAND] = right_hand;


    g_otto.has_hands = (left_hand != -1 && right_hand != -1);

 
    for (int i = 0; i < SERVO_COUNT; i++) {
        g_otto.servo_trim[i] = 0;
        if (g_otto.servo_pins[i] != -1) {
            g_otto.oscillator_indices[i] = oscillator_create(0);
        } else {
            g_otto.oscillator_indices[i] = -1;
        }
    }

    otto_attach_servos();
    g_otto.is_otto_resting = false;
}

/**
 * @brief Initialize hand servos only, without affecting leg servos
 */
void otto_init_hands_only(int left_hand, int right_hand)
{
    // Only set hand pins, don't affect leg servos
    g_otto.servo_pins[LEFT_HAND] = left_hand;
    g_otto.servo_pins[RIGHT_HAND] = right_hand;
    g_otto.has_hands = (left_hand != -1 && right_hand != -1);
    
    // Only create hand oscillators
    g_otto.servo_trim[LEFT_HAND] = 0;
    g_otto.servo_trim[RIGHT_HAND] = 0;
    
    if (g_otto.servo_pins[LEFT_HAND] != -1) {
        g_otto.oscillator_indices[LEFT_HAND] = oscillator_create(0);
    }
    if (g_otto.servo_pins[RIGHT_HAND] != -1) {
        g_otto.oscillator_indices[RIGHT_HAND] = oscillator_create(0);
    }
    
    // Only attach hand servos
    if (g_otto.servo_pins[LEFT_HAND] != -1 && g_otto.oscillator_indices[LEFT_HAND] != -1) {
        oscillator_attach(g_otto.oscillator_indices[LEFT_HAND], g_otto.servo_pins[LEFT_HAND], false);
    }
    if (g_otto.servo_pins[RIGHT_HAND] != -1 && g_otto.oscillator_indices[RIGHT_HAND] != -1) {
        oscillator_attach(g_otto.oscillator_indices[RIGHT_HAND], g_otto.servo_pins[RIGHT_HAND], false);
    }
}

///////////////////////////////////////////////////////////////////
//-- ATTACH & DETACH FUNCTIONS ----------------------------------//
///////////////////////////////////////////////////////////////////
void otto_attach_servos()
{
    for (int i = 0; i < SERVO_COUNT; i++) {
        if (g_otto.servo_pins[i] != -1 && g_otto.oscillator_indices[i] != -1) {
            oscillator_attach(g_otto.oscillator_indices[i], g_otto.servo_pins[i], false);
        }
    }
}

void otto_detach_servos()
{
    for (int i = 0; i < SERVO_COUNT; i++) {
        if (g_otto.oscillator_indices[i] != -1) {
            oscillator_detach(g_otto.oscillator_indices[i]);
        }
    }
}

///////////////////////////////////////////////////////////////////
//-- OSCILLATORS TRIMS ------------------------------------------//
///////////////////////////////////////////////////////////////////
void otto_set_trims(int left_leg, int right_leg, int left_foot, int right_foot, int left_hand, int right_hand)
{
    g_otto.servo_trim[LEFT_LEG] = left_leg;
    g_otto.servo_trim[RIGHT_LEG] = right_leg;
    g_otto.servo_trim[LEFT_FOOT] = left_foot;
    g_otto.servo_trim[RIGHT_FOOT] = right_foot;

    if (g_otto.has_hands) {
        g_otto.servo_trim[LEFT_HAND] = left_hand;
        g_otto.servo_trim[RIGHT_HAND] = right_hand;
    }

    for (int i = 0; i < SERVO_COUNT; i++) {
        if (g_otto.oscillator_indices[i] != -1) {
            oscillator_set_trim(g_otto.oscillator_indices[i], g_otto.servo_trim[i]);
        }
    }
}

///////////////////////////////////////////////////////////////////
//-- BASIC MOTION FUNCTIONS -------------------------------------//
///////////////////////////////////////////////////////////////////
void otto_move_servos(int time, int servo_target[])
{
    if (g_otto.is_otto_resting == true) {
        g_otto.is_otto_resting = false;
    }

    g_otto.final_time = millis() + time;
    if (time > 10) {
        for (int i = 0; i < SERVO_COUNT; i++) {
            if (g_otto.oscillator_indices[i] != -1) {
                g_otto.increment[i] =
                    (servo_target[i] - oscillator_get_position(g_otto.oscillator_indices[i])) / (time / 10.0);
            }
        }

        for (int iteration = 1; millis() < g_otto.final_time; iteration++) {
            g_otto.partial_time = millis() + 10;
            for (int i = 0; i < SERVO_COUNT; i++) {
                if (g_otto.oscillator_indices[i] != -1) {
                    oscillator_set_position(g_otto.oscillator_indices[i],
                                            oscillator_get_position(g_otto.oscillator_indices[i]) +
                                                g_otto.increment[i]);
                }
            }
            tal_system_sleep(10);
        }
    } else {
        for (int i = 0; i < SERVO_COUNT; i++) {
            if (g_otto.oscillator_indices[i] != -1) {
                oscillator_set_position(g_otto.oscillator_indices[i], servo_target[i]);
            }
        }
        tal_system_sleep(time);
    }

    // final adjustment to the target.
    bool f = true;
    int adjustment_count = 0;
    while (f && adjustment_count < 10) {
        f = false;
        for (int i = 0; i < SERVO_COUNT; i++) {
            if (g_otto.oscillator_indices[i] != -1 &&
                servo_target[i] != oscillator_get_position(g_otto.oscillator_indices[i])) {
                f = true;
                break;
            }
        }
        if (f) {
            for (int i = 0; i < SERVO_COUNT; i++) {
                if (g_otto.oscillator_indices[i] != -1) {
                    oscillator_set_position(g_otto.oscillator_indices[i], servo_target[i]);
                }
            }
            tal_system_sleep(10);
            adjustment_count++;
        }
    }
}

void otto_move_single(int position, int servo_number)
{
    if (position > 180)
        position = 90;
    if (position < 0)
        position = 90;

    if (g_otto.is_otto_resting == true) {
        g_otto.is_otto_resting = false;
    }

    if (servo_number >= 0 && servo_number < SERVO_COUNT && g_otto.oscillator_indices[servo_number] != -1) {
        oscillator_set_position(g_otto.oscillator_indices[servo_number], position);
    }
}

void otto_oscillate_servos(int amplitude[SERVO_COUNT], int offset[SERVO_COUNT], int period,
                           double phase_diff[SERVO_COUNT], float cycle)
{
    for (int i = 0; i < SERVO_COUNT; i++) {
        if (g_otto.oscillator_indices[i] != -1) {
            oscillator_set_o(g_otto.oscillator_indices[i], offset[i]);
            oscillator_set_a(g_otto.oscillator_indices[i], amplitude[i]);
            oscillator_set_t(g_otto.oscillator_indices[i], period);
            oscillator_set_ph(g_otto.oscillator_indices[i], phase_diff[i]);
        }
    }

    unsigned long ref = millis();
    unsigned long end_time = (unsigned long)(period * cycle + ref);

    while (millis() < end_time) {
        for (int i = 0; i < SERVO_COUNT; i++) {
            if (g_otto.oscillator_indices[i] != -1) {
                oscillator_refresh(g_otto.oscillator_indices[i]);
            }
        }
        tal_system_sleep(5);
    }
    tal_system_sleep(10);
}

void otto_execute(int amplitude[SERVO_COUNT], int offset[SERVO_COUNT], int period, double phase_diff[SERVO_COUNT],
                  float steps)
{
    if (g_otto.is_otto_resting == true) {
        g_otto.is_otto_resting = false;
    }

    // 关键修复：在开始执行前重置所有振荡器相位，确保每次运动都从相同初始状态开始
    otto_reset_all_oscillators();

    int cycles = (int)steps;

    //-- Execute complete cycles
    if (cycles >= 1)
        for (int i = 0; i < cycles; i++)
            otto_oscillate_servos(amplitude, offset, period, phase_diff, 1.0);

    //-- Execute the final not complete cycle
    otto_oscillate_servos(amplitude, offset, period, phase_diff, steps - cycles);
    tal_system_sleep(10);
}

///////////////////////////////////////////////////////////////////
//-- HOME = Otto at rest position -------------------------------//
///////////////////////////////////////////////////////////////////
void otto_home(bool hands_down)
{
  
    // if (g_otto.is_otto_resting == false) { // Go to rest position only if necessary

        // 关键修复：在回到home位置前，先重置所有振荡器相位，确保状态完全重置
        otto_reset_all_oscillators();

        int homes[SERVO_COUNT];
        for (int i = 0; i < SERVO_COUNT; i++) {
            if (i == LEFT_HAND || i == RIGHT_HAND) {
                if (hands_down) {
                   
                    if (i == LEFT_HAND) {
                        homes[i] = HAND_HOME_POSITION;
                    } else {                                 // RIGHT_HAND
                        homes[i] = 180 - HAND_HOME_POSITION; 
                    }
                } else {
                    
                    if (g_otto.oscillator_indices[i] != -1) {
                        homes[i] = oscillator_get_position(g_otto.oscillator_indices[i]);
                    } else {
                        homes[i] = 90;
                    }
                }
            } else {
                
                homes[i] = 90;
            }
        }

        otto_move_servos(500, homes);
    //     g_otto.is_otto_resting = true;
    // }

    tal_system_sleep(200);
}

bool otto_get_rest_state()
{
    return g_otto.is_otto_resting;
}

void otto_set_rest_state(bool state)
{
    g_otto.is_otto_resting = state;
}

///////////////////////////////////////////////////////////////////
//-- PREDETERMINED MOTION SEQUENCES -----------------------------//
///////////////////////////////////////////////////////////////////
//-- Otto movement: Jump
//--  Parameters:
//--    steps: Number of steps
//--    T: Period
//---------------------------------------------------------
void otto_jump(float steps, int period)
{
    int up[SERVO_COUNT] = {90, 90, 150, 30, HAND_HOME_POSITION, 180 - HAND_HOME_POSITION};
    otto_move_servos(period, up);
    int down[SERVO_COUNT] = {90, 90, 90, 90, HAND_HOME_POSITION, 180 - HAND_HOME_POSITION};
    otto_move_servos(period, down);
}

//---------------------------------------------------------
//-- Otto gait: Walking  (forward or backward)
//--  Parameters:
//--    * steps:  Number of steps
//--    * T : Period
//--    * Dir: Direction: FORWARD / BACKWARD
//---------------------------------------------------------
void otto_walk(float steps, int period, int dir, int amount)
{
    //-- Oscillator parameters for walking
    //-- Hip sevos are in phase
    //-- Feet servos are in phase
    //-- Hip and feet are 90 degrees out of phase
    //--      -90 : Walk forward
    //--       90 : Walk backward
    //-- Feet servos also have the same offset (for tiptoe a little bit)
    int A[SERVO_COUNT] = {30, 30, 30, 30, 0, 0};
    int O[SERVO_COUNT] = {0, 0, 5, -5, HAND_HOME_POSITION - 90, HAND_HOME_POSITION};
    double phase_diff[SERVO_COUNT] = {0, 0, DEG2RAD(dir * -90), DEG2RAD(dir * -90), 0, 0};

    if (amount > 0 && g_otto.has_hands) {
        
        A[LEFT_HAND] = amount;
        A[RIGHT_HAND] = amount;

        
        phase_diff[LEFT_HAND] = phase_diff[RIGHT_LEG]; 
        phase_diff[RIGHT_HAND] = phase_diff[LEFT_LEG]; 
    } else {
        A[LEFT_HAND] = 0;
        A[RIGHT_HAND] = 0;
    }

    //-- Let's oscillate the servos!
    otto_execute(A, O, period, phase_diff, steps);
}

//---------------------------------------------------------
//-- Otto gait: Turning (left or right)
//--  Parameters:
//--   * Steps: Number of steps
//--   * T: Period
//--   * Dir: Direction: LEFT / RIGHT
//---------------------------------------------------------
void otto_turn(float steps, int period, int dir, int amount)
{
    //-- Same coordination than for walking (see Otto::walk)
    //-- The Amplitudes of the hip's oscillators are not igual
    //-- When the right hip servo amplitude is higher, the steps taken by
    //--   the right leg are bigger than the left. So, the robot describes an
    //--   left arc
    int A[SERVO_COUNT] = {30, 30, 30, 30, 0, 0};
    int O[SERVO_COUNT] = {0, 0, 5, -5, HAND_HOME_POSITION - 90, HAND_HOME_POSITION};
    double phase_diff[SERVO_COUNT] = {0, 0, DEG2RAD(-90), DEG2RAD(-90), 0, 0};

    if (dir == LEFT) {
        A[0] = 30; //-- Left hip servo
        A[1] = 0;  //-- Right hip servo
    } else {
        A[0] = 0;
        A[1] = 30;
    }


    if (amount > 0 && g_otto.has_hands) {
     
        A[LEFT_HAND] = amount;
        A[RIGHT_HAND] = amount;

        
        phase_diff[LEFT_HAND] = phase_diff[LEFT_LEG];   
        phase_diff[RIGHT_HAND] = phase_diff[RIGHT_LEG]; 
    } else {
        A[LEFT_HAND] = 0;
        A[RIGHT_HAND] = 0;
    }

    //-- Let's oscillate the servos!
    otto_execute(A, O, period, phase_diff, steps);
}

//---------------------------------------------------------
//-- Otto gait: Lateral bend
//--  Parameters:
//--    steps: Number of bends
//--    T: Period of one bend
//--    dir: RIGHT=Right bend LEFT=Left bend
//---------------------------------------------------------
void otto_bend(int steps, int period, int dir)
{
    // Parameters of all the movements. Default: Left bend
    int bend1[SERVO_COUNT] = {90, 90, 62, 35, HAND_HOME_POSITION, 180 - HAND_HOME_POSITION};
    int bend2[SERVO_COUNT] = {90, 90, 62, 105, HAND_HOME_POSITION, 180 - HAND_HOME_POSITION};
    int homes[SERVO_COUNT] = {90, 90, 90, 90, HAND_HOME_POSITION, 180 - HAND_HOME_POSITION};

    // Time of one bend, constrained in order to avoid movements too fast.
    // T=max(T, 600);
    // Changes in the parameters if right direction is chosen
    if (dir == -1) {
        bend1[2] = 180 - 35;
        bend1[3] = 180 - 60; // Not 65. Otto is unbalanced
        bend2[2] = 180 - 105;
        bend2[3] = 180 - 60;
    }

    // Time of the bend movement. Fixed parameter to avoid falls
    int T2 = 800;

    // Bend movement
    for (int i = 0; i < steps; i++) {
        otto_move_servos(T2 / 2, bend1);
        otto_move_servos(T2 / 2, bend2);
        tal_system_sleep(period * 0.8);
        otto_move_servos(500, homes);
    }
}

//---------------------------------------------------------
//-- Otto gait: Shake a leg
//--  Parameters:
//--    steps: Number of shakes
//--    T: Period of one shake
//--    dir: RIGHT=Right leg LEFT=Left leg
//---------------------------------------------------------
void otto_shake_leg(int steps, int period, int dir)
{
    // This variable change the amount of shakes
    int numberLegMoves = 2;

    // Parameters of all the movements. Default: Right leg
    int shake_leg1[SERVO_COUNT] = {90, 90, 58, 35, HAND_HOME_POSITION, 180 - HAND_HOME_POSITION};
    int shake_leg2[SERVO_COUNT] = {90, 90, 58, 120, HAND_HOME_POSITION, 180 - HAND_HOME_POSITION};
    int shake_leg3[SERVO_COUNT] = {90, 90, 58, 60, HAND_HOME_POSITION, 180 - HAND_HOME_POSITION};
    int homes[SERVO_COUNT] = {90, 90, 90, 90, HAND_HOME_POSITION, 180 - HAND_HOME_POSITION};

    // Changes in the parameters if left leg is chosen
    if (dir == -1) {
        shake_leg1[2] = 180 - 35;
        shake_leg1[3] = 180 - 58;
        shake_leg2[2] = 180 - 120;
        shake_leg2[3] = 180 - 58;
        shake_leg3[2] = 180 - 60;
        shake_leg3[3] = 180 - 58;
    }

    // Time of the bend movement. Fixed parameter to avoid falls
    int T2 = 1000;
    // Time of one shake, constrained in order to avoid movements too fast.
    period = period - T2;
    period = MAX(period, 200 * numberLegMoves);

    for (int j = 0; j < steps; j++) {
        // Bend movement
        otto_move_servos(T2 / 2, shake_leg1);
        otto_move_servos(T2 / 2, shake_leg2);

        // Shake movement
        for (int i = 0; i < numberLegMoves; i++) {
            otto_move_servos(period / (2 * numberLegMoves), shake_leg3);
            otto_move_servos(period / (2 * numberLegMoves), shake_leg2);
        }
        otto_move_servos(500, homes); // Return to home position
    }

    tal_system_sleep(period);
}

//---------------------------------------------------------
//-- Otto movement: up & down
//--  Parameters:
//--    * steps: Number of jumps
//--    * T: Period
//--    * h: Jump height: SMALL / MEDIUM / BIG
//--              (or a number in degrees 0 - 90)
//---------------------------------------------------------
void otto_up_down(float steps, int period, int height)
{
    //-- Both feet are 180 degrees out of phase
    //-- Feet amplitude and offset are the same
    //-- Initial phase for the right foot is -90, so that it starts
    //--   in one extreme position (not in the middle)
    int A[SERVO_COUNT] = {0, 0, height, height, 0, 0};
    int O[SERVO_COUNT] = {0, 0, height, -height, HAND_HOME_POSITION, 180 - HAND_HOME_POSITION};
    double phase_diff[SERVO_COUNT] = {0, 0, DEG2RAD(-90), DEG2RAD(90), 0, 0};

    //-- Let's oscillate the servos!
    otto_execute(A, O, period, phase_diff, steps);
}

//---------------------------------------------------------
//-- Otto movement: swinging side to side
//--  Parameters:
//--     steps: Number of steps
//--     T : Period
//--     h : Amount of swing (from 0 to 50 aprox)
//---------------------------------------------------------
void otto_swing(float steps, int period, int height)
{
    //-- Both feets are in phase. The offset is half the amplitude
    //-- It causes the robot to swing from side to side
    int A[SERVO_COUNT] = {0, 0, height, height};
    int O[SERVO_COUNT] = {0, 0, height / 2, -height / 2};
    double phase_diff[SERVO_COUNT] = {0, 0, DEG2RAD(0), DEG2RAD(0)};

    //-- Let's oscillate the servos!
    otto_execute(A, O, period, phase_diff, steps);
}

//---------------------------------------------------------
//-- Otto movement: swinging side to side without touching the floor with the heel
//--  Parameters:
//--     steps: Number of steps
//--     T : Period
//--     h : Amount of swing (from 0 to 50 aprox)
//---------------------------------------------------------
void otto_tiptoe_swing(float steps, int period, int height)
{
    //-- Both feets are in phase. The offset is not half the amplitude in order to tiptoe
    //-- It causes the robot to swing from side to side
    int A[SERVO_COUNT] = {0, 0, height, height};
    int O[SERVO_COUNT] = {0, 0, height, -height};
    double phase_diff[SERVO_COUNT] = {0, 0, 0, 0};

    //-- Let's oscillate the servos!
    otto_execute(A, O, period, phase_diff, steps);
}

//---------------------------------------------------------
//-- Otto gait: Jitter
//--  Parameters:
//--    steps: Number of jitters
//--    T: Period of one jitter
//--    h: height (Values between 5 - 25)
//---------------------------------------------------------
void otto_jitter(float steps, int period, int height)
{
    //-- Both feet are 180 degrees out of phase
    //-- Feet amplitude and offset are the same
    //-- Initial phase for the right foot is -90, so that it starts
    //--   in one extreme position (not in the middle)
    //-- h is constrained to avoid hit the feets
    height = MIN(25, height);
    int A[SERVO_COUNT] = {height, height, 0, 0};
    int O[SERVO_COUNT] = {0, 0, 0, 0};
    double phase_diff[SERVO_COUNT] = {DEG2RAD(-90), DEG2RAD(90), 0, 0};

    //-- Let's oscillate the servos!
    otto_execute(A, O, period, phase_diff, steps);
}

//---------------------------------------------------------
//-- Otto gait: Ascending & turn (Jitter while up&down)
//--  Parameters:
//--    steps: Number of bends
//--    T: Period of one bend
//--    h: height (Values between 5 - 15)
//---------------------------------------------------------
void otto_ascending_turn(float steps, int period, int height)
{
    //-- Both feet and legs are 180 degrees out of phase
    //-- Initial phase for the right foot is -90, so that it starts
    //--   in one extreme position (not in the middle)
    //-- h is constrained to avoid hit the feets
    height = MIN(13, height);
    int A[SERVO_COUNT] = {height, height, height, height};
    int O[SERVO_COUNT] = {0, 0, height + 4, -height + 4};
    double phase_diff[SERVO_COUNT] = {DEG2RAD(-90), DEG2RAD(90), DEG2RAD(-90), DEG2RAD(90)};

    //-- Let's oscillate the servos!
    otto_execute(A, O, period, phase_diff, steps);
}

//---------------------------------------------------------
//-- Otto gait: Moonwalker. Otto moves like Michael Jackson
//--  Parameters:
//--    Steps: Number of steps
//--    T: Period
//--    h: Height. Typical valures between 15 and 40
//--    dir: Direction: LEFT / RIGHT
//---------------------------------------------------------
void otto_moonwalker(float steps, int period, int height, int dir)
{
    //-- This motion is similar to that of the caterpillar robots: A travelling
    //-- wave moving from one side to another
    //-- The two Otto's feet are equivalent to a minimal configuration. It is known
    //-- that 2 servos can move like a worm if they are 120 degrees out of phase
    //-- In the example of Otto, the two feet are mirrored so that we have:
    //--    180 - 120 = 60 degrees. The actual phase difference given to the oscillators
    //--  is 60 degrees.
    //--  Both amplitudes are equal. The offset is half the amplitud plus a little bit of
    //-   offset so that the robot tiptoe lightly

    int A[SERVO_COUNT] = {0, 0, height, height};
    int O[SERVO_COUNT] = {0, 0, height / 2 + 2, -height / 2 - 2};
    int phi = -dir * 90;
    double phase_diff[SERVO_COUNT] = {0, 0, DEG2RAD(phi), DEG2RAD(-60 * dir + phi)};

    //-- Let's oscillate the servos!
    otto_execute(A, O, period, phase_diff, steps);
}

//----------------------------------------------------------
//-- Otto gait: Crusaito. A mixture between moonwalker and walk
//--   Parameters:
//--     steps: Number of steps
//--     T: Period
//--     h: height (Values between 20 - 50)
//--     dir:  Direction: LEFT / RIGHT
//-----------------------------------------------------------
void otto_crusaito(float steps, int period, int height, int dir)
{
    int A[SERVO_COUNT] = {25, 25, height, height};
    int O[SERVO_COUNT] = {0, 0, height / 2 + 4, -height / 2 - 4};
    double phase_diff[SERVO_COUNT] = {90, 90, DEG2RAD(0), DEG2RAD(-60 * dir)};

    //-- Let's oscillate the servos!
    otto_execute(A, O, period, phase_diff, steps);
}

//---------------------------------------------------------
//-- Otto gait: Flapping
//--  Parameters:
//--    steps: Number of steps
//--    T: Period
//--    h: height (Values between 10 - 30)
//--    dir: direction: FOREWARD, BACKWARD
//---------------------------------------------------------
void otto_flapping(float steps, int period, int height, int dir)
{
    int A[SERVO_COUNT] = {12, 12, height, height};
    int O[SERVO_COUNT] = {0, 0, height - 10, -height + 10};
    double phase_diff[SERVO_COUNT] = {DEG2RAD(0), DEG2RAD(180), DEG2RAD(-90 * dir), DEG2RAD(90 * dir)};

    //-- Let's oscillate the servos!
    otto_execute(A, O, period, phase_diff, steps);
}

void otto_enable_servo_limit(int diff_limit)
{
    for (int i = 0; i < SERVO_COUNT; i++) {
        if (g_otto.oscillator_indices[i] != -1) {
            oscillator_set_limiter(g_otto.oscillator_indices[i], diff_limit);
        }
    }
}

void otto_disable_servo_limit()
{
    for (int i = 0; i < SERVO_COUNT; i++) {
        if (g_otto.oscillator_indices[i] != -1) {
            oscillator_disable_limiter(g_otto.oscillator_indices[i]);
        }
    }
}

///////////////////////////////////////////////////////////////////
//-- hand wave --------------------------------------------------// -------------------------------------------//
///////////////////////////////////////////////////////////////////

//---------------------------------------------------------
//-- hand wave: hand up
//--  Parameters:
//--    period:  time period of each cycle
//--    dir: direction 1=left, -1=right, 0=both
//---------------------------------------------------------
void otto_hands_up(int period, int dir)
{
    if (!g_otto.has_hands) {
        return;
    }

    int target[SERVO_COUNT] = {90, 90, 90, 90, HAND_HOME_POSITION, 180 - HAND_HOME_POSITION};

    if (dir == 0) {
        target[LEFT_HAND] = 170;
        target[RIGHT_HAND] = 10;
    } else if (dir == 1) {
        target[LEFT_HAND] = 170;
        if (g_otto.oscillator_indices[RIGHT_HAND] != -1) {
            target[RIGHT_HAND] = oscillator_get_position(g_otto.oscillator_indices[RIGHT_HAND]);
        }
    } else if (dir == -1) {
        target[RIGHT_HAND] = 10;
        if (g_otto.oscillator_indices[LEFT_HAND] != -1) {
            target[LEFT_HAND] = oscillator_get_position(g_otto.oscillator_indices[LEFT_HAND]);
        }
    }

    otto_move_servos(period, target);
}

//---------------------------------------------------------
//--   Hands Wave Down
//--  Parameters:
//--    period:  time period of each cycle
//--    dir: direction 1=left, -1=right, 0=both
//---------------------------------------------------------
void otto_hands_down(int period, int dir)
{
    if (!g_otto.has_hands) {
        return;
    }

    int target[SERVO_COUNT] = {90, 90, 90, 90, HAND_HOME_POSITION, 180 - HAND_HOME_POSITION};

    if (dir == 1) {
        if (g_otto.oscillator_indices[RIGHT_HAND] != -1) {
            target[RIGHT_HAND] = oscillator_get_position(g_otto.oscillator_indices[RIGHT_HAND]);
        }
    } else if (dir == -1) {
        if (g_otto.oscillator_indices[LEFT_HAND] != -1) {
            target[LEFT_HAND] = oscillator_get_position(g_otto.oscillator_indices[LEFT_HAND]);
        }
    }

    otto_move_servos(period, target);
}

//---------------------------------------------------------
//-- otto_hand_wave: wave
//--  Parameters:
//--    period:  time period of each cycle
//--    dir: direction 1=left, -1=right, 0=both
//---------------------------------------------------------
void otto_hand_wave(int period, int dir)
{
    if (!g_otto.has_hands) {
        return;
    }

    
    const int wave_amplitude = 30;     
    const int wave_cycles = 5;         
    const int raise_time = 300;        
    const int wave_time = period / 10; 

   
    const int left_raised = 170;
    const int right_raised = 10;

   
    int positions[SERVO_COUNT];
    for (int i = 0; i < SERVO_COUNT; i++) {
        positions[i] =
            (g_otto.oscillator_indices[i] != -1) ? oscillator_get_position(g_otto.oscillator_indices[i]) : 90;
    }


    bool wave_left = (dir == LEFT || dir == BOTH);
    bool wave_right = (dir == RIGHT || dir == BOTH);

  
    if (wave_left)
        positions[LEFT_HAND] = left_raised;
    if (wave_right)
        positions[RIGHT_HAND] = right_raised;
    otto_move_servos(raise_time, positions);


    for (int cycle = 0; cycle < wave_cycles; cycle++) {

        if (wave_left)
            positions[LEFT_HAND] = left_raised - wave_amplitude;
        if (wave_right)
            positions[RIGHT_HAND] = right_raised + wave_amplitude;
        otto_move_servos(wave_time, positions);


        if (wave_left)
            positions[LEFT_HAND] = left_raised + wave_amplitude;
        if (wave_right)
            positions[RIGHT_HAND] = right_raised - wave_amplitude;
        otto_move_servos(wave_time, positions);
    }

    if (wave_left)
        positions[LEFT_HAND] = HAND_HOME_POSITION;
    if (wave_right)
        positions[RIGHT_HAND] = 180 - HAND_HOME_POSITION;
    otto_move_servos(raise_time, positions);
}


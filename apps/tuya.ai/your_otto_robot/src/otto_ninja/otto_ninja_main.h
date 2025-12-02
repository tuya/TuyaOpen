#ifndef __OTTO_NINJA_MAIN_H__
#define __OTTO_NINJA_MAIN_H__

#include "tuya_cloud_types.h"
#include "tuya_iot_dp.h"

void set_joystick_x(int8_t value); // Set joystick X-axis value, range -100 to 100, 0 is neutral, negative is left, positive is right
int8_t get_joystick_x(void); // Get joystick X-axis value, range -100 to 100, 0 is neutral, negative is left, positive is right
void set_joystick_y(int8_t value); // Set joystick Y-axis value, range -100 to 100, 0 is neutral, negative is backward, positive is forward
int8_t get_joystick_y(void); // Get joystick Y-axis value, range -100 to 100, 0 is neutral, negative is backward, positive is forward

void set_sport_mode_change(bool value); // Set sport mode change flag, true means mode switch is needed, switches to roll mode
bool get_sport_mode_change(void); // Get sport mode change flag, true means mode switch is needed, switches to roll mode

void set_mode_counter(int value); // Set mode counter, 0 is walk mode, 1 is roll mode
int get_mode_counter(void); // Get mode counter, 0 is walk mode, 1 is roll mode   
void otto_ninja_main(void); // Main function, starts otto ninja thread
OPERATE_RET otto_ninja_dp_obj_proc(dp_obj_recv_t *dpobj);

#endif
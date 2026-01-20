#ifndef CLOCK_TIME_H_
#define CLOCK_TIME_H_

#include "clock_types.h"

void clock_time_service_init_once(void);
void clock_time_get(clock_ui_state_t *state_out);

void clock_time_set_source(clock_time_src_t src);
clock_time_src_t clock_time_get_source(void);

void clock_time_sync_start_once(void);

#endif /* CLOCK_TIME_H_ */

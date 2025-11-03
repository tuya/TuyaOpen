#include "movement_table.h"

// 声明外部定义的运动表
extern const MovementTable_t standby_table;
extern const MovementTable_t forward_table;
extern const MovementTable_t backward_table;
extern const MovementTable_t turn_left_table;
extern const MovementTable_t turn_right_table;
extern const MovementTable_t climb_table;
extern const MovementTable_t descend_table;
extern const MovementTable_t turn_left_fast_table;
extern const MovementTable_t turn_right_fast_table;

// 运动表函数实现
const MovementTable_t* get_standby_table(void) {
    return &standby_table;
}

const MovementTable_t* get_forward_table(void) {
    return &forward_table;
}

const MovementTable_t* get_backward_table(void) {
    return &backward_table;
}

const MovementTable_t* get_turn_left_table(void) {
    return &turn_left_table;
}

const MovementTable_t* get_turn_right_table(void) {
    return &turn_right_table;
}

const MovementTable_t* get_climb_table(void) {
    return &climb_table;
}

const MovementTable_t* get_descend_table(void) {
    return &descend_table;
}

const MovementTable_t* get_turn_left_fast_table(void) {
    return &turn_left_fast_table;
}

const MovementTable_t* get_turn_right_fast_table(void) {
    return &turn_right_fast_table;
}

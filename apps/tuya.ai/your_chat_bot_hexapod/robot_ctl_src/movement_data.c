#include "movement_table.h"
#include "base.h"

// 机械参数常量
#define STANDBY_Z (LEG_JOINT3_TO_TIP * 0.9659f - LEG_JOINT2_TO_JOINT3 * 0.5f)
#define LEFTRIGHT_X (LEG_MOUNT_LEFT_RIGHT_X + LEG_ROOT_TO_JOINT1 + LEG_JOINT1_TO_JOINT2 + (LEG_JOINT2_TO_JOINT3 * 0.8660f) + LEG_JOINT3_TO_TIP * 0.2588f)
#define OTHER_X (LEG_MOUNT_OTHER_X + (LEG_ROOT_TO_JOINT1 + LEG_JOINT1_TO_JOINT2 + (LEG_JOINT2_TO_JOINT3 * 0.8660f) + LEG_JOINT3_TO_TIP * 0.2588f) * 0.7071f)
#define OTHER_Y (LEG_MOUNT_OTHER_Y + (LEG_ROOT_TO_JOINT1 + LEG_JOINT1_TO_JOINT2 + (LEG_JOINT2_TO_JOINT3 * 0.8660f) + LEG_JOINT3_TO_TIP * 0.2588f) * 0.7071f)

// 六条腿的基础位置
#define P1X OTHER_X
#define P1Y OTHER_Y
#define P1Z -STANDBY_Z
#define P2X LEFTRIGHT_X
#define P2Y 0
#define P2Z -STANDBY_Z
#define P3X OTHER_X
#define P3Y -OTHER_Y
#define P3Z -STANDBY_Z
#define P4X -OTHER_X
#define P4Y -OTHER_Y
#define P4Z -STANDBY_Z
#define P5X -LEFTRIGHT_X
#define P5Y 0
#define P5Z -STANDBY_Z
#define P6X -OTHER_X
#define P6Y OTHER_Y
#define P6Z -STANDBY_Z

// 待机位置数据
static const Locations_t standby_path[] = {
    // 步骤0: 待机位置
    {
        .points = {
            {.x = P1X, .y = P1Y, .z = P1Z},      // 腿0
            {.x = P2X, .y = P2Y, .z = P2Z},      // 腿1
            {.x = P3X, .y = P3Y, .z = P3Z},      // 腿2
            {.x = P4X, .y = P4Y, .z = P4Z},      // 腿3
            {.x = P5X, .y = P5Y, .z = P5Z},      // 腿4
            {.x = P6X, .y = P6Y, .z = P6Z}       // 腿5
        }
    }
};

static const int standby_entries[] = {0};

const MovementTable_t standby_table = {
    .table = standby_path,
    .length = 1,
    .stepDuration = 1000,
    .entries = standby_entries,
    .entriesCount = 1
};

// 前进运动数据（基于C++版本的backward_paths，但方向相反）
static const Locations_t forward_path[] = {
    // 步骤0: 准备姿势
    {
        .points = {
            {.x = P1X + 0.00f, .y = P1Y + 0.00f, .z = P1Z + 25.00f},      // 腿0
            {.x = P2X + 0.00f, .y = P2Y + 0.00f, .z = P2Z + 0.00f},       // 腿1
            {.x = P3X + 0.00f, .y = P3Y + 0.00f, .z = P3Z + 25.00f},      // 腿2
            {.x = P4X + 0.00f, .y = P4Y + 0.00f, .z = P4Z + 0.00f},       // 腿3
            {.x = P5X + 0.00f, .y = P5Y + 0.00f, .z = P5Z + 25.00f},      // 腿4
            {.x = P6X + 0.00f, .y = P6Y + 0.00f, .z = P6Z + 0.00f}        // 腿5
        }
    },
    // 步骤1: 抬起右前腿和左后腿
    {
        .points = {
            {.x = P1X + 0.00f, .y = P1Y + -7.73f, .z = P1Z + 23.78f},    // 腿0
            {.x = P2X + 0.00f, .y = P2Y + 5.00f, .z = P2Z + 0.00f},       // 腿1
            {.x = P3X + 0.00f, .y = P3Y + -7.73f, .z = P3Z + 23.78f},     // 腿2
            {.x = P4X + 0.00f, .y = P4Y + 5.00f, .z = P4Z + 0.00f},       // 腿3
            {.x = P5X + 0.00f, .y = P5Y + -7.73f, .z = P5Z + 23.78f},     // 腿4
            {.x = P6X + 0.00f, .y = P6Y + 5.00f, .z = P6Z + 0.00f}        // 腿5
        }
    },
    // 步骤2: 移动右前腿和左后腿向前
    {
        .points = {
            {.x = P1X + 0.00f, .y = P1Y + -14.69f, .z = P1Z + 20.23f},   // 腿0
            {.x = P2X + 0.00f, .y = P2Y + 10.00f, .z = P2Z + 0.00f},      // 腿1
            {.x = P3X + 0.00f, .y = P3Y + -14.69f, .z = P3Z + 20.23f},    // 腿2
            {.x = P4X + 0.00f, .y = P4Y + 10.00f, .z = P4Z + 0.00f},      // 腿3
            {.x = P5X + 0.00f, .y = P5Y + -14.69f, .z = P5Z + 20.23f},    // 腿4
            {.x = P6X + 0.00f, .y = P6Y + 10.00f, .z = P6Z + 0.00f}       // 腿5
        }
    },
    // 步骤3: 放下右前腿和左后腿
    {
        .points = {
            {.x = P1X + 0.00f, .y = P1Y + -20.23f, .z = P1Z + 14.69f},   // 腿0
            {.x = P2X + 0.00f, .y = P2Y + 15.00f, .z = P2Z + 0.00f},      // 腿1
            {.x = P3X + 0.00f, .y = P3Y + -20.23f, .z = P3Z + 14.69f},    // 腿2
            {.x = P4X + 0.00f, .y = P4Y + 15.00f, .z = P4Z + 0.00f},      // 腿3
            {.x = P5X + 0.00f, .y = P5Y + -20.23f, .z = P5Z + 14.69f},    // 腿4
            {.x = P6X + 0.00f, .y = P6Y + 15.00f, .z = P6Z + 0.00f}       // 腿5
        }
    },
    // 步骤4: 抬起左前腿和右后腿
    {
        .points = {
            {.x = P1X + 0.00f, .y = P1Y + -23.78f, .z = P1Z + 7.73f},    // 腿0
            {.x = P2X + 0.00f, .y = P2Y + 20.00f, .z = P2Z + 0.00f},      // 腿1
            {.x = P3X + 0.00f, .y = P3Y + -23.78f, .z = P3Z + 7.73f},     // 腿2
            {.x = P4X + 0.00f, .y = P4Y + 20.00f, .z = P4Z + 0.00f},      // 腿3
            {.x = P5X + 0.00f, .y = P5Y + -23.78f, .z = P5Z + 7.73f},     // 腿4
            {.x = P6X + 0.00f, .y = P6Y + 20.00f, .z = P6Z + 0.00f}       // 腿5
        }
    },
    // 步骤5: 移动左前腿和右后腿向前
    {
        .points = {
            {.x = P1X + 0.00f, .y = P1Y + -25.00f, .z = P1Z + 0.00f},    // 腿0
            {.x = P2X + 0.00f, .y = P2Y + 25.00f, .z = P2Z + 0.00f},      // 腿1
            {.x = P3X + 0.00f, .y = P3Y + -25.00f, .z = P3Z + 0.00f},     // 腿2
            {.x = P4X + 0.00f, .y = P4Y + 25.00f, .z = P4Z + 0.00f},      // 腿3
            {.x = P5X + 0.00f, .y = P5Y + -25.00f, .z = P5Z + 0.00f},     // 腿4
            {.x = P6X + 0.00f, .y = P6Y + 25.00f, .z = P6Z + 0.00f}       // 腿5
        }
    },
    // 步骤6: 放下左前腿和右后腿
    {
        .points = {
            {.x = P1X + 0.00f, .y = P1Y + -20.00f, .z = P1Z + 0.00f},    // 腿0
            {.x = P2X + 0.00f, .y = P2Y + 23.78f, .z = P2Z + 7.73f},      // 腿1
            {.x = P3X + 0.00f, .y = P3Y + -20.00f, .z = P3Z + 0.00f},     // 腿2
            {.x = P4X + 0.00f, .y = P4Y + 23.78f, .z = P4Z + 7.73f},      // 腿3
            {.x = P5X + 0.00f, .y = P5Y + -20.00f, .z = P5Z + 0.00f},     // 腿4
            {.x = P6X + 0.00f, .y = P6Y + 23.78f, .z = P6Z + 7.73f}       // 腿5
        }
    }
};

static const int forward_entries[] = {0, 10};

const MovementTable_t forward_table = {
    .table = forward_path,
    .length = 7,
    .stepDuration = 20,
    .entries = forward_entries,
    .entriesCount = 2
};

// 后退运动数据（直接使用C++版本的backward_paths）
static const Locations_t backward_path[] = {
    // 步骤0: 准备姿势
    {
        .points = {
            {.x = P1X + 0.00f, .y = P1Y + 0.00f, .z = P1Z + 25.00f},      // 腿0
            {.x = P2X + 0.00f, .y = P2Y + 0.00f, .z = P2Z + 0.00f},       // 腿1
            {.x = P3X + 0.00f, .y = P3Y + 0.00f, .z = P3Z + 25.00f},      // 腿2
            {.x = P4X + 0.00f, .y = P4Y + 0.00f, .z = P4Z + 0.00f},       // 腿3
            {.x = P5X + 0.00f, .y = P5Y + 0.00f, .z = P5Z + 25.00f},      // 腿4
            {.x = P6X + 0.00f, .y = P6Y + 0.00f, .z = P6Z + 0.00f}        // 腿5
        }
    },
    // 步骤1: 抬起右前腿和左后腿
    {
        .points = {
            {.x = P1X + 0.00f, .y = P1Y + -7.73f, .z = P1Z + 23.78f},    // 腿0
            {.x = P2X + 0.00f, .y = P2Y + 5.00f, .z = P2Z + 0.00f},       // 腿1
            {.x = P3X + 0.00f, .y = P3Y + -7.73f, .z = P3Z + 23.78f},     // 腿2
            {.x = P4X + 0.00f, .y = P4Y + 5.00f, .z = P4Z + 0.00f},       // 腿3
            {.x = P5X + 0.00f, .y = P5Y + -7.73f, .z = P5Z + 23.78f},     // 腿4
            {.x = P6X + 0.00f, .y = P6Y + 5.00f, .z = P6Z + 0.00f}        // 腿5
        }
    },
    // 步骤2: 移动右前腿和左后腿向后
    {
        .points = {
            {.x = P1X + 0.00f, .y = P1Y + -14.69f, .z = P1Z + 20.23f},   // 腿0
            {.x = P2X + 0.00f, .y = P2Y + 10.00f, .z = P2Z + 0.00f},      // 腿1
            {.x = P3X + 0.00f, .y = P3Y + -14.69f, .z = P3Z + 20.23f},    // 腿2
            {.x = P4X + 0.00f, .y = P4Y + 10.00f, .z = P4Z + 0.00f},      // 腿3
            {.x = P5X + 0.00f, .y = P5Y + -14.69f, .z = P5Z + 20.23f},    // 腿4
            {.x = P6X + 0.00f, .y = P6Y + 10.00f, .z = P6Z + 0.00f}       // 腿5
        }
    },
    // 步骤3: 放下右前腿和左后腿
    {
        .points = {
            {.x = P1X + 0.00f, .y = P1Y + -20.23f, .z = P1Z + 14.69f},   // 腿0
            {.x = P2X + 0.00f, .y = P2Y + 15.00f, .z = P2Z + 0.00f},      // 腿1
            {.x = P3X + 0.00f, .y = P3Y + -20.23f, .z = P3Z + 14.69f},    // 腿2
            {.x = P4X + 0.00f, .y = P4Y + 15.00f, .z = P4Z + 0.00f},      // 腿3
            {.x = P5X + 0.00f, .y = P5Y + -20.23f, .z = P5Z + 14.69f},    // 腿4
            {.x = P6X + 0.00f, .y = P6Y + 15.00f, .z = P6Z + 0.00f}       // 腿5
        }
    },
    // 步骤4: 抬起左前腿和右后腿
    {
        .points = {
            {.x = P1X + 0.00f, .y = P1Y + -23.78f, .z = P1Z + 7.73f},    // 腿0
            {.x = P2X + 0.00f, .y = P2Y + 20.00f, .z = P2Z + 0.00f},      // 腿1
            {.x = P3X + 0.00f, .y = P3Y + -23.78f, .z = P3Z + 7.73f},     // 腿2
            {.x = P4X + 0.00f, .y = P4Y + 20.00f, .z = P4Z + 0.00f},      // 腿3
            {.x = P5X + 0.00f, .y = P5Y + -23.78f, .z = P5Z + 7.73f},     // 腿4
            {.x = P6X + 0.00f, .y = P6Y + 20.00f, .z = P6Z + 0.00f}       // 腿5
        }
    },
    // 步骤5: 移动左前腿和右后腿向后
    {
        .points = {
            {.x = P1X + 0.00f, .y = P1Y + -25.00f, .z = P1Z + 0.00f},    // 腿0
            {.x = P2X + 0.00f, .y = P2Y + 25.00f, .z = P2Z + 0.00f},      // 腿1
            {.x = P3X + 0.00f, .y = P3Y + -25.00f, .z = P3Z + 0.00f},     // 腿2
            {.x = P4X + 0.00f, .y = P4Y + 25.00f, .z = P4Z + 0.00f},      // 腿3
            {.x = P5X + 0.00f, .y = P5Y + -25.00f, .z = P5Z + 0.00f},     // 腿4
            {.x = P6X + 0.00f, .y = P6Y + 25.00f, .z = P6Z + 0.00f}       // 腿5
        }
    },
    // 步骤6: 放下左前腿和右后腿
    {
        .points = {
            {.x = P1X + 0.00f, .y = P1Y + -20.00f, .z = P1Z + 0.00f},    // 腿0
            {.x = P2X + 0.00f, .y = P2Y + 23.78f, .z = P2Z + 7.73f},      // 腿1
            {.x = P3X + 0.00f, .y = P3Y + -20.00f, .z = P3Z + 0.00f},     // 腿2
            {.x = P4X + 0.00f, .y = P4Y + 23.78f, .z = P4Z + 7.73f},      // 腿3
            {.x = P5X + 0.00f, .y = P5Y + -20.00f, .z = P5Z + 0.00f},     // 腿4
            {.x = P6X + 0.00f, .y = P6Y + 23.78f, .z = P6Z + 7.73f}       // 腿5
        }
    }
};

static const int backward_entries[] = {0, 10};

const MovementTable_t backward_table = {
    .table = backward_path,
    .length = 7,
    .stepDuration = 20,
    .entries = backward_entries,
    .entriesCount = 2
};

// 左转运动数据（简化版本）
static const Locations_t turn_left_path[] = {
    // 步骤0: 准备姿势
    {
        .points = {
            {.x = P1X, .y = P1Y, .z = P1Z},      // 腿0
            {.x = P2X, .y = P2Y, .z = P2Z},      // 腿1
            {.x = P3X, .y = P3Y, .z = P3Z},      // 腿2
            {.x = P4X, .y = P4Y, .z = P4Z},      // 腿3
            {.x = P5X, .y = P5Y, .z = P5Z},      // 腿4
            {.x = P6X, .y = P6Y, .z = P6Z}       // 腿5
        }
    },
    // 步骤1: 抬起右前腿和左后腿
    {
        .points = {
            {.x = P1X + 0.00f, .y = P1Y + 0.00f, .z = P1Z + 20.00f},     // 腿0
            {.x = P2X + 0.00f, .y = P2Y + 0.00f, .z = P2Z + 0.00f},      // 腿1
            {.x = P3X + 0.00f, .y = P3Y + 0.00f, .z = P3Z + 0.00f},      // 腿2
            {.x = P4X + 0.00f, .y = P4Y + 0.00f, .z = P4Z + 0.00f},      // 腿3
            {.x = P5X + 0.00f, .y = P5Y + 0.00f, .z = P5Z + 20.00f},     // 腿4
            {.x = P6X + 0.00f, .y = P6Y + 0.00f, .z = P6Z + 0.00f}       // 腿5
        }
    },
    // 步骤2: 移动右前腿和左后腿向左
    {
        .points = {
            {.x = P1X + 0.00f, .y = P1Y + 30.00f, .z = P1Z + 20.00f},    // 腿0
            {.x = P2X + 0.00f, .y = P2Y + 0.00f, .z = P2Z + 0.00f},      // 腿1
            {.x = P3X + 0.00f, .y = P3Y + 0.00f, .z = P3Z + 0.00f},      // 腿2
            {.x = P4X + 0.00f, .y = P4Y + 0.00f, .z = P4Z + 0.00f},      // 腿3
            {.x = P5X + 0.00f, .y = P5Y + 30.00f, .z = P5Z + 20.00f},    // 腿4
            {.x = P6X + 0.00f, .y = P6Y + 0.00f, .z = P6Z + 0.00f}       // 腿5
        }
    },
    // 步骤3: 放下右前腿和左后腿
    {
        .points = {
            {.x = P1X + 0.00f, .y = P1Y + 30.00f, .z = P1Z + 0.00f},     // 腿0
            {.x = P2X + 0.00f, .y = P2Y + 0.00f, .z = P2Z + 0.00f},      // 腿1
            {.x = P3X + 0.00f, .y = P3Y + 0.00f, .z = P3Z + 0.00f},      // 腿2
            {.x = P4X + 0.00f, .y = P4Y + 0.00f, .z = P4Z + 0.00f},      // 腿3
            {.x = P5X + 0.00f, .y = P5Y + 30.00f, .z = P5Z + 0.00f},     // 腿4
            {.x = P6X + 0.00f, .y = P6Y + 0.00f, .z = P6Z + 0.00f}       // 腿5
        }
    }
};

static const int turn_left_entries[] = {0, 1};

const MovementTable_t turn_left_table = {
    .table = turn_left_path,
    .length = 4,
    .stepDuration = 20,
    .entries = turn_left_entries,
    .entriesCount = 2
};

// 右转运动数据（简化版本）
static const Locations_t turn_right_path[] = {
    // 步骤0: 准备姿势
    {
        .points = {
            {.x = P1X, .y = P1Y, .z = P1Z},      // 腿0
            {.x = P2X, .y = P2Y, .z = P2Z},      // 腿1
            {.x = P3X, .y = P3Y, .z = P3Z},      // 腿2
            {.x = P4X, .y = P4Y, .z = P4Z},      // 腿3
            {.x = P5X, .y = P5Y, .z = P5Z},      // 腿4
            {.x = P6X, .y = P6Y, .z = P6Z}       // 腿5
        }
    },
    // 步骤1: 抬起右前腿和左后腿
    {
        .points = {
            {.x = P1X + 0.00f, .y = P1Y + 0.00f, .z = P1Z + 20.00f},     // 腿0
            {.x = P2X + 0.00f, .y = P2Y + 0.00f, .z = P2Z + 0.00f},      // 腿1
            {.x = P3X + 0.00f, .y = P3Y + 0.00f, .z = P3Z + 0.00f},      // 腿2
            {.x = P4X + 0.00f, .y = P4Y + 0.00f, .z = P4Z + 0.00f},      // 腿3
            {.x = P5X + 0.00f, .y = P5Y + 0.00f, .z = P5Z + 20.00f},     // 腿4
            {.x = P6X + 0.00f, .y = P6Y + 0.00f, .z = P6Z + 0.00f}       // 腿5
        }
    },
    // 步骤2: 移动右前腿和左后腿向右
    {
        .points = {
            {.x = P1X + 0.00f, .y = P1Y + -30.00f, .z = P1Z + 20.00f},   // 腿0
            {.x = P2X + 0.00f, .y = P2Y + 0.00f, .z = P2Z + 0.00f},      // 腿1
            {.x = P3X + 0.00f, .y = P3Y + 0.00f, .z = P3Z + 0.00f},      // 腿2
            {.x = P4X + 0.00f, .y = P4Y + 0.00f, .z = P4Z + 0.00f},      // 腿3
            {.x = P5X + 0.00f, .y = P5Y + -30.00f, .z = P5Z + 20.00f},   // 腿4
            {.x = P6X + 0.00f, .y = P6Y + 0.00f, .z = P6Z + 0.00f}       // 腿5
        }
    },
    // 步骤3: 放下右前腿和左后腿
    {
        .points = {
            {.x = P1X + 0.00f, .y = P1Y + -30.00f, .z = P1Z + 0.00f},    // 腿0
            {.x = P2X + 0.00f, .y = P2Y + 0.00f, .z = P2Z + 0.00f},      // 腿1
            {.x = P3X + 0.00f, .y = P3Y + 0.00f, .z = P3Z + 0.00f},      // 腿2
            {.x = P4X + 0.00f, .y = P4Y + 0.00f, .z = P4Z + 0.00f},      // 腿3
            {.x = P5X + 0.00f, .y = P5Y + -30.00f, .z = P5Z + 0.00f},    // 腿4
            {.x = P6X + 0.00f, .y = P6Y + 0.00f, .z = P6Z + 0.00f}       // 腿5
        }
    }
};

static const int turn_right_entries[] = {0, 1};

const MovementTable_t turn_right_table = {
    .table = turn_right_path,
    .length = 4,
    .stepDuration = 20,
    .entries = turn_right_entries,
    .entriesCount = 2
};

// 攀爬运动数据（简化版本）
static const Locations_t climb_path[] = {
    // 步骤0: 准备姿势
    {
        .points = {
            {.x = P1X, .y = P1Y, .z = P1Z},      // 腿0
            {.x = P2X, .y = P2Y, .z = P2Z},      // 腿1
            {.x = P3X, .y = P3Y, .z = P3Z},      // 腿2
            {.x = P4X, .y = P4Y, .z = P4Z},      // 腿3
            {.x = P5X, .y = P5Y, .z = P5Z},      // 腿4
            {.x = P6X, .y = P6Y, .z = P6Z}       // 腿5
        }
    },
    // 步骤1: 抬起所有腿
    {
        .points = {
            {.x = P1X + 0.00f, .y = P1Y + 0.00f, .z = P1Z + 20.00f},     // 腿0
            {.x = P2X + 0.00f, .y = P2Y + 0.00f, .z = P2Z + 20.00f},     // 腿1
            {.x = P3X + 0.00f, .y = P3Y + 0.00f, .z = P3Z + 20.00f},     // 腿2
            {.x = P4X + 0.00f, .y = P4Y + 0.00f, .z = P4Z + 20.00f},     // 腿3
            {.x = P5X + 0.00f, .y = P5Y + 0.00f, .z = P5Z + 20.00f},     // 腿4
            {.x = P6X + 0.00f, .y = P6Y + 0.00f, .z = P6Z + 20.00f}      // 腿5
        }
    },
    // 步骤2: 移动所有腿向上
    {
        .points = {
            {.x = P1X + 0.00f, .y = P1Y + 0.00f, .z = P1Z + 40.00f},     // 腿0
            {.x = P2X + 0.00f, .y = P2Y + 0.00f, .z = P2Z + 40.00f},     // 腿1
            {.x = P3X + 0.00f, .y = P3Y + 0.00f, .z = P3Z + 40.00f},     // 腿2
            {.x = P4X + 0.00f, .y = P4Y + 0.00f, .z = P4Z + 40.00f},     // 腿3
            {.x = P5X + 0.00f, .y = P5Y + 0.00f, .z = P5Z + 40.00f},     // 腿4
            {.x = P6X + 0.00f, .y = P6Y + 0.00f, .z = P6Z + 40.00f}      // 腿5
        }
    },
    // 步骤3: 放下所有腿
    {
        .points = {
            {.x = P1X + 0.00f, .y = P1Y + 0.00f, .z = P1Z + 40.00f},     // 腿0
            {.x = P2X + 0.00f, .y = P2Y + 0.00f, .z = P2Z + 40.00f},     // 腿1
            {.x = P3X + 0.00f, .y = P3Y + 0.00f, .z = P3Z + 40.00f},     // 腿2
            {.x = P4X + 0.00f, .y = P4Y + 0.00f, .z = P4Z + 40.00f},     // 腿3
            {.x = P5X + 0.00f, .y = P5Y + 0.00f, .z = P5Z + 40.00f},     // 腿4
            {.x = P6X + 0.00f, .y = P6Y + 0.00f, .z = P6Z + 40.00f}      // 腿5
        }
    }
};

static const int climb_entries[] = {0, 1};

const MovementTable_t climb_table = {
    .table = climb_path,
    .length = 4,
    .stepDuration = 50,
    .entries = climb_entries,
    .entriesCount = 2
};

// 下降运动数据（简化版本）
static const Locations_t descend_path[] = {
    // 步骤0: 准备姿势
    {
        .points = {
            {.x = P1X + 0.00f, .y = P1Y + 0.00f, .z = P1Z + 40.00f},     // 腿0
            {.x = P2X + 0.00f, .y = P2Y + 0.00f, .z = P2Z + 40.00f},     // 腿1
            {.x = P3X + 0.00f, .y = P3Y + 0.00f, .z = P3Z + 40.00f},     // 腿2
            {.x = P4X + 0.00f, .y = P4Y + 0.00f, .z = P4Z + 40.00f},     // 腿3
            {.x = P5X + 0.00f, .y = P5Y + 0.00f, .z = P5Z + 40.00f},     // 腿4
            {.x = P6X + 0.00f, .y = P6Y + 0.00f, .z = P6Z + 40.00f}      // 腿5
        }
    },
    // 步骤1: 抬起所有腿
    {
        .points = {
            {.x = P1X + 0.00f, .y = P1Y + 0.00f, .z = P1Z + 60.00f},     // 腿0
            {.x = P2X + 0.00f, .y = P2Y + 0.00f, .z = P2Z + 60.00f},     // 腿1
            {.x = P3X + 0.00f, .y = P3Y + 0.00f, .z = P3Z + 60.00f},     // 腿2
            {.x = P4X + 0.00f, .y = P4Y + 0.00f, .z = P4Z + 60.00f},     // 腿3
            {.x = P5X + 0.00f, .y = P5Y + 0.00f, .z = P5Z + 60.00f},     // 腿4
            {.x = P6X + 0.00f, .y = P6Y + 0.00f, .z = P6Z + 60.00f}      // 腿5
        }
    },
    // 步骤2: 移动所有腿向下
    {
        .points = {
            {.x = P1X + 0.00f, .y = P1Y + 0.00f, .z = P1Z + 0.00f},      // 腿0
            {.x = P2X + 0.00f, .y = P2Y + 0.00f, .z = P2Z + 0.00f},      // 腿1
            {.x = P3X + 0.00f, .y = P3Y + 0.00f, .z = P3Z + 0.00f},      // 腿2
            {.x = P4X + 0.00f, .y = P4Y + 0.00f, .z = P4Z + 0.00f},      // 腿3
            {.x = P5X + 0.00f, .y = P5Y + 0.00f, .z = P5Z + 0.00f},      // 腿4
            {.x = P6X + 0.00f, .y = P6Y + 0.00f, .z = P6Z + 0.00f}       // 腿5
        }
    },
    // 步骤3: 放下所有腿
    {
        .points = {
            {.x = P1X + 0.00f, .y = P1Y + 0.00f, .z = P1Z + 0.00f},      // 腿0
            {.x = P2X + 0.00f, .y = P2Y + 0.00f, .z = P2Z + 0.00f},      // 腿1
            {.x = P3X + 0.00f, .y = P3Y + 0.00f, .z = P3Z + 0.00f},      // 腿2
            {.x = P4X + 0.00f, .y = P4Y + 0.00f, .z = P4Z + 0.00f},      // 腿3
            {.x = P5X + 0.00f, .y = P5Y + 0.00f, .z = P5Z + 0.00f},      // 腿4
            {.x = P6X + 0.00f, .y = P6Y + 0.00f, .z = P6Z + 0.00f}       // 腿5
        }
    }
};

static const int descend_entries[] = {0, 1};

const MovementTable_t descend_table = {
    .table = descend_path,
    .length = 4,
    .stepDuration = 50,
    .entries = descend_entries,
    .entriesCount = 2
};

// 快速左转运动数据（简化版本）
static const Locations_t turn_left_fast_path[] = {
    // 步骤0: 准备姿势
    {
        .points = {
            {.x = P1X, .y = P1Y, .z = P1Z},      // 腿0
            {.x = P2X, .y = P2Y, .z = P2Z},      // 腿1
            {.x = P3X, .y = P3Y, .z = P3Z},      // 腿2
            {.x = P4X, .y = P4Y, .z = P4Z},      // 腿3
            {.x = P5X, .y = P5Y, .z = P5Z},      // 腿4
            {.x = P6X, .y = P6Y, .z = P6Z}       // 腿5
        }
    },
    // 步骤1: 抬起右前腿和左后腿
    {
        .points = {
            {.x = P1X + 0.00f, .y = P1Y + 0.00f, .z = P1Z + 20.00f},     // 腿0
            {.x = P2X + 0.00f, .y = P2Y + 0.00f, .z = P2Z + 0.00f},      // 腿1
            {.x = P3X + 0.00f, .y = P3Y + 0.00f, .z = P3Z + 0.00f},      // 腿2
            {.x = P4X + 0.00f, .y = P4Y + 0.00f, .z = P4Z + 0.00f},      // 腿3
            {.x = P5X + 0.00f, .y = P5Y + 0.00f, .z = P5Z + 20.00f},     // 腿4
            {.x = P6X + 0.00f, .y = P6Y + 0.00f, .z = P6Z + 0.00f}       // 腿5
        }
    },
    // 步骤2: 移动右前腿和左后腿向左
    {
        .points = {
            {.x = P1X + 0.00f, .y = P1Y + 50.00f, .z = P1Z + 20.00f},    // 腿0
            {.x = P2X + 0.00f, .y = P2Y + 0.00f, .z = P2Z + 0.00f},      // 腿1
            {.x = P3X + 0.00f, .y = P3Y + 0.00f, .z = P3Z + 0.00f},      // 腿2
            {.x = P4X + 0.00f, .y = P4Y + 0.00f, .z = P4Z + 0.00f},      // 腿3
            {.x = P5X + 0.00f, .y = P5Y + 50.00f, .z = P5Z + 20.00f},    // 腿4
            {.x = P6X + 0.00f, .y = P6Y + 0.00f, .z = P6Z + 0.00f}       // 腿5
        }
    },
    // 步骤3: 放下右前腿和左后腿
    {
        .points = {
            {.x = P1X + 0.00f, .y = P1Y + 50.00f, .z = P1Z + 0.00f},     // 腿0
            {.x = P2X + 0.00f, .y = P2Y + 0.00f, .z = P2Z + 0.00f},      // 腿1
            {.x = P3X + 0.00f, .y = P3Y + 0.00f, .z = P3Z + 0.00f},      // 腿2
            {.x = P4X + 0.00f, .y = P4Y + 0.00f, .z = P4Z + 0.00f},      // 腿3
            {.x = P5X + 0.00f, .y = P5Y + 50.00f, .z = P5Z + 0.00f},     // 腿4
            {.x = P6X + 0.00f, .y = P6Y + 0.00f, .z = P6Z + 0.00f}       // 腿5
        }
    }
};

static const int turn_left_fast_entries[] = {0, 1};

const MovementTable_t turn_left_fast_table = {
    .table = turn_left_fast_path,
    .length = 4,
    .stepDuration = 10,
    .entries = turn_left_fast_entries,
    .entriesCount = 2
};

// 快速右转运动数据（简化版本）
static const Locations_t turn_right_fast_path[] = {
    // 步骤0: 准备姿势
    {
        .points = {
            {.x = P1X, .y = P1Y, .z = P1Z},      // 腿0
            {.x = P2X, .y = P2Y, .z = P2Z},      // 腿1
            {.x = P3X, .y = P3Y, .z = P3Z},      // 腿2
            {.x = P4X, .y = P4Y, .z = P4Z},      // 腿3
            {.x = P5X, .y = P5Y, .z = P5Z},      // 腿4
            {.x = P6X, .y = P6Y, .z = P6Z}       // 腿5
        }
    },
    // 步骤1: 抬起右前腿和左后腿
    {
        .points = {
            {.x = P1X + 0.00f, .y = P1Y + 0.00f, .z = P1Z + 20.00f},     // 腿0
            {.x = P2X + 0.00f, .y = P2Y + 0.00f, .z = P2Z + 0.00f},      // 腿1
            {.x = P3X + 0.00f, .y = P3Y + 0.00f, .z = P3Z + 0.00f},      // 腿2
            {.x = P4X + 0.00f, .y = P4Y + 0.00f, .z = P4Z + 0.00f},      // 腿3
            {.x = P5X + 0.00f, .y = P5Y + 0.00f, .z = P5Z + 20.00f},     // 腿4
            {.x = P6X + 0.00f, .y = P6Y + 0.00f, .z = P6Z + 0.00f}       // 腿5
        }
    },
    // 步骤2: 移动右前腿和左后腿向右
    {
        .points = {
            {.x = P1X + 0.00f, .y = P1Y + -50.00f, .z = P1Z + 20.00f},   // 腿0
            {.x = P2X + 0.00f, .y = P2Y + 0.00f, .z = P2Z + 0.00f},      // 腿1
            {.x = P3X + 0.00f, .y = P3Y + 0.00f, .z = P3Z + 0.00f},      // 腿2
            {.x = P4X + 0.00f, .y = P4Y + 0.00f, .z = P4Z + 0.00f},      // 腿3
            {.x = P5X + 0.00f, .y = P5Y + -50.00f, .z = P5Z + 20.00f},   // 腿4
            {.x = P6X + 0.00f, .y = P6Y + 0.00f, .z = P6Z + 0.00f}       // 腿5
        }
    },
    // 步骤3: 放下右前腿和左后腿
    {
        .points = {
            {.x = P1X + 0.00f, .y = P1Y + -50.00f, .z = P1Z + 0.00f},    // 腿0
            {.x = P2X + 0.00f, .y = P2Y + 0.00f, .z = P2Z + 0.00f},      // 腿1
            {.x = P3X + 0.00f, .y = P3Y + 0.00f, .z = P3Z + 0.00f},      // 腿2
            {.x = P4X + 0.00f, .y = P4Y + 0.00f, .z = P4Z + 0.00f},      // 腿3
            {.x = P5X + 0.00f, .y = P5Y + -50.00f, .z = P5Z + 0.00f},    // 腿4
            {.x = P6X + 0.00f, .y = P6Y + 0.00f, .z = P6Z + 0.00f}       // 腿5
        }
    }
};

static const int turn_right_fast_entries[] = {0, 1};

const MovementTable_t turn_right_fast_table = {
    .table = turn_right_fast_path,
    .length = 4,
    .stepDuration = 10,
    .entries = turn_right_fast_entries,
    .entriesCount = 2
};

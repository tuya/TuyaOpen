#include "servo.h"
#include "tkl_pwm.h"
#include "tal_thread.h"
#include "tal_log.h"   
#include <stdlib.h>   
#include <time.h>     
#include "OLED.h"

STATIC THREAD_HANDLE g_shake_thread = NULL;   // 摇头线程句柄
STATIC BOOL_T        g_shake_enable = TRUE;   

#define SERVO_PWM_FREQUENCY        50      // 舵机频率固定为50Hz
#define SERVO_MIN_DUTY             250     // 0.5ms脉冲宽度（2.5%占空比）
#define SERVO_MAX_DUTY             1250    // 2.5ms脉冲宽度（12.5%占空比）
#define SERVO_DUTY_70_DEGREE             639     // 0.5ms脉冲宽度（2.5%占空比）
#define SERVO_MID_DUTY                   750   // 90°（1.5ms）← 这是中值

//摇头的定义
#define SHAKE_CYCLES        3       /* 左右来回次数 */
#define SHAKE_ANGLE_LEFT    30      /* 最左角度 */
#define SHAKE_ANGLE_RIGHT   150     /* 最右角度 */

//点头的定义
#define NOD_CYCLES        3         /* 上下来回次数 */
#define NOD_ANGLE_UP    30      /* 最上角度 */
#define NOD_ANGLE_DOWN   90     /* 最下角度 */

typedef enum {
    ACT_SHAKE = 0,
    ACT_NOD,
    ACT_LOOK_LEFT,
    ACT_LOOK_RIGHT,
    ACT_LOOK_UP,
    ACT_LOOK_DOWN,
    ACT_MAX_NUM,
    ACT_STOP
} SERVO_ACT_E;

/* 动作名称（调试用） */
STATIC CONST CHAR_T *act_name[ACT_MAX_NUM] = {
    "shake", "nod", "left", "right", "up", "down"
};

// 舵机配置结构体
typedef struct {
    TUYA_PWM_NUM_E pwm_id;    // PWM通道（如TUYA_PWM_NUM_0）
    UINT32_T min_duty;        // 最小占空比（对应0°）
    UINT32_T max_duty;        // 最大占空比（对应180°）
    UINT32_T current_duty;    // 当前占空比（用于状态跟踪）
    BOOL_T reverse_polarity;  // 是否反转极性（某些舵机可能需要）
} SERVO_CFG_T;

// 全局舵机配置数组（根据实际硬件连接修改）
STATIC SERVO_CFG_T sg_servo_cfg[SERVO_NUM] = {

    //这里由于装机问题，舵机可能有装反的情况，需要看情况设置 ，具体其他PWMIO口看servo_ctrl.c的结构体
    {TUYA_PWM_NUM_0, SERVO_MIN_DUTY, SERVO_MAX_DUTY, SERVO_MID_DUTY, TRUE}, // 0上舵机  <PIN18 
    {TUYA_PWM_NUM_1, SERVO_MIN_DUTY, SERVO_MAX_DUTY, SERVO_DUTY_70_DEGREE, TRUE}, // 1下舵机  <PIN24  ID4
    
};

STATIC THREAD_HANDLE sg_pwm_handle;
/**
 * @brief 舵机初始化
 * @return OPERATE_RET: 0 成功，其余错误码表示失败
 */
OPERATE_RET servo_hardware_init(void)
{
    TAL_PR_NOTICE("Servo hardware init start");
    OPERATE_RET rt = OPRT_OK;
    static uint32_t count = 0;
    for (int i = 0; i < SERVO_NUM; i++) {

            TUYA_PWM_BASE_CFG_T pwm_cfg = {
                .cycle = 20000000, // 20ms周期，原来是20000
                .count_mode = TUYA_PWM_CNT_UP,
                .duty = sg_servo_cfg[i].current_duty,
                .frequency = SERVO_PWM_FREQUENCY,
                .polarity = sg_servo_cfg[i].reverse_polarity ? 
                            TUYA_PWM_NEGATIVE : TUYA_PWM_POSITIVE,
        };

        
        TUYA_CALL_ERR_GOTO(tkl_pwm_init(sg_servo_cfg[i].pwm_id, &pwm_cfg), __error);
        TUYA_CALL_ERR_GOTO(tkl_pwm_start(sg_servo_cfg[i].pwm_id), __error);
    }
    TAL_PR_NOTICE("All servos initialized");

    //robot_param_init(); 
    //home_xy();

    return rt;

__error:
    for (int i = 0; i < SERVO_NUM; i++) {
        tkl_pwm_stop(sg_servo_cfg[i].pwm_id);
    }
    tal_thread_delete(sg_pwm_handle);
    TAL_PR_NOTICE("Servo task exited");
    //初始化随机种子
    srand((unsigned)tal_system_get_tick_count());   // 用系统 tick 做种子

    return rt;
}

/**
 * @brief 设置舵机PWM值
 * @param[in/out] servo_id: 
 * @param[in/out] pwm: 
 * @return STATIC: 0 成功，其余错误码表示失败
 */

STATIC VOID servo_pwm_set(UINT32_T pwm0, UINT32_T pwm1, UINT32_T pwm2)
{
    OPERATE_RET rt = OPRT_OK;

    for (int i = 0; i < SERVO_NUM; i++) {
        SERVO_CFG_T* servo = &sg_servo_cfg[i];
        
#ifdef SERVO_TEST_PENDULUM
        if(servo->current_duty < 600) {
        servo->current_duty = 900;
             } else { 
        servo->current_duty = 600;
             }
#endif

        //if(servo->current_duty < 600) {
        //    servo->current_duty = 900;
        //} else {
         //   servo->current_duty = 600;
        //}

        // 添加范围限制
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
        TAL_PR_DEBUG("Servo[%d] set duty: %d", i, servo->current_duty);
    }
}

// 设置舵机角度（0-180度）
OPERATE_RET servo_set_angle(int servo_id, float angle) {
    SERVO_CFG_T *servo = &sg_servo_cfg[servo_id];
    
    // 角度范围限制
    angle = (angle < 0) ? 0 : (angle > 180) ? 180 : angle;
    
    // 计算占空比
    UINT_T duty = servo->min_duty + (UINT_T)((angle / 180.0f) * 
                      (servo->max_duty - servo->min_duty));

    // 更新PWM配置
    TUYA_PWM_BASE_CFG_T cfg = {
        .polarity = servo->reverse_polarity ? TUYA_PWM_NEGATIVE : TUYA_PWM_POSITIVE,
        .duty = duty,
        .cycle = 20000000, // 20ms周期20000
        .frequency = 50
    };

    OPERATE_RET ret = tkl_pwm_info_set(servo->pwm_id, &cfg);
    if (OPRT_OK != ret) {
        TAL_PR_ERR("Servo[%d] set duty %d failed! Err:%d", 
                  servo_id, duty, ret);
        return ret;
    }

    // 重新启动PWM应用新配置
    //return tkl_pwm_start(servo->pwm_id);
}

//舵机平滑转动
//body身体运动
void servo_bodysmoothmove(int8_t startangle, int8_t endangle ,int16_t duration_ms , int8_t steps)
{
    for(int i = 0 ; i <= steps; i++)
    {
        float t = (float)i / steps;
        int8_t angle = startangle + (endangle - startangle) * t;
        servo_set_angle(1 ,angle);
        tal_system_sleep(duration_ms / steps);
    }
    
}

void servo_headsmoothmove(int8_t startangle, int8_t endangle ,int16_t duration_ms , int8_t steps)
{
    for(int i = 0 ; i <= steps; i++)
    {
        float t = (float)i / steps;
        int8_t angle = startangle + (endangle - startangle) * t;
        servo_set_angle(0 ,angle);
        tal_system_sleep(duration_ms / steps);
    }
    
}

void servo_action_nod(void)
{
    /* 下舵机固定到 90°，保证只点头不摇头 */
    //servo_set_angle(1, 90);
    tal_system_sleep(150);

    for (int i = 0; i < NOD_CYCLES; i++)
    {
        /* 中 → 上 */
        servo_headsmoothmove(10, 30, 500, 50);
        //tal_system_sleep(300);

        /* 中 → 下 */
        servo_headsmoothmove(30, 10, 500, 50);
        //tal_system_sleep(300);

        /* 中 → 上 */
        //servo_headsmoothmove(30, 0, 500, 50);
        //tal_system_sleep(300);

        //servo_headsmoothmove(90, 20, 300, 50);
        //tal_system_sleep(300);

    }

    /* 最后多停一会儿，让动作看起来完整 */
    tal_system_sleep(200);

}

void servo_action_shake(void)
{
    /* 上舵机固定到 90°，保证只摇头不点头 */
    //servo_set_angle(0, 90);
    tal_system_sleep(150);

    for (int i = 0; i < SHAKE_CYCLES; i++)
    {
        /* left → right */
        servo_bodysmoothmove(30, 90 ,500, 50);
        //tal_system_sleep(300);


        /* right → left */
        servo_bodysmoothmove(90, 30 ,500, 50);
        //tal_system_sleep(300);

        /* left → right */
        //servo_bodysmoothmove(30, 90 ,300, 50);
        //tal_system_sleep(300);

    }

    /* 最后多停一会儿，让动作看起来完整 */
    tal_system_sleep(200);
}
void head_down(void)
{

    servo_headsmoothmove(20, 40, 400, 50);//朝下看
    tal_system_sleep(300);

}

void head_up(void)
{

    servo_headsmoothmove(30, 10, 400, 50);//朝上看
    tal_system_sleep(300);

}

void head_left(void)
{

    servo_bodysmoothmove(90, 60 ,500, 50);//left
    tal_system_sleep(300);

}

void head_right(void)
{

    servo_bodysmoothmove(90, 120 ,500, 50);//right
    tal_system_sleep(300);

}



/* 执行一次“随机动作” */
STATIC VOID servo_do_random_action(VOID)
{
    SERVO_ACT_E act = rand() % ACT_MAX_NUM;
    TAL_PR_DEBUG("[servo] do action: %s", act_name[act]);

    switch (act) 
    {
    case ACT_SHAKE:      
    servo_action_shake(); 
    break;
    case ACT_NOD:        
    servo_action_nod(); 
    break;
    case ACT_LOOK_LEFT:  
    head_left(); 
    break;
    case ACT_LOOK_RIGHT:
     head_right(); 
     break;
    case ACT_LOOK_UP:    
    head_up(); 
    break;
    case ACT_LOOK_DOWN: 
     head_down(); 
     break;
    case ACT_STOP:
    tal_system_sleep(3000);
    break;
    
    default: break;
    }
}

void servo_action_shake_loop(void)
{
    if (!g_shake_enable) return;
    servo_do_random_action();   

}

/* 摇头线程入口 */
VOID_T servo_shake_thread(VOID_T *arg)
{
    while (g_shake_enable) {
        servo_action_shake_loop();         // 摇一次
        tal_system_sleep(2000);            // 每 2 s 摇一次
    }
    g_shake_thread = NULL;                 // 线程即将结束
    tal_thread_delete(NULL);               
}

/* 启动摇头线程 */
VOID_T servo_shake_start(void)
{
    if (g_shake_thread != NULL) return;    // 已经跑起来了

    THREAD_CFG_T cfg = {
        .stackDepth = 2048,
        .priority   = THREAD_PRIO_3,
        .thrdname   = "shake_task"
    };
    g_shake_enable = TRUE;
    tal_thread_create_and_start(&g_shake_thread, NULL, NULL,
                               servo_shake_thread, NULL, &cfg);
}

/* 停止线程 */
void servo_shake_stop(void)
{
    g_shake_enable = FALSE;   // 线程里下一次判断就会自然退出
}
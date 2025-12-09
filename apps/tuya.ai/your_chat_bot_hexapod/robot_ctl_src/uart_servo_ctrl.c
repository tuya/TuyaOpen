#include "tal_uart.h"
#include "tal_system.h"
#include "tal_log.h"
#include "tuya_cloud_types.h"
#include <stdio.h>
#include <stdlib.h>
#include "tal_workq_service.h"
#include "move_ctrl.h"




static volatile UINT8_T local_status = MOVE_STATE_IDLE;
static volatile UINT32_T move_step = 3; //每次移动的步数
static volatile UINT32_T curr_step = 3; //每次移动的步数
static const UINT32_T default_move_step = 1; //默认每次移动的步数
#define UART_BOUND 115200u
// STATIC THREAD_HANDLE my_uart_thread = NULL;
UINT16_T servo_status_list[10][18]={0}; //舵机状态数组
static BOOL_T is_obstacle = FALSE;

typedef struct {
    UINT8_T  state;
    UINT32_T step;
}move_state_info_t;

//==================================================================================================//


void exute_robot_movement(void *arg);

//===================================================================================================//



OPERATE_RET read_all_servo_status(UINT8_T idx)
{
    INT_T i = 0;
    char send_buf[32]={0};
    char read_buf[32]={0};
    memset(send_buf,0,sizeof(send_buf));
    for(i=1;i<=18;i++)
    {
        sprintf(send_buf,"#%03dPRAD!",i);
        tal_uart_write(TUYA_UART_NUM_0,(const uint8_t *)send_buf,strlen(send_buf));
        PR_DEBUG("send_buf:%s",send_buf);
        tal_system_sleep(50);
        char cnt=0;
        //判断读取超时时间
        while(tal_uart_get_rx_data_size(TUYA_UART_NUM_0)<10){
            cnt++;
            if(cnt>5){
                break;
            }
            tal_system_sleep(10);
        }
        if(cnt>5) return OPRT_TIMEOUT;
        (void)tal_uart_read(TUYA_UART_NUM_0,(uint8_t *)read_buf,sizeof(read_buf));
        //正确的返回字符串如：#010P1480!
        if(read_buf[0]!='#'||read_buf[9]!='!'||read_buf[4]!='P')
        {
            PR_DEBUG("data err buff:%s",read_buf);
            return OPRT_COM_ERROR;
        }

        servo_status_list[idx][i-1]=atoi(&read_buf[5]);
        PR_DEBUG("read_buf:%s,servo status:%d",read_buf,servo_status_list[idx][i-1]);
    }

    return OPRT_OK;
}

void ctrl_single_servo(UINT8_T id,UINT32_T pwm_duty,UINT32_T time)
{
    char send_buf[32]={0};
    memset(send_buf,0,sizeof(send_buf));
    if(id<1||id>18)
    {
        PR_DEBUG("id err");
        return;
    }
    if(pwm_duty<500 || pwm_duty>2500)
    {
        PR_DEBUG("pwm_duty err");
        return;
    }
    if(time<1||time>9999)
    {
        PR_DEBUG("time err");
        return;
    }
    sprintf(send_buf,"#%03dP%04dT%04d!",id,pwm_duty,time);
    
    
    tal_uart_write(TUYA_UART_NUM_0,(const uint8_t *)send_buf,strlen(send_buf));
    PR_DEBUG("ctrl_single_servo:id:%d,pwm_duty:%d,time:%d",id,pwm_duty,time);
}

void release_all_servo_power(VOID)
{
    char send_buf[] ="#255PULK!";
    tal_uart_write(TUYA_UART_NUM_0,(const uint8_t *)send_buf,strlen(send_buf));
    PR_DEBUG("release_all_servo_power");
}

OPERATE_RET send_move_step_cmd_table(const char **cmd_table,int table_len)
{
    OPERATE_RET rt = OPRT_OK;
    int i = 0;

    for(i=0;i<table_len;i++)
    {
        
        rt =tal_uart_write(TUYA_UART_NUM_0,(const uint8_t *)cmd_table[i],strlen(cmd_table[i]));
        if(rt < 0)
        {
            PR_DEBUG("send move cmd err:%d",rt);
            return rt;
        }
        // PR_DEBUG("send move cmd:%s",cmd_table[i]);
        tal_system_sleep(80);    
    }

    return rt;
}

void robort_movement(VOID  *arg)
{
    move_state_info_t *move_info = (move_state_info_t *)arg;
    while(move_info->step >0)
    {
        send_move_step_cmd_table(move_front_table,move_front_table_size);
        move_info->step--;
        tal_system_sleep(100);    
    }
    is_obstacle = FALSE;
    PR_DEBUG("=======move step set end=======");
}


/**
 * @brief: 机器人移动状态控制
 * @param: status: 移动状态
 * @return: none
 */
void robot_move_status_ctl(UINT8_T status)
{
    PR_DEBUG("=======robot move status ctl:%s=======", move_statu_name[status]);

    switch (status)
    {
        case MOVE_STATE_FORWARD:
            send_move_step_cmd_table(move_front_table,move_front_table_size); 
            break;
        case MOVE_STATE_BACKWARD:
            send_move_step_cmd_table(move_back_table,move_back_table_size);
            break;
        case MOVE_STATE_TURN_LEFT:
            send_move_step_cmd_table(__turn_left_table,__turn_left_table_size);
        break;
        case MOVE_STATE_TURN_RIGHT:
            send_move_step_cmd_table(__turn_right_table,__turn_right_table_size);
        break;
        case MOVE_STATE_DANCE:{
            char *step1=  "{#012P1041T0300!#011P1485T0300!#010P1596T0300!#015P1107T0300!#014P1433T0300!#013P1744T0300!#009P1144T0300!#008P1493T0300!#007P1581T0300!#018P1144T0300!#017P1493T0300!#016P1581T0300!#006P1107T0300!#005P1433T0300!#004P1744T0300!#003P1041T0300!#002P1485T0300!#001P1596T0300!}";
            char *step2 = "{#012P1900T0300!#011P1426T0300!#010P1767T0300!#015P1967T0300!#014P1485T0300!#013P1611T0300!#009P1870T0300!#008P1493T0300!#007P1581T0300!#018P1870T0300!#017P1493T0300!#016P1581T0300!#006P1967T0300!#005P1485T0300!#004P1611T0300!#003P1900T0300!#002P1426T0300!#001P1767T0300!}";
            char *step3 = "{#001P1500T0300!#002P1500T0300!#003P1500T0300!#004P1500T0300!#005P1500T0300!#006P1500T0300!#007P1500T0300!#008P1500T0300!#009P1500T0300!#010P1500T0300!#011P1500T0300!#012P1500T0300!#013P1500T0300!#014P1500T0300!#015P1500T0300!#016P1500T0300!#017P1500T0300!#018P1500T0300!}";
            char *step4 = "{#012P1500T0300!#011P1352T0300!#010P1663T0300!#015P1500T0300!#014P1352T0300!#013P1663T0300!#009P1500T0300!#008P1352T0300!#007P1663T0300!#018P1500T0300!#017P1352T0300!#016P1663T0300!#006P1500T0300!#005P1352T0300!#004P1663T0300!#003P1500T0300!#002P1352T0300!#001P1663T0300!}";
            char *step5 = "{#012P1500T0300!#011P1648T0300!#010P1367T0300!#015P1500T0300!#014P1648T0300!#013P1367T0300!#009P1500T0300!#008P1648T0300!#007P1367T0300!#018P1500T0300!#017P1648T0300!#016P1367T0300!#006P1500T0300!#005P1648T0300!#004P1367T0300!#003P1500T0300!#002P1648T0300!#001P1367T0300!}";
            
            tal_uart_write(TUYA_UART_NUM_0,(const uint8_t *)step1,strlen(step1));
            tal_system_sleep(1000);
            tal_uart_write(TUYA_UART_NUM_0,(const uint8_t *)step2,strlen(step2));
            tal_system_sleep(1000);
            tal_uart_write(TUYA_UART_NUM_0,(const uint8_t *)step3,strlen(step3));
            tal_system_sleep(500);
            tal_uart_write(TUYA_UART_NUM_0,(const uint8_t *)step4,strlen(step4));
            tal_system_sleep(1000);
            tal_uart_write(TUYA_UART_NUM_0,(const uint8_t *)step5,strlen(step5));
            tal_system_sleep(1000);
            // send_move_step_cmd_table(move_dance_table,move_dance_table_size);
            break;
        }
        case MOVE_STATE_SHAKE_HANDS:{
            send_move_step_cmd_table(move_shake_hands_table,move_shake_hands_table_size);
            tal_system_sleep(100);    
            ctrl_single_servo(14,1900,200);
            tal_system_sleep(200);    
            ctrl_single_servo(13,800,200);
            tal_system_sleep(200);    
            ctrl_single_servo(13,600,100);
            tal_system_sleep(100);    
            ctrl_single_servo(13,800,100);
            tal_system_sleep(200);    
            ctrl_single_servo(13,600,100);
            tal_system_sleep(100);    
            ctrl_single_servo(13,800,100);
            tal_system_sleep(200);    
            ctrl_single_servo(13,600,100);
            tal_system_sleep(5*1000);

            break;
        }
        case MOVE_STATE_STAND:
            send_move_step_cmd_table(move_stand_table,move_stand_table_size);
            tal_system_sleep(5*1000);
            // robot_move_status_ctl(MOVE_STATE_RESET);
        break;
        case MOVE_STATE_SIT:
            send_move_step_cmd_table(move_sit_table,move_sit_table_size);
            tal_system_sleep(5*1000);
        break;
        case MOVE_STATE_RESET:
            send_move_step_cmd_table(idle_table,idle_table_size);
        break;
        case MOVE_STATE_IDLE:
            tal_system_sleep(1000); 
        break;
        default:
            break;
    }
    
}

void exute_robot_movement(void *arg)
{
    move_state_info_t *move_info = (move_state_info_t *)arg;
    PR_DEBUG("=======exute move state:%s start, step:%d=======", move_statu_name[move_info->state], move_info->step);
    is_obstacle = FALSE;
    while(move_info->step > 0)
    {
        robot_move_status_ctl(move_info->state);
        move_info->step--;
        // tal_system_sleep(100);    
    }
    robot_move_status_ctl(MOVE_STATE_RESET);
    PR_DEBUG("=======exute move state:%s end=======", move_statu_name[move_info->state]);
       // 如果是动态分配的内存，需要在这里释放
    if (move_info != NULL) {
        free(move_info);
        move_info = NULL;
    }
    
}

/**
 * @brief: 设置机器人每次移动的步数
 * @param: step: 步数
 * @return: none
 */
void robot_move_step_set(UINT32_T step)
{
    curr_step = step;
    PR_DEBUG("=======move step set:%d=======",step);
}

void  robot_walking_status_set(UINT8_T status ,UINT32_T step)
{
    
    local_status = status;
    curr_step = step;

    // 动态分配内存
    move_state_info_t *move_info = (move_state_info_t *)malloc(sizeof(move_state_info_t));
    if (move_info == NULL) {
        PR_DEBUG("Memory allocation failed");
        return;
    }
    
    move_info->state = status;
    move_info->step = curr_step;
    tal_workq_schedule(WORKQ_SYSTEM, exute_robot_movement, move_info);
    curr_step=default_move_step;

}





OPERATE_RET servo_uart_init(VOID)
{
    OPERATE_RET rt = OPRT_OK;

    TAL_UART_CFG_T cfg = {0};
    cfg.base_cfg.baudrate = UART_BOUND;
    cfg.base_cfg.databits = TUYA_UART_DATA_LEN_8BIT;
    cfg.base_cfg.stopbits = TUYA_UART_STOP_LEN_1BIT;
    cfg.base_cfg.parity = TUYA_UART_PARITY_TYPE_NONE;
    cfg.rx_buffer_size = 256;
    cfg.open_mode = O_BLOCK;

    tal_uart_deinit(TUYA_UART_NUM_0);
    TUYA_CALL_ERR_RETURN(tal_uart_init(TUYA_UART_NUM_0,&cfg));//usrt 0 init   


    #if 0
    THREAD_CFG_T   param;
    param.priority   = THREAD_PRIO_3;
    param.stackDepth = 1024*4;
    param.thrdname   = "my_servo_uart_thread";

    TUYA_CALL_ERR_RETURN( tal_thread_create_and_start(&my_uart_thread, NULL, NULL,_robot_ctl_uart_task ,NULL , &param));
    #endif

    PR_DEBUG("======_robot_uart_task init success==============");

    return rt;
}


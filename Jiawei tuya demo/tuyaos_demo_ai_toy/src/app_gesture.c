#include "tkl_i2c.h"
#include "tkl_pinmux.h"
#include "uni_log.h"
#include "tal_thread.h"
#include "tal_system.h"
#include "tal_semaphore.h"
#include "app_gesture.h"

#define GESTURE_I2C_ADDR                0x73 // PAJ7620 I2C Address

#define PAJ7620_REG_BANK_SEL            0xEF
#define PAJ7620_REG_CHIP_ID_L           0x00
#define PAJ7620_REG_CHIP_ID_H           0x01
#define PAJ7620_REG_INT_FLAG_1          0x43
#define PAJ7620_REG_INT_FLAG_2          0x44
#define PAJ7620_REG_TG_ENABLE           0x72

#define PAJ7620_VAL(val, maskbit)		( val << maskbit )

//! 根据安装角度调整


#define GES_LEFT_FLAG                   PAJ7620_VAL(1,0)
#define GES_RIGHT_FLAG                  PAJ7620_VAL(1,1)
#define GES_UP_FLAG                     PAJ7620_VAL(1,2)
#define GES_DOWN_FLAG                   PAJ7620_VAL(1,3)
#define GES_FORWARD_FLAG                PAJ7620_VAL(1,4)
#define GES_BACKWARD_FLAG               PAJ7620_VAL(1,5)
#define GES_CLOCKWISE_FLAG              PAJ7620_VAL(1,6)
#define GES_COUNT_CLOCKWISE_FLAG        PAJ7620_VAL(1,7)
#define GES_WAVE_FLAG                   PAJ7620_VAL(1,0)

STATIC GESTURE_CB_T s_gesture_cb = NULL;

STATIC THREAD_HANDLE s_gesture_thread_handle = NULL;

STATIC SEM_HANDLE s_gesture_notify;


static uint8_t __gesture_i2c_read_uint8(uint8_t reg)
{
    uint8_t value = 0;
    tkl_i2c_master_send(TUYA_I2C_NUM_0, GESTURE_I2C_ADDR, &reg, 1, TRUE);
    tkl_i2c_master_receive(TUYA_I2C_NUM_0, GESTURE_I2C_ADDR, &value, 1, FALSE);
    PR_TRACE("read reg: %02x, value: %02x", reg, value);
    return value;
}

static int __gesture_i2c_write_uint8(uint8_t reg, uint8_t value)
{
    OPERATE_RET rt = OPRT_OK;
    uint8_t data[2] = {reg, value};
    rt = tkl_i2c_master_send(TUYA_I2C_NUM_0, GESTURE_I2C_ADDR, data, 2, FALSE);
    PR_DEBUG("write reg: %02x, value: %02x", reg, value);
    return rt;
}

static GESTURE_TYPE_E __gesture_map(GESTURE_TYPE_E gesture)
{   
    static const uint8_t *desc[] = {
        "None",
        "Right",
        "Left",
        "Up",
        "Down",
        "Forward",
        "Backward",
        "Clockwise",
        "Anticlockwise",
        "Wave"
    };
    // typedef enum {
    //     GESTURE_NONE = 0,
    //     GESTURE_RIGHT,
    //     GESTURE_LEFT,
    //     GESTURE_UP,
    //     GESTURE_DOWN,
    //     GESTURE_FORWARD,
    //     GESTURE_BACKWARD,
    //     GESTURE_CLOCKWISE,
    //     GESTURE_ANTICLOCKWISE,
    //     GESTURE_WAVE
    // } GESTURE_TYPE_E;

    uint8_t map[] = {
        GESTURE_NONE,
        GESTURE_BACKWARD,       // GESTURE_RIGHT,
        GESTURE_FORWARD,        // GESTURE_LEFT,
        GESTURE_RIGHT,          // GESTURE_UP,  
        GESTURE_LEFT,           // GESTURE_DOWN,
        GESTURE_DOWN,           // GESTURE_FORWARD,
        GESTURE_UP,             // GESTURE_BACKWARD,
        GESTURE_CLOCKWISE,      // GESTURE_ANTICLOCKWISE,
        GESTURE_ANTICLOCKWISE,  // GESTURE_CLOCKWISE,
        GESTURE_WAVE
    }; 

    PR_DEBUG("gesture map %d-%d -> %s", gesture, map[gesture], desc[map[gesture]]);

    return map[gesture];
}

STATIC VOID __gesture_thread_process(VOID *arg)
{
    uint8_t data = 0, data1 = 0;
    GESTURE_TYPE_E gesture = GESTURE_NONE;

    while (1) {
        gesture = GESTURE_NONE;

        tal_semaphore_wait(s_gesture_notify, SEM_WAIT_FOREVER);

        data = __gesture_i2c_read_uint8(PAJ7620_REG_INT_FLAG_1);
        switch (data) {
        case GES_RIGHT_FLAG:
            PR_DEBUG("Right");
            gesture = GESTURE_RIGHT;
            break;
        case GES_LEFT_FLAG:
            PR_DEBUG("Left");
            gesture = GESTURE_LEFT;
            break;
        case GES_UP_FLAG:
            PR_DEBUG("Up");
            gesture = GESTURE_UP;
            break;
        case GES_DOWN_FLAG:
            PR_DEBUG("Down");
            gesture = GESTURE_DOWN;
            break;
        case GES_FORWARD_FLAG:
            PR_DEBUG("Forward");
            gesture = GESTURE_FORWARD;
            break;
        case GES_BACKWARD_FLAG:
            PR_DEBUG("Backward");
            gesture = GESTURE_BACKWARD;
            break;
        case GES_CLOCKWISE_FLAG:
            PR_DEBUG("Clockwise");
            gesture = GESTURE_CLOCKWISE;
            break;
        case GES_COUNT_CLOCKWISE_FLAG:
            PR_DEBUG("Anticlockwise");
            gesture = GESTURE_ANTICLOCKWISE;
            break;
        default:
            data1 = __gesture_i2c_read_uint8(PAJ7620_REG_INT_FLAG_2);
            if (data1 == GES_WAVE_FLAG) {
                PR_DEBUG("Wave");
                gesture = GESTURE_WAVE;
            }
            break;
        }

        gesture = __gesture_map(gesture);

        if (s_gesture_cb && gesture != GESTURE_NONE) {
            s_gesture_cb(gesture);
        }
        // tal_system_sleep(100);
    }
}

VOID gesture_irq_notify(VOID *args)
{
    tal_semaphore_post(s_gesture_notify);
}

// 修改：初始化接口，回调参数类型
OPERATE_RET app_gesture_init(GESTURE_CB_T cb)
{
    OPERATE_RET rt = OPRT_OK;
    uint16_t chip_id = 0;
    TUYA_IIC_BASE_CFG_T i2c_cfg = {
        .role = TUYA_IIC_MODE_MASTER,
        .speed = TUYA_IIC_BUS_SPEED_400K,
        .addr_width = TUYA_IIC_ADDRESS_7BIT,
    };
    THREAD_CFG_T thrd_param = {0};
    thrd_param.thrdname = "gesture_monitor";
    thrd_param.priority = THREAD_PRIO_1;
    thrd_param.stackDepth = 1024 + 1024;

    PR_DEBUG("i2c0 scl pin: %d, sda pin: %d", GUSTURE_SCL_PIN, GUSTURE_SDA_PIN);
    tkl_io_pinmux_config(GUSTURE_SCL_PIN, TUYA_IIC0_SCL);
    tkl_io_pinmux_config(GUSTURE_SDA_PIN, TUYA_IIC0_SDA);

    // 保存回调
    s_gesture_cb = cb;

    TUYA_CALL_ERR_GOTO(tkl_i2c_init(TUYA_I2C_NUM_0, &i2c_cfg), ERR_EXIT);

    tal_semaphore_create_init(&s_gesture_notify, 0, 1);

    // init gpio
    TUYA_GPIO_BASE_CFG_T key_cfg = {
        .mode = TUYA_GPIO_PULLUP,
        .direct = TUYA_GPIO_INPUT,
        .level = TUYA_GPIO_LEVEL_HIGH
    };

    TUYA_GPIO_IRQ_T irq_cfg = {
        .mode = TUYA_GPIO_IRQ_FALL,
        .cb   = gesture_irq_notify,
        .arg  = (void *)(intptr_t)GUSTURE_INT_PIN,  
    };

    TUYA_CALL_ERR_LOG(tkl_gpio_init(GUSTURE_INT_PIN, &key_cfg));
    TUYA_CALL_ERR_LOG(tal_gpio_irq_init(GUSTURE_INT_PIN, &irq_cfg));
    TUYA_CALL_ERR_LOG(tkl_gpio_irq_enable(GUSTURE_INT_PIN));

    // 切换到 BANK 0
    __gesture_i2c_write_uint8(PAJ7620_REG_BANK_SEL, 0x00);

    // 读取 Chip ID
    chip_id = __gesture_i2c_read_uint8(PAJ7620_REG_CHIP_ID_H);
    chip_id = (chip_id << 8) | __gesture_i2c_read_uint8(PAJ7620_REG_CHIP_ID_L);
    PR_DEBUG("Gesture sensor chip_id: 0x%04X", chip_id);
    if (chip_id != 0x7620) { // 检查芯片ID
        PR_ERR("Gesture sensor not found, chip_id: 0x%04X", chip_id);
        return OPRT_COM_ERROR;
    }

    // 切换到 BANK 1 使能模组，然后切换回 BANK 0
    __gesture_i2c_write_uint8(PAJ7620_REG_BANK_SEL, 0x01);
    __gesture_i2c_write_uint8(PAJ7620_REG_TG_ENABLE, 0x01);
    __gesture_i2c_write_uint8(PAJ7620_REG_BANK_SEL, 0x00);

    rt = tal_thread_create_and_start(&s_gesture_thread_handle, NULL, NULL, __gesture_thread_process, NULL, &thrd_param);
    if (rt != OPRT_OK) {
        PR_ERR("tal_thread_create_and_start failed, rt: %d", rt);
        goto ERR_EXIT;
    }

    PR_INFO("Gesture sensor initialized successfully");

    return OPRT_OK;

ERR_EXIT:
    return rt;
}

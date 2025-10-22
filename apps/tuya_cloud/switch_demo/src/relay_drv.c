#include "tkl_gpio.h"
#include "relay_drv.h"

#define RELAY_GPIO_PIN  20 // 根据实际硬件修改

void relay_drv_init(void)
{
    TUYA_GPIO_BASE_CFG_T cfg ={
        .mode = TUYA_GPIO_PUSH_PULL,
        .direct = TUYA_GPIO_OUTPUT,
        .level = TUYA_GPIO_LEVEL_LOW,
    };
    tkl_gpio_init(RELAY_GPIO_PIN,&cfg);
    tkl_gpio_write(RELAY_GPIO_PIN, TUYA_GPIO_LEVEL_LOW); // 关闭
}

void relay_on(void)
{
    tkl_gpio_write(RELAY_GPIO_PIN, 1); // 输出高电平，打开继电器
}

void relay_off(void)
{
    tkl_gpio_write(RELAY_GPIO_PIN, 0); // 输出低电平，关闭继电器
}
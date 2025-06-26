/**
 * @file tdl_display_manage.c
 * @version 0.1
 * @date 2025-05-27
 */

#include "tkl_gpio.h"
#include "tkl_memory.h"

#if defined(ENABLE_PWM) && (ENABLE_PWM==1)
#include "tkl_pwm.h"
#endif

#include "tal_api.h"
#include "tuya_list.h"

#include "tdl_display_driver.h"
#include "tdl_display_manage.h"

/***********************************************************
************************macro define************************
***********************************************************/
#define TDL_DISP_DRAW_BUF_ALIGN        4

/***********************************************************
***********************typedef define***********************
***********************************************************/
typedef struct {
    struct tuya_list_head   node; 
    bool                    is_open;  
    char                    name[DISPLAY_DEV_NAME_MAX_LEN+1];
    MUTEX_HANDLE            mutex; 
     
    TDL_DISP_DEV_INFO_T     info;
    TUYA_DISPLAY_BL_CTRL_T  bl;
    TUYA_DISPLAY_IO_CTRL_T  power;

    TDD_DISP_DEV_HANDLE_T   tdd_hdl;
    TDD_DISP_INTFS_T        intfs;
}DISPLAY_DEVICE_T;

/***********************************************************
********************function declaration********************
***********************************************************/


/***********************************************************
***********************variable define**********************
***********************************************************/
static struct tuya_list_head sg_display_list = LIST_HEAD_INIT(sg_display_list);  

/***********************************************************
***********************function define**********************
***********************************************************/
static DISPLAY_DEVICE_T *__find_display_device(char *name)
{
    DISPLAY_DEVICE_T *display_dev = NULL;
    struct tuya_list_head *pos = NULL;

    if(NULL == name) {
        return NULL;
    }

    tuya_list_for_each(pos, &sg_display_list) {
        display_dev = tuya_list_entry(pos, DISPLAY_DEVICE_T, node);
        if(0 == strncmp(display_dev->name, name, DISPLAY_DEV_NAME_MAX_LEN)) {
            return display_dev;
        }
    }

    return NULL;
}

static void __tdl_blacklight_init(TUYA_DISPLAY_BL_CTRL_T *bl_cfg)
{
    TUYA_GPIO_BASE_CFG_T cfg;

    if(NULL == bl_cfg) {
        return;
    }

    if(bl_cfg->type == TUYA_DISP_BL_TP_GPIO) {
        cfg.mode   = TUYA_GPIO_PUSH_PULL;
        cfg.direct = TUYA_GPIO_OUTPUT;
        cfg.level  = (bl_cfg->gpio.active_level == TUYA_GPIO_LEVEL_LOW)? TUYA_GPIO_LEVEL_HIGH: TUYA_GPIO_LEVEL_LOW;
        tkl_gpio_init(bl_cfg->gpio.pin, &cfg);
    }else if(bl_cfg->type == TUYA_DISP_BL_TP_PWM) {
#if defined(ENABLE_PWM) && (ENABLE_PWM==1)
        tkl_pwm_init(bl_cfg->pwm.id, &bl_cfg->pwm.cfg);
#endif
    }else if(bl_cfg->type == TUYA_DISP_BL_TP_NONE){
        PR_NOTICE("There is no backlight control pin on the board");
    }else {
        PR_NOTICE("not support bl type:%d", bl_cfg->type);
    }

    return;
}

static void __tdl_power_ctrl_io_init(TUYA_DISPLAY_IO_CTRL_T *power)
{
    TUYA_GPIO_BASE_CFG_T cfg;

    if(NULL == power || power->pin >= TUYA_GPIO_NUM_MAX) {
        return;
    }

    cfg.mode   = TUYA_GPIO_PUSH_PULL;
    cfg.direct = TUYA_GPIO_OUTPUT;
    cfg.level  = power->active_level;
    tkl_gpio_init(power->pin, &cfg);
}

static void __tdl_power_ctrl_io_deinit(TUYA_DISPLAY_IO_CTRL_T *power)
{
    if(NULL == power) {
        return;
    }

    tkl_gpio_deinit(power->pin);
}


static void __tdl_blacklight_deinit(TUYA_DISPLAY_BL_CTRL_T *bl_cfg)
{
    if(NULL == bl_cfg) {
        return;
    }

    if(bl_cfg->type == TUYA_DISP_BL_TP_GPIO) {
        tkl_gpio_deinit(bl_cfg->gpio.pin);
    }else if(bl_cfg->type == TUYA_DISP_BL_TP_PWM) {
#if defined(ENABLE_PWM) && (ENABLE_PWM==1)
        tkl_pwm_deinit(bl_cfg->pwm.id);
#endif
    }else if(bl_cfg->type == TUYA_DISP_BL_TP_NONE){
        PR_NOTICE("There is no backlight control pin on the board");
    }else {
        PR_NOTICE("not support bl type:%d", bl_cfg->type);
    }

    return;
}

TDL_DISP_HANDLE_T tdl_disp_find_dev(char *name)
{
    return (TDL_DISP_HANDLE_T)__find_display_device(name);
}


OPERATE_RET tdl_disp_dev_open(TDL_DISP_HANDLE_T disp_hdl)
{
    OPERATE_RET rt = OPRT_OK;
    DISPLAY_DEVICE_T *display_dev = NULL;

    if(NULL == disp_hdl) {
        return OPRT_INVALID_PARM;
    }

    display_dev = (DISPLAY_DEVICE_T *)disp_hdl;

    if(display_dev->is_open) {
        return OPRT_OK;
    }

    if(NULL == display_dev->mutex) {
        TUYA_CALL_ERR_RETURN(tal_mutex_create_init(&display_dev->mutex));
    }

    __tdl_power_ctrl_io_init(&display_dev->power);

    if(display_dev->intfs.open) {
        TUYA_CALL_ERR_RETURN(display_dev->intfs.open(display_dev->tdd_hdl));
    }

    __tdl_blacklight_init(&display_dev->bl);

    display_dev->is_open = true;

    return OPRT_OK;
}

OPERATE_RET tdl_disp_dev_flush(TDL_DISP_HANDLE_T disp_hdl, TDL_DISP_FRAME_BUFF_T *frame_buff)
{
    OPERATE_RET rt = OPRT_OK;
    DISPLAY_DEVICE_T *display_dev = NULL;

    if(NULL == disp_hdl || NULL == frame_buff) {
        return OPRT_INVALID_PARM;
    }

    display_dev = (DISPLAY_DEVICE_T *)disp_hdl;

    if(false == display_dev->is_open) {
        return OPRT_COM_ERROR;
    }

    if(display_dev->intfs.flush) {
        TUYA_CALL_ERR_RETURN(display_dev->intfs.flush(display_dev->tdd_hdl, frame_buff));
    }

    return OPRT_OK;
}

OPERATE_RET tdl_disp_dev_get_info(TDL_DISP_HANDLE_T disp_hdl, TDL_DISP_DEV_INFO_T *dev_info)
{
    DISPLAY_DEVICE_T *display_dev = NULL;

    if(NULL == disp_hdl || NULL == dev_info) {
        return OPRT_INVALID_PARM;
    }

    display_dev = (DISPLAY_DEVICE_T *)disp_hdl;

    memcpy(dev_info, &display_dev->info, sizeof(TDL_DISP_DEV_INFO_T));

    return OPRT_OK;
}

OPERATE_RET tdl_disp_set_brightness(TDL_DISP_HANDLE_T disp_hdl, uint8_t brightness)
{
    DISPLAY_DEVICE_T *display_dev = NULL;

    if(NULL == disp_hdl) {
        return OPRT_INVALID_PARM;
    }

    display_dev = (DISPLAY_DEVICE_T *)disp_hdl;

    if(display_dev->bl.type == TUYA_DISP_BL_TP_GPIO) {
        if(brightness) {
            tkl_gpio_write(display_dev->bl.gpio.pin, display_dev->bl.gpio.active_level);
        }else {
            tkl_gpio_write(display_dev->bl.gpio.pin, (display_dev->bl.gpio.active_level == TUYA_GPIO_LEVEL_HIGH)?\
                                                      TUYA_GPIO_LEVEL_LOW: TUYA_GPIO_LEVEL_HIGH);
        }
    }else if(display_dev->bl.type == TUYA_DISP_BL_TP_PWM) {
#if defined(ENABLE_PWM) && (ENABLE_PWM==1)
        if(brightness) {
            display_dev->bl.pwm.cfg.duty = brightness*100;
            tkl_pwm_info_set(display_dev->bl.pwm.id, &display_dev->bl.pwm.cfg);
            tkl_pwm_start(display_dev->bl.pwm.id);
        }else {
            tkl_pwm_stop(display_dev->bl.pwm.id);
        }
#endif
    }else if(display_dev->bl.type == TUYA_DISP_BL_TP_NONE) {
        PR_NOTICE("There is no backlight control pin on the board");
    }else {
        return OPRT_NOT_SUPPORTED;
    }

    return OPRT_OK;
}

OPERATE_RET tdl_disp_dev_close(TDL_DISP_HANDLE_T disp_hdl)
{   
    OPERATE_RET rt = OPRT_OK;
    DISPLAY_DEVICE_T *display_dev = NULL;

    if(NULL == disp_hdl) {
        return OPRT_INVALID_PARM;
    }

    display_dev = (DISPLAY_DEVICE_T *)disp_hdl;

    if(false == display_dev->is_open) {
        return OPRT_OK;
    }

    if(display_dev->intfs.close) {
        TUYA_CALL_ERR_RETURN(display_dev->intfs.close(display_dev->tdd_hdl));
    }

    __tdl_blacklight_deinit(&display_dev->bl);

    __tdl_power_ctrl_io_deinit(&display_dev->power);

    display_dev->is_open = false;

    return OPRT_OK;
}

TDL_DISP_FRAME_BUFF_T *tdl_disp_create_frame_buff(DISP_FB_RAM_TP_E type, uint32_t len)
{
    TDL_DISP_FRAME_BUFF_T *fb = NULL;
    DISP_FB_RAM_TP_E fb_type = type;
    uint32_t size = 0, frame_len = 0;
    uint8_t *p_frame = NULL;
    
    frame_len = len + TDL_DISP_DRAW_BUF_ALIGN - 1;
    size = sizeof(TDL_DISP_FRAME_BUFF_T) + frame_len;

#if defined(ENABLE_EXT_RAM) && (ENABLE_EXT_RAM == 1)
    if(type == DISP_FB_TP_SRAM) {
        fb = tkl_system_malloc(size);
    }else {
        fb = tkl_system_psram_malloc(size);
    }
#else
    fb = tkl_system_malloc(size);
    fb_type = DISP_FB_TP_SRAM;
#endif

    if(fb == NULL) {
        return NULL;
    }

    memset(fb, 0, size);

    p_frame = (uint8_t *)fb + sizeof(TDL_DISP_FRAME_BUFF_T);
    p_frame += TDL_DISP_DRAW_BUF_ALIGN - 1;
    p_frame = (uint8_t *)((uint32_t) p_frame & ~(TDL_DISP_DRAW_BUF_ALIGN - 1));

    fb->type  = fb_type;
    fb->frame = p_frame; 
    fb->len   = len;

    return fb;
}

void tdl_disp_free_frame_buff(TDL_DISP_FRAME_BUFF_T *frame_buff)
{
    if(frame_buff) {
        if(frame_buff->type == DISP_FB_TP_SRAM) {
            tkl_system_free(frame_buff);
        }else {
#if defined(ENABLE_EXT_RAM) && (ENABLE_EXT_RAM == 1)
            tkl_system_psram_free(frame_buff);
#else 
            tkl_system_free(frame_buff);
#endif
        }
    }
}

OPERATE_RET tdl_disp_device_register(char *name, TDD_DISP_DEV_HANDLE_T tdd_hdl, \
                                     TDD_DISP_INTFS_T *intfs, TDD_DISP_DEV_INFO_T *dev_info)
{
    DISPLAY_DEVICE_T *display_dev = NULL;

    if(NULL == name || NULL == tdd_hdl || NULL == intfs || NULL == dev_info) {
        return OPRT_INVALID_PARM;
    }

    NEW_LIST_NODE(DISPLAY_DEVICE_T, display_dev);
    if(NULL == display_dev) {
        return OPRT_MALLOC_FAILED;
    }
    memset(display_dev, 0, sizeof(DISPLAY_DEVICE_T));

    strncpy(display_dev->name, name, DISPLAY_DEV_NAME_MAX_LEN);

    display_dev->info.type     = dev_info->type;
    display_dev->info.width    = dev_info->width;
    display_dev->info.height   = dev_info->height;
    display_dev->info.fmt      = dev_info->fmt;
    display_dev->info.rotation = dev_info->rotation;

    memcpy(&display_dev->bl, &dev_info->bl, sizeof(TUYA_DISPLAY_BL_CTRL_T));
    memcpy(&display_dev->power, &dev_info->power, sizeof(TUYA_DISPLAY_IO_CTRL_T));

    display_dev->tdd_hdl = tdd_hdl;

    memcpy(&display_dev->intfs, intfs, sizeof(TDD_DISP_INTFS_T));

    tuya_list_add(&display_dev->node, &sg_display_list);

    return OPRT_OK;
}
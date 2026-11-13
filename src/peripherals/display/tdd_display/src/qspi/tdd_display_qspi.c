/**
 * @file tdd_display_qspi.c
 * @brief Common TDD QSPI display transport.
 *
 * Panel drivers provide command framing through DISP_QSPI_BASE_CFG_T. This
 * transport owns QSPI initialization, optional software CS, reset/init-table
 * execution, window setup, frame flushing and optional TE synchronization.
 *
 * @copyright Copyright (c) 2021-2026 Tuya Inc. All Rights Reserved.
 */

#include "tal_api.h"

#if defined(ENABLE_QSPI) && (ENABLE_QSPI == 1)
#include "tkl_qspi.h"
#include "tkl_gpio.h"

#include "tdd_display_qspi.h"

#define DISP_QSPI_RESET_PRE_MS       20U
#define DISP_QSPI_RESET_LOW_MS       20U
#define DISP_QSPI_RESET_HIGH_MS      120U
#define DISP_QSPI_TX_TIMEOUT_MS      1000U
#define DISP_QSPI_TE_HIGH_TIMEOUT_MS 30U
#define DISP_QSPI_TE_LOW_TIMEOUT_MS  40U

typedef struct {
    MUTEX_HANDLE                mutex;
    DISP_QSPI_BASE_CFG_T        cfg;
    const uint8_t              *init_seq;
    bool                        irq_enabled;
    bool                        te_sync;
    uint32_t                    flush_seq;
    uint16_t                    win_x1;
    uint16_t                    win_x2;
    uint16_t                    win_y1;
    uint16_t                    win_y2;
} DISP_QSPI_DEV_T;

typedef enum {
    TDD_QSPI_FRAME_REQUEST = 0,
    TDD_QSPI_FRAME_EXIT,
} TDD_QSPI_FRAME_EVENT_E;

typedef struct {
    TDD_QSPI_FRAME_EVENT_E  event;
    TDL_DISP_FRAME_BUFF_T  *frame_buff;
} TDD_DISP_QSPI_MSG_T;

typedef struct {
    TUYA_QSPI_NUM_E         port;
    SEM_HANDLE              tx_sem;
    SEM_HANDLE              exit_sem;
    QUEUE_HANDLE            queue;
    THREAD_HANDLE           task;
    TDD_DISP_DEV_HANDLE_T   device;
    TDL_DISP_FRAME_BUFF_T  *display_fb;
    uint8_t                 is_task_running;
    uint8_t                 is_period_flush;
} TDD_DISP_QSPI_SYNC_T;

static TDD_DISP_QSPI_SYNC_T sg_disp_qspi_sync[TUYA_QSPI_NUM_MAX] = {0};

static void __display_qspi_task(void *args);

static bool __disp_qspi_pin_valid(TUYA_GPIO_NUM_E pin)
{
    return pin < TUYA_GPIO_NUM_MAX;
}

static void __disp_qspi_event_cb(TUYA_QSPI_NUM_E port, TUYA_QSPI_IRQ_EVT_E event)
{
    if (event == TUYA_QSPI_EVENT_TX && port < TUYA_QSPI_NUM_MAX &&
        sg_disp_qspi_sync[port].tx_sem != NULL) {
        (void)tal_semaphore_post(sg_disp_qspi_sync[port].tx_sem);
    }
}

static OPERATE_RET __disp_qspi_cs_write(const DISP_QSPI_BASE_CFG_T *cfg,
                                        TUYA_GPIO_LEVEL_E level)
{
    if (!__disp_qspi_pin_valid(cfg->cs_pin)) {
        return OPRT_INVALID_PARM;
    }
    return tkl_gpio_write(cfg->cs_pin, level);
}

static OPERATE_RET __disp_qspi_gpio_init(const DISP_QSPI_BASE_CFG_T *cfg)
{
    TUYA_GPIO_BASE_CFG_T pin_cfg = {
        .mode = TUYA_GPIO_PUSH_PULL,
        .direct = TUYA_GPIO_OUTPUT,
        .level = TUYA_GPIO_LEVEL_HIGH,
    };
    OPERATE_RET rt;

    if (__disp_qspi_pin_valid(cfg->rst_pin)) {
        rt = tkl_gpio_init(cfg->rst_pin, &pin_cfg);
        if (rt != OPRT_OK) {
            return rt;
        }
    }

    if (!__disp_qspi_pin_valid(cfg->cs_pin)) {
        return OPRT_INVALID_PARM;
    }
    rt = tkl_gpio_init(cfg->cs_pin, &pin_cfg);
    if (rt != OPRT_OK) {
        return rt;
    }

    if (cfg->te_sync_enable) {
        if (!__disp_qspi_pin_valid(cfg->te_pin)) {
            return OPRT_INVALID_PARM;
        }
        TUYA_GPIO_BASE_CFG_T te_cfg = {
            .mode = TUYA_GPIO_FLOATING,
            .direct = TUYA_GPIO_INPUT,
            .level = TUYA_GPIO_LEVEL_NONE,
        };
        rt = tkl_gpio_init(cfg->te_pin, &te_cfg);
        if (rt != OPRT_OK) {
            return rt;
        }
    }

    return OPRT_OK;
}

static OPERATE_RET __disp_qspi_init(DISP_QSPI_DEV_T *dev)
{
    TUYA_QSPI_BASE_CFG_T qspi_cfg = {
        .role = TUYA_QSPI_ROLE_MASTER,
        .mode = dev->cfg.mode,
        .freq_hz = dev->cfg.freq_hz,
        .use_dma = dev->cfg.use_dma,
        .dma_data_lines = dev->cfg.data_lines,
        .type = TUYA_QSPI_TYPE_LCD,
    };
    OPERATE_RET rt;

    rt = tkl_qspi_init(dev->cfg.port, &qspi_cfg);
    if (rt != OPRT_OK) {
        return rt;
    }

    dev->irq_enabled = false;
    if (dev->cfg.use_dma) {
        rt = tkl_qspi_irq_init(dev->cfg.port, __disp_qspi_event_cb);
        if (rt == OPRT_OK) {
            rt = tkl_qspi_irq_enable(dev->cfg.port);
            if (rt != OPRT_OK) {
                return rt;
            }
            dev->irq_enabled = true;
        } else if (rt != OPRT_NOT_SUPPORTED) {
            return rt;
        }
    }

    return OPRT_OK;
}

static OPERATE_RET __disp_qspi_send_cmd(DISP_QSPI_BASE_CFG_T *cfg, uint8_t cmd,
                                        uint8_t *data, uint32_t data_len)
{
    TUYA_QSPI_CMD_T qspi_cmd = {0};
    OPERATE_RET rt;

    qspi_cmd.op = TUYA_QSPI_WRITE;
    qspi_cmd.cmd[0] = cfg->cmd_opcode;
    qspi_cmd.cmd_size = 1U;
    qspi_cmd.cmd_lines = TUYA_QSPI_1WIRE;
    qspi_cmd.addr_size = 3U;
    qspi_cmd.addr_lines = TUYA_QSPI_1WIRE;
    qspi_cmd.addr[0] = 0U;
    qspi_cmd.addr[1] = cmd;
    qspi_cmd.addr[2] = 0U;
    qspi_cmd.data = data;
    qspi_cmd.data_size = data_len;
    qspi_cmd.data_lines = TUYA_QSPI_1WIRE;

    rt = __disp_qspi_cs_write(cfg, TUYA_GPIO_LEVEL_LOW);
    if (rt == OPRT_OK) {
        rt = tkl_qspi_comand(cfg->port, &qspi_cmd);
    }
    OPERATE_RET release_rt = __disp_qspi_cs_write(cfg, TUYA_GPIO_LEVEL_HIGH);
    return rt == OPRT_OK ? release_rt : rt;
}

static void __disp_qspi_wait_te(DISP_QSPI_DEV_T *dev,
                                const TDL_DISP_FRAME_BUFF_T *fb)
{
    uint32_t start_ms;
    uint32_t wait_div = dev->cfg.te_wait_div ? dev->cfg.te_wait_div : 1U;
    TUYA_GPIO_LEVEL_E level;

    if (!dev->cfg.te_sync_enable || !dev->te_sync || fb == NULL ||
        (dev->flush_seq++ % wait_div) != 0U) {
        return;
    }

    start_ms = (uint32_t)tal_system_get_millisecond();
    while (tkl_gpio_read(dev->cfg.te_pin, &level) == OPRT_OK &&
           level == dev->cfg.te_active_level) {
        if ((uint32_t)tal_system_get_millisecond() - start_ms >=
            DISP_QSPI_TE_HIGH_TIMEOUT_MS) {
            dev->te_sync = false;
            PR_ERR("QSPI TE active timeout, continue unsynchronized");
            return;
        }
    }

    while (tkl_gpio_read(dev->cfg.te_pin, &level) == OPRT_OK &&
           level != dev->cfg.te_active_level) {
        if ((uint32_t)tal_system_get_millisecond() - start_ms >=
            DISP_QSPI_TE_LOW_TIMEOUT_MS) {
            dev->te_sync = false;
            PR_ERR("QSPI TE inactive timeout, continue unsynchronized");
            return;
        }
    }
}

static void __disp_qspi_set_window(DISP_QSPI_DEV_T *dev,
                                   const TDL_DISP_FRAME_BUFF_T *fb)
{
    uint8_t range[4];
    uint16_t x_start = (uint16_t)(fb->x_start + dev->cfg.x_offset);
    uint16_t y_start = (uint16_t)(fb->y_start + dev->cfg.y_offset);
    uint16_t x_end = (uint16_t)(x_start + fb->width - 1U);
    uint16_t y_end = (uint16_t)(y_start + fb->height - 1U);
    OPERATE_RET rt;

    if (dev->win_x1 != x_start || dev->win_x2 != x_end) {
        range[0] = (uint8_t)(x_start >> 8U);
        range[1] = (uint8_t)x_start;
        range[2] = (uint8_t)(x_end >> 8U);
        range[3] = (uint8_t)x_end;
        rt = __disp_qspi_send_cmd(&dev->cfg, dev->cfg.cmd_caset, range,
                                  sizeof(range));
        if (rt != OPRT_OK) {
            return;
        }
        dev->win_x1 = x_start;
        dev->win_x2 = x_end;
    }

    if (dev->win_y1 != y_start || dev->win_y2 != y_end) {
        range[0] = (uint8_t)(y_start >> 8U);
        range[1] = (uint8_t)y_start;
        range[2] = (uint8_t)(y_end >> 8U);
        range[3] = (uint8_t)y_end;
        (void)__disp_qspi_send_cmd(&dev->cfg, dev->cfg.cmd_raset, range,
                                   sizeof(range));
        dev->win_y1 = y_start;
        dev->win_y2 = y_end;
    }
}

static OPERATE_RET __disp_qspi_send_frame(DISP_QSPI_DEV_T *dev,
                                          TDL_DISP_FRAME_BUFF_T *fb)
{
    const QSPI_CMD_FRAME_T *pixel_cmd =
        dev->cfg.data_lines == TUYA_QSPI_1WIRE ?
        &dev->cfg.pixel_cmd_1wire : &dev->cfg.pixel_cmd_4wire;
    TUYA_QSPI_CMD_T qspi_cmd = {0};
    OPERATE_RET rt;

    __disp_qspi_wait_te(dev, fb);

    qspi_cmd.op = TUYA_QSPI_WRITE;
    qspi_cmd.cmd[0] = pixel_cmd->cmd;
    qspi_cmd.cmd_size = 1U;
    qspi_cmd.cmd_lines = pixel_cmd->cmd_lines;
    qspi_cmd.addr_size = pixel_cmd->addr_size;
    qspi_cmd.addr_lines = pixel_cmd->addr_lines;
    memcpy(qspi_cmd.addr, pixel_cmd->addr,
           qspi_cmd.addr_size < sizeof(qspi_cmd.addr) ?
           qspi_cmd.addr_size : sizeof(qspi_cmd.addr));
    qspi_cmd.dummy_cycle = pixel_cmd->dummy_cycle;

    rt = __disp_qspi_cs_write(&dev->cfg, TUYA_GPIO_LEVEL_LOW);
    if (rt == OPRT_OK) {
        rt = tkl_qspi_comand(dev->cfg.port, &qspi_cmd);
    }
    if (rt == OPRT_OK) {
        rt = tkl_qspi_send(dev->cfg.port, fb->frame, fb->len);
    }
    if (rt == OPRT_OK && dev->irq_enabled) {
        rt = tal_semaphore_wait(sg_disp_qspi_sync[dev->cfg.port].tx_sem,
                                DISP_QSPI_TX_TIMEOUT_MS);
    }
    OPERATE_RET release_rt = __disp_qspi_cs_write(&dev->cfg,
                                                  TUYA_GPIO_LEVEL_HIGH);
    return rt == OPRT_OK ? release_rt : rt;
}

static OPERATE_RET __disp_qspi_display_frame(TDD_DISP_DEV_HANDLE_T device,
                                              TDL_DISP_FRAME_BUFF_T *fb)
{
    DISP_QSPI_DEV_T *dev = (DISP_QSPI_DEV_T *)device;

    if (dev == NULL || fb == NULL || fb->frame == NULL || fb->len == 0U) {
        return OPRT_INVALID_PARM;
    }

    __disp_qspi_set_window(dev, fb);
    return __disp_qspi_send_frame(dev, fb);
}

static OPERATE_RET __tdd_disp_reset(TUYA_GPIO_NUM_E rst_pin)
{
    if (!__disp_qspi_pin_valid(rst_pin)) {
        return OPRT_OK;
    }

    (void)tkl_gpio_write(rst_pin, TUYA_GPIO_LEVEL_HIGH);
    tal_system_sleep(DISP_QSPI_RESET_PRE_MS);
    (void)tkl_gpio_write(rst_pin, TUYA_GPIO_LEVEL_LOW);
    tal_system_sleep(DISP_QSPI_RESET_LOW_MS);
    (void)tkl_gpio_write(rst_pin, TUYA_GPIO_LEVEL_HIGH);
    tal_system_sleep(DISP_QSPI_RESET_HIGH_MS);
    return OPRT_OK;
}

static OPERATE_RET __tdd_disp_init_seq(DISP_QSPI_BASE_CFG_T *cfg,
                                        const uint8_t *init_seq)
{
    const uint8_t *line = init_seq;
    OPERATE_RET rt;

    if (line == NULL) {
        return OPRT_INVALID_PARM;
    }

    rt = __tdd_disp_reset(cfg->rst_pin);
    if (rt != OPRT_OK) {
        return rt;
    }

    while (*line != 0U) {
        uint8_t data_len = (uint8_t)(line[0] - 1U);
        uint8_t delay_ms = line[1];
        uint8_t cmd = line[2];
        uint8_t *data = data_len == 0U ? NULL : (uint8_t *)&line[3];

        rt = __disp_qspi_send_cmd(cfg, cmd, data, data_len);
        if (rt != OPRT_OK) {
            PR_ERR("QSPI display init command 0x%02X failed: %d", cmd, rt);
            return rt;
        }
        if (delay_ms != 0U) {
            tal_system_sleep(delay_ms);
        }
        line += line[0] + 2U;
    }

    return OPRT_OK;
}

static OPERATE_RET __disp_qspi_sync_init(TUYA_QSPI_NUM_E port,
                                         TDD_DISP_DEV_HANDLE_T device,
                                         uint8_t is_period_flush,
                                         bool irq_enabled)
{
    TDD_DISP_QSPI_SYNC_T *sync;
    OPERATE_RET rt;

    if (port >= TUYA_QSPI_NUM_MAX || device == NULL) {
        return OPRT_INVALID_PARM;
    }

    sync = &sg_disp_qspi_sync[port];
    sync->port = port;
    sync->device = device;
    sync->is_period_flush = is_period_flush;

    if (irq_enabled && sync->tx_sem == NULL) {
        TUYA_CALL_ERR_RETURN(tal_semaphore_create_init(&sync->tx_sem, 0, 1));
    }
    if (sync->exit_sem == NULL) {
        TUYA_CALL_ERR_RETURN(tal_semaphore_create_init(&sync->exit_sem, 0, 1));
    }
    if (sync->queue == NULL) {
        TUYA_CALL_ERR_RETURN(tal_queue_create_init(
            &sync->queue, sizeof(TDD_DISP_QSPI_MSG_T), 4));
    }
    if (sync->task == NULL) {
        THREAD_CFG_T thread_cfg = {4096, THREAD_PRIO_0, "qspi_task"};
        TUYA_CALL_ERR_RETURN(tal_thread_create_and_start(
            &sync->task, NULL, NULL, __display_qspi_task, sync, &thread_cfg));
    }

    return OPRT_OK;
}

static void __display_qspi_task(void *args)
{
    TDD_DISP_QSPI_SYNC_T *sync = (TDD_DISP_QSPI_SYNC_T *)args;
    TDD_DISP_QSPI_MSG_T msg = {0};

    if (sync == NULL) {
        return;
    }

    sync->is_task_running = 1U;
    while (sync->is_task_running) {
        OPERATE_RET rt = tal_queue_fetch(sync->queue, &msg,
                                         sync->is_period_flush ?
                                         15U : QUEUE_WAIT_FOREVER);
        if (rt != OPRT_OK) {
            if (!sync->is_period_flush) {
                continue;
            }
            msg.event = TDD_QSPI_FRAME_REQUEST;
            msg.frame_buff = sync->display_fb;
        }

        if (msg.event == TDD_QSPI_FRAME_EXIT) {
            sync->is_task_running = 0U;
            if (sync->exit_sem != NULL) {
                while (tal_queue_fetch(sync->queue, &msg, 0U) == OPRT_OK) {
                    if (msg.frame_buff != NULL && msg.frame_buff->free_cb != NULL) {
                        msg.frame_buff->free_cb(msg.frame_buff);
                    }
                }
                (void)tal_semaphore_post(sync->exit_sem);
            }
            continue;
        }

        if (msg.frame_buff != NULL) {
            rt = __disp_qspi_display_frame(sync->device, msg.frame_buff);
            if (rt != OPRT_OK) {
                PR_ERR("QSPI frame flush failed: %d", rt);
            }
        }

        if (sync->is_period_flush) {
            if (sync->display_fb != msg.frame_buff) {
                TDL_DISP_FRAME_BUFF_T *old = sync->display_fb;
                sync->display_fb = msg.frame_buff;
                if (old != NULL && old->free_cb != NULL) {
                    old->free_cb(old);
                }
            }
        } else if (msg.frame_buff != NULL && msg.frame_buff->free_cb != NULL) {
            msg.frame_buff->free_cb(msg.frame_buff);
        }
    }

    THREAD_HANDLE task = sync->task;
    sync->task = NULL;
    tal_thread_delete(task);
}

static OPERATE_RET __tdd_display_qspi_open(TDD_DISP_DEV_HANDLE_T device)
{
    DISP_QSPI_DEV_T *dev = (DISP_QSPI_DEV_T *)device;
    OPERATE_RET rt;

    if (dev == NULL) {
        return OPRT_INVALID_PARM;
    }

    /* Initialize QSPI first, then override the board CS pin as GPIO. This
     * keeps CS ownership in the TDD transport regardless of whether the QSPI
     * controller has hardware NSS or force-CS support. */
    rt = __disp_qspi_init(dev);
    if (rt != OPRT_OK) {
        return rt;
    }
    rt = __disp_qspi_gpio_init(&dev->cfg);
    if (rt != OPRT_OK) {
        return rt;
    }
    rt = __tdd_disp_init_seq(&dev->cfg, dev->init_seq);
    if (rt != OPRT_OK) {
        return rt;
    }

    if (dev->mutex == NULL) {
        TUYA_CALL_ERR_RETURN(tal_mutex_create_init(&dev->mutex));
    }

    dev->te_sync = dev->cfg.te_sync_enable;
    return __disp_qspi_sync_init(dev->cfg.port, device,
                                 dev->cfg.has_vram ? 0U : 1U,
                                 dev->irq_enabled);
}

static OPERATE_RET __tdd_display_qspi_flush(TDD_DISP_DEV_HANDLE_T device,
                                             TDL_DISP_FRAME_BUFF_T *fb)
{
    DISP_QSPI_DEV_T *dev = (DISP_QSPI_DEV_T *)device;
    TDD_DISP_QSPI_MSG_T msg = {TDD_QSPI_FRAME_REQUEST, fb};
    OPERATE_RET rt;

    if (dev == NULL || fb == NULL) {
        return OPRT_INVALID_PARM;
    }

    tal_mutex_lock(dev->mutex);
    rt = tal_queue_post(sg_disp_qspi_sync[dev->cfg.port].queue, &msg,
                        SEM_WAIT_FOREVER);
    tal_mutex_unlock(dev->mutex);
    return rt;
}

static OPERATE_RET __tdd_display_qspi_close(TDD_DISP_DEV_HANDLE_T device)
{
    DISP_QSPI_DEV_T *dev = (DISP_QSPI_DEV_T *)device;
    TDD_DISP_QSPI_MSG_T msg = {TDD_QSPI_FRAME_EXIT, NULL};
    OPERATE_RET rt;

    if (dev == NULL) {
        return OPRT_INVALID_PARM;
    }

    if (sg_disp_qspi_sync[dev->cfg.port].task != NULL) {
        rt = tal_queue_post(sg_disp_qspi_sync[dev->cfg.port].queue, &msg,
                            SEM_WAIT_FOREVER);
        if (rt != OPRT_OK) {
            return rt;
        }
        rt = tal_semaphore_wait(sg_disp_qspi_sync[dev->cfg.port].exit_sem,
                                SEM_WAIT_FOREVER);
        if (rt != OPRT_OK) {
            return rt;
        }
    }

    return tkl_qspi_deinit(dev->cfg.port);
}

OPERATE_RET tdd_disp_qspi_panel_register(char *name,
                                         const TDD_DISP_QSPI_CFG_T *panel_cfg,
                                         const DISP_QSPI_DEVICE_CFG_T *dev_cfg)
{
    TDD_DISP_QSPI_CFG_T cfg;

    if (name == NULL || panel_cfg == NULL || dev_cfg == NULL) {
        return OPRT_INVALID_PARM;
    }

    cfg = *panel_cfg;
    cfg.cfg.width = dev_cfg->width;
    cfg.cfg.height = dev_cfg->height;
    cfg.cfg.x_offset = dev_cfg->x_offset;
    cfg.cfg.y_offset = dev_cfg->y_offset;
    cfg.cfg.pixel_fmt = dev_cfg->pixel_fmt;
    cfg.cfg.rst_pin = dev_cfg->rst_pin;
    cfg.cfg.cs_pin = dev_cfg->cs_pin;
    cfg.cfg.port = dev_cfg->port;
    cfg.cfg.freq_hz = dev_cfg->spi_clk;
    cfg.cfg.mode = dev_cfg->mode;
    cfg.cfg.data_lines = dev_cfg->data_lines;
    cfg.cfg.te_pin = dev_cfg->te_pin;
    cfg.cfg.te_active_level = dev_cfg->te_active_level;
    cfg.cfg.use_dma = dev_cfg->use_dma;
    cfg.cfg.te_sync_enable = dev_cfg->te_sync_enable;
    cfg.cfg.te_wait_div = dev_cfg->te_wait_div;
    cfg.rotation = dev_cfg->rotation;
    cfg.bl = dev_cfg->bl;
    cfg.power = dev_cfg->power;

    return tdd_disp_qspi_device_register(name, &cfg);
}

OPERATE_RET tdd_disp_qspi_device_register(char *name, TDD_DISP_QSPI_CFG_T *qspi)
{
    DISP_QSPI_DEV_T *dev;
    TDD_DISP_DEV_INFO_T info = {0};
    OPERATE_RET rt;

    if (name == NULL || qspi == NULL) {
        return OPRT_INVALID_PARM;
    }

    dev = tal_malloc(sizeof(*dev));
    if (dev == NULL) {
        return OPRT_MALLOC_FAILED;
    }
    memset(dev, 0, sizeof(*dev));
    dev->cfg = qspi->cfg;
    dev->init_seq = qspi->init_seq;

    info.type = TUYA_DISPLAY_QSPI;
    info.width = qspi->cfg.width;
    info.height = qspi->cfg.height;
    info.fmt = qspi->cfg.pixel_fmt;
    info.rotation = qspi->rotation;
    info.is_swap = qspi->is_swap;
    info.has_vram = qspi->cfg.has_vram;
    info.bl = qspi->bl;
    info.power = qspi->power;

    TDD_DISP_INTFS_T intfs = {
        .open = __tdd_display_qspi_open,
        .flush = __tdd_display_qspi_flush,
        .close = __tdd_display_qspi_close,
    };

    rt = tdl_disp_device_register(name, (TDD_DISP_DEV_HANDLE_T)dev, &intfs,
                                  &info);
    if (rt != OPRT_OK) {
        tal_free(dev);
    }
    return rt;
}

OPERATE_RET tdd_disp_qspi_send_cmd(DISP_QSPI_BASE_CFG_T *cfg, uint8_t cmd,
                                    uint8_t *data, uint32_t data_len)
{
    if (cfg == NULL || (data == NULL && data_len != 0U)) {
        return OPRT_INVALID_PARM;
    }
    return __disp_qspi_send_cmd(cfg, cmd, data, data_len);
}

#endif

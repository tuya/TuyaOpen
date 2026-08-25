#include "tal_api.h"

#if defined(DISPLAY_NAME)
#include "tdl_display_manage.h"
#include "tdl_display_driver.h"
#include "tdd_display_custom.h"
#include "sl_tuya_display.h"

static OPERATE_RET __tdl_display_custom_open(TDD_DISP_DEV_HANDLE_T device)
{
    if (NULL == device) {
        return OPRT_INVALID_PARM;
    }

    sl_tuya_display_init();

    return OPRT_OK;
}

static OPERATE_RET __tdl_display_custom_flush(TDD_DISP_DEV_HANDLE_T device, TDL_DISP_FRAME_BUFF_T *frame_buff)
{
    OPERATE_RET rt = OPRT_OK;

    if (NULL == device || NULL == frame_buff) {
        return OPRT_INVALID_PARM;
    }

    sl_tuya_display_flush(frame_buff->width, frame_buff->height, frame_buff->frame, frame_buff->len);

    return rt;
}

static OPERATE_RET __tdl_display_custom_close(TDD_DISP_DEV_HANDLE_T device)
{
    return OPRT_NOT_SUPPORTED;
}

OPERATE_RET tdd_disp_custom_register(char *name)
{
    OPERATE_RET rt = OPRT_OK;
    TDD_DISP_DEV_INFO_T disp_info;
    static uint32_t handle;

    if (NULL == name) {
        return OPRT_INVALID_PARM;
    }

    disp_info.type = TUYA_DISPLAY_SPI;
    disp_info.width = 320;
    disp_info.height = 240;
    disp_info.fmt = TUYA_PIXEL_FMT_RGB565;
    disp_info.rotation = TUYA_DISPLAY_ROTATION_0;
    disp_info.bl.type = TUYA_DISP_BL_TP_NONE;

    TDD_DISP_INTFS_T disp_spi_intfs = {
        .open = __tdl_display_custom_open,
        .flush = __tdl_display_custom_flush,
        .close = __tdl_display_custom_close,
    };

    TUYA_CALL_ERR_RETURN(tdl_disp_device_register(name, (TDD_DISP_DEV_HANDLE_T)&handle, &disp_spi_intfs, &disp_info));

    return OPRT_OK;
}

#endif

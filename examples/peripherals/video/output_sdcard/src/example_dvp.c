/**
 * @file example_dvp.c
 * @version 0.1
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#include "tuya_cloud_types.h"
#include "tal_api.h"
#include "tkl_output.h"

#include "tkl_video_in.h"
#include "tkl_video_enc.h"

#include "tkl_fs.h"
/***********************************************************
************************macro define************************
***********************************************************/
#define CAPTURED_FRAME_PATH_LEN 128
#define VIDEO_FILE_DIR          "/sdcard"

/***********************************************************
***********************typedef define***********************
***********************************************************/


/***********************************************************
***********************variable define**********************
***********************************************************/
static char captured_frame_path[CAPTURED_FRAME_PATH_LEN] = {0};

/***********************************************************
***********************function define**********************
***********************************************************/
static OPERATE_RET  __fs_sd_init(void)
{
    OPERATE_RET rt = OPRT_OK;

    TUYA_CALL_ERR_RETURN(tkl_fs_mount(VIDEO_FILE_DIR, DEV_SDCARD));
    
    PR_NOTICE("mount sd card success ");

    return rt;
} 

static OPERATE_RET __fs_sd_save_file(char *file_path, void *data, uint32_t data_len)
{
    OPERATE_RET rt = OPRT_OK;

    if (NULL == file_path || NULL == data || 0 == data_len) {
        return OPRT_INVALID_PARM;
    }

    // Check if the file already exists
    int is_exist = 0;
    TUYA_CALL_ERR_RETURN(tkl_fs_is_exist(file_path, &is_exist));

    if (is_exist) {
        tkl_fs_remove(file_path);
        PR_DEBUG("remove file %s", file_path);
    }

    // Create the file
    TUYA_FILE file_hdl = tkl_fopen(file_path, "w");
    if (file_hdl == NULL) {
        PR_ERR("Failed to create file %s", file_path);
        return OPRT_FILE_OPEN_FAILED;
    }
    PR_NOTICE("File %s created successfully", file_path);

    // Write data to the file
    int write_len = tkl_fwrite(data, data_len, file_hdl);
    if (write_len != data_len) {
        PR_ERR("Failed to write data to file %s, expected %d bytes, wrote %d bytes", 
                   file_path, data_len, write_len);
        tkl_fclose(file_hdl);
        return OPRT_COM_ERROR;
    }
    PR_NOTICE("Data written to file %s successfully, length: %d bytes", file_path, write_len);

    // Close the file
    TUYA_CALL_ERR_RETURN(tkl_fclose(file_hdl));

    return rt;
}

static int __dvp_h264_cb(TKL_VENC_FRAME_T *pframe)
{
    OPERATE_RET rt = OPRT_OK;

    if (NULL == pframe || NULL == pframe->pbuf || 0 == pframe->buf_size) {
        return 0;
    }

    if (pframe->frametype != TKL_VIDEO_I_FRAME) {
        return 0;
    }

    memset(captured_frame_path, 0, CAPTURED_FRAME_PATH_LEN);

    POSIX_TM_S local_tm;
    memset((uint8_t *)&local_tm, 0x00, SIZEOF(POSIX_TM_S));
    rt = tal_time_get_local_time_custom(0, &local_tm);
    if (rt != OPRT_OK) {
        PR_ERR("Failed to get local time, rt = %d", rt);
        return 0;
    }
    snprintf(captured_frame_path, CAPTURED_FRAME_PATH_LEN, "%s/%02d_%02d_%02d",\
            VIDEO_FILE_DIR, local_tm.tm_hour, local_tm.tm_min, local_tm.tm_sec);
    PR_NOTICE("File name: %s", captured_frame_path);

    // Save the frame to a file
    rt = __fs_sd_save_file(captured_frame_path, pframe->pbuf, pframe->buf_size);
    if (rt != OPRT_OK) {
        PR_ERR("Failed to save file, rt = %d", rt);
        return 0;
    }
    PR_DEBUG("File saved successfully: %s", captured_frame_path);

    return 0;
}

static OPERATE_RET __example_dvp_init(void)
{
    OPERATE_RET rt = OPRT_OK;
    TKL_VI_CONFIG_T vi_config;
    TKL_VI_EXT_CONFIG_T ext_conf;

    ext_conf.type = TKL_VI_EXT_CONF_CAMERA;
    ext_conf.camera.camera_type = TKL_VI_CAMERA_TYPE_DVP;
    ext_conf.camera.fmt = TKL_CODEC_VIDEO_H264;

    ext_conf.camera.power_pin = TUYA_GPIO_NUM_51;
    ext_conf.camera.active_level = TUYA_GPIO_LEVEL_HIGH;
    ext_conf.camera.i2c.clk = TUYA_GPIO_NUM_13;
    ext_conf.camera.i2c.sda = TUYA_GPIO_NUM_15;
    ext_conf.camera.fps = 15;
    ext_conf.camera.height = 480;
    ext_conf.camera.width = 480;

    vi_config.isp.width = 480;
    vi_config.isp.height = 320;
    vi_config.isp.fps = 15;

    vi_config.pdata = &ext_conf;
    vi_config.disp_cb = NULL;

    TUYA_CALL_ERR_RETURN(tkl_vi_init(&vi_config, 0));

    TKL_VENC_CONFIG_T h264_config;
    // DVP:0; UVC:1
    h264_config.enable_h264_pipeline = 0; // dvp
    h264_config.put_cb = __dvp_h264_cb;
    PR_NOTICE("h264_config.put_cb:%p", h264_config.put_cb);
    // TUYA_CALL_ERR_RETURN(tkl_venc_init(0, &h264_config, 0));
 
    return rt;
}

/**
 * @brief user_main
 *
 * @return none
 */
void user_main(void)
{
    OPERATE_RET rt = OPRT_OK;

    /* basic init */
    tal_log_init(TAL_LOG_LEVEL_DEBUG, 1024, (TAL_LOG_OUTPUT_CB)tkl_log_output);

    PR_NOTICE("Application information:");
    PR_NOTICE("Project name:        %s", PROJECT_NAME);
    PR_NOTICE("App version:         %s", PROJECT_VERSION);
    PR_NOTICE("Compile time:        %s", __DATE__);
    PR_NOTICE("TuyaOpen version:    %s", OPEN_VERSION);
    PR_NOTICE("TuyaOpen commit-id:  %s", OPEN_COMMIT);
    PR_NOTICE("Platform chip:       %s", PLATFORM_CHIP);
    PR_NOTICE("Platform board:      %s", PLATFORM_BOARD);
    PR_NOTICE("Platform commit-id:  %s", PLATFORM_COMMIT);

    TUYA_CALL_ERR_LOG(__fs_sd_init());

    TUYA_CALL_ERR_LOG(__example_dvp_init());

    while(1) {
        tal_system_sleep(10*1000);
        PR_NOTICE("dvp is running, captured frame path: %s", captured_frame_path);
    }

    return;
}

/**
 * @brief main
 *
 * @param argc
 * @param argv
 * @return void
 */
#if OPERATING_SYSTEM == SYSTEM_LINUX
void main(int argc, char *argv[])
{
    user_main();

    while (1) {
        tal_system_sleep(500);
    }
}
#else

/* Tuya thread handle */
static THREAD_HANDLE ty_app_thread = NULL;

/**
 * @brief  task thread
 *
 * @param[in] arg:Parameters when creating a task
 * @return none
 */
static void tuya_app_thread(void *arg)
{
    user_main();

    tal_thread_delete(ty_app_thread);
    ty_app_thread = NULL;
}

void tuya_app_main(void)
{
    THREAD_CFG_T thrd_param = {4096, 4, "tuya_app_main"};
    tal_thread_create_and_start(&ty_app_thread, NULL, NULL, tuya_app_thread, NULL, &thrd_param);
}
#endif
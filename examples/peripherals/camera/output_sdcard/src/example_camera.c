/**
 * @file example_camera.c
 * @version 0.1
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#include "tuya_cloud_types.h"

#include "tal_api.h"
#include "tkl_output.h"
#include "tkl_fs.h"

#include "board_com_api.h"


#include "tdl_camera_manage.h"
/***********************************************************
*************************micro define***********************
***********************************************************/
#define CAPTURED_FRAME_PATH_LEN 128
#define VIDEO_FILE_DIR          "/sdcard"

/***********************************************************
***********************typedef define***********************
***********************************************************/

/***********************************************************
***********************variable define**********************
***********************************************************/
static TDL_CAMERA_HANDLE_T sg_tdl_camera_hdl = NULL;
static bool sg_is_sdcard_init = false;
static char sg_captured_frame_path[CAPTURED_FRAME_PATH_LEN] = {0};

/***********************************************************
***********************function define**********************
***********************************************************/
static OPERATE_RET __sdcard_save_file(char *file_path, void *data, uint32_t data_len)
{
    OPERATE_RET rt = OPRT_OK;

    if (NULL == file_path || NULL == data || 0 == data_len) {
        return OPRT_INVALID_PARM;
    }

    // Check if the file already exists
    int is_exist = 0;
    tkl_fs_is_exist(file_path, &is_exist);

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

OPERATE_RET __get_camera_h264_frame_cb(TDL_CAMERA_HANDLE_T hdl,  TDL_CAMERA_FRAME_T *frame)
{

    OPERATE_RET rt = OPRT_OK;

    if (NULL == hdl || NULL == frame->data || 0 == frame->data_len) {
        return 0;
    }

    if (0 == frame->is_i_frame) {
        return 0;
    }

    if(false == sg_is_sdcard_init){
        return 0;
    }

    memset(sg_captured_frame_path, 0, CAPTURED_FRAME_PATH_LEN);

    POSIX_TM_S local_tm;
    memset((uint8_t *)&local_tm, 0x00, SIZEOF(POSIX_TM_S));
    rt = tal_time_get_local_time_custom(0, &local_tm);
    if (rt != OPRT_OK) {
        PR_ERR("Failed to get local time, rt = %d", rt);
        return 0;
    }
    snprintf(sg_captured_frame_path, CAPTURED_FRAME_PATH_LEN, "%s/%02d_%02d_%02d",\
            VIDEO_FILE_DIR, local_tm.tm_hour, local_tm.tm_min, local_tm.tm_sec);
    PR_NOTICE("File name: %s", sg_captured_frame_path);

    // Save the frame to a file
    rt = __sdcard_save_file(sg_captured_frame_path, frame->data, frame->data_len);
    if (rt != OPRT_OK) {
        PR_ERR("Failed to save file, rt = %d", rt);
        return 0;
    }
    PR_DEBUG("File saved successfully: %s", sg_captured_frame_path);

    return 0;   


}

static OPERATE_RET __sdcard_init(void)
{
    OPERATE_RET rt = OPRT_OK;

    TUYA_CALL_ERR_RETURN(tkl_fs_mount(VIDEO_FILE_DIR, DEV_SDCARD));

    sg_is_sdcard_init = true;
    
    PR_NOTICE("mount sd card success ");


    return rt;
}

static OPERATE_RET __camera_init(void)
{
    OPERATE_RET rt = OPRT_OK;
    TDL_CAMERA_CFG_T cfg;

    memset(&cfg, 0, sizeof(TDL_CAMERA_CFG_T));

    sg_tdl_camera_hdl = tdl_camera_find_dev(CAMERA_NAME);
    if(NULL == sg_tdl_camera_hdl) {
        PR_ERR("camera dev %s not found", CAMERA_NAME);
        return OPRT_NOT_FOUND;
    }

    cfg.fps     = EXAMPLE_CAMERA_FPS;
    cfg.width   = EXAMPLE_CAMERA_WIDTH;
    cfg.height  = EXAMPLE_CAMERA_HEIGHT;
    cfg.out_fmt = TDL_CAMERA_FMT_H264;

    cfg.get_encoded_frame_cb = __get_camera_h264_frame_cb;

    TUYA_CALL_ERR_RETURN(tdl_camera_dev_open(sg_tdl_camera_hdl, &cfg));

    PR_NOTICE("camera init success");

    return OPRT_OK;
}


/**
 * @brief user_main
 *
 * @param[in] param:Task parameters
 * @return none
 */
void user_main(void)
{
    OPERATE_RET rt = OPRT_OK;

    /* basic init */
    tal_log_init(TAL_LOG_LEVEL_DEBUG, 4096, (TAL_LOG_OUTPUT_CB)tkl_log_output);

    /*hardware register*/
    board_register_hardware();

    TUYA_CALL_ERR_LOG(__sdcard_init());

    TUYA_CALL_ERR_LOG(__camera_init());

    while(1) {
        tal_system_sleep(1000);
    }

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
    (void) arg;

    user_main();

    tal_thread_delete(ty_app_thread);
    ty_app_thread = NULL;
}

void tuya_app_main(void)
{
    THREAD_CFG_T thrd_param = {0};
    thrd_param.stackDepth = 1024 * 4;
    thrd_param.priority = THREAD_PRIO_1;
    thrd_param.thrdname = "tuya_app_main";
    tal_thread_create_and_start(&ty_app_thread, NULL, NULL, tuya_app_thread, NULL, &thrd_param);
}
#endif

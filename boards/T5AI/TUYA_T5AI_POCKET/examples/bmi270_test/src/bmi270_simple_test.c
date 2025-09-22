/**
 * @file bmi270_simple_test.c
 * @author Tuya Inc.
 * @brief Simple BMI270 sensor test application
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#include "tal_api.h"
#include "tkl_output.h"
#include "tal_cli.h"
#include "tal_system.h"
#include "board_bmi270_api.h"

/**
 * @brief Simple BMI270 test function
 */
static void bmi270_simple_test(void)
{
    bmi270_dev_t bmi270_dev;
    bmi270_sensor_data_t sensor_data;
    OPERATE_RET ret;
    
    PR_DEBUG("=== BMI270 Simple Test ===");
    
    /* Initialize BMI270 */
    ret = board_bmi270_init(&bmi270_dev);
    if (ret != OPRT_OK) {
        PR_ERR("BMI270 initialization failed: %d", ret);
        return;
    }
    
    PR_DEBUG("BMI270 initialized successfully");
    
    /* Force reset to ensure clean state */
    PR_DEBUG("Forcing sensor reset to clean state...");
    ret = board_bmi270_force_reset(&bmi270_dev);
    if (ret != OPRT_OK) {
        PR_ERR("BMI270 force reset failed: %d", ret);
        return;
    }
    
    /* Wait for sensor to stabilize */
    PR_DEBUG("Waiting for sensor to stabilize...");
    tal_system_sleep(1000);
    
    /* Check if sensor is ready */
    if (!board_bmi270_is_ready(&bmi270_dev)) {
        PR_ERR("BMI270 sensor is not ready");
        return;
    }
    PR_DEBUG("BMI270 sensor is ready");
    
    /* Read sensor data multiple times to check for variations */
    PR_DEBUG("Reading initial sensor data...");
    for (int i = 0; i < 5; i++) {
        ret = board_bmi270_read_data(&bmi270_dev, &sensor_data);
        if (ret == OPRT_OK) {
            PR_DEBUG("Sample %d: Acc(%d,%d,%d) Gyr(%d,%d,%d)", 
                     i + 1, sensor_data.acc_x, sensor_data.acc_y, sensor_data.acc_z,
                     sensor_data.gyr_x, sensor_data.gyr_y, sensor_data.gyr_z);
        } else {
            PR_ERR("Sample %d: Read failed: %d", i + 1, ret);
        }
        tal_system_sleep(200);
    }
    
    /* Read sensor data */
    ret = board_bmi270_read_data(&bmi270_dev, &sensor_data);
    if (ret == OPRT_OK) {
        PR_DEBUG("Raw Sensor Data:");
        PR_DEBUG("  Accelerometer: X=%d, Y=%d, Z=%d", 
                 sensor_data.acc_x, sensor_data.acc_y, sensor_data.acc_z);
        PR_DEBUG("  Gyroscope: X=%d, Y=%d, Z=%d", 
                 sensor_data.gyr_x, sensor_data.gyr_y, sensor_data.gyr_z);
        
        /* Convert to physical units */
        float acc_x_g = (float)sensor_data.acc_x * 2.0f / 32768.0f;
        float acc_y_g = (float)sensor_data.acc_y * 2.0f / 32768.0f;
        float acc_z_g = (float)sensor_data.acc_z * 2.0f / 32768.0f;
        float gyr_x_dps = (float)sensor_data.gyr_x * 2000.0f / 32768.0f;
        float gyr_y_dps = (float)sensor_data.gyr_y * 2000.0f / 32768.0f;
        float gyr_z_dps = (float)sensor_data.gyr_z * 2000.0f / 32768.0f;
        
        PR_DEBUG("Physical Units:");
        PR_DEBUG("  Accelerometer: X=%.3fg, Y=%.3fg, Z=%.3fg", acc_x_g, acc_y_g, acc_z_g);
        PR_DEBUG("  Gyroscope: X=%.1fdps, Y=%.1fdps, Z=%.1fdps", gyr_x_dps, gyr_y_dps, gyr_z_dps);
        
        /* Check for sensor issues */
        if (sensor_data.gyr_z == -32768) {
            PR_ERR("WARNING: Gyroscope Z-axis stuck at minimum value - possible sensor issue");
        }
        if (sensor_data.acc_y == 0) {
            PR_ERR("WARNING: Accelerometer Y-axis stuck at zero - possible sensor issue");
        }
    } else {
        PR_ERR("Failed to read sensor data: %d", ret);
    }
    
    /* Continuous monitoring for 10 seconds */
    PR_DEBUG("Starting continuous monitoring for 10 seconds...");
    int valid_samples = 0;
    int total_samples = 0;
    
    for (int i = 0; i < 50; i++) {
        ret = board_bmi270_read_data(&bmi270_dev, &sensor_data);
        if (ret == OPRT_OK) {
            valid_samples++;
            float acc_x_g = (float)sensor_data.acc_x * 2.0f / 32768.0f;
            float acc_y_g = (float)sensor_data.acc_y * 2.0f / 32768.0f;
            float acc_z_g = (float)sensor_data.acc_z * 2.0f / 32768.0f;
            float gyr_x_dps = (float)sensor_data.gyr_x * 2000.0f / 32768.0f;
            float gyr_y_dps = (float)sensor_data.gyr_y * 2000.0f / 32768.0f;
            float gyr_z_dps = (float)sensor_data.gyr_z * 2000.0f / 32768.0f;
            
            PR_DEBUG("Sample %d: Acc(%.3f,%.3f,%.3f)g, Gyr(%.1f,%.1f,%.1f)dps", 
                     i + 1, acc_x_g, acc_y_g, acc_z_g, gyr_x_dps, gyr_y_dps, gyr_z_dps);
        } else {
            PR_ERR("Sample %d: Read failed: %d", i + 1, ret);
        }
        total_samples++;
        tal_system_sleep(200); /* 200ms interval */
    }
    
    PR_DEBUG("Monitoring completed: %d/%d valid samples", valid_samples, total_samples);
    
    /* Deinitialize BMI270 */
    board_bmi270_deinit(&bmi270_dev);
    PR_DEBUG("BMI270 test completed");
}

/**
 * @brief Main test function
 */
static void user_main(void)
{
    tal_log_init(TAL_LOG_LEVEL_DEBUG, 1024, (TAL_LOG_OUTPUT_CB)tkl_log_output);
    PR_DEBUG("BMI270 Simple Test Application Starting...");
    
    /* Wait for system to stabilize */
    tal_system_sleep(2000);
    
    /* Run simple test */
    bmi270_simple_test();
    
    PR_DEBUG("BMI270 Simple Test Application Completed");
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
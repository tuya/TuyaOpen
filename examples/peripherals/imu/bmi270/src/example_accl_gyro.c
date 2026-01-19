/**
 * @file example_accl_gyro.c
 * @brief example_accl_gyro module is used to demonstrate the usage of accelerometer and gyroscope peripherals.
 * @version 0.1
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#include "tuya_cloud_types.h"
#include "tal_api.h"
#include "tkl_output.h"
#include "tkl_pinmux.h"
#include "tkl_i2c.h"
#include "bmi270.h"
#include "bmi270_common.h"

/***********************************************************
************************macro define************************
***********************************************************/
/*! Earth's gravity in m/s^2 */
#define GRAVITY_EARTH  (9.80665f)
/*! Macros to select the sensors                   */
#define ACCEL          UINT8_C(0x00)
#define GYRO           UINT8_C(0x01)

/* BMI270 I2C Configuration */
#define BMI270_I2C_PORT              TUYA_I2C_NUM_0
#define BMI270_I2C_ADDR              BMI2_I2C_PRIM_ADDR  /* BMI270 I2C address (ADDR pin = 0) */
#define BMI270_I2C_ADDR_ALT          BMI2_I2C_SEC_ADDR   /* BMI270 I2C address (ADDR pin = 1) */

#ifndef EXAMPLE_I2C_SCL_PIN
#define EXAMPLE_I2C_SCL_PIN TUYA_GPIO_NUM_20
#endif

#ifndef EXAMPLE_I2C_SDA_PIN
#define EXAMPLE_I2C_SDA_PIN TUYA_GPIO_NUM_21
#endif
/***********************************************************
***********************typedef define***********************
***********************************************************/

/***********************************************************
***********************variable define**********************
***********************************************************/
/* I2C configuration for BMI270 */
static TUYA_IIC_BASE_CFG_T g_bmi270_i2c_cfg = {
    .role = TUYA_IIC_MODE_MASTER,
    .speed = TUYA_IIC_BUS_SPEED_100K,
    .addr_width = TUYA_IIC_ADDRESS_7BIT
};
/***********************************************************
***********************function define**********************
***********************************************************/
static int8_t set_accel_gyro_config(struct bmi2_dev *bmi2_dev);
static float lsb_to_mps2(int16_t val, float g_range, uint8_t bit_width);
static float lsb_to_dps(int16_t val, float dps, uint8_t bit_width);

/**
 * @brief user_main
 *
 * @return none
 */
void user_main(void)
{
    /* Status of api are returned to this variable. */
    OPERATE_RET ret = OPRT_OK;

    /* Assign accel and gyro sensor to variable. */
    uint8_t sensor_list[2] = { BMI2_ACCEL, BMI2_GYRO };

    /* Sensor initialization configuration. */
    struct bmi2_dev bmi2_dev;

    /* Create an instance of sensor data structure. */
    struct bmi2_sens_data sensor_data = { { 0 } };

    /* Initialize the interrupt status of accel and gyro. */
    uint16_t int_status = 0;

    float x = 0, y = 0, z = 0;

    /* basic init */
    tal_log_init(TAL_LOG_LEVEL_DEBUG, 4096, (TAL_LOG_OUTPUT_CB)tkl_log_output);

    PR_NOTICE("Application information:");
    PR_NOTICE("Project name:        %s", PROJECT_NAME);
    PR_NOTICE("App version:         %s", PROJECT_VERSION);
    PR_NOTICE("Compile time:        %s", __DATE__);
    PR_NOTICE("TuyaOpen version:    %s", OPEN_VERSION);
    PR_NOTICE("TuyaOpen commit-id:  %s", OPEN_COMMIT);
    PR_NOTICE("Platform chip:       %s", PLATFORM_CHIP);
    PR_NOTICE("Platform board:      %s", PLATFORM_BOARD);
    PR_NOTICE("Platform commit-id:  %s", PLATFORM_COMMIT);

    /* Configure I2C pins */
    tkl_io_pinmux_config(EXAMPLE_I2C_SCL_PIN, TUYA_IIC0_SCL);
    tkl_io_pinmux_config(EXAMPLE_I2C_SDA_PIN, TUYA_IIC0_SDA);

    /* Initialize I2C */
    ret = tkl_i2c_init(BMI270_I2C_PORT, &g_bmi270_i2c_cfg);
    if (ret != OPRT_OK) {
        PR_ERR("Failed to initialize I2C: %d", ret);
        return;
    }

    /* Interface reference is given as a parameter
     * For I2C : BMI2_I2C_INTF
     * For SPI : BMI2_SPI_INTF
     */
    uint8_t i2c_port = BMI270_I2C_PORT;
    bmi2_dev.intf_ptr = &(i2c_port);
    ret = bmi2_interface_init(&bmi2_dev, BMI2_I2C_INTF);
    bmi2_error_codes_print_result(ret);

    /* Initialize bmi270. */
    ret = bmi270_init(&bmi2_dev);
    bmi2_error_codes_print_result(ret);

    /* Accel and gyro configuration settings. */
    ret = set_accel_gyro_config(&bmi2_dev);
    bmi2_error_codes_print_result(ret);

    /* NOTE:
    * Accel and Gyro enable must be done after setting configurations
    */
    ret = bmi270_sensor_enable(sensor_list, 2, &bmi2_dev);
    bmi2_error_codes_print_result(ret);

    while(1) {
        /* To get the data ready interrupt status of accel and gyro. */
        ret = bmi2_get_int_status(&int_status, &bmi2_dev);
        bmi2_error_codes_print_result(ret);

        /* To check the data ready interrupt status and print the status for 10 samples. */
        if ((int_status & BMI2_ACC_DRDY_INT_MASK) && (int_status & BMI2_GYR_DRDY_INT_MASK))
        {
            /* Get accel and gyro data for x, y and z axis. */
            ret = bmi2_get_sensor_data(&sensor_data, &bmi2_dev);
            bmi2_error_codes_print_result(ret);

            PR_DEBUG("--------------------------- Sensor Data -------------------------------");
            PR_DEBUG("|                           Accelerometer Data                        |");
            PR_DEBUG("-----------------------------------------------------------------------");
            PR_DEBUG("| Raw Data (LSB)   |  X: %6d     |  Y: %6d     |  Z: %6d     |", 
                   sensor_data.acc.x, sensor_data.acc.y, sensor_data.acc.z);

            /* Converting lsb to meter per second squared for 16 bit accelerometer at 2G range. */
            x = lsb_to_mps2(sensor_data.acc.x, 2, bmi2_dev.resolution);
            y = lsb_to_mps2(sensor_data.acc.y, 2, bmi2_dev.resolution);
            z = lsb_to_mps2(sensor_data.acc.z, 2, bmi2_dev.resolution);

            PR_DEBUG("| Value (m/s²)     |  X: %6.2f     |  Y: %6.2f     |  Z: %6.2f     |", x, y, z);
            PR_DEBUG("-----------------------------------------------------------------------");
            PR_DEBUG("|                           Gyroscope Data                            |");
            PR_DEBUG("-----------------------------------------------------------------------");
            PR_DEBUG("| Raw Data (LSB)   |  X: %6d     |  Y: %6d     |  Z: %6d     |",
                   sensor_data.gyr.x, sensor_data.gyr.y, sensor_data.gyr.z);

            /* Converting lsb to degree per second for 16 bit gyro at 2000dps range. */
            x = lsb_to_dps(sensor_data.gyr.x, 2000, bmi2_dev.resolution);
            y = lsb_to_dps(sensor_data.gyr.y, 2000, bmi2_dev.resolution);
            z = lsb_to_dps(sensor_data.gyr.z, 2000, bmi2_dev.resolution);

            PR_DEBUG("| Value (dps)      |  X: %6.2f     |  Y: %6.2f     |  Z: %6.2f     |", x, y, z);
            PR_DEBUG("-----------------------------------------------------------------------\n");
        }       
        tal_system_sleep(1000);
    }

    return;
}

/*!
 * @brief This internal API is used to set configurations for accel and gyro.
 */
static int8_t set_accel_gyro_config(struct bmi2_dev *bmi2_dev)
{
    /* Status of api are returned to this variable. */
    int8_t ret;

    /* Structure to define accelerometer and gyro configuration. */
    struct bmi2_sens_config config[2];

    /* Configure the type of feature. */
    config[ACCEL].type = BMI2_ACCEL;
    config[GYRO].type = BMI2_GYRO;

    /* Get default configurations for the type of feature selected. */
    ret = bmi270_get_sensor_config(config, 2, bmi2_dev);
    bmi2_error_codes_print_result(ret);

    /* Map data ready interrupt to interrupt pin. */
    ret = bmi2_map_data_int(BMI2_DRDY_INT, BMI2_INT1, bmi2_dev);
    bmi2_error_codes_print_result(ret);

    if (ret == BMI2_OK)
    {
        /* NOTE: The user can change the following configuration parameters according to their requirement. */
        /* Set Output Data Rate */
        config[ACCEL].cfg.acc.odr = BMI2_ACC_ODR_200HZ;

        /* Gravity range of the sensor (-/- 2G, 4G, 8G, 16G). */
        config[ACCEL].cfg.acc.range = BMI2_ACC_RANGE_2G;

        /* The bandwidth parameter is used to configure the number of sensor samples that are averaged
         * if it is set to 2, then 2^(bandwidth parameter) samples
         * are averaged, resulting in 4 averaged samples.
         * Note1 : For more information, refer the datasheet.
         * Note2 : A higher number of averaged samples will result in a lower noise level of the signal, but
         * this has an adverse effect on the power consumed.
         */
        config[ACCEL].cfg.acc.bwp = BMI2_ACC_NORMAL_AVG4;

        /* Enable the filter performance mode where averaging of samples
         * will be done based on above set bandwidth and ODR.
         * There are two modes
         *  0 -> Ultra low power mode
         *  1 -> High performance mode(Default)
         * For more info refer datasheet.
         */
        config[ACCEL].cfg.acc.filter_perf = BMI2_PERF_OPT_MODE;

        /* The user can change the following configuration parameters according to their requirement. */
        /* Set Output Data Rate */
        config[GYRO].cfg.gyr.odr = BMI2_GYR_ODR_200HZ;

        /* Gyroscope Angular Rate Measurement Range.By default the range is 2000dps. */
        config[GYRO].cfg.gyr.range = BMI2_GYR_RANGE_2000;

        /* Gyroscope bandwidth parameters. By default the gyro bandwidth is in normal mode. */
        config[GYRO].cfg.gyr.bwp = BMI2_GYR_NORMAL_MODE;

        /* Enable/Disable the noise performance mode for precision yaw rate sensing
         * There are two modes
         *  0 -> Ultra low power mode(Default)
         *  1 -> High performance mode
         */
        config[GYRO].cfg.gyr.noise_perf = BMI2_POWER_OPT_MODE;

        /* Enable/Disable the filter performance mode where averaging of samples
         * will be done based on above set bandwidth and ODR.
         * There are two modes
         *  0 -> Ultra low power mode
         *  1 -> High performance mode(Default)
         */
        config[GYRO].cfg.gyr.filter_perf = BMI2_PERF_OPT_MODE;

        /* Set the accel and gyro configurations. */
        ret = bmi270_set_sensor_config(config, 2, bmi2_dev);
        bmi2_error_codes_print_result(ret);
    }

    return ret;
}

/*!
 * @brief This function converts lsb to meter per second squared for 16 bit accelerometer at
 * range 2G, 4G, 8G or 16G.
 */
static float lsb_to_mps2(int16_t val, float g_range, uint8_t bit_width)
{
    float half_scale = ((float)(1 << bit_width) / 2.0f);

    return (GRAVITY_EARTH * val * g_range) / half_scale;
}

/*!
 * @brief This function converts lsb to degree per second for 16 bit gyro at
 * range 125, 250, 500, 1000 or 2000dps.
 */
static float lsb_to_dps(int16_t val, float dps, uint8_t bit_width)
{
    float half_scale = ((float)(1 << bit_width) / 2.0f);

    return (dps / ((half_scale))) * (val);
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
    THREAD_CFG_T thrd_param = {0};
    thrd_param.stackDepth = 1024 * 4;
    thrd_param.priority = THREAD_PRIO_1;
    thrd_param.thrdname = "tuya_app_main";
    tal_thread_create_and_start(&ty_app_thread, NULL, NULL, tuya_app_thread, NULL, &thrd_param);
}
#endif
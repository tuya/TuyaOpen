/**
 * @file soil_moisture.c
 * @brief 土壤湿度传感器驱动实现
 * @details 使用TuyaOS ADC API读取土壤湿度传感器的模拟信号
 * 
 * 【硬件连接】
 *     传感器VCC ──── 3.3V
 *     传感器GND ──── GND
 *     传感器AO  ──── GPIO12 (ADC输入)
 * 
 * 【ADC读数与湿度关系】（典型特性）
 * - 土壤干燥 → 传感器阻值高 → 输出电压高 → ADC值大
 * - 土壤湿润 → 传感器阻值低 → 输出电压低 → ADC值小
 * 
 * 【校准说明】
 * 1. 将传感器放在干燥空气中，记录ADC值作为 SOIL_MOISTURE_CAL_DRY
 * 2. 将传感器插入湿润土壤或水中，记录ADC值作为 SOIL_MOISTURE_CAL_WET
 * 3. 根据记录的值修改头文件中的校准参数
 * 
 * @version 1.0
 * @date 2025-01-18
 */

#include "soil_moisture.h"
#include "tkl_adc.h"
#include "tkl_pinmux.h"
#include "tal_system.h"
#include "tal_thread.h"
#include "tal_log.h"

/*============================================================================*/
/*                              私有变量定义                                    */
/*============================================================================*/

static BOOL_T sg_initialized = FALSE;
static UINT8_T sg_adc_channel = 0;
static THREAD_HANDLE sg_thread_handle = NULL;
static BOOL_T sg_thread_running = FALSE;
static BOOL_T sg_thread_stop_flag = FALSE;
static UINT32_T sg_read_interval_ms = 1000;
static SOIL_MOISTURE_CB sg_user_callback = NULL;
static UINT8_T sg_cached_moisture_percent = 50;

/* 用于记录ADC值范围的统计变量 */
static INT32_T sg_adc_min = 4095;
static INT32_T sg_adc_max = 0;
static UINT32_T sg_sample_count = 0;

/*============================================================================*/
/*                              私有函数实现                                    */
/*============================================================================*/

/**
 * @brief 土壤湿度传感器周期性读取线程函数
 */
static VOID_T _soil_moisture_thread(PVOID_T args)
{
    (VOID_T)args;
    
    TAL_PR_NOTICE("==============================================");
    TAL_PR_NOTICE("Soil moisture sensor thread started");
    TAL_PR_NOTICE("GPIO Pin: %d, ADC Channel: %d", SOIL_MOISTURE_GPIO_PIN, sg_adc_channel);
    TAL_PR_NOTICE("Read interval: %d ms", sg_read_interval_ms);
    TAL_PR_NOTICE("==============================================");
    TAL_PR_NOTICE("【校准模式】请观察以下日志确定ADC范围:");
    TAL_PR_NOTICE("  1. 干燥状态(空气中)的ADC值 -> CAL_DRY");
    TAL_PR_NOTICE("  2. 湿润状态(水中/湿土)的ADC值 -> CAL_WET");
    TAL_PR_NOTICE("==============================================");
    
    sg_thread_running = TRUE;
    
    while (!sg_thread_stop_flag) {
        SOIL_MOISTURE_DATA_T data;
        
        if (soil_moisture_read(&data) == OPRT_OK) {
            sg_cached_moisture_percent = data.moisture_percent;
            sg_sample_count++;
            
            /* 更新统计值 */
            if (data.raw_value < sg_adc_min) {
                sg_adc_min = data.raw_value;
            }
            if (data.raw_value > sg_adc_max) {
                sg_adc_max = data.raw_value;
            }
            
            /* 详细日志输出，便于校准 */
            TAL_PR_NOTICE("--------------------------------------------");
            TAL_PR_NOTICE("[Soil Moisture #%d]", sg_sample_count);
            TAL_PR_NOTICE("  RAW ADC  : %d (范围: 0~4095)", data.raw_value);
            TAL_PR_NOTICE("  Voltage  : %d mV (范围: 0~2400)", data.voltage_mv);
            TAL_PR_NOTICE("  Moisture : %d%% (未校准，仅参考)", data.moisture_percent);
            TAL_PR_NOTICE("  ----------");
            TAL_PR_NOTICE("  统计: MIN=%d, MAX=%d, Range=%d", 
                        sg_adc_min, sg_adc_max, sg_adc_max - sg_adc_min);
            
            if (sg_user_callback) {
                sg_user_callback(&data);
            }
        }
        
        tal_system_sleep(sg_read_interval_ms);
    }
    
    TAL_PR_NOTICE("==============================================");
    TAL_PR_NOTICE("Soil moisture sensor thread exiting");
    TAL_PR_NOTICE("Final statistics: MIN=%d, MAX=%d, Samples=%d", 
                sg_adc_min, sg_adc_max, sg_sample_count);
    TAL_PR_NOTICE("==============================================");
    
    sg_thread_running = FALSE;
}

/*============================================================================*/
/*                              公共API函数实现                                  */
/*============================================================================*/

OPERATE_RET soil_moisture_init(VOID_T)
{
    OPERATE_RET ret;
    TUYA_ADC_BASE_CFG_T adc_cfg;
    
    if (sg_initialized) {
        TAL_PR_WARN("Soil moisture sensor already initialized");
        return OPRT_OK;
    }
    
    /* GPIO引脚到ADC通道的映射 */
    INT32_T adc_func = tkl_io_pin_to_func(SOIL_MOISTURE_GPIO_PIN, TUYA_IO_TYPE_ADC);
    if (adc_func < 0) {
        TAL_PR_ERR("Failed to get ADC channel for GPIO %d, ret=%d", 
                   SOIL_MOISTURE_GPIO_PIN, adc_func);
        return OPRT_COM_ERROR;
    }
    
    sg_adc_channel = adc_func & 0xFF;
    TAL_PR_NOTICE("Soil moisture sensor using GPIO %d, ADC channel %d", 
                  SOIL_MOISTURE_GPIO_PIN, sg_adc_channel);
    
    /* 重新配置 ADC_NUM_0，同时包含光敏电阻(ADC15)和土壤湿度(ADC14)两个通道 */
    memset(&adc_cfg, 0, sizeof(adc_cfg));
    adc_cfg.ch_list.data = BIT(sg_adc_channel) | BIT(15);  /* ADC14 + ADC15 */
    adc_cfg.ch_nums = 2;
    adc_cfg.width = SOIL_MOISTURE_ADC_WIDTH;
    adc_cfg.type = TUYA_ADC_INNER_SAMPLE_VOL;
    adc_cfg.mode = TUYA_ADC_CONTINUOUS;
    adc_cfg.conv_cnt = 8;
    
    /* 先释放之前的配置，再重新初始化 */
    tkl_adc_deinit(TUYA_ADC_NUM_0);
    
    ret = tkl_adc_init(TUYA_ADC_NUM_0, &adc_cfg);
    if (ret != OPRT_OK) {
        TAL_PR_ERR("ADC init failed for soil moisture, ret=%d", ret);
        return ret;
    }
    
    sg_initialized = TRUE;
    TAL_PR_NOTICE("Soil moisture sensor initialized successfully (ADC ch14+ch15)");
    
    return OPRT_OK;
}

OPERATE_RET soil_moisture_deinit(VOID_T)
{
    if (!sg_initialized) {
        return OPRT_OK;
    }
    
    soil_moisture_stop_periodic();
    /* 不释放ADC，因为是与光敏电阻共享的 */
    sg_initialized = FALSE;
    
    TAL_PR_NOTICE("Soil moisture sensor deinitialized");
    return OPRT_OK;
}

OPERATE_RET soil_moisture_read_raw(INT32_T *raw_value)
{
    OPERATE_RET ret;
    
    if (!sg_initialized) {
        TAL_PR_ERR("Soil moisture sensor not initialized");
        return OPRT_COM_ERROR;
    }
    
    if (raw_value == NULL) {
        return OPRT_INVALID_PARM;
    }
    
    ret = tkl_adc_read_single_channel(TUYA_ADC_NUM_0, sg_adc_channel, raw_value);
    if (ret != OPRT_OK) {
        TAL_PR_ERR("ADC read failed, ret=%d", ret);
        return ret;
    }
    
    return OPRT_OK;
}

OPERATE_RET soil_moisture_read_voltage(INT32_T *voltage_mv)
{
    OPERATE_RET ret;
    INT32_T raw_value;
    
    if (voltage_mv == NULL) {
        return OPRT_INVALID_PARM;
    }
    
    ret = soil_moisture_read_raw(&raw_value);
    if (ret != OPRT_OK) {
        return ret;
    }
    
    *voltage_mv = (raw_value * SOIL_MOISTURE_REF_VOLTAGE_MV) / SOIL_MOISTURE_ADC_MAX;
    return OPRT_OK;
}

OPERATE_RET soil_moisture_read(SOIL_MOISTURE_DATA_T *data)
{
    OPERATE_RET ret;
    
    if (data == NULL) {
        return OPRT_INVALID_PARM;
    }
    
    /* 读取ADC原始值 */
    ret = soil_moisture_read_raw(&data->raw_value);
    if (ret != OPRT_OK) {
        return ret;
    }
    
    /* 计算电压值（mV） */
    data->voltage_mv = (data->raw_value * SOIL_MOISTURE_REF_VOLTAGE_MV) / SOIL_MOISTURE_ADC_MAX;
    
    /* ============ 校准测试模式 ============
     * 暂时禁用校准，直接输出原始值
     * 请记录：
     *   1. 空气中的 RAW ADC 值
     *   2. 水中的 RAW ADC 值
     * 然后告诉我这两个值，我来设置正确的校准参数
     */
    INT32_T percent;
    
    /* 未校准：简单映射到0-100%，仅供参考 */
    percent = (data->raw_value * 100) / SOIL_MOISTURE_ADC_MAX;
    
    /* 边界检查 */
    if (percent < 0) percent = 0;
    if (percent > 100) percent = 100;
    
    data->moisture_percent = (UINT8_T)percent;
    
    return OPRT_OK;
}

UINT8_T soil_moisture_get_percent(VOID_T)
{
    return sg_cached_moisture_percent;
}

OPERATE_RET soil_moisture_start_periodic(UINT32_T interval_ms, SOIL_MOISTURE_CB callback)
{
    OPERATE_RET ret;
    
    THREAD_CFG_T thread_cfg = {
        .priority = THREAD_PRIO_2,
        .stackDepth = 2048,
        .thrdname = "soil_moisture"
    };
    
    if (!sg_initialized) {
        ret = soil_moisture_init();
        if (ret != OPRT_OK) {
            return ret;
        }
    }
    
    if (sg_thread_running) {
        TAL_PR_WARN("Soil moisture periodic read already running");
        return OPRT_OK;
    }
    
    /* 重置统计变量 */
    sg_adc_min = 4095;
    sg_adc_max = 0;
    sg_sample_count = 0;
    
    sg_read_interval_ms = (interval_ms > 0) ? interval_ms : 1000;
    sg_user_callback = callback;
    sg_thread_stop_flag = FALSE;
    
    ret = tal_thread_create_and_start(&sg_thread_handle, NULL, NULL,
                                      _soil_moisture_thread, NULL, &thread_cfg);
    if (ret != OPRT_OK) {
        TAL_PR_ERR("Failed to create soil moisture thread, ret=%d", ret);
        return ret;
    }
    
    return OPRT_OK;
}

OPERATE_RET soil_moisture_stop_periodic(VOID_T)
{
    if (!sg_thread_running) {
        return OPRT_OK;
    }
    
    sg_thread_stop_flag = TRUE;
    
    UINT32_T wait_count = 0;
    while (sg_thread_running && wait_count < 50) {
        tal_system_sleep(100);
        wait_count++;
    }
    
    if (sg_thread_handle) {
        tal_thread_delete(sg_thread_handle);
        sg_thread_handle = NULL;
    }
    
    TAL_PR_NOTICE("Soil moisture periodic read stopped");
    return OPRT_OK;
}

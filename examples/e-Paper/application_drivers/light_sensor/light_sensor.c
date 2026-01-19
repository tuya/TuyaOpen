/**
 * @file light_sensor.c
 * @brief 光敏电阻传感器驱动实现
 * @details 使用TuyaOS ADC API读取光敏电阻的模拟信号，实现环境光照强度检测
 * 
 * 【硬件原理】
 * 光敏电阻（LDR/Light Dependent Resistor）是一种光控可变电阻，其阻值随光照强度变化：
 * - 光照越强 → 阻值越小（几百欧姆）
 * - 光照越弱 → 阻值越大（可达几兆欧姆）
 * 
 * 【电路连接】（典型分压电路）
 *     VCC (3.3V)
 *        │
 *       ┌┴┐
 *       │ │ 光敏电阻 (LDR)
 *       └┬┘
 *        ├──────── GPIO13 (ADC输入)
 *       ┌┴┐
 *       │ │ 固定电阻 (10K)
 *       └┬┘
 *        │
 *       GND
 * 
 * 【ADC读数与光照关系】（当LDR在上端时）
 * - 光照强 → LDR阻值低 → 分压点电压低 → ADC值小
 * - 光照弱 → LDR阻值高 → 分压点电压高 → ADC值大
 * 因此需要反转处理，使光照强时百分比为100%
 * 
 * @version 1.0
 * @date 2025-12-12
 */

#include "light_sensor.h"
#include "tkl_adc.h"        /* TuyaOS ADC底层驱动API */
#include "tkl_pinmux.h"     /* TuyaOS 引脚复用配置API */
#include "tal_system.h"     /* TuyaOS 系统API（延时等） */
#include "tal_thread.h"     /* TuyaOS 线程管理API */
#include "tal_log.h"        /* TuyaOS 日志打印API */

/*============================================================================*/
/*                              私有变量定义                                    */
/*============================================================================*/

/**
 * @brief 模块初始化标志
 * @note TRUE表示ADC已完成初始化，可以进行读取操作
 */
static BOOL_T sg_initialized = FALSE;

/**
 * @brief ADC通道号
 * @note 通过GPIO引脚映射得到，不同GPIO对应不同ADC通道
 *       例如：GPIO13 对应 ADC通道15
 */
static UINT8_T sg_adc_channel = 0;

/**
 * @brief 周期读取线程句柄
 * @note 用于管理后台自动采集线程的生命周期
 */
static THREAD_HANDLE sg_thread_handle = NULL;

/**
 * @brief 线程运行状态标志
 * @note TRUE表示线程正在运行中，用于防止重复启动
 */
static BOOL_T sg_thread_running = FALSE;

/**
 * @brief 线程停止请求标志
 * @note 设为TRUE时，线程会在下一个循环周期中退出
 *       这是一种优雅的线程退出机制
 */
static BOOL_T sg_thread_stop_flag = FALSE;

/**
 * @brief 周期读取间隔时间（毫秒）
 * @note 默认1000ms，可通过light_sensor_start_periodic()设置
 */
static UINT32_T sg_read_interval_ms = 1000;

/**
 * @brief 用户注册的数据回调函数
 * @note 每次读取到新数据时调用，传递给上层应用处理
 */
static LIGHT_SENSOR_CB sg_user_callback = NULL;

/**
 * @brief 缓存的光照百分比值
 * @note 用于线程安全地获取最新光照值
 *       其他模块调用light_sensor_get_light_percent()时返回此值
 *       避免多线程同时访问ADC导致的资源竞争
 */
static UINT8_T sg_cached_light_percent = 50;

/*============================================================================*/
/*                              私有函数实现                                    */
/*============================================================================*/

/**
 * @brief 光敏传感器周期性读取线程函数
 * @param[in] args 线程参数（未使用）
 * 
 * @details 线程工作流程：
 *          1. 进入循环，检查停止标志
 *          2. 调用light_sensor_read()读取完整传感器数据
 *          3. 更新缓存值（供其他线程安全获取）
 *          4. 打印调试日志
 *          5. 调用用户回调函数（如果已注册）
 *          6. 休眠指定间隔时间
 *          7. 重复步骤1-6，直到停止标志被设置
 * 
 * @note 线程优先级为THREAD_PRIO_2（中等优先级）
 *       栈大小为2048字节，对于简单的ADC读取足够
 */
static VOID_T _light_sensor_thread(PVOID_T args)
{
    /* 消除未使用参数的编译警告 */
    (VOID_T)args;
    
    /* 打印线程启动信息，便于调试 */
    TAL_PR_NOTICE("Light sensor thread started, interval=%dms", sg_read_interval_ms);
    
    /* 设置运行标志，表示线程已开始工作 */
    sg_thread_running = TRUE;
    
    /*=== 主循环：持续读取传感器数据直到收到停止信号 ===*/
    while (!sg_thread_stop_flag) {
        /* 定义传感器数据结构，用于存储读取结果 */
        LIGHT_SENSOR_DATA_T data;
        
        /* 读取传感器完整数据（原始值、电压、百分比） */
        if (light_sensor_read(&data) == OPRT_OK) {
            
            /*--- 步骤1: 更新缓存值 ---*/
            /* 将最新的光照百分比保存到缓存变量
             * 这样其他线程可以通过light_sensor_get_light_percent()
             * 安全地获取光照值，而不需要直接访问ADC硬件 */
            sg_cached_light_percent = data.light_percent;
            
            /*--- 步骤2: 打印调试日志 ---*/
            /* 输出原始ADC值、电压值和光照百分比，便于调试和校准 */
            TAL_PR_NOTICE("Light: raw=%d, voltage=%dmV, percent=%d%%", 
                        data.raw_value, data.voltage_mv, data.light_percent);
            
            /*--- 步骤3: 调用用户回调函数 ---*/
            /* 如果用户注册了回调函数，将数据传递给上层应用
             * 回调函数可用于：触发亮度调节、记录数据、联动其他设备等 */
            if (sg_user_callback) {
                sg_user_callback(&data);
            }
        }
        
        /*--- 步骤4: 休眠等待下一次采集 ---*/
        /* 释放CPU资源，等待指定间隔后再次采集
         * 间隔时间越短，响应越灵敏，但CPU占用越高 */
        tal_system_sleep(sg_read_interval_ms);
    }
    
    /* 线程即将退出，打印通知信息 */
    TAL_PR_NOTICE("Light sensor thread exiting");
    
    /* 清除运行标志，表示线程已停止 */
    sg_thread_running = FALSE;
}

/*============================================================================*/
/*                              公共API函数实现                                  */
/*============================================================================*/

/**
 * @brief 初始化光敏传感器模块
 * @return OPRT_OK: 初始化成功
 *         OPRT_COM_ERROR: GPIO到ADC通道映射失败
 *         其他: ADC初始化失败
 * 
 * @details 初始化流程：
 *          1. 检查是否已初始化（避免重复初始化）
 *          2. 将GPIO引脚映射到对应的ADC通道
 *          3. 配置ADC参数（位宽、采样模式等）
 *          4. 调用底层API完成ADC硬件初始化
 * 
 * @note 使用前必须先调用此函数
 *       如果调用light_sensor_start_periodic()时未初始化，
 *       会自动调用本函数进行初始化
 */
OPERATE_RET light_sensor_init(VOID_T)
{
    OPERATE_RET ret;
    TUYA_ADC_BASE_CFG_T adc_cfg;
    
    /*=== 步骤1: 检查初始化状态 ===*/
    /* 如果已经初始化过，直接返回成功，避免重复配置 */
    if (sg_initialized) {
        TAL_PR_WARN("Light sensor already initialized");
        return OPRT_OK;
    }
    
    /*=== 步骤2: GPIO引脚到ADC通道的映射 ===*/
    /* T5芯片的GPIO和ADC通道有对应关系，需要通过API查询
     * 例如: GPIO13 -> ADC通道15
     * tkl_io_pin_to_func()函数返回该GPIO对应的ADC功能编号
     * 返回值的低8位即为ADC通道号 */
    INT32_T adc_func = tkl_io_pin_to_func(LIGHT_SENSOR_GPIO_PIN, TUYA_IO_TYPE_ADC);
    if (adc_func < 0) {
        /* 映射失败，可能是该GPIO不支持ADC功能 */
        TAL_PR_ERR("Failed to get ADC channel for GPIO %d, ret=%d", 
                   LIGHT_SENSOR_GPIO_PIN, adc_func);
        return OPRT_COM_ERROR;
    }
    
    /* 提取ADC通道号（低8位） */
    sg_adc_channel = adc_func & 0xFF;
    TAL_PR_NOTICE("Light sensor using GPIO %d, ADC channel %d", 
                  LIGHT_SENSOR_GPIO_PIN, sg_adc_channel);
    
    /*=== 步骤3: 配置ADC参数 ===*/
    /* 清零配置结构体，确保所有字段初始化 */
    memset(&adc_cfg, 0, sizeof(adc_cfg));
    
    /* ch_list.data: 使用位图方式指定要使用的通道
     * 例如：通道5 -> BIT(5) = 0x20 */
    adc_cfg.ch_list.data = BIT(sg_adc_channel);
    
    /* ch_nums: 同时使用的通道数量，这里只用一个通道 */
    adc_cfg.ch_nums = 1;
    
    /* width: ADC采样位宽，12位对应0~4095的范围
     * 位宽越高，精度越高，但转换时间也越长 */
    adc_cfg.width = LIGHT_SENSOR_ADC_WIDTH;
    
    /* type: 参考电压类型
     * TUYA_ADC_INNER_SAMPLE_VOL: 使用芯片内部参考电压（约2.4V）
     * 也可以选择外部参考电压，但需要额外硬件支持 */
    adc_cfg.type = TUYA_ADC_INNER_SAMPLE_VOL;
    
    /* mode: 采样模式
     * TUYA_ADC_CONTINUOUS: 连续采样模式，ADC持续工作
     * TUYA_ADC_SINGLE: 单次采样模式，触发后只采样一次 */
    adc_cfg.mode = TUYA_ADC_CONTINUOUS;
    
    /* conv_cnt: 每次读取时进行多少次采样并取平均
     * 增加采样次数可以减少噪声干扰，提高读数稳定性
     * 但会增加每次读取的时间 */
    adc_cfg.conv_cnt = 8;
    
    /*=== 步骤4: 执行ADC硬件初始化 ===*/
    ret = tkl_adc_init(TUYA_ADC_NUM_0, &adc_cfg);
    if (ret != OPRT_OK) {
        TAL_PR_ERR("ADC init failed, ret=%d", ret);
        return ret;
    }
    
    /* 标记初始化完成 */
    sg_initialized = TRUE;
    TAL_PR_NOTICE("Light sensor initialized successfully");
    
    return OPRT_OK;
}

/**
 * @brief 反初始化光敏传感器模块
 * @return OPRT_OK: 反初始化成功
 * 
 * @details 释放流程：
 *          1. 停止周期性读取线程（如果正在运行）
 *          2. 释放ADC硬件资源
 *          3. 清除初始化标志
 * 
 * @note 在系统休眠或不需要光照检测时调用，可降低功耗
 */
OPERATE_RET light_sensor_deinit(VOID_T)
{
    /* 如果未初始化，无需反初始化 */
    if (!sg_initialized) {
        return OPRT_OK;
    }
    
    /* 首先停止周期性读取线程，避免访问已释放的资源 */
    light_sensor_stop_periodic();
    
    /* 释放ADC硬件资源 */
    tkl_adc_deinit(TUYA_ADC_NUM_0);
    
    /* 清除初始化标志 */
    sg_initialized = FALSE;
    TAL_PR_NOTICE("Light sensor deinitialized");
    
    return OPRT_OK;
}

/**
 * @brief 读取ADC原始值
 * @param[out] raw_value 输出参数，存储ADC原始读数（0~4095）
 * @return OPRT_OK: 读取成功
 *         OPRT_COM_ERROR: 模块未初始化
 *         OPRT_INVALID_PARM: 参数无效（空指针）
 *         其他: ADC读取错误
 * 
 * @details ADC原始值范围说明（12位ADC）：
 *          - 0: 输入电压 = 0V
 *          - 4095: 输入电压 = 参考电压（约2.4V）
 *          - 中间值线性对应
 * 
 * @note 此函数直接访问ADC硬件，如需频繁读取建议使用周期读取模式
 */
OPERATE_RET light_sensor_read_raw(INT32_T *raw_value)
{
    OPERATE_RET ret;
    
    /*=== 参数检查 ===*/
    /* 检查模块是否已初始化 */
    if (!sg_initialized) {
        TAL_PR_ERR("Light sensor not initialized");
        return OPRT_COM_ERROR;
    }
    
    /* 检查输出参数是否有效 */
    if (raw_value == NULL) {
        return OPRT_INVALID_PARM;
    }
    
    /*=== 读取ADC值 ===*/
    /* tkl_adc_read_single_channel(): 从指定通道读取一个ADC值
     * 参数1: ADC设备号（TUYA_ADC_NUM_0）
     * 参数2: 通道号
     * 参数3: 输出值指针 */
    ret = tkl_adc_read_single_channel(TUYA_ADC_NUM_0, sg_adc_channel, raw_value);
    if (ret != OPRT_OK) {
        TAL_PR_ERR("ADC read failed, ret=%d", ret);
        return ret;
    }
    
    return OPRT_OK;
}

/**
 * @brief 读取电压值（毫伏）
 * @param[out] voltage_mv 输出参数，存储计算得到的电压值（mV）
 * @return OPRT_OK: 读取成功
 *         OPRT_INVALID_PARM: 参数无效
 *         其他: 读取错误
 * 
 * @details 电压计算公式：
 *          voltage = raw_value × 参考电压 / ADC最大值
 *          例如: raw=2048 → voltage = 2048 × 2400 / 4095 ≈ 1200mV
 * 
 * @note 返回的电压值是ADC输入引脚的电压，即分压点电压
 */
OPERATE_RET light_sensor_read_voltage(INT32_T *voltage_mv)
{
    OPERATE_RET ret;
    INT32_T raw_value;
    
    /* 检查输出参数 */
    if (voltage_mv == NULL) {
        return OPRT_INVALID_PARM;
    }
    
    /* 首先读取原始ADC值 */
    ret = light_sensor_read_raw(&raw_value);
    if (ret != OPRT_OK) {
        return ret;
    }
    
    /*=== 电压换算 ===*/
    /* 公式: 电压(mV) = ADC原始值 × 参考电压(mV) / ADC最大值
     * LIGHT_SENSOR_REF_VOLTAGE_MV = 2400 (2.4V)
     * LIGHT_SENSOR_ADC_MAX = 4095 (12位ADC) */
    *voltage_mv = (raw_value * LIGHT_SENSOR_REF_VOLTAGE_MV) / LIGHT_SENSOR_ADC_MAX;
    
    return OPRT_OK;
}

/**
 * @brief 读取完整的传感器数据（原始值+电压+光照百分比）
 * @param[out] data 输出参数，存储完整的传感器数据
 * @return OPRT_OK: 读取成功
 *         OPRT_INVALID_PARM: 参数无效
 *         其他: 读取错误
 * 
 * @details 光照百分比计算流程：
 *          1. 读取ADC原始值
 *          2. 计算电压值（可选，用于调试）
 *          3. 使用校准参数将原始值映射到0~100%
 *          4. 根据配置决定是否反转（光强时百分比高）
 * 
 *          【校准参数说明】
 *          LIGHT_SENSOR_CAL_MIN: 强光时的ADC值（如30）
 *          LIGHT_SENSOR_CAL_MAX: 暗环境时的ADC值（如200）
 *          映射公式: percent = (raw - CAL_MIN) × 100 / (CAL_MAX - CAL_MIN)
 * 
 * @note 这是最常用的读取函数，一次调用获取所有数据
 */
OPERATE_RET light_sensor_read(LIGHT_SENSOR_DATA_T *data)
{
    OPERATE_RET ret;
    
    /* 检查输出参数 */
    if (data == NULL) {
        return OPRT_INVALID_PARM;
    }
    
    /*=== 步骤1: 读取ADC原始值 ===*/
    ret = light_sensor_read_raw(&data->raw_value);
    if (ret != OPRT_OK) {
        return ret;
    }
    
    /*=== 步骤2: 计算电压值（mV）===*/
    /* 电压 = 原始值 × 参考电压 / 最大值 */
    data->voltage_mv = (data->raw_value * LIGHT_SENSOR_REF_VOLTAGE_MV) / LIGHT_SENSOR_ADC_MAX;
    
    /*=== 步骤3: 计算光照百分比 ===*/
    /* 使用校准参数进行范围映射，将实际ADC范围映射到0~100% */
    
    /* 计算校准范围 */
    INT32_T cal_range = LIGHT_SENSOR_CAL_MAX - LIGHT_SENSOR_CAL_MIN;
    INT32_T percent;
    
    if (cal_range > 0) {
        /*--- 使用校准参数进行映射 ---*/
        
        /* 首先将原始值限制在校准范围内（钳位处理）
         * 防止计算结果超出0~100范围 */
        INT32_T clamped = data->raw_value;
        if (clamped < LIGHT_SENSOR_CAL_MIN) {
            clamped = LIGHT_SENSOR_CAL_MIN;  /* 超亮环境，限制为最小值 */
        }
        if (clamped > LIGHT_SENSOR_CAL_MAX) {
            clamped = LIGHT_SENSOR_CAL_MAX;  /* 极暗环境，限制为最大值 */
        }
        
        /* 线性映射: 将[CAL_MIN, CAL_MAX]映射到[0, 100]
         * percent = (clamped - CAL_MIN) × 100 / (CAL_MAX - CAL_MIN) */
        percent = ((clamped - LIGHT_SENSOR_CAL_MIN) * 100) / cal_range;
    } else {
        /* 校准参数无效（范围为0或负数），使用默认全范围映射 */
        percent = (data->raw_value * 100) / LIGHT_SENSOR_ADC_MAX;
    }
    
    /*=== 步骤4: 反转处理（可选）===*/
#if LIGHT_SENSOR_INVERT
    /* 光敏电阻特性：光照越强 → 阻值越低 → ADC值越低
     * 为了直观表示（光照强时百分比高），需要反转
     * 反转公式: 最终百分比 = 100 - 原始百分比 */
    percent = 100 - percent;
#endif
    
    /*=== 步骤5: 边界检查 ===*/
    /* 确保最终值在0~100范围内 */
    if (percent < 0) percent = 0;
    if (percent > 100) percent = 100;
    
    /* 转换为8位无符号数存储 */
    data->light_percent = (UINT8_T)percent;
    
    return OPRT_OK;
}

/**
 * @brief 获取光照强度百分比（从缓存读取，线程安全）
 * @return 光照强度百分比（0~100%）
 * 
 * @details 此函数返回由周期读取线程更新的缓存值：
 *          - 不直接访问ADC硬件
 *          - 可从任何线程安全调用
 *          - 值的实时性取决于周期读取间隔
 * 
 *          使用场景：
 *          - 屏幕亮度自动调节
 *          - 光控开关逻辑
 *          - 环境光记录
 * 
 * @note 调用此函数前需先启动周期读取：light_sensor_start_periodic()
 *       否则返回的是初始默认值（50%）
 */
UINT8_T light_sensor_get_light_percent(VOID_T)
{
    /* 直接返回缓存值
     * 缓存值由_light_sensor_thread()线程定期更新
     * 避免多线程同时访问ADC硬件导致的资源竞争问题 */
    return sg_cached_light_percent;
}

/**
 * @brief 启动周期性自动读取（创建独立线程）
 * @param[in] interval_ms 读取间隔时间（毫秒），0或过小会使用默认值1000ms
 * @param[in] callback 数据回调函数，每次读取后调用，可为NULL
 * @return OPRT_OK: 启动成功
 *         其他: 初始化或线程创建失败
 * 
 * @details 功能说明：
 *          - 创建一个后台线程，按指定间隔自动读取传感器数据
 *          - 每次读取后更新缓存值，可通过light_sensor_get_light_percent()获取
 *          - 如果注册了回调函数，每次读取后会调用回调
 * 
 *          线程参数：
 *          - 优先级: THREAD_PRIO_2（中等）
 *          - 栈大小: 2048字节
 *          - 线程名: "light_sensor"
 * 
 * @note 如果模块未初始化，会自动调用light_sensor_init()
 *       重复调用不会创建多个线程，会直接返回成功
 */
OPERATE_RET light_sensor_start_periodic(UINT32_T interval_ms, LIGHT_SENSOR_CB callback)
{
    OPERATE_RET ret;
    
    /* 线程配置参数 */
    THREAD_CFG_T thread_cfg = {
        .priority = THREAD_PRIO_2,      /* 中等优先级，不影响关键任务 */
        .stackDepth = 2048,             /* 2KB栈空间，足够ADC读取使用 */
        .thrdname = "light_sensor"      /* 线程名，便于调试时识别 */
    };
    
    /*=== 步骤1: 确保模块已初始化 ===*/
    if (!sg_initialized) {
        /* 自动执行初始化，简化使用流程 */
        ret = light_sensor_init();
        if (ret != OPRT_OK) {
            return ret;
        }
    }
    
    /*=== 步骤2: 检查是否已在运行 ===*/
    if (sg_thread_running) {
        /* 线程已在运行，避免重复创建 */
        TAL_PR_WARN("Light sensor periodic read already running");
        return OPRT_OK;
    }
    
    /*=== 步骤3: 保存配置参数 ===*/
    /* 间隔时间，至少1ms，默认1000ms */
    sg_read_interval_ms = (interval_ms > 0) ? interval_ms : 1000;
    
    /* 保存用户回调函数 */
    sg_user_callback = callback;
    
    /* 清除停止标志，允许线程运行 */
    sg_thread_stop_flag = FALSE;
    
    /*=== 步骤4: 创建并启动线程 ===*/
    /* tal_thread_create_and_start(): 一步完成线程创建和启动
     * 参数1: 线程句柄输出
     * 参数2: 运行函数（已启动时调用，这里不用）
     * 参数3: 退出函数（线程退出时调用，这里不用）
     * 参数4: 线程入口函数
     * 参数5: 传给线程的参数
     * 参数6: 线程配置 */
    ret = tal_thread_create_and_start(&sg_thread_handle, NULL, NULL,
                                      _light_sensor_thread, NULL, &thread_cfg);
    if (ret != OPRT_OK) {
        TAL_PR_ERR("Failed to create light sensor thread, ret=%d", ret);
        return ret;
    }
    
    return OPRT_OK;
}

/**
 * @brief 停止周期性自动读取
 * @return OPRT_OK: 停止成功
 * 
 * @details 停止流程：
 *          1. 设置停止标志，通知线程退出
 *          2. 等待线程自然退出（最多5秒）
 *          3. 删除线程资源
 * 
 *          优雅退出机制：
 *          - 不强制终止线程，而是设置标志等待其在下一个循环中退出
 *          - 这样可以确保线程正常完成当前操作，避免资源泄漏
 * 
 * @note 如果线程未在运行，直接返回成功
 *       调用此函数后，可以再次调用start_periodic重新启动
 */
OPERATE_RET light_sensor_stop_periodic(VOID_T)
{
    /* 如果线程未运行，无需停止 */
    if (!sg_thread_running) {
        return OPRT_OK;
    }
    
    /*=== 步骤1: 设置停止标志 ===*/
    /* 线程在下一个循环检测到此标志后会自动退出 */
    sg_thread_stop_flag = TRUE;
    
    /*=== 步骤2: 等待线程退出 ===*/
    /* 循环等待，每100ms检查一次，最多等待50次（5秒）
     * 超时后强制继续，避免无限等待 */
    UINT32_T wait_count = 0;
    while (sg_thread_running && wait_count < 50) {
        tal_system_sleep(100);  /* 休眠100ms */
        wait_count++;
    }
    
    /*=== 步骤3: 释放线程资源 ===*/
    if (sg_thread_handle) {
        /* 删除线程，释放系统资源 */
        tal_thread_delete(sg_thread_handle);
        sg_thread_handle = NULL;
    }
    
    TAL_PR_NOTICE("Light sensor periodic read stopped");
    return OPRT_OK;
}

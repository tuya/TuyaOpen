# I2C 驱动

## 简要说明

I2C (Inter-Integrated Circuit) 是一种串行的同步通信总线。 I2C 串行总线有两根信号线，一根是双向的数据线 SDA，另一根是时钟线 SCL。所有接到 I2C 总线设备上的串行数据 SDA 都接到总线的 SDA 上，各设备的时钟线 SCL 接到总线的 SCL 上。

I2C 典型接线方式如下:

![image-20220406143504112](https://images.tuyacn.com/fe-static/docs/img/fd7da59e-749b-4a3a-b171-411459568eed.png)

总线的通信由主机控制，即主机是数据的传送（发出启动信号）、发出时钟信号以及传送结束时发出停止信号的设备。被主机寻访的设备称为从机。每个接到 I2C 总线的设备都有一个唯一的地址，以便于主机寻访。主机和从机的数据传送，可以由主机发送数据到从机，也可以由从机发到主机。I2C 支持 7 位或者 10 位从设备地址模式。I2C 总线在开始条件后的首字节决定哪个被控器将被主控器选择，当主机输出一地址时，系统中的每一从设备都将开始条件后的地址和自己的地址进行比较，如果相同，该从机即认为自己被主机寻址。

## 索引

### tkl_i2c_init

```c
OPERATE_RET tkl_i2c_init(TUYA_I2C_NUM_E port, const TUYA_IIC_BASE_CFG_T *cfg);
```

- 功能描述:
  - 通过端口号和基础配置初始化对应的 I2C 实例，返回初始化结果。
  
- 参数:
  - `port`: 端口号。
  
  - `cfg`: I2C 基础配置，包含角色，速率，地址宽度 。
  
    ```
    typedef struct {
        TUYA_IIC_ROLE_E      role;
        TUYA_IIC_SPEED_E     speed;
        TUYA_IIC_ADDR_MODE_E addr_width;
    } TUYA_IIC_BASE_CFG_T;
    ```
  
    #### TUYA_IIC_ROLE_E:
  
    | 名字                 | 定义        | 备注 |
    | :------------------- | :---------- | :--- |
    | TUYA_IIC_MODE_MASTER | I2C 主机模式 |      |
    | TUYA_IIC_MODE_MASTER | I2C 从机模式 |      |
  
    #### TUYA_IIC_SPEED_E：
  
    | 名字                    | 定义                | 备注 |
    | :---------------------- | :------------------ | :--- |
    | TUYA_IIC_BUS_SPEED_100K | I2C 标准速度(100KHz) |      |
    | TUYA_IIC_BUS_SPEED_400K | I2C 快速速度(400KHz) |      |
    | TUYA_IIC_BUS_SPEED_1M   | I2C 标准+速度(1MHz)  |      |
    | TUYA_IIC_BUS_SPEED_3_4M | I2C 高速速度(3.4MHz) |      |
  
    #### TUYA_IIC_ADDR_MODE_E：
  
    | 名字                   | 定义           | 备注 |
    | :--------------------- | :------------- | :--- |
    | TUYA_IIC_ADDRESS_7BIT  | 7bit 地址模式  |      |
    | TUYA_IIC_ADDRESS_10BIT | 10bit 地址模式 |      |

- 返回值:
  - OPRT_OK 成功，其他请参考文件 `tuya_error_code.h`，`OS_ADAPTER_I2C` 定义部分。

### tkl_i2c_deinit

```c
OPERATE_RET tkl_i2c_deinit(TUYA_I2C_NUM_E port)
```

- 功能描述:
  - I2C 实例反初始化。该接口会停止 I2C。
  - 实例正在进行的传输（如果有），并且释放相关的软硬件资源。
- 参数:
  - `port`: 端口号。
- 返回值:
  - OPRT_OK 成功，其他请参考文件 `tuya_error_code.h`，`OS_ADAPTER_I2C` 定义部分。

### tkl_i2c_irq_init

```c
OPERATE_RET tkl_i2c_irq_init(TUYA_I2C_NUM_E port, TUYA_I2C_IRQ_CB cb);
```

- 功能描述:

  - I2C 中断初始化。

- 参数:

  - `port`: 端口号。

  - `cb`: I2C 中断回调函数。

    回调函数类型 TUYA_I2C_IRQ_CB 定义如下：

    ```c
    typedef void (*TUYA_I2C_IRQ_CB)(TUYA_I2C_NUM_E port, TUYA_I2C_IRQ_EVT_E event);
    ```

    其中 port 为端口号，event 为传给回调函数的事件类型。

    I2C 回调事件枚举类型 TUYA_I2C_IRQ_EVT_E 定义如下：

| 名字                               | 定义                                | 备注 |
|:-----------------------------------|:-----------------------------------|:------|
| TUYA_IIC_EVENT_TRANSFER_DONE       | 传输完成事件                        |      |
| TUYA_IIC_EVENT_TRANSFER_INCOMPLETE | 传输未完成事件                      |      |
| TUYA_IIC_EVENT_SLAVE_TRANSMIT      | 从设备发送操作请求事件              |      |
| TUYA_IIC_EVENT_SLAVE_RECEIVE       | 从设备接收操作请求事件              |      |
| TUYA_IIC_EVENT_ADDRESS_NACK        | 地址未应答事件                      |      |
| TUYA_IIC_EVENT_GENERAL_CALL        | 指示收到 general call（地址为0）事件 |      |
| TUYA_IIC_EVENT_ARBITRATION_LOST    | 主机仲裁丢失事件                    |      |
| TUYA_IIC_EVENT_BUS_ERROR           | 总线错误事件                        |      |
| TUYA_IIC_EVENT_BUS_CLEAR           | 总线清除完成事件                    |      |

- 返回值:

  - OPRT_OK 成功，其他请参考文件 `tuya_error_code.h`，`OS_ADAPTER_I2C` 定义部分。

### tkl_i2c_irq_enable

```c
OPERATE_RET tkl_i2c_irq_enable(TUYA_I2C_NUM_E port);
```

- 功能描述:
  - 打开 I2C 中断。
- 参数:
  - `port`: 端口号。
- 返回值:
  - OPRT_OK 成功，其他请参考文件 `tuya_error_code.h`，`OS_ADAPTER_I2C` 定义部分。

### tkl_i2c_irq_disable

```c
OPERATE_RET tkl_i2c_irq_disable(TUYA_I2C_NUM_E port);
```

- 功能描述:
  - 关闭 I2C 中断。
- 参数:
  - `port`: 端口号。
- 返回值:
  - OPRT_OK 成功，其他请参考文件 `tuya_error_code.h`，`OS_ADAPTER_I2C` 定义部分。

### tkl_i2c_master_send

```c
OPERATE_RET tkl_i2c_master_send(TUYA_I2C_NUM_E port, uint16_t dev_addr, const void *data, uint32_t size, BOOL_T xfer_pending);
```

- 功能描述:
  - I2C 作为主机时启动数据发送。
- 参数:
  - `port`: 端口号。
  - `dev_addr`: Slave 设备地址。
  - `data`: 待发送数据的缓冲区地址。
  - `size`: 待发送数据的长度。
  - `xfer_pending`:
  - 发送完成后是否发送停止位。1-不发送停止位，0-发送停止位。
- 返回值:
  - 错误码，OPRT_OK 成功，其他请参考文件 `tuya_error_code.h`，`OS_ADAPTER_I2C` 定义部分。

### tkl_i2c_master_receive

```c
OPERATE_RET tkl_i2c_master_receive(TUYA_I2C_NUM_E port, uint16_t dev_addr, void *data, uint32_t size, BOOL_T xfer_pending);
```

- 功能描述:
- 参数:
  - `port`: 端口号。
  - `dev_addr`: Slave 设备地址。
  - `data`: 待接收数据的缓冲区地址。
  - `size`: 待接收数据的长度。
  - `xfer_pending`:
  - 接收完成后是否发送停止位。1-不发送停止位，0-发送停止位。
- 返回值:
  - 错误码，OPRT_OK 成功，其他请参考文件 `tuya_error_code.h`，`OS_ADAPTER_I2C` 定义部分。

### tkl_i2c_set_slave_addr

```c
OPERATE_RET tkl_i2c_set_slave_addr(TUYA_I2C_NUM_E port, uint16_t dev_addr);
```

- 功能描述:
  - 配置 I2C 的从设备地址。
- 参数:
  - `port`: 端口号。
  - `dev_addr`:  I2C 从设备通信地址。
- 返回值:
  - 错误码，OPRT_OK 成功，其他请参考文件 `tuya_error_code.h`，`OS_ADAPTER_I2C` 定义部分。

### tkl_i2c_slave_send

```c
OPERATE_RET tkl_i2c_slave_send(TUYA_I2C_NUM_E port, const void *data, uint32_t size);
```

- 功能描述:
  - i2c作为从机时启动数据发送。
- 参数:
  - `port`: 端口号。
  - `data`: 待发送数据的缓冲区地址。
  - `size`: 从机待发送数据的长度。
- 返回值:
  - 错误码，OPRT_OK 成功，其他请参考文件 `tuya_error_code.h`，`OS_ADAPTER_I2C` 定义部分。

### tkl_i2c_slave_receive

```c
OPERATE_RET tkl_i2c_slave_receive(TUYA_I2C_NUM_E port, void *data, uint32_t size);
```

- 功能描述:
  - i2c作为从机时启动数据接收。
- 参数:
  - `port`: 端口号。
  - `data`: 待接收数据的缓冲区地址。
  - `size`: 待接收数据的长度。
- 返回值:
  - 错误码，OPRT_OK 成功，其他请参考文件 `tuya_error_code.h`，`OS_ADAPTER_I2C` 定义部分。



### tkl_i2c_get_status

```c
TUYA_IIC_STATUS_T tkl_i2c_get_status(TUYA_I2C_NUM_E port);
```

- 功能描述:
  - 获取当前时刻i2c 的状态。
- 参数:
  - `port`: 端口号。
- 返回值:
  -  I2C 状态的结构体。I2C 的状态定义见 TUYA_I2C_STATUS_T

### tkl_i2c_get_data_count

```c
int32_t tkl_i2c_get_data_count(TUYA_I2C_NUM_E port);
```

- 功能描述:
  - 获取 I2C 已传输得数据个数。
  - 针对 tkl_i2c_master_send，传输或发送的字节数。
  - 针对 tkl_i2c_master_receive，接收到的字节数。
  - 针对 tkl_i2c_slave_send，传输的字节数。
  - 针对 tkl_i2c_slave_receive，接收的字节数。
- 参数:
  - `port`: 端口号
- 返回值:
  - 已传输数据个数

#### TUYA_IIC_STATUS_T:

| 名字                 | 定义                     | 备注 |
| :------------------- | :----------------------- | :--- |
| busy : 1             | 传输或发送忙状态位       |      |
| mode : 1             | 模式位。1-主，0-从       |      |
| direction : 1        | 传输方向：1-接收，0-发送 |      |
| general_call : 1     | general call 指示        |      |
| arbitration_lost : 1 | 主机失去仲裁             |      |
| bus_error : 1        | 总线错误                 |      |

### tkl_i2c_reset

```c
OPERATE_RET tkl_i2c_reset(TUYA_I2C_NUM_E port);
```

- 功能描述:
  - 复位i2c。
- 参数:
  - `port`: 端口号。
- 返回值:
  - 错误码，OPRT_OK 成功，其他请参考文件 `tuya_error_code.h`，`OS_ADAPTER_I2C` 定义部分。

## 示例

## 1.I2C 示例一

```c
static uint16_t cb_transfer_flag = 0xff;

static void i2c_event_cb_fun(int32_t idx, TUYA_IIC_IRQ_EVT_E event)
{
    if (idx == I2C_NUM_0){
        cb_transfer_flag = event;
    }
}

void tuya_i2c_master_test(void)
{
    OPERATE_RET ret;
 	TUYA_IIC_BASE_CFG_T cfg;
    //receive buffer
    char rcv_buf[10] ;
    //data to send
    char send_buf[10] = {0,1,2,3,4,5,6,7,8,9};
    
    tkl_io_pinmux_config(TUYA_IO_PIN_0, TUYA_IIC0_SCL);
    tkl_io_pinmux_config(TUYA_IO_PIN_1, TUYA_IIC0_SDA);
    
    cfg.role = TUYA_IIC_MODE_MASTER;
    cfg.speed = TUYA_IIC_BUS_SPEED_100K;
    cfg.addr_width = TUYA_IIC_ADDRESS_7BIT;

    ret = tkl_i2c_init(I2C_NUM_0, &cfg);
    if (ret != OPRT_OK) {
        //fail
        return;
    }
    
    ret = tkl_i2c_irq_init(I2C_NUM_0, i2c_event_cb_fun);
    if (ret != OPRT_OK) {
        //fail
        return;
    }
    
    ret = tkl_i2c_irq_enable(I2C_NUM_0);
    if (ret != OPRT_OK) {
        //fail
        return;
    }

    ret = tkl_i2c_master_send(I2C_NUM_0, 0x57, send_buf, sizeof(send_buf), 0);
    if (ret < 0) {
        //failed
    }
    while (cb_transfer_flag == 0xff);

    //check transfer result
    if (cb_transfer_flag == IIC_EVENT_TRANSFER_DONE) {
        //transmit done
    } else {
        //failed
    }
    cb_transfer_flag = 0xff;

    ret = tkl_i2c_master_receive(I2C_NUM_0, 0x57, rcv_buf, sizeof(rcv_buf), 0);
    if (ret < 0) {
        //failed
    }

    while (cb_transfer_flag == 0xff);

    //check transfer result
    if (cb_transfer_flag == IIC_EVENT_TRANSFER_DONE) {
        //transmit done
    } else {
        //failed
    }

    ret = tkl_i2c_irq_disable(I2C_NUM_0);
    if (ret != OPRT_OK) {
        //fail
        return;
    }
    //uninitialize iic 
    ret = tkl_i2c_deinit(I2C_NUM_0);
    if (ret != 0) {
       //failed
    }
}
```

## 1.I2C 示例二

```c
void tuya_i2c_slave_test(void)
{
    OPERATE_RET ret;
 	TUYA_IIC_BASE_CFG_T cfg;
    TUYA_IIC_STATUS_T st;
    //receive buffer
    char rcv_buf[10] ;
    //data to send
    char send_buf[10] = {0,1,2,3,4,5,6,7,8,9};
    int32_t cnt = 100;
    
    tkl_io_pinmux_config(TUYA_IO_PIN_0, TUYA_IIC0_SCL);
    tkl_io_pinmux_config(TUYA_IO_PIN_1, TUYA_IIC0_SDA);
    
    cfg.role = TUYA_IIC_MODE_SLAVE;
    cfg.speed = TUYA_IIC_BUS_SPEED_100K;
    cfg.addr_width = TUYA_IIC_ADDRESS_7BIT;
    
    ret = tkl_i2c_init(I2C_NUM_0, &cfg);
    if (ret != OPRT_OK) {
        //fail
        return;
    }
    
    ret = tkl_i2c_set_slave_addr(I2C_NUM_0, 0x57);
    if (ret != OPRT_OK) {
        //fail
        return;
    }
        
    ret = tkl_i2c_slave_send(I2C_NUM_0, send_buf, sizeof(send_buf));
    if (ret < 0) {
        //failed
    }
    //wait send done, waiting for 100 ms max
    st.busy = 1;
    cnt = 100;
    while(cnt--) {
        tkl_system_sleep(1);
        //check status
        st = tkl_i2c_get_status(I2C_NUM_0);
        //transmit done
        if (st.busy == 0){
            break;
        }
    }
    
    ret = tkl_i2c_slave_receive(I2C_NUM_0, rcv_buf, sizeof(rcv_buf));
    if (ret < 0) {
        //failed
    }
    //wait send done, waiting for 100 ms max
    st.busy = 1;
    cnt = 100;
    while(cnt--) {
        tkl_system_sleep(1);
        //check status
        st = tkl_i2c_get_status(I2C_NUM_0);
        //transmit done
        if (st.busy == 0){
            break;
        }
    }

    //uninitialize iic 
    ret = tkl_i2c_deinit(I2C_NUM_0);
    if (ret != 0) {
       //failed
    }

}
```


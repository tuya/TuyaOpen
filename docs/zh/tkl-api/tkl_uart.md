# UART 驱动

## 简要说明

UART(Universal Asynchronous Receiver/Transmitter) 是一种通用串行数据总线，用于异步通信。该总线双向通信，可以实现全双工传输和接收。

## API 描述

### tkl_uart_init

```c
OPERATE_RET tkl_uart_init(TUYA_UART_NUM_E port_id, TUYA_UART_BASE_CFG_T *cfg);
```

- 功能描述：
  - 通过端口号和基础配置初始化对应的 UART 实例，返回初始化结果 。
- 参数：
  - `port_id`: 端口号。

    ```c
    typedef enum {
        TUYA_UART_NUM_0,		    // UART 0
        TUYA_UART_NUM_1,		    // UART 1
        TUYA_UART_NUM_2,	        // UART 2
        TUYA_UART_NUM_3,	        // UART 3
        TUYA_UART_NUM_4,	        // UART 4
        TUYA_UART_NUM_5,	        // UART 5
        TUYA_UART_NUM_MAX,
    } TUYA_UART_NUM_E;
    ```
   - `cfg`: UART 基础配置，包含波特率，奇偶校验位，停止位，流控制

    ```c
    typedef struct {
        uint32_t                    baudrate;
        TUYA_UART_PARITY_TYPE_E     parity;
        TUYA_UART_DATA_LEN_E        databits;
        TUYA_UART_STOP_LEN_E        stopbits;
        TUYA_UART_FLOWCTRL_TYPE_E   flowctrl;
    } TUYA_UART_BASE_CFG_T;
    ```
  
#### TUYA_UART_PARITY_TYPE_E:

| 名字                 | 定义        | 备注 |
|:-------------------------|:---------------------|:------|
| TUYA_UART_PARITY_TYPE_NONE | 0x0 (没有校验位) |    |
| TUYA_UART_PARITY_TYPE_ODD  | 0x1 (奇数校验位) |    |
| TUYA_UART_PARITY_TYPE_EVEN | 0x2 (偶数校验位) |    |

#### TUYA_UART_DATA_LEN_E:

| 名字                    | 定义                | 备注 |
|:-------------------------|:---------------------|:------|
| TUYA_UART_DATA_LEN_5BIT | 0x5 (5位数据长度)   |      |
| TUYA_UART_DATA_LEN_6BIT | 0x6 (6位数据长度)   |      |
| TUYA_UART_DATA_LEN_7BIT | 0x7 (7位数据长度)   |      |
| TUYA_UART_DATA_LEN_8BIT | 0x8 (8位数据长度)   |      |

#### TUYA_UART_STOP_LEN_E:

| 名字                    | 定义               | 备注 |
|:-------------------------|:---------------------|:------|
| TUYA_UART_STOP_LEN_1BIT    | 0x1 (1位停止位) |      |
| TUYA_UART_STOP_LEN_1_5BIT1 | 0x2 (1.5位停止位) |    |
| TUYA_UART_STOP_LEN_2BIT    | 0x3 (2位停止位) |    |

#### TUYA_UART_FLOWCTRL_TYPE_E:

| 名字                    | 定义               | 备注 |
|:-------------------------|:---------------------|:------|
| TUYA_UART_FLOWCTRL_NONE | 0x0 (没有进行流控制) |    |
| TUYA_UART_FLOWCTRL_RTSCTS  | 0x1 (请求/清除发送) |    |
| TUYA_UART_FLOWCTRL_XONXOFF | 0x2 (暂停传输/回复传输) |    |
| TUYA_UART_FLOWCTRL_DTRDSR |  0x3 (数据终端准备好/数据准备好) |    |

- 返回值：
  - OPRT_OK 成功，其他请参考文件 `tuya_error_code.h`，uart type 定义部分。

### tkl_uart_deinit:

```c
OPERATE_RET tkl_uart_deinit(TUYA_UART_NUM_E port_id);
```

- 功能描述：
  - UART 反初始化。该接口会停止 UART。
  - 结束正在进行的传输（如果有），并且释放相关的软硬件资源。

- 参数：
  - `port_id`: 端口号。

   ```c
   typedef enum {
       TUYA_UART_NUM_0,		    // UART 0
       TUYA_UART_NUM_1,		    // UART 1
       TUYA_UART_NUM_2,	        // UART 2
       TUYA_UART_NUM_3,	        // UART 3
       TUYA_UART_NUM_4,	        // UART 4
       TUYA_UART_NUM_5,	        // UART 5
       TUYA_UART_NUM_MAX,
   } TUYA_UART_NUM_E;
   ```

- 返回值:
  - OPRT_OK 成功，其他请参考文件 `tuya_error_code.h`，uart type 定义部分。

### tkl_uart_write:

  ```c
  int tkl_uart_write(TUYA_UART_NUM_E port_id, void *buff, uint16_t len);
  ```

- 功能描述：
  - UART 发送数据。
- 参数：
  - `part_id`: 端口号。

  ```c
  typedef enum {
      TUYA_UART_NUM_0,		    // UART 0
      TUYA_UART_NUM_1,		    // UART 1
      TUYA_UART_NUM_2,	        // UART 2
      TUYA_UART_NUM_3,	        // UART 3
      TUYA_UART_NUM_4,	        // UART 4
      TUYA_UART_NUM_5,	        // UART 5
      TUYA_UART_NUM_MAX,
  } TUYA_UART_NUM_E;
  ```

  - `buff`: 发送数据缓冲。
  - `len`: 发送数据长度。
- 返回值：
  - `>=0`  发送数据长度。
  - `< 0 ` 发送数据错误。
  
### tkl_uart_rx_irq_cb_reg:

```c
void tkl_uart_rx_irq_cb_reg(TUYA_UART_NUM_E port_id, TUYA_UART_IRQ_CB rx_cb);
```
- 功能描述：
  - 注册 UART 接受中断回调函数。
- 参数：
  - `part_id`: 端口号。

  ```c
  typedef enum {
      TUYA_UART_NUM_0,		    // UART 0
      TUYA_UART_NUM_1,		    // UART 1
      TUYA_UART_NUM_2,	        // UART 2
      TUYA_UART_NUM_3,	        // UART 3
      TUYA_UART_NUM_4,	        // UART 4
      TUYA_UART_NUM_5,	        // UART 5
      TUYA_UART_NUM_MAX,
  } TUYA_UART_NUM_E;
  ```
  - `rx_cb`: 接受中断回调函数。

    回调函数类型 `TUYA_UART_IRQ_CB` 定义如下：

    ```c
    typedef void (*TUYA_UART_IRQ_CB)(TUYA_UART_NUM_E port_id);
    ```

    其中 `port_id` 为端口号。
- 返回值：
  - `void`: 空

### tkl_uart_tx_irq_cb_reg:

```c
void tkl_uart_tx_irq_cb_reg(TUYA_UART_NUM_E port_id, TUYA_UART_IRQ_CB tx_cb);
```
- 功能描述：
  - 注册 UART 接受中断回调函数。
- 参数：
  - `part_id`: 端口号。
  
  ```c
  typedef enum {
      TUYA_UART_NUM_0,		    // UART 0
      TUYA_UART_NUM_1,		    // UART 1
      TUYA_UART_NUM_2,	        // UART 2
      TUYA_UART_NUM_3,	        // UART 3
      TUYA_UART_NUM_4,	        // UART 4
      TUYA_UART_NUM_5,	        // UART 5
      TUYA_UART_NUM_MAX,
  } TUYA_UART_NUM_E;
  ```
   - `tx_cb`: 发送中断回调函数。

    回调函数类型 `TUYA_UART_IRQ_CB` 定义如下：

    ```c
    typedef void (*TUYA_UART_IRQ_CB)(TUYA_UART_NUM_E port_id);
    ```

    其中 `port_id` 为端口号。
- 返回值：
  - `void`: 空

### tkl_uart_read:

```c
int tkl_uart_read(TUYA_UART_NUM_E port_id, void *buff, uint16_t len);
```
- 功能描述：
  
  - 读取uart中的数据。
  
- 参数：

  - `part_id`: 端口号。

  ```c
  typedef enum {
      TUYA_UART_NUM_0,		    // UART 0
      TUYA_UART_NUM_1,		    // UART 1
      TUYA_UART_NUM_2,	        // UART 2
      TUYA_UART_NUM_3,	        // UART 3
      TUYA_UART_NUM_4,	        // UART 4
      TUYA_UART_NUM_5,	        // UART 5
      TUYA_UART_NUM_MAX,
  } TUYA_UART_NUM_E;
  ```

  - `buff`: 数据接收缓冲区。
  - `len`:  需要接受的数据长度。

- 返回值：
  - `>=0`  发送数据长度。
  - `< 0 ` 发送数据错误。

### tkl_uart_set_tx_int:

```c
OPERATE_RET tkl_uart_set_tx_int(TUYA_UART_NUM_E port_id, BOOL_T enable);
```

- 功能描述：
  - enabe/disable uart 发送中断
- 参数：
   - `port_id`:端口号。 

  ```c
  typedef enum {
      TUYA_UART_NUM_0,		    // UART 0
      TUYA_UART_NUM_1,		    // UART 1
      TUYA_UART_NUM_2,	        // UART 2
      TUYA_UART_NUM_3,	        // UART 3
      TUYA_UART_NUM_4,	        // UART 4
      TUYA_UART_NUM_5,	        // UART 5
      TUYA_UART_NUM_MAX,
  } TUYA_UART_NUM_E;
  ```

  - `enable`:中断enabel/disable
    - 0: disable
    - 1: enable
- 返回值：
  - OPRT_OK 成功，其他请参考文件 `tuya_error_code.h`。

### tkl_uart_set_rx_flowctrl:

```c
OPERATE_RET tkl_uart_set_rx_flowctrl(TUYA_UART_NUM_E port_id, BOOL_T enable);
```
- 功能描述：
  - enable/disable uart 接受流控制。
- 参数：
   - `port_id`:端口号。 

  ```c
  typedef enum {
      TUYA_UART_NUM_0,		    // UART 0
      TUYA_UART_NUM_1,		    // UART 1
      TUYA_UART_NUM_2,	        // UART 2
      TUYA_UART_NUM_3,	        // UART 3
      TUYA_UART_NUM_4,	        // UART 4
      TUYA_UART_NUM_5,	        // UART 5
      TUYA_UART_NUM_MAX,
  } TUYA_UART_NUM_E;
  ```

  - `enable`: 流控制 enabel/disable
    - 0: disable
    - 1: enable

- 返回值：
  - OPRT_OK 成功，其他请参考文件 `tuya_error_code.h`。

### tkl_uart_wait_for_data：

```c
OPERATE_RET tkl_uart_wait_for_data(TUYA_UART_NUM_E port_id, int timeout_ms);
```

- 功能描述：
  - 等待接受数据
- 参数：
   - `port_id`: 端口号。 

  ```c
  typedef enum {
      TUYA_UART_NUM_0,		    // UART 0
      TUYA_UART_NUM_1,		    // UART 1
      TUYA_UART_NUM_2,	        // UART 2
      TUYA_UART_NUM_3,	        // UART 3
      TUYA_UART_NUM_4,	        // UART 4
      TUYA_UART_NUM_5,	        // UART 5
      TUYA_UART_NUM_MAX,
  } TUYA_UART_NUM_E;
  ```
  - `timeout_ms`: 等待毫秒数。

- 返回值：
  - OPRT_OK 成功，其他请参考文件 `tuya_error_code.h`。

### tkl_uart_ioctl：

```c
OPERATE_RET tkl_uart_ioctl(TUYA_UART_NUM_E port_id, uint32_t cmd, void *arg);
```

- 功能描述：
  - uart command control 
  
- 参数：
   - `port_id`:端口号。 

  ```c
  typedef enum {
      TUYA_UART_NUM_0,		    // UART 0
      TUYA_UART_NUM_1,		    // UART 1
      TUYA_UART_NUM_2,	        // UART 2
      TUYA_UART_NUM_3,	        // UART 3
      TUYA_UART_NUM_4,	        // UART 4
      TUYA_UART_NUM_5,	        // UART 5
      TUYA_UART_NUM_MAX,
  } TUYA_UART_NUM_E;
  ```
  - `cmd`: UART 控制命令。

    #### TUYA_UART_IOCTL_CMD_E：

| 名字                    | 定义               | 备注 |
|:-------------------------|:---------------------|:------|
| TUYA_UART_SUSPEND_CMD  | 0x0 (暂停 UART)        |    |
| TUYA_UART_RESUME_CMD   | 0x1 (恢复 UART)     |    |
| TUYA_UART_FLUSH_CMD    | 0x2 (刷新 UART 缓冲)    |    |
| TUYA_UART_RECONFIG_CMD | 0x3 (重新初始化 UART)  |    |
| TUYA_UART_USER_CMD     | 0x4 (用户自定义命令)  |    |

  - `arg`: 对应控制命令的参数
  
- 返回值：
  - OPRT_OK 成功，其他请参考文件 `tuya_error_code.h`。

## 示例 

### 1.uart 示例一

```c
int uart_loopback_test(void)
{
    uint32_t port_id;
    TUYA_UART_BASE_CFG_T cfg;
    OPERATE_RET ret = OPRT_OK;
    const int bufsize = 8;
    unsigned char tx[bufsize], rx[bufsize];
    int bytes;
    int i;

    // start 
    port_id = TUYA_UART_NUM_0;

    cfg.baudrate = 115200;
    cfg.databits = TUYA_UART_DATA_LEN_8BIT;
    cfg.parity = TUYA_UART_PARITY_TYPE_NONE;
    cfg.stopbits = TUYA_UART_STOP_LEN_1BIT;
    cfg.flowctrl = TUYA_UART_FLOWCTRL_NONE;

    ret = tkl_uart_init(port_id, &cfg);
    
    for (i = 0; i < bufsize; i++) {
        tx[i] = 'A' + i;
    }

    // loop 3 times
    for(i = 0; i < 3; i ++) {
        bzero(rx, sizeof(rx));

        bytes = tkl_uart_write(port_id, tx, sizeof(tx));
        if (bytes <= 0) {
            // fail
            ret = OPRT_COM_ERROR;
         } else {
             // wait at most 5 seconds until the data is ready
            ret = tkl_uart_wait_for_data(port_id, 5000);
            if (ret == OPRT_TIMEOUT) {
               // timeout
               tkl_uart_deinit(port_id);
               return OPRT_COM_ERROR;
            }

            bytes = tkl_uart_read(port_id, rx, sizeof(rx));
            if (bytes < 0) {
                // fail
                ret = OPRT_COM_ERROR;
            } else {
                if (memcmp(tx, rx, bufsize) != 0) {
                    // data is not identical
                    ret = OPRT_COM_ERROR;
                } else {
                    ret = OPRT_OK;
                }
            }
        }

        if (ret != OPRT_OK) {
            // fail 
        } else {
            // ok 
        }
    }
    //deinit 
    tkl_uart_deinit(port_id);
    return ret;
}

```

### 2.uart 示例二

```c
static int sg_rx_flag = 0;
static int sg_tx_flag = 0;

static void tuya_rx_cb(TUYA_UART_NUM_E port_id)
{
    // mutex lock
    sg_rx_flag = 1;
    // mutex unlock
}

static static void tuya_tx_cb(TUYA_UART_NUM_E port_id)
{
    // mutex lock
    sg_tx_flag = 1;
    // mutex unlock
}

int uart_loopback_test(void)
{
    uint32_t port_id;
    TUYA_UART_BASE_CFG_T cfg;
    OPERATE_RET ret = OPRT_OK;
    const int bufsize = 8;
    unsigned char tx[bufsize], rx[bufsize];
    int bytes;
    static int sl_first_time =1;
    int i;

    // start 
    sg_rx_flag = 0;
    sg_tx_flag = 0;

    port_id = TUYA_UART_NUM_0;

    cfg.baudrate = 115200;
    cfg.databits = TUYA_UART_DATA_LEN_8BIT;
    cfg.parity = TUYA_UART_PARITY_TYPE_NONE;
    cfg.stopbits = TUYA_UART_STOP_LEN_1BIT;
    cfg.flowctrl = TUYA_UART_FLOWCTRL_NONE;

    ret = tkl_uart_init(port_id, &cfg);

    if(tkl_uart_set_tx_int(port_id,1) < 0) {
        //fail
        tkl_uart_deinit(port_id);
        return OPRT_COM_ERROR;
    }

    for (i = 0; i < bufsize; i++) {
        tx[i] = 'A' + i;
    }
    
    // loop 3 times
    for(i=0;i<3;i++) {

        bzero(rx, sizeof(rx));
     

        if(sl_first_time || sg_tx_flag) {
            bytes = tkl_uart_write(port_id, tx, sizeof(tx));
        }else {
            continue;
        }

        if (bytes <= 0) {
            // fail
             ret = OPRT_COM_ERROR;
        } else {
        // wait at most 5 seconds until the data is ready
        ret = tkl_uart_wait_for_data(port_id, 5000);
        
        if(sg_rx_flag) {
            bytes = tkl_uart_read(port_id, rx, sizeof(rx));
            if (bytes < 0) {
               // fail
               ret = OPRT_COM_ERROR;
            } else {
                if (memcmp(tx, rx, bufsize) != 0) {
                // data is not identical
                ret = OPRT_COM_ERROR;
                } else {
                    ret = OPRT_OK;
                }
            }
            sg_rx_flag = 0;
           }
        }
        if (ret != OPRT_OK) {
        // fail 
        } else {
        // ok 
        } 
    }  
    // disabe int 
    tkl_uart_set_tx_int(port_id,0);
    //deinit 
    tkl_uart_deinit(port_id);
    return ret;
}

```


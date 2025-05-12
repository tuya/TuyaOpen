# UART Driver

## Brief Description

UART (Universal Asynchronous Receiver/Transmitter) is a universal serial data bus used for asynchronous communication. This bus supports two-way communication, enabling full-duplex transmission and reception.

## API Description

### tkl_uart_init

```c
OPERATE_RET tkl_uart_init(TUYA_UART_NUM_E port_id, TUYA_UART_BASE_CFG_T *cfg);
```

- Function Description:
  - Initializes the corresponding UART instance via the port number and basic configuration, returning the initialization result.
- Parameters:
  - `port_id`: Port number.

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
   - `cfg`: UART basic configuration, including baud rate, parity bit, stop bit, and flow control

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

| Name                 | Definition        | Remarks |
|:-------------------------|:---------------------|:------|
| TUYA_UART_PARITY_TYPE_NONE | 0x0 (No parity bit) |    |
| TUYA_UART_PARITY_TYPE_ODD  | 0x1 (Odd parity bit) |    |
| TUYA_UART_PARITY_TYPE_EVEN | 0x2 (Even parity bit) |    |

#### TUYA_UART_DATA_LEN_E:

| Name                    | Definition                | Remarks |
|:-------------------------|:---------------------|:------|
| TUYA_UART_DATA_LEN_5BIT | 0x5 (5-bit data length) |     |
| TUYA_UART_DATA_LEN_6BIT | 0x6 (6-bit data length) |     |
| TUYA_UART_DATA_LEN_7BIT | 0x7 (7-bit data length) |     |
| TUYA_UART_DATA_LEN_8BIT | 0x8 (8-bit data length) |     |

#### TUYA_UART_STOP_LEN_E:

| Name                    | Definition               | Remarks |
|:-------------------------|:---------------------|:------|
| TUYA_UART_STOP_LEN_1BIT    | 0x1 (1-bit stop bit) |      |
| TUYA_UART_STOP_LEN_1_5BIT1 | 0x2 (1.5-bit stop bit) |    |
| TUYA_UART_STOP_LEN_2BIT    | 0x3 (2-bit stop bit) |    |

#### TUYA_UART_FLOWCTRL_TYPE_E:

| Name                    | Definition               | Remarks |
|:-------------------------|:---------------------|:------|
| TUYA_UART_FLOWCTRL_NONE | 0x0 (No flow control) |    |
| TUYA_UART_FLOWCTRL_RTSCTS  | 0x1 (Request/Clear to Send) |    |
| TUYA_UART_FLOWCTRL_XONXOFF | 0x2 (Transmit/Receive On/Off) |    |
| TUYA_UART_FLOWCTRL_DTRDSR |  0x3 (Data Terminal Ready/Data Set Ready) |    |

- Return Value:
  - OPRT_OK Success, others please refer to the file `tuya_error_code.h`, UART type definition section.

### tkl_uart_deinit:

```c
OPERATE_RET tkl_uart_deinit(TUYA_UART_NUM_E port_id);
```

- Function Description:
  - UART deinitialization. This interface will stop the UART.
  - Ends any ongoing transmission (if any) and releases related software and hardware resources.

- Parameters:
  - `port_id`: Port number.

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

- Return Value:
  - OPRT_OK Success, others please refer to the file `tuya_error_code.h`, UART type definition section.

### tkl_uart_write:

  ```c
  int tkl_uart_write(TUYA_UART_NUM_E port_id, void *buff, uint16_t len);
  ```

- Function Description:
  - UART sends data.
- Parameters:
  - `part_id`: Port number.

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

  - `buff`: Data buffer to send.
  - `len`: Length of data to send.
- Return Value:
  - `>=0` Length of data sent.
  - `< 0 ` Error sending data.
  
### tkl_uart_rx_irq_cb_reg:

```c
void tkl_uart_rx_irq_cb_reg(TUYA_UART_NUM_E port_id, TUYA_UART_IRQ_CB rx_cb);
```
- Function Description:
  - Registers the UART receive interrupt callback function.
- Parameters:
  - `part_id`: Port number.

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
  - `rx_cb`: Receive interrupt callback function.

    Callback function type `TUYA_UART_IRQ_CB` is defined as follows:

    ```c
    typedef void (*TUYA_UART_IRQ_CB)(TUYA_UART_NUM_E port_id);
    ```

    Where `port_id` is the port number.
- Return Value:
  - `void`: None

### tkl_uart_tx_irq_cb_reg:

```c
void tkl_uart_tx_irq_cb_reg(TUYA_UART_NUM_E port_id, TUYA_UART_IRQ_CB tx_cb);
```
- Function Description:
  - Registers the UART transmit interrupt callback function.
- Parameters:
  - `part_id`: Port number.
  
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
   - `tx_cb`: Transmit interrupt callback function.

    Callback function type `TUYA_UART_IRQ_CB` is defined as follows:

    ```c
    typedef void (*TUYA_UART_IRQ_CB)(TUYA_UART_NUM_E port_id);
    ```

    Where `port_id` is the port number.
- Return Value:
  - `void`: None

### tkl_uart_read:

```c
int tkl_uart_read(TUYA_UART_NUM_E port_id, void *buff, uint16_t len);
```
- Function Description:
  
  - Reads data from the UART.
  
- Parameters:

  - `part_id`: Port number.

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

  - `buff`: Data receive buffer.
  - `len`: Length of data to receive.

- Return Value:
  - `>=0` Length of data received.
  - `< 0 ` Error receiving data.

### tkl_uart_set_tx_int:

```c
OPERATE_RET tkl_uart_set_tx_int(TUYA_UART_NUM_E port_id, BOOL_T enable);
```

- Function Description:
  - Enable/disable UART transmit interrupt
- Parameters:
   - `port_id`: Port number. 

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

  - `enable`: Interrupt enable/disable
    - 0: disable
    - 1: enable
- Return Value:
  - OPRT_OK Success, others please refer to the file `tuya_error_code.h`.

### tkl_uart_set_rx_flowctrl:

```c
OPERATE_RET tkl_uart_set_rx_flowctrl(TUYA_UART_NUM_E port_id, BOOL_T enable);
```
- Function Description:
  - Enable/disable UART receive flow control.
- Parameters:
   - `port_id`: Port number. 

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

  - `enable`: Flow control enable/disable
    - 0: disable
    - 1: enable

- Return Value:
  - OPRT_OK Success, others please refer to the file `tuya_error_code.h`.

### tkl_uart_wait_for_data：

```c
OPERATE_RET tkl_uart_wait_for_data(TUYA_UART_NUM_E port_id, int timeout_ms);
```

- Function Description:
  - Waits for received data
- Parameters:
   - `port_id`: Port number. 

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
  - `timeout_ms`: Wait time in milliseconds.

- Return Value:
  - OPRT_OK Success, others please refer to the file `tuya_error_code.h`.

### tkl_uart_ioctl：

```c
OPERATE_RET tkl_uart_ioctl(TUYA_UART_NUM_E port_id, uint32_t cmd, void *arg);
```

- Function Description:
  - UART command control 
  
- Parameters:
   - `port_id`: Port number. 

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
  - `cmd`: UART control command.

    #### TUYA_UART_IOCTL_CMD_E：

| Name                    | Definition               | Remarks |
|:-------------------------|:---------------------|:------|
| TUYA_UART_SUSPEND_CMD  | 0x0 (Suspend UART)        |    |
| TUYA_UART_RESUME_CMD   | 0x1 (Resume UART)     |    |
| TUYA_UART_FLUSH_CMD    | 0x2 (Flush UART buffer)    |    |
| TUYA_UART_RECONFIG_CMD | 0x3 (Reinitialize UART)  |    |
| TUYA_UART_USER_CMD     | 0x4 (User-defined command)  |    |

  - `arg`: Parameters corresponding to the control command
  
- Return Value:
  - OPRT_OK Success, others please refer to the file `tuya_error_code.h`.

# Examples

## 1. UART Example 1

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

## 2. UART Example 2

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

#ifndef __JIELI_BOARD_COM_API_H__
#define __JIELI_BOARD_COM_API_H__

#include "tuya_cloud_types.h"

/* Set to 1 to route logs to USB CDC virtual serial port, or 0 to use hardware UART */
#ifndef ENABLE_JIELI_USB_CDC_LOG
#define ENABLE_JIELI_USB_CDC_LOG 1
#endif

OPERATE_RET board_register_hardware(void);

#endif /* __JIELI_BOARD_COM_API_H__ */

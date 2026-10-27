#ifndef __JIELI_BOARD_COM_API_H__
#define __JIELI_BOARD_COM_API_H__

#include "tuya_cloud_types.h"

#ifndef CONFIG_UI_ENABLE
#define CONFIG_UI_ENABLE 1
#endif

OPERATE_RET board_register_hardware(void);

#endif /* __JIELI_BOARD_COM_API_H__ */

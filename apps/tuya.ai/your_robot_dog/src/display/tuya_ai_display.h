/**
 * @file tuya_ai_display.h
 * @author www.tuya.com
 * @version 0.1
 * @date 2025-02-07
 *
 * @copyright Copyright (c) tuya.inc 2025
 *
 */

 #ifndef __TUYA_AI_DISPLAY_H__
 #define __TUYA_AI_DISPLAY_H__
 
 #include "tuya_cloud_types.h"
 #include "tuya_cloud_com_defs.h"
 #include "app_display.h"

 #ifdef __cplusplus
 extern "C" {
 #endif
 
 /***********************************************************
 *********************** macro define ***********************
 ***********************************************************/

 /***********************************************************
 ********************** typedef define **********************
 ***********************************************************/
/* Use TY_DISPLAY_TYPE_E / TY_DISPLAY_MSG_T from app_display.h */

 /***********************************************************
 ******************* function declaration *******************
 ***********************************************************/
 
OPERATE_RET tuya_ai_display_msg(char *msg, int len, TY_DISPLAY_TYPE_E display_tp);

#ifdef __cplusplus
}
#endif

#endif /* __TUYA_AI_DISPLAY_H__ */


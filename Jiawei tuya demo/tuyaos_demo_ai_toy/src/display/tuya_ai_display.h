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
 #include "tuya_device_cfg.h"

 #ifdef __cplusplus
 extern "C" {
 #endif
 
 /***********************************************************
 *********************** macro define ***********************
 ***********************************************************/

 /***********************************************************
 ********************** typedef define **********************
 ***********************************************************/
 typedef enum {
    TY_DISPLAY_TP_HUMAN_CHAT,
    TY_DISPLAY_TP_AI_CHAT,
    TY_DISPLAY_TP_STAT_SLEEP,
    TY_DISPLAY_TP_STAT_WAKEUP,
    TY_DISPLAY_TP_STAT_NETCFG,
    TY_DISPLAY_TP_STAT_NET,
    TY_DISPLAY_TP_STAT_POWERON,
    TY_DISPLAY_TP_STAT_ONLINE,
    TY_DISPLAY_TP_CHAT_MODE,
    TY_DISPLAY_TP_CHAT_STAT,
    TY_DISPLAY_TP_MALLOC,
    TY_DISPLAY_TP_EMOJI,
    TY_DISPLAY_TP_VOLUME,
    TY_DISPLAY_TP_ASR_EMOJI,
    TY_DISPLAY_TP_STAT_IDLE,
    TY_DISPLAY_TP_STAT_LISTEN,
    TY_DISPLAY_TP_STAT_SPEAK,
    TY_DISPLAY_TP_STAT_BATTERY,
    TY_DISPLAY_TP_STAT_CHARGING,
    TY_DISPLAY_TP_LANGUAGE,
} TY_DISPLAY_TYPE_E;

typedef struct  {
    TY_DISPLAY_TYPE_E   type;
    UINT_T              len;
    CHAR_T             *data;
} TY_DISPLAY_MSG_T;

 /***********************************************************
 ******************* function declaration *******************
 ***********************************************************/
 
void tuya_ai_display_init(void);
OPERATE_RET tuya_ai_display_msg(char *msg, int len, TY_DISPLAY_TYPE_E display_tp);

#ifdef __cplusplus
}
#endif

#endif /* __TUYA_AI_DISPLAY_H__ */


/**
 * @file guided_wechat_core.h
 * @brief Private entry points for the standalone Guided WeChat UI.
 */
#ifndef __GUIDED_WECHAT_CORE_H__
#define __GUIDED_WECHAT_CORE_H__

#include "tuya_cloud_types.h"

#ifdef __cplusplus
extern "C" {
#endif

OPERATE_RET guided_wechat_core_init(void);
OPERATE_RET guided_wechat_register_with_init(OPERATE_RET (*disp_init)(void));
OPERATE_RET guided_wechat_core_register(void);

#ifdef __cplusplus
}
#endif

#endif /* __GUIDED_WECHAT_CORE_H__ */

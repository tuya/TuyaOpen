/**
 * @file tal_cellular_mds.h
 * @brief
 * @version 0.1
 * @date 2022-04-28
 *
 * @copyright Copyright (c) 2021-2022 Tuya Inc. All Rights Reserved.
 *
 * Permission is hereby granted, to any person obtaining a copy of this software and
 * associated documentation files (the "Software"), Under the premise of complying
 * with the license of the third-party open source software contained in the software,
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software.
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 */
/**
 * @file tuya_cellular_mds.h
 * @author www.tuya.com
 * @brief 蜂窝模组基础服务API定义。
 *
 * @copyright Copyright (c) tuya.inc 2021
 */

#ifndef TAL_CELLULAR_MDS
#define TAL_CELLULAR_MDS

#include "tuya_cloud_types.h"
#if !defined(ENABLE_CELLULAR_PLUGIN) || ENABLE_CELLULAR_PLUGIN == 0
#include "tkl_cellular_base.h"
#include "tkl_cellular_mds.h"
#else
// 包含at版本的蜂窝接口头文件
#endif

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 初始化蜂窝移动数据服务
 * @param simId sim卡ID
 * @return 0 成功 其它 失败
 */
OPERATE_RET tal_cellular_mds_init(uint8_t sim_id);

/**
 * @brief 获取蜂窝移动数据服务的鉴权状态
 * @param simId sim卡ID
 * @return 蜂窝移动数据鉴权状态，查看 @TUYA_CELLULAR_MDS_STATUS_E 定义
 */
TUYA_CELLULAR_MDS_STATUS_E tal_cellular_mds_get_status(uint8_t sim_id);

/**
 * @brief 蜂窝移动数据PDP激活
 * @param simId sim卡ID
 * @param apn 运营商APN设置
 * @param username 用户名
 * @param password 密码
 * @return 0 成功 其它 失败
 */
OPERATE_RET tal_cellular_mds_pdp_active(uint8_t sim_id,char * apn, char * username, char * password);

/**
 * @brief 蜂窝移动数据指定CID PDP激活
 * @param simId sim卡ID
 * @param cid Specify the PDP Context Identifier
 * @param apn 运营商APN设置
 * @param username 用户名
 * @param password 密码
 * @return 0 成功 其它 失败
 */
OPERATE_RET tal_cellular_mds_adv_pdp_active(uint8_t sim_id,uint8_t cid,TUYA_MDS_PDP_TYPE_E pdp_type,char * apn, char * username, char * password);

/**
 * @brief 蜂窝移动数据PDP去激活
* @param simId sim卡ID
 * @return 0 成功 其它 失败
 */
OPERATE_RET tal_cellular_mds_pdp_deactive(uint8_t sim_id);

/**
 * @brief 蜂窝移动数据指定CID PDP去激活
 * @param simId sim卡ID
 * @param cid Specify the PDP Context Identifier
 * @return 0 成功 其它 失败
 */
OPERATE_RET tal_cellular_mds_adv_pdp_deactive(uint8_t sim_id,uint8_t cid);

/**
 * @brief 蜂窝移动数据PDP自动重激活设置
 * @param simId sim卡ID
 * @param enable TRUE 开启自动重新激活 FALSE 关闭自动重新激活
 * @return 0 成功 其它 失败
 */
OPERATE_RET tal_cellular_mds_pdp_auto_reactive(uint8_t sim_id,bool enable);

/**
 * @brief 注册蜂窝数据服务状态变化通知函数
 * @param fun 状态变化通知函数
 * @return 0 成功  其它 失败
 */
OPERATE_RET tal_cellular_mds_register_state_notify(uint8_t sim_id,TKL_MDS_NOTIFY fun);

/**
 * @brief   Get device ip address.
 * @param   ip: The type of NW_IP_S
 * @return  OPRT_OK: success  Other: fail
 */
OPERATE_RET tal_cellular_mds_get_ip(uint8_t sim_id,NW_IP_S *ip);

/**
 * @brief 获取蜂窝是否已经已启动了pdp active
 * @param sim_id sim id 单卡支持0,双卡支持0,1
 * @param status 启动状态
 * @return OPRT_OK 获取成功 其它 获取失败
 */
OPERATE_RET tal_cellular_mds_get_pdp_start_status(uint8_t sim_id, bool *status);

#ifdef __cplusplus
}
#endif

#endif /* TAL_CELLULAR_MDS */

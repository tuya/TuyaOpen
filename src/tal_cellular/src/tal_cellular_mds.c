#include "tal_cellular_mds.h"
#include "tkl_init_cellular.h"

static bool s_pdp_start[2] = {false};
#define CHECK_SIM_ID(a) do { \
            if (a > 1) { \
                return OPRT_INVALID_PARM; \
            }\
        } while(0)\

/**
 * @brief 初始化蜂窝移动数据服务
 * @param simId sim卡ID
 * @return 0 成功 其它 失败
 */
OPERATE_RET tal_cellular_mds_init(uint8_t sim_id)
{
    if(tkl_cellular_mds_desc_get()->mds_init == NULL) {
        return OPRT_NOT_SUPPORTED;
    }
    CHECK_SIM_ID(sim_id);
    return tkl_cellular_mds_desc_get()->mds_init(sim_id);
}

/**
 * @brief 获取蜂窝移动数据服务的鉴权状态
 * @param simId sim卡ID
 * @return 蜂窝移动数据鉴权状态，查看 @TUYA_CELLULAR_MDS_STATUS_E 定义
 */
TUYA_CELLULAR_MDS_STATUS_E tal_cellular_mds_get_status(uint8_t sim_id)
{
    if(tkl_cellular_mds_desc_get()->get_mds_status == NULL) {
        return OPRT_NOT_SUPPORTED;
    }
    CHECK_SIM_ID(sim_id);
    return tkl_cellular_mds_desc_get()->get_mds_status(sim_id);
}

/**
 * @brief 蜂窝移动数据PDP激活
 * @param simId sim卡ID
 * @param apn 运营商APN设置
 * @param username 用户名
 * @param password 密码
 * @return 0 成功 其它 失败
 */
OPERATE_RET tal_cellular_mds_pdp_active(uint8_t sim_id,char * apn, char * username, char * password)
{
    if(tkl_cellular_mds_desc_get()->pdp_active == NULL) {
        return OPRT_NOT_SUPPORTED;
    }
    CHECK_SIM_ID(sim_id);
    OPERATE_RET ret =  tkl_cellular_mds_desc_get()->pdp_active(sim_id, apn, username, password);
    if(ret == OPRT_OK) {
        s_pdp_start[sim_id] = true;
    }
    return ret;
}

/**
 * @brief 蜂窝移动数据指定CID PDP激活
 * @param simId sim卡ID
 * @param cid Specify the PDP Context Identifier
 * @param apn 运营商APN设置
 * @param username 用户名
 * @param password 密码
 * @return 0 成功 其它 失败
 */
OPERATE_RET tal_cellular_mds_adv_pdp_active(uint8_t sim_id,uint8_t cid,TUYA_MDS_PDP_TYPE_E pdp_type,char * apn, char * username, char * password)
{
    if(tkl_cellular_mds_desc_get()->adv_pdp_active == NULL) {
        return OPRT_NOT_SUPPORTED;
    }
    CHECK_SIM_ID(sim_id);
    OPERATE_RET ret =  tkl_cellular_mds_desc_get()->adv_pdp_active(sim_id, cid, pdp_type, apn, username, password);
    if(ret == OPRT_OK) {
        s_pdp_start[sim_id] = true;
    }
    return ret;
}

/**
 * @brief 蜂窝移动数据PDP去激活
* @param simId sim卡ID
 * @return 0 成功 其它 失败
 */
OPERATE_RET tal_cellular_mds_pdp_deactive(uint8_t sim_id)
{
    if(tkl_cellular_mds_desc_get()->pdp_deactive == NULL) {
        return OPRT_NOT_SUPPORTED;
    }
    CHECK_SIM_ID(sim_id);
    OPERATE_RET ret = tkl_cellular_mds_desc_get()->pdp_deactive(sim_id);
    if(ret == OPRT_OK) {
        s_pdp_start[sim_id] = false;
    }
    return ret;
}

/**
 * @brief 蜂窝移动数据指定CID PDP去激活
 * @param simId sim卡ID
 * @param cid Specify the PDP Context Identifier
 * @return 0 成功 其它 失败
 */
OPERATE_RET tal_cellular_mds_adv_pdp_deactive(uint8_t sim_id,uint8_t cid)
{
    if(tkl_cellular_mds_desc_get()->adv_pdp_deactive == NULL) {
        return OPRT_NOT_SUPPORTED;
    }
    CHECK_SIM_ID(sim_id);
    OPERATE_RET ret = tkl_cellular_mds_desc_get()->adv_pdp_deactive(sim_id, cid);
    if(ret == OPRT_OK) {
        s_pdp_start[sim_id] = false;
    }
    return ret;
}

/**
 * @brief 蜂窝移动数据PDP自动重激活设置
 * @param simId sim卡ID
 * @param enable TRUE 开启自动重新激活 FALSE 关闭自动重新激活
 * @return 0 成功 其它 失败
 */
OPERATE_RET tal_cellular_mds_pdp_auto_reactive(uint8_t sim_id,bool enable)
{
    if(tkl_cellular_mds_desc_get()->pdp_auto_reactive_enable == NULL) {
        return OPRT_NOT_SUPPORTED;
    }
    CHECK_SIM_ID(sim_id);
    return tkl_cellular_mds_desc_get()->pdp_auto_reactive_enable(sim_id, enable);
}

/**
 * @brief 注册蜂窝数据服务状态变化通知函数
 * @param fun 状态变化通知函数
 * @return 0 成功  其它 失败
 */
OPERATE_RET tal_cellular_mds_register_state_notify(uint8_t sim_id,TKL_MDS_NOTIFY fun)
{
    if(tkl_cellular_mds_desc_get()->registr_mds_net_notify == NULL) {
        return OPRT_NOT_SUPPORTED;
    }
    CHECK_SIM_ID(sim_id);
    return tkl_cellular_mds_desc_get()->registr_mds_net_notify(sim_id, fun);
}

/**
 * @brief   Get device ip address.
 * @param   ip: The type of NW_IP_S
 * @return  OPRT_OK: success  Other: fail
 */
OPERATE_RET tal_cellular_mds_get_ip(uint8_t sim_id,NW_IP_S *ip)
{
    if(tkl_cellular_mds_desc_get()->get_ip == NULL) {
        return OPRT_NOT_SUPPORTED;
    }
    CHECK_SIM_ID(sim_id);
    return tkl_cellular_mds_desc_get()->get_ip(sim_id, ip);
}

/**
 * @brief 获取蜂窝是否已经已启动了pdp active
 * @param sim_id sim id 单卡支持0,双卡支持0,1
 * @param status 启动状态
 * @return OPRT_OK 获取成功 其它 获取失败
 */
OPERATE_RET tal_cellular_mds_get_pdp_start_status(uint8_t sim_id, bool *status)
{
    CHECK_SIM_ID(sim_id);
    *status = s_pdp_start[sim_id];
    return OPRT_OK;
}
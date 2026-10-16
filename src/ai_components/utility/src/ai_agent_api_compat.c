/**
 * @file ai_agent_api_compat.c
 * @brief [tuyaopen-core 适配] core 的 svc_ai_agent 相对上游裁剪掉的三个导出接口的
 *        补齐实现，使上游 ai_mode / ai_picture 的调用点可以原样保留。
 *
 * 背景：上游 `src/tuya_ai_service/svc_ai_agent` 导出下列三个接口，core
 * （agent-core/svc_ai_agent）未导出；本仓 ai_components 为上游版本，调用它们：
 *
 *   1) tuya_ai_agent_set_scode(CHAR_T *scode)
 *      上游：写 ctx.scode。core：同名函数存在但为 STATIC（tuya_ai_agent.c:117），
 *      且 ctx.scode 恒为空串（core 内只有 set_scode(NULL) 一处调用，且该实现
 *      `if (scode)` 对 NULL 直接返回）——get_scode(NULL) 取到的就是 ""。
 *      本仓唯一调用方 ai_mode 传的 AI_AGENT_SCODE_DEFAULT 即 ""（ai_manage_mode.h:34），
 *      ⇒ 空实现与 core 现状语义等价。
 *
 *   2) tuya_ai_agent_server_vad_ctrl(BOOL_T flag)
 *      上游：写 ctx.enable_serv_vad。core：字段在（tuya_ai_agent.c:56，
 *      init 时置 TRUE:1111，作为会话属性 asr.enableVad 上报:1364），但**无 setter**。
 *      空实现 ⇒ 服务端 VAD 保持 core 默认（开）。这是 dev 树 agent_demo 已验证的
 *      组合（服务端 VAD 定轮次边界，全链路 ASR→NLG→TTS 通）；与上游 HOLD 模式
 *      "本地 VAD 定边界"存在行为差异，待 core 恢复 setter 后自动消除。
 *
 *   3) tuya_ai_agent_set_session_param(CHAR_T *value)
 *      上游：解析 JSON 存 ctx.session_param，随会话下发。core 无该字段。
 *      空实现 ⇒ ai_picture 的 output 尺寸等 session param 不下发（仅
 *      ENABLE_COMP_AI_PICTURE=y 时可达）。
 *
 * 移除条件：core 恢复上述导出后，删除本文件（否则重复定义，链接期即报错——刻意
 * 不做 weak，避免"core 已恢复但仍走空实现"的静默偏差）。
 *
 * @copyright Copyright (c) 2021-2026 Tuya Inc. All Rights Reserved.
 */

#include "tuya_cloud_types.h"
#include "tal_log.h"

OPERATE_RET tuya_ai_agent_set_scode(CHAR_T *scode)
{
    (void)scode;
    return OPRT_OK;
}

VOID tuya_ai_agent_server_vad_ctrl(BOOL_T flag)
{
    (void)flag;
}

VOID tuya_ai_agent_set_session_param(CHAR_T *value)
{
    (void)value;
}

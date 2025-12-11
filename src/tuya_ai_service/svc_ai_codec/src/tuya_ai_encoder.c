#include "tuya_cloud_types.h"
#include "tuya_ai_encoder.h"
#include "tuya_list.h"
#include "tal_memory.h"

typedef struct {
    TUYA_AI_ENCODER_T *encoder;
    LIST_HEAD node;
} ENCODER_MANAGE, *P_ENCODER_MANAGE;

STATIC BOOL_T s_init = FALSE;
STATIC LIST_HEAD s_all_encoders;

STATIC OPERATE_RET __tuya_ai_encoder_init(VOID)
{
    if (s_init)
        return OPRT_OK;

    INIT_LIST_HEAD(&s_all_encoders);

    s_init = TRUE;
    return OPRT_OK;
}

OPERATE_RET tuya_ai_register_encoder(TUYA_AI_ENCODER_T *encoder)
{
    if (!encoder)
        return OPRT_INVALID_PARM;

    if (!s_init)
        __tuya_ai_encoder_init();

    P_ENCODER_MANAGE entry = NULL;
    LIST_HEAD *pos = NULL;
    tuya_list_for_each(pos, &s_all_encoders) {
        entry = tuya_list_entry(pos, ENCODER_MANAGE, node);
        if (entry->encoder == encoder) {
            return OPRT_OK;
        } else if (entry->encoder->codec_type == encoder->codec_type) {
            return OPRT_COM_ERROR;
        }
    }

    entry = (P_ENCODER_MANAGE)tal_malloc(sizeof(ENCODER_MANAGE));
    if (!entry)
        return OPRT_MALLOC_FAILED;

    entry->encoder = encoder;
    tuya_list_add(&entry->node, &s_all_encoders);

    return OPRT_OK;
}

OPERATE_RET tuya_ai_unregister_encoder(TUYA_AI_ENCODER_T *encoder)
{
    if (!encoder)
        return OPRT_INVALID_PARM;

    if (!s_init)
        return OPRT_RESOURCE_NOT_READY;

    P_ENCODER_MANAGE entry = NULL;
    LIST_HEAD *pos = NULL;

    tuya_list_for_each(pos, &s_all_encoders) {
        entry = tuya_list_entry(pos, ENCODER_MANAGE, node);
        if (entry->encoder == encoder) {
            tuya_list_del(&entry->node);
            tal_free(entry);
            return OPRT_OK;
        }
    }

    return OPRT_NOT_EXIST;
}

TUYA_AI_ENCODER_T *tuya_ai_get_encoder(AI_AUDIO_CODEC_TYPE codec_type)
{
    if (!s_init)
        return NULL;

    P_ENCODER_MANAGE entry = NULL;
    LIST_HEAD *pos = NULL;

    tuya_list_for_each(pos, &s_all_encoders) {
        entry = tuya_list_entry(pos, ENCODER_MANAGE, node);
        if (entry->encoder->codec_type == codec_type) {
            return entry->encoder;
        }
    }

    return NULL;
}

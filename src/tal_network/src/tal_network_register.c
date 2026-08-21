/**
 * @file tal_network_register.c
 * @brief tal_network_register module is used to
 * @version 0.1
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#include "tal_network_register.h"

// 1 lwip/ socket -> posix
// 2 tkl
// 3 at_modem

/***********************************************************
************************macro define************************
***********************************************************/
#define DEFAULT_NETWORK_CARD_TYPE TAL_NET_TYPE_POSIX

/***********************************************************
***********************typedef define***********************
***********************************************************/
typedef struct {
    TAL_NETWORK_CARD_TYPE_E active_card_type;
    /* Source address of the active connection, 0 when no link is up. Kept here
     * rather than in TAL_NETWORK_CARD_T.ipaddr because several connections can
     * share one card type (on T5AI both wifi and cellular are PLATFORM), so this
     * is a property of the active link, not of the card. */
    TUYA_IP_ADDR_T active_ipaddr;
    TAL_NETWORK_CARD_T *active_card[TAL_NET_TYPE_MAX];
} TAL_NETWORK_CARD_MANAGER_T;

/***********************************************************
********************function declaration********************
***********************************************************/

/***********************************************************
***********************variable define**********************
***********************************************************/
#if (defined(ENABLE_LIBLWIP) && (ENABLE_LIBLWIP == 1)) || 100 == OPERATING_SYSTEM
extern TAL_NETWORK_CARD_T tal_network_card_posix;
#else
extern TAL_NETWORK_CARD_T tal_network_card_platform;
#endif

TAL_NETWORK_CARD_MANAGER_T tal_network_card_manager = {
#if (defined(ENABLE_LIBLWIP) && (ENABLE_LIBLWIP == 1)) || 100 == OPERATING_SYSTEM
    .active_card[TAL_NET_TYPE_POSIX] = &tal_network_card_posix,
    .active_card_type = TAL_NET_TYPE_POSIX,
#else
    .active_card[TAL_NET_TYPE_PLATFORM] = &tal_network_card_platform,
    .active_card_type = TAL_NET_TYPE_PLATFORM,
#endif
};

/***********************************************************
***********************function define**********************
***********************************************************/

OPERATE_RET tal_network_card_init(void)
{
#if (defined(ENABLE_LIBLWIP) && (ENABLE_LIBLWIP == 1)) || 100 == OPERATING_SYSTEM
    extern TAL_NETWORK_CARD_T tal_network_card_posix;
    tal_network_card_manager.active_card[TAL_NET_TYPE_POSIX] = &tal_network_card_posix;
    tal_network_card_manager.active_card_type = TAL_NET_TYPE_POSIX;
#else
    extern TAL_NETWORK_CARD_T tal_network_card_platform;
    tal_network_card_manager.active_card[TAL_NET_TYPE_PLATFORM] = &tal_network_card_platform;
    tal_network_card_manager.active_card_type = TAL_NET_TYPE_PLATFORM;
#endif

    return OPRT_OK;
}

OPERATE_RET tal_network_card_set_active(TAL_NETWORK_CARD_TYPE_E type)
{
    if (type >= TAL_NET_TYPE_MAX) {
        return OPRT_INVALID_PARM;
    }

    tal_network_card_manager.active_card_type = type;

    return OPRT_OK;
}

TAL_NETWORK_CARD_TYPE_E tal_network_card_get_active_type(void)
{
    return tal_network_card_manager.active_card_type;
}

OPERATE_RET tal_network_card_set_active_ip(TUYA_IP_ADDR_T ipaddr)
{
    tal_network_card_manager.active_ipaddr = ipaddr;

    return OPRT_OK;
}

TUYA_IP_ADDR_T tal_network_card_get_active_ip(void)
{
    return tal_network_card_manager.active_ipaddr;
}

TAL_NETWORK_OPS_T *tal_network_get_active_ops(void)
{
    TAL_NETWORK_CARD_T *card = tal_network_card_manager.active_card[tal_network_card_manager.active_card_type];
    if (card == NULL) {
        return NULL;
    }

    return &card->ops;
}

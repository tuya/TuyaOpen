/**
 * @file netcfg.c
 * @brief Implementation of network configuration management for Tuya devices.
 *
 * This file contains the core functionalities for managing network
 * configurations on Tuya devices. It includes mechanisms for initializing
 * network configuration sessions, handling different network configuration
 * modes, and invoking specific network configuration handlers based on the
 * device's current state and user input.
 *
 * The implementation leverages TAL for event handling, work queue management,
 * and list operations to efficiently manage network configuration sessions and
 * callbacks. It defines a netcfg_session_t structure for session management and
 * provides a callback mechanism (__netcfg_start_msg_cb) to start network
 * configuration processes.
 *
 * The network configuration process is crucial for setting up devices to
 * connect to local networks and the internet, enabling them to communicate with
 * Tuya's cloud services and other devices.
 *
 * @copyright Copyright (c) 2021-2024 Tuya Inc. All Rights Reserved.
 *
 */

#include "netcfg.h"
#include "tal_api.h"
#include "tuya_list.h"
#include "tal_workq_service.h"
#include "tal_event.h"

#define COUNT_OF(X) ((sizeof(X)) / (sizeof(X[0])))

#define IS_SET(n, mode) (((n) & (mode)) == (n))

typedef struct {
    LIST_HEAD listHead;
    int       type;
} netcfg_session_t;

static bool              isInited         = false;
static netcfg_session_t *p_netcfg_session = NULL;

static void __netcfg_start_msg_cb(void *msg)
{
    netcfg_handler_t *pHandler = NULL;
    pHandler                   = (netcfg_handler_t *)msg;

    int ret = pHandler->start(pHandler->type, pHandler->netcfg_finish_cb, pHandler->args);
    if (ret == OPRT_OK) {
        pHandler->isStarted = true;
    }
    PR_DEBUG("start 0x%x ret:%d", pHandler->type, ret);
}

/**
 * @brief Registers a network configuration handler.
 *
 * This function is used to register a network configuration handler for a
 * specific type of network configuration.
 *
 *  Network configuration registration interface, Tuya smartcfg, AP netcfg, BT
 * netcfg, etc., are all registered to this module. During registration, based
 * on the type of network configuration registered, the mode is set in advance,
 * and the actions that need to be performed by the mode, for example: For EZ
 * configuration, set to station mode, perform actions: scan AP, and store AP
 * information. For AP configuration, set to softap/stationap mode, perform
 * actions: set the AP's SSID, and start the AP.
 *
 * @param type The type of network configuration.
 * @param start The callback function to start the network configuration.
 * @param stop The callback function to stop the network configuration.
 * @return Returns 0 on success, or a negative error code on failure.
 */
int netcfg_register(int type, netcfg_start_cb_t start, netcfg_stop_cb_t stop)
{
    P_LIST_HEAD       pPos;
    netcfg_handler_t *phandler;
    tuya_list_for_each(pPos, &(p_netcfg_session->listHead))
    {
        phandler = tuya_list_entry(pPos, netcfg_handler_t, node);
        if (phandler->type == type) {
            return OPRT_INVALID_PARM;
        }
    }

    NEW_LIST_NODE(netcfg_handler_t, phandler);
    if (phandler == NULL) {
        return OPRT_MALLOC_FAILED;
    }

    phandler->type             = type;
    phandler->start            = start;
    phandler->stop             = stop;
    phandler->isStarted        = false;
    phandler->netcfg_finish_cb = NULL;

    tuya_list_add(&phandler->node, &p_netcfg_session->listHead);

    return OPRT_OK;
}

/**
 * Unregisters a network configuration of the specified type.
 *
 * @param type The type of network configuration to unregister.
 * @return 0 if the network configuration was successfully unregistered,
 * otherwise a negative error code.
 */
int netcfg_unregister(int type)
{
    if (p_netcfg_session == NULL) {
        return OPRT_COM_ERROR;
    }
    P_LIST_HEAD       pPos, pNext;
    netcfg_handler_t *phandler;
    tuya_list_for_each_safe(pPos, pNext, &(p_netcfg_session->listHead))
    {
        phandler = tuya_list_entry(pPos, netcfg_handler_t, node);
        if (phandler->type == type) {
            DeleteNodeAndFree(phandler, node);
            return OPRT_OK;
        }
    }

    return OPRT_INVALID_PARM;
}

/**
 * @brief Retrieves the netcfg handler based on the specified type.
 *
 * This function returns a pointer to the netcfg handler associated with the
 * given type.
 *
 * @param type The type of the netcfg handler to retrieve.
 * @return A pointer to the netcfg handler if found, or NULL if not found.
 */
netcfg_handler_t *netcfg_get_handler(int type)
{

    if (p_netcfg_session == NULL) {
        return NULL;
    }

    P_LIST_HEAD       pPos;
    netcfg_handler_t *phandler;
    tuya_list_for_each(pPos, &(p_netcfg_session->listHead))
    {
        phandler = tuya_list_entry(pPos, netcfg_handler_t, node);
        if (phandler->type == type) {
            return phandler;
        }
    }

    return NULL;
}

/**
 * @brief Initializes the network configuration.
 *
 * This function initializes the network configuration module.
 *
 * @return Returns 0 on success, or a negative error code on failure.
 */
int netcfg_init(void)
{
    if (p_netcfg_session == NULL) {
        p_netcfg_session = tal_malloc(sizeof(netcfg_session_t));
        if (p_netcfg_session == NULL) {
            return OPRT_MALLOC_FAILED;
        }
        INIT_LIST_HEAD(&p_netcfg_session->listHead);
    }

    return OPRT_OK;
}

/**
 * @brief Uninitializes the network configuration module.
 *
 * This function is responsible for cleaning up any resources used by the
 * network configuration module. It should be called when the module is no
 * longer needed.
 *
 * @return Returns 0 on success, or a negative error code on failure.
 */
int netcfg_uninit(void)
{
    if (p_netcfg_session) {
        P_LIST_HEAD       pPos, pNext;
        netcfg_handler_t *phandler;
        tal_workq_cancel(WORKQ_HIGHTPRI, __netcfg_start_msg_cb, NULL);

        tuya_list_for_each_safe(pPos, pNext, &(p_netcfg_session->listHead))
        {
            phandler = tuya_list_entry(pPos, netcfg_handler_t, node);
            if (phandler) {
                DeleteNodeAndFree(phandler, node);
            }
        }
        tal_free(p_netcfg_session);
        p_netcfg_session = NULL;
    }

    return OPRT_OK;
}

/**
 * @brief Retrieves the number of registered network configurations.
 *
 * This function returns the count of registered network configurations.
 *
 * @return The number of registered network configurations.
 */
int netcfg_get_register_count(void)
{

    if (p_netcfg_session == NULL) {
        return 0;
    }
    int               count = 0;
    P_LIST_HEAD       pPos;
    netcfg_handler_t *phandler;
    tuya_list_for_each(pPos, &(p_netcfg_session->listHead))
    {
        phandler = tuya_list_entry(pPos, netcfg_handler_t, node);
        if (phandler) {
            count++;
        }
    }
    return count;
}

/**
 * Retrieves the count of register started events in the network configuration
 * module.
 *
 * @return The count of register started events.
 */
int netcfg_get_register_started_count(void)
{
    if (p_netcfg_session == NULL) {
        return 0;
    }
    int               count = 0;
    P_LIST_HEAD       pPos;
    netcfg_handler_t *phandler;
    tuya_list_for_each(pPos, &(p_netcfg_session->listHead))
    {
        phandler = tuya_list_entry(pPos, netcfg_handler_t, node);
        if (phandler->isStarted) {
            count++;
        }
    }
    return count;
}

/**
 * @brief Starts the network configuration process.
 *
 * This function is used to initiate the network configuration process.
 *
 * @param type The type of network configuration.
 * @param netcfg_finish_cb The callback function to be called when the network
 * configuration is finished.
 * @param args Additional arguments to be passed to the callback function.
 *
 * @return The result of the network configuration start operation.
 */
int netcfg_start(int type, netcfg_finish_cb_t netcfg_finish_cb, void *args)
{
    netcfg_handler_t *pHandler = NULL;

    if (!isInited) {
        // netcfg_mode_handler_process();
        isInited = true;
        // tal_event_publish(EVENT_WIFI_STATUS_UPDATE, NULL);
    }

    PR_DEBUG("netcfg module start type:0x%x", type);
    pHandler = netcfg_get_handler(type);
    if (pHandler == NULL) {
        PR_ERR("netcfg type 0x%x is not regist", type);
        return OPRT_INVALID_PARM;
    } else {
        pHandler->netcfg_finish_cb = netcfg_finish_cb;
        pHandler->args             = args;
        tal_workq_schedule(WORKQ_HIGHTPRI, __netcfg_start_msg_cb, pHandler);
    }

    return OPRT_OK;
}

/**
 * @brief Checks if the netcfg module has been initialized.
 *
 * This function returns a boolean value indicating whether the netcfg module
 * has been initialized or not.
 *
 * @return BOOL_T - Returns `TRUE` if the netcfg module has been initialized,
 * `FALSE` otherwise.
 */
BOOL_T is_netcfg_inited(void)
{
    return isInited;
}

/**
 * @brief Stops the network configuration process.
 *
 * This function is used to stop the network configuration process for a
 * specific type.
 *
 * @param type The type of network configuration to stop.
 *
 * @return 0 if the network configuration process was successfully stopped,
 * otherwise an error code.
 */
int netcfg_stop(int type)
{

    int               ret      = OPRT_OK;
    netcfg_handler_t *pHandler = NULL;

    if (type != 0) {
        pHandler = netcfg_get_handler(type);
        if (pHandler == NULL) {
            return OPRT_INVALID_PARM;
        } else {
            // CAPTURE_NETCFG_DATA(type, TY_DATA_NETCFG_STEP8);
            PR_DEBUG("netcfg module stop type:0x%x", type);
            if (pHandler->isStarted) {
                pHandler->isStarted = false;
                ret                 = pHandler->stop(type);
                if (ret != OPRT_OK) {
                    PR_ERR("netcfg module stop type:%d failed", type);
                    return ret;
                } else {
                    return OPRT_OK;
                }
            }
        }
    } else { /*stop netcfg module*/

        if (p_netcfg_session == NULL) {
            return OPRT_COM_ERROR;
        }
        P_LIST_HEAD pPos;
        tuya_list_for_each(pPos, &(p_netcfg_session->listHead))
        {
            pHandler = tuya_list_entry(pPos, netcfg_handler_t, node);
            if (pHandler->isStarted) {
                pHandler->isStarted = false;
                PR_DEBUG("netcfg module stop type:0x%x", pHandler->type);
                ret = pHandler->stop(pHandler->type);
                if (ret != OPRT_OK) {
                    PR_ERR("netcfg module stop type:0x%x fail", type);
                    continue;
                }
            }
        }
        /*destroy netcfg module*/
        netcfg_uninit();
    }
    return OPRT_OK;
}

/**
 * @brief Starts the network configuration process for other devices.
 *
 * This function initiates the network configuration process for other devices.
 *
 * @param type The type of network configuration to start.
 *
 * @return Returns 0 on success, or a negative error code on failure.
 */
int netcfg_start_other_all(int type)
{
    int               ret      = OPRT_OK;
    netcfg_handler_t *pHandler = NULL;
    if (p_netcfg_session == NULL) {
        return OPRT_COM_ERROR;
    }

    P_LIST_HEAD pPos;
    tuya_list_for_each(pPos, &(p_netcfg_session->listHead))
    {
        pHandler = tuya_list_entry(pPos, netcfg_handler_t, node);
        if ((pHandler->netcfg_finish_cb) && (!pHandler->isStarted) && (type != pHandler->type)) {
            PR_DEBUG("netcfg module start type:0x%x", pHandler->type);
            ret = netcfg_start(pHandler->type, pHandler->netcfg_finish_cb, pHandler->args);
            if (ret != OPRT_OK) {
                PR_ERR("netcfg module start type:0x%x fail", type);
                continue;
            }
        }
    }

    return OPRT_OK;
}

/**
 * Stops all other network configurations of the specified type.
 *
 * @param type The type of network configuration to stop.
 * @return Returns 0 on success, or a negative error code on failure.
 */
int netcfg_stop_other_all(int type)
{

    int               ret      = OPRT_OK;
    netcfg_handler_t *pHandler = NULL;

    if (p_netcfg_session == NULL) {
        return OPRT_COM_ERROR;
    }

    P_LIST_HEAD pPos;
    tuya_list_for_each(pPos, &(p_netcfg_session->listHead))
    {
        pHandler = tuya_list_entry(pPos, netcfg_handler_t, node);
        if (pHandler->isStarted && (type != pHandler->type)) {
            pHandler->isStarted = false;
            PR_DEBUG("netcfg module stop type:0x%x", pHandler->type);
            ret = pHandler->stop(pHandler->type);
            if (ret != OPRT_OK) {
                PR_ERR("netcfg module stop type:0x%x fail", type);
                continue;
            }
        }
    }

    return OPRT_OK;
}

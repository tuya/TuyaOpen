/**
 * @ tuya_ipc_p2p_error.h
 * @brief p2p err define
 * @version 0.1
 * @date 2021-11-17
 *
 * @copyright Copyright (c) tuya.inc 2011
 *
 */

#ifndef _TUYA_IPC_P2P_ERROR_H_
#define _TUYA_IPC_P2P_ERROR_H_

#ifdef __cplusplus
extern "C" {
#endif

#define ERROR_P2P_SUCCESSFUL                         0   /** p2p operation success*/
#define ERROR_P2P_NOT_INITIALIZED                    -1  /** p2p has not init*/
#define ERROR_P2P_ALREADY_INITIALIZED                -2  /** p2p has inited*/
#define ERROR_P2P_TIME_OUT                           -3  /** p2p has inited*/
#define ERROR_P2P_INVALID_ID                         -4  /** p2p invalid id*/
#define ERROR_P2P_INVALID_PARAMETER                  -5  /** p2p invalid param*/
#define ERROR_P2P_DEVICE_NOT_ONLINE                  -6  /** device outline*/
#define ERROR_P2P_FAIL_TO_RESOLVE_NAME               -7  /** p2p name err*/
#define ERROR_P2P_INVALID_PREFIX                     -8  /** p2p prefix err*/
#define ERROR_P2P_ID_OUT_OF_DATE                     -9  /** device outline*/
#define ERROR_P2P_NO_RELAY_SERVER_AVAILABLE          -10 /** server no relay*/
#define ERROR_P2P_INVALID_SESSION_HANDLE             -11 /** invalid session*/
#define ERROR_P2P_SESSION_CLOSED_REMOTE              -12 /** remote close*/
#define ERROR_P2P_SESSION_CLOSED_TIMEOUT             -13 /** close timeout*/
#define ERROR_P2P_SESSION_CLOSED_CALLED              -14 /** close called*/
#define ERROR_P2P_REMOTE_SITE_BUFFER_FULL            -15 /** remote buffer full*/
#define ERROR_P2P_USER_LISTEN_BREAK                  -16 /** listen break*/
#define ERROR_P2P_MAX_SESSION                        -17 /** limit max session*/
#define ERROR_P2P_UDP_PORT_BIND_FAILED               -18 /** port bind fail*/
#define ERROR_P2P_USER_CONNECT_BREAK                 -19 /** connect err*/
#define ERROR_P2P_SESSION_CLOSED_INSUFFICIENT_MEMORY -20 /** memory insufficent*/
#define ERROR_P2P_INVALID_APILICENSE                 -21 /** invalid apilicense*/
#define ERROR_P2P_FAIL_TO_CREATE_THREAD              -22 /** create pthread fail*/

#ifdef __cplusplus
}
#endif

#endif

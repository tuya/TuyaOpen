
#include <stdio.h>
#include "tuya_error.h"

#define BUILD_ERR(code, msg)                                                                                           \
    {                                                                                                                  \
        code, msg " (" #code ")"                                                                                       \
    }
static const struct {
    int code;
    const char *msg;
} err_str[] = {
    BUILD_ERR(TUYA_P2P_EINSTUNMSG, "Invalid STUN message"),
    BUILD_ERR(TUYA_P2P_EINSTUNMSGLEN, "Invalid STUN message length"),
    BUILD_ERR(TUYA_P2P_EINSTUNMSGTYPE, "Invalid or unexpected STUN message type"),
    BUILD_ERR(TUYA_P2P_ESTUNTIMEDOUT, "STUN transaction has timed out"),

    BUILD_ERR(TUYA_P2P_ESTUNTOOMANYATTR, "Too many STUN attributes"),
    BUILD_ERR(TUYA_P2P_ESTUNINATTRLEN, "Invalid STUN attribute length"),
    BUILD_ERR(TUYA_P2P_ESTUNDUPATTR, "Found duplicate STUN attribute"),
    BUILD_ERR(TUYA_P2P_ESTUNFINGERPRINT, "STUN FINGERPRINT verification failed"),
    BUILD_ERR(TUYA_P2P_ESTUNMSGINTPOS, "Invalid STUN attribute after MESSAGE-INTEGRITY"),
    BUILD_ERR(TUYA_P2P_ESTUNFINGERPOS, "Invalid STUN attribute after FINGERPRINT"),
    BUILD_ERR(TUYA_P2P_ESTUNNOMAPPEDADDR, "STUN (XOR-)MAPPED-ADDRESS attribute not found"),
    BUILD_ERR(TUYA_P2P_ESTUNIPV6NOTSUPP, "STUN IPv6 attribute not supported"),
    BUILD_ERR(TUYA_P2P_EINVAF, "Invalid address family value in STUN message"),
    BUILD_ERR(TUYA_P2P_ESTUNINSERVER, "Invalid STUN server or server not configured"),
    BUILD_ERR(TUYA_P2P_ESTUNDESTROYED, "STUN object has been destoyed"),

    BUILD_ERR(TUYA_P2P_ENOICE, "ICE session not available"),
    BUILD_ERR(TUYA_P2P_EICEINPROGRESS, "ICE check is in progress"),
    BUILD_ERR(TUYA_P2P_EICEFAILED, "ICE connectivity check has failed"),
    BUILD_ERR(TUYA_P2P_EICEMISMATCH, "Default destination does not match any ICE candidates"),
    BUILD_ERR(TUYA_P2P_EICEINCOMPID, "Invalid ICE component ID"),
    BUILD_ERR(TUYA_P2P_EICEINCANDID, "Invalid ICE candidate ID"),
    BUILD_ERR(TUYA_P2P_EICEINSRCADDR, "Source address mismatch"),
    BUILD_ERR(TUYA_P2P_EICEMISSINGSDP, "Missing ICE SDP attribute"),
    BUILD_ERR(TUYA_P2P_EICEINCANDSDP, "Invalid SDP candidate attribute"),
    BUILD_ERR(TUYA_P2P_EICENOHOSTCAND, "No host candidate associated with srflx"),
    BUILD_ERR(TUYA_P2P_EICENOMTIMEOUT, "Controlled timed-out waiting for  Controlling  to send nominated check"),

    BUILD_ERR(TUYA_P2P_ETURNINTP, "Invalid or unsupported TURN transport")};

void tuya_p2p_strerror(int32_t statcode, char *buf, size_t bufsize)
{
    if (buf == NULL || bufsize <= 0) {
        return;
    }

    if (statcode == TUYA_P2P_SUCCESS) {
        snprintf(buf, bufsize, "Success");
    } else {
        int i;
        for (i = 0; i < sizeof(err_str) / sizeof(err_str[0]); ++i) {
            if (err_str[i].code == statcode) {
                snprintf(buf, bufsize, "%s", err_str[i].msg);
                return;
            }
        }
    }
    snprintf(buf, bufsize, "Unknown tuya p2p error %d", statcode);
    return;
}

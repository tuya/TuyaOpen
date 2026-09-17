/**
 * @file guided_wechat_state.h
 * @brief Read-only snapshot of authorization, networking and cloud state.
 */

#ifndef __GUIDED_WECHAT_STATE_H__
#define __GUIDED_WECHAT_STATE_H__

#include <stdbool.h>

#define GUIDED_WECHAT_VALUE_LEN 64
#define GUIDED_WECHAT_SSID_LEN  34

typedef enum {
    GUIDED_WECHAT_AUTH_NONE = 0,
    GUIDED_WECHAT_AUTH_STORAGE,
    GUIDED_WECHAT_AUTH_COMPILED,
} guided_wechat_auth_source_t;

typedef enum {
    GUIDED_WECHAT_PAIRING_IDLE = 0,
    GUIDED_WECHAT_PAIRING_STARTING,
    GUIDED_WECHAT_PAIRING_WAITING_PHONE,
    GUIDED_WECHAT_PAIRING_CONNECTING,
} guided_wechat_pairing_state_t;

typedef struct {
    guided_wechat_auth_source_t auth_source;
    guided_wechat_pairing_state_t pairing_state;
    bool link_up;
    bool activated;
    bool cloud_online;
    char ssid[GUIDED_WECHAT_SSID_LEN];
    char version[GUIDED_WECHAT_VALUE_LEN];
    char sdk_version[GUIDED_WECHAT_VALUE_LEN];
    char product_id[GUIDED_WECHAT_VALUE_LEN];
    char device_id[GUIDED_WECHAT_VALUE_LEN];
    char uuid_masked[GUIDED_WECHAT_VALUE_LEN];
    char authkey_masked[GUIDED_WECHAT_VALUE_LEN];
} guided_wechat_device_state_t;

void guided_wechat_state_read(guided_wechat_device_state_t *state);
bool guided_wechat_state_equal(const guided_wechat_device_state_t *left,
                               const guided_wechat_device_state_t *right);

/* Pairing state notifications from the application/SDK event boundary. */
void guided_wechat_pairing_request(void);
void guided_wechat_pairing_begin(void);
void guided_wechat_pairing_data_received(void);
void guided_wechat_pairing_complete(void);
void guided_wechat_pairing_reset(void);

/* Persist the user intent to resume the guided network step after reboot. */
void guided_wechat_reprovision_mark(void);
bool guided_wechat_reprovision_pending(void);
void guided_wechat_reprovision_clear(void);

#endif /* __GUIDED_WECHAT_STATE_H__ */

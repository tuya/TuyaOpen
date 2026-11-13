/**
 * @file desk_buddy_test.h
 * @brief Asynchronous hardware self-test service for Desk Buddy.
 *
 * @copyright Copyright (c) 2021-2026 Tuya Inc. All Rights Reserved.
 */

#ifndef _DESK_BUDDY_TEST_H_
#define _DESK_BUDDY_TEST_H_

#include "tuya_cloud_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    DESK_BUDDY_TEST_FRONT_CAMERA = 0,
    DESK_BUDDY_TEST_REAR_CAMERA,
    DESK_BUDDY_TEST_SD_CARD,
    DESK_BUDDY_TEST_ENCODER,
    DESK_BUDDY_TEST_LED,
    DESK_BUDDY_TEST_MOTION,
    DESK_BUDDY_TEST_MAX,
} DESK_BUDDY_TEST_ID_E;

typedef enum {
    DESK_BUDDY_TEST_STATUS_PASS = 0,
    DESK_BUDDY_TEST_STATUS_FAIL,
    DESK_BUDDY_TEST_STATUS_SKIPPED,
    DESK_BUDDY_TEST_STATUS_STOPPED,
} DESK_BUDDY_TEST_STATUS_E;

typedef struct {
    DESK_BUDDY_TEST_ID_E id;
    DESK_BUDDY_TEST_STATUS_E status;
    char detail[96];
} DESK_BUDDY_TEST_RESULT_T;

OPERATE_RET desk_buddy_test_init(void);

/**
 * @brief Start one peripheral test.
 *
 * Only one test can be pending at a time. Camera, SD, LED and motion tests
 * close their resources before reporting completion. Encoder remains open
 * after a successful probe so the UI can keep reading its position.
 */
OPERATE_RET desk_buddy_test_request(DESK_BUDDY_TEST_ID_E id);

/**
 * @brief Cancel an active test and close resources owned by this service.
 *
 * Camera, SD, LED and motion cleanup is completed by their worker paths. The
 * encoder is closed synchronously because it intentionally remains open after
 * a successful test.
 */
OPERATE_RET desk_buddy_test_stop(DESK_BUDDY_TEST_ID_E id);
bool desk_buddy_test_poll_result(DESK_BUDDY_TEST_RESULT_T *result);
OPERATE_RET desk_buddy_test_get_encoder_state(int32_t *position, uint8_t *pressed);

#ifdef __cplusplus
}
#endif

#endif /* _DESK_BUDDY_TEST_H_ */

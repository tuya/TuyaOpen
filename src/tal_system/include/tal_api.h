/**
 * @file tal_api.h
 * @brief Header file that aggregates all Tuya Abstract Layer (TAL) APIs.
 *
 * This file serves as a central inclusion point for all Tuya Abstract Layer
 * (TAL) components, facilitating access to various system-level functionalities
 * such as logging, memory management, threading, and more. It includes headers
 * for modules that provide interfaces for operating system abstractions,
 * hardware abstraction layers, and utility services. The purpose of TAL is to
 * offer a unified and simplified API layer over different platforms, enhancing
 * portability and reducing the complexity of application development on the
 * Tuya IoT platform.
 *
 * Included modules cover a wide range of functionalities:
 * - Logging and diagnostics (tal_log.h)
 * - Memory management (tal_memory.h)
 * - Concurrency primitives (tal_mutex.h, tal_semaphore.h, tal_thread.h)
 * - OTA updates (tal_ota.h)
 * - Inter-thread communication (tal_queue.h, tal_workqueue.h)
 * - Time and timers (tal_sleep.h, tal_sw_timer.h, tal_time_service.h)
 * - System utilities (tal_system.h)
 * - Hardware interfaces (tal_uart.h)
 * - Configuration storage (tal_kv.h)
 * - Security and encryption (tal_security.h)
 * - Event handling (tal_event.h)
 *
 * This header ensures that applications built on the Tuya platform can leverage
 * these abstractions to achieve platform independence, focusing on application
 * logic rather than underlying system and hardware specifics.
 *
 * @note This file is part of the Tuya IoT Development Platform and is intended
 * for use in Tuya-based applications. It is subject to the platform's license
 * and copyright terms.
 *
 * @copyright Copyright (c) 2021-2024 Tuya Inc. All Rights Reserved.
 *
 */

#ifndef __TAL_API_H__
#define __TAL_API_H__

#include "tuya_cloud_types.h"

#include "tal_log.h"
#include "tal_memory.h"
#include "tal_mutex.h"
#include "tal_ota.h"
#include "tal_queue.h"
#include "tal_semaphore.h"
#include "tal_sleep.h"
#include "tal_system.h"
#include "tal_thread.h"
#include "tal_time_service.h"
#include "tal_network.h"
#include "tal_workqueue.h"
#include "tal_cli.h"
#include "tal_uart.h"
#include "tal_kv.h"
#include "tal_time_service.h"
#include "tal_workq_service.h"
#include "tal_security.h"
#include "tal_sw_timer.h"
#include "tal_event.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifdef __cplusplus
}
#endif /* __TAL_LOG_H__ */

#endif

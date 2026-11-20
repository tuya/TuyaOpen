/*
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

#ifndef _TUYA_HS_PORT_H
#define _TUYA_HS_PORT_H

#include <assert.h>
#include "tuya_ble_os_adapter.h"

#ifdef __cplusplus
extern "C" {
#endif

#define BLE_ASSERT_ACTIVE()

#define BLE_PANIC_ASSERT(rc) TUYA_HS_ASSERT(rc);

tuya_ble_eventq *tuya_port_get_dflt_eventq(void);

int  tuya_ble_pre_init(void);
void tuya_ble_host_main_run(void *arg);
void tuya_ble_host_main_exit(void);
int  tuya_get_pre_init_flag(void);

#ifdef __cplusplus
}
#endif

#endif /* _TUYA_HS_PORT_H */

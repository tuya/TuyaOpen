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

#include <stddef.h>
#include "tuya_ble_mbuf.h"
#include "tuya_ble_mempool.h"
#include "tuya_ble_cfg.h"
#include "ble_hs.h"
#include "tuya_hs_port.h"
#include "tuya_ble_os_adapter.h"
#include "tuya_ble_hci.h"

static void* host_main_thread_hdl = NULL;
static tuya_ble_eventq g_eventq_dflt;
static int pre_init_flag = 0;

extern void ty_os_msys_init(void);
extern void os_mempool_module_init(void);
void tuya_ble_host_pre_init(void *param)
{
    if(pre_init_flag == 0) {
        /* Initialize default event queue */
        tuya_ble_eventq_init(&g_eventq_dflt);
        pre_init_flag = 1;
    
        /* Initialize the global memory pool */
        os_mempool_module_init();
        ty_os_msys_init();
        /* Initialize the host */
        ble_hs_init();
    }
}


tuya_ble_eventq *tuya_port_get_dflt_eventq(void)
{
    return &g_eventq_dflt;
}

int tuya_ble_pre_init(void)
{
    tuya_ble_hci_buf_init();
    tuya_ble_host_pre_init(NULL);
    return 0;
}

void tuya_ble_host_loop_run(void *arg)
{
    struct tuya_ble_event *ev;

    while (tal_thread_get_state(host_main_thread_hdl) == THREAD_STATE_RUNNING) {     
        ev = tuya_ble_eventq_get(tuya_port_get_dflt_eventq(), 1000);
        if (!ev) {
            continue;
        }
        tuya_ble_event_run(ev);
    }
}
static  int init_thread_flag = 0;
void tuya_ble_host_main_run(void *arg)
{
    if (init_thread_flag == 0) {
        tuya_ble_thread_create(&host_main_thread_hdl, "host_main_thread_hdl", TUYA_BLE_HOST_STACK_SIZE, TUYA_HS_BLE_HOST_TASK_PRIORITY, tuya_ble_host_loop_run, NULL);
        init_thread_flag = 1;
    }
}

int tuya_get_pre_init_flag(void)
{
    return pre_init_flag;
}

void tuya_ble_host_main_exit(void)
{
    if(init_thread_flag == 1) {
        tuya_ble_thread_release(host_main_thread_hdl);
        // host_main_thread_hdl = NULL;
        init_thread_flag = 0;
    }
}

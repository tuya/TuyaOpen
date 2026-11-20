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

#ifndef H_TY_HS_OPT_AUTO_
#define H_TY_HS_OPT_AUTO_

#include "tuya_ble_cfg.h"

#ifdef __cplusplus
extern "C" {
#endif

/***
 * Automatic options.
 *
 * These settings are generated automatically from the user-specified syscfg
 * settings.
 */

#undef TY_HS_BLE_ADVERTISE
#define TY_HS_BLE_ADVERTISE ((TY_HS_BLE_ROLE_BROADCASTER) || (TY_HS_BLE_ROLE_PERIPHERAL))

#undef TY_HS_BLE_SCAN
#define TY_HS_BLE_SCAN ((TY_HS_BLE_ROLE_CENTRAL) || (TY_HS_BLE_ROLE_OBSERVER))

#undef TY_HS_BLE_CONNECT
#define TY_HS_BLE_CONNECT ((TY_HS_BLE_ROLE_CENTRAL) || (TY_HS_BLE_ROLE_PERIPHERAL))

#undef TY_HS_BLE_CUT_EVT
#define TY_HS_BLE_CUT_EVT 1

#undef TY_HS_BLE_CUT_ATT
#define TY_HS_BLE_CUT_ATT 1

/** Supported client ATT commands. */

#undef TY_HS_BLE_ATT_CLT_FIND_INFO
#define TY_HS_BLE_ATT_CLT_FIND_INFO ((TY_HS_BLE_GATT_DISC_ALL_DSCS))

#undef TY_HS_BLE_ATT_CLT_FIND_TYPE
#define TY_HS_BLE_ATT_CLT_FIND_TYPE ((TY_HS_BLE_GATT_DISC_SVC_UUID))

#undef TY_HS_BLE_ATT_CLT_READ_TYPE
#define TY_HS_BLE_ATT_CLT_READ_TYPE                                                                                    \
    ((TY_HS_BLE_GATT_FIND_INC_SVCS) || (TY_HS_BLE_GATT_DISC_ALL_CHRS) || (TY_HS_BLE_GATT_DISC_CHRS_UUID) ||            \
     (TY_HS_BLE_GATT_READ_UUID))

#undef TY_HS_BLE_ATT_CLT_READ
#define TY_HS_BLE_ATT_CLT_READ ((TY_HS_BLE_GATT_READ) || (TY_HS_BLE_GATT_READ_LONG) || (TY_HS_BLE_GATT_FIND_INC_SVCS))

#undef TY_HS_BLE_ATT_CLT_READ_BLOB
#define TY_HS_BLE_ATT_CLT_READ_BLOB ((TY_HS_BLE_GATT_READ_LONG))

#undef TY_HS_BLE_ATT_CLT_READ_MULT
#define TY_HS_BLE_ATT_CLT_READ_MULT ((TY_HS_BLE_GATT_READ_MULT))

#undef TY_HS_BLE_ATT_CLT_READ_GROUP_TYPE
#define TY_HS_BLE_ATT_CLT_READ_GROUP_TYPE ((TY_HS_BLE_GATT_DISC_ALL_SVCS))

#undef TY_HS_BLE_ATT_CLT_WRITE
#define TY_HS_BLE_ATT_CLT_WRITE ((TY_HS_BLE_GATT_WRITE))

#undef TY_HS_BLE_ATT_CLT_WRITE_NO_RSP
#define TY_HS_BLE_ATT_CLT_WRITE_NO_RSP ((TY_HS_BLE_GATT_WRITE_NO_RSP))

#undef TY_HS_BLE_ATT_CLT_PREP_WRITE
#define TY_HS_BLE_ATT_CLT_PREP_WRITE ((TY_HS_BLE_GATT_WRITE_LONG))

#undef TY_HS_BLE_ATT_CLT_EXEC_WRITE
#define TY_HS_BLE_ATT_CLT_EXEC_WRITE ((TY_HS_BLE_GATT_WRITE_LONG))

#undef TY_HS_BLE_ATT_CLT_NOTIFY
#define TY_HS_BLE_ATT_CLT_NOTIFY ((TY_HS_BLE_GATT_NOTIFY))

#undef TY_HS_BLE_ATT_CLT_INDICATE
#define TY_HS_BLE_ATT_CLT_INDICATE ((TY_HS_BLE_GATT_INDICATE))

/** Security manager settings. */

#undef TY_HS_BLE_SM
#define TY_HS_BLE_SM (TY_HS_BLE_SM_LEGACY || TY_HS_BLE_SM_SC)

#ifdef __cplusplus
}
#endif

#endif

#include "tkl_bluetooth.h"
//#include "tuya_ble_hs_init.h"
#include "ble_svc_gap.h"
#include "ble_svc_gatt.h"
#include "ble_gap.h"
#include "ble_hs_log.h"
#include "tal_log.h"
#include "tkl_mutex.h"
// Standard Files
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "tal_system.h"
#include "tkl_hci.h"
#include "tal_memory.h"

#if (TY_HS_BLE_ROLE_CENTRAL)
typedef struct {
    uint8_t role; /*< Refer to @ TKL_BLE_ROLE_SERVER and TKL_BLE_ROLE_CLIENT */
    TKL_BLE_GATT_SVC_DISC_TYPE_T service;
    TKL_BLE_GATT_CHAR_DISC_TYPE_T char_disc;
    TKL_BLE_GATT_DESC_DISC_TYPE_T desc_disc;
} TKL_BLUETOOTH_CLIENT_PARAMS_T;
#endif

typedef struct {
    uint8_t role; /*< Refer to @ TKL_BLE_ROLE_SERVER and TKL_BLE_ROLE_CLIENT */
    struct {
        uint8_t *buffer;
        uint16_t size;
        uint16_t handle;
    } read_char[1];
} TKL_BLUETOOTH_SERVER_PARAMS_T;

static TKL_BLE_GAP_EVT_FUNC_CB tkl_bluetooth_gap_callback;
static TKL_BLE_GATT_EVT_FUNC_CB tkl_bluetooth_gatt_callback;

/* Tuya Ble Host Stack*/
// 60 bytes
static struct ble_gatt_svc_def *tuya_gatt_svcs = NULL;
// 144 bytes
#define TUYA_BLE_GATT_CHAR_MAX_NUM 4
static struct ble_gatt_chr_def tuya_gatt_chars[2][TUYA_BLE_GATT_CHAR_MAX_NUM]; //*tuya_gatt_chars = NULL;

// 12 bytes
static TKL_BLUETOOTH_SERVER_PARAMS_T tuya_ble_server;

static struct ble_gap_event_listener tuya_ble_event_listener;
static int gatts_service_flag = FALSE;
TKL_MUTEX_HANDLE tkl_ble_stack_mutex = NULL;
static int stack_sync_flag = 0;
extern int tuya_ble_hs_notify(uint16_t conn_handle, uint16_t svc_handle, uint8_t *notify_data, uint16_t data_len);

#if (TY_HS_BLE_ROLE_CENTRAL)
static TKL_BLUETOOTH_CLIENT_PARAMS_T tuya_ble_client;
#endif

static int tuya_ble_host_scan_event(struct ble_gap_event *event, void *arg)
{
    TKL_BLE_GAP_PARAMS_EVT_T gap_event;

    memset(&gap_event, 0, sizeof(TKL_BLE_GAP_PARAMS_EVT_T));
    gap_event.conn_handle = TKL_BLE_GATT_INVALID_HANDLE;

    if (event->type == BLE_GAP_EVENT_DISC) {
        gap_event.type = TKL_BLE_GAP_EVT_ADV_REPORT;
        gap_event.result = 0;
        gap_event.conn_handle = 0;
        if (BLE_HCI_ADV_RPT_EVTYPE_ADV_IND == event->disc.event_type ||
            BLE_HCI_ADV_RPT_EVTYPE_NONCONN_IND == event->disc.event_type) {
            gap_event.gap_event.adv_report.adv_type = TKL_BLE_ADV_DATA;
        } else if (BLE_HCI_ADV_RPT_EVTYPE_SCAN_RSP == event->disc.event_type) {
            gap_event.gap_event.adv_report.adv_type = TKL_BLE_RSP_DATA;
        } else {
            // BLE_HS_LOG(INFO, "BLE_GAP_EVENT_DISC Find None Useful ADV (%d)", event->disc.event_type);
            return -1;
        }

        gap_event.gap_event.adv_report.rssi = event->disc.rssi;
        gap_event.gap_event.adv_report.data.length = event->disc.length_data;
        gap_event.gap_event.adv_report.data.p_data = (uint8_t *)event->disc.data;

        // uint8_t i;
        // for (i = 0; i < 6; i++)
        // {
        //     gap_event.gap_event.adv_report.peer_addr.addr[i] = event->disc.addr.val[i];
        // }

        memcpy(gap_event.gap_event.adv_report.peer_addr.addr, event->disc.addr.val, 6);

        if (event->disc.addr.type == BLE_ADDR_PUBLIC) {
            gap_event.gap_event.adv_report.peer_addr.type = TKL_BLE_GAP_ADDR_TYPE_PUBLIC;
        } else if (event->disc.addr.type == BLE_ADDR_RANDOM) {
            gap_event.gap_event.adv_report.peer_addr.type = TKL_BLE_GAP_ADDR_TYPE_RANDOM;
        } else {
            return -1; // Not Support
        }

        // BLE_HS_LOG(INFO, "BLE_GAP_EVENT_DISC");

        if (tkl_bluetooth_gap_callback && gap_event.conn_handle != TKL_BLE_GATT_INVALID_HANDLE) {
            tkl_bluetooth_gap_callback(&gap_event);
        }
    }
    return 0;
}

static int tuya_ble_host_gap_event(struct ble_gap_event *event, void *arg)
{
    TKL_BLE_GAP_PARAMS_EVT_T gap_event;
    TKL_BLE_GATT_PARAMS_EVT_T gatt_event;

    TKL_BLUETOOTH_SERVER_PARAMS_T *p_role = arg;

    memset(&gap_event, 0, sizeof(TKL_BLE_GAP_PARAMS_EVT_T));
    memset(&gatt_event, 0, sizeof(TKL_BLE_GATT_PARAMS_EVT_T));
    gap_event.conn_handle = TKL_BLE_GATT_INVALID_HANDLE;
    gatt_event.conn_handle = TKL_BLE_GATT_INVALID_HANDLE;

    switch (event->type) {
    case BLE_GAP_EVENT_CONNECT:
        gap_event.type = TKL_BLE_GAP_EVT_CONNECT;
        gap_event.result = event->connect.status;
        gap_event.conn_handle = event->connect.conn_handle;

        gap_event.gap_event.connect.role = p_role->role;
#if defined(TARGET_BT_PLATFORM) && (TARGET_BT_PLATFORM == BK_BT_PLATFORM)
        if (p_role->role == TKL_BLE_ROLE_SERVER) {
            struct ble_gap_conn_desc desc;
            TKL_BLE_GAP_CONN_PARAMS_T param;
            ble_gap_conn_find(event->connect.conn_handle, &desc);
            BLE_HS_LOG(NOTICE, "[BLE_GAP_EVENT_CONNECT]interval 0x:%x timeout 0x:%x\r\n", desc.conn_itvl,
                       desc.supervision_timeout);
            // if(desc.supervision_timeout < 200)
            /*
            {
                 param.connection_timeout = 1000;
                 param.conn_interval_max = 32;
                 param.conn_interval_min = 32;
                 param.conn_latency = 0;
                 param.conn_sup_timeout = param.connection_timeout;
                 tkl_ble_gap_conn_param_update(event->connect.conn_handle, &param);
             }*/
        }
#endif
        BLE_HS_LOG(INFO, "BLE_GAP_EVENT_CONNECT(0x%02x), handle = 0x%02x, Role(%d)\n", event->connect.status,
                   event->connect.conn_handle, p_role->role);
        break;

    case BLE_GAP_EVENT_DISCONNECT:
        gap_event.type = TKL_BLE_GAP_EVT_DISCONNECT;
        gap_event.result = 0;
        gap_event.conn_handle = event->disconnect.conn.conn_handle;

        gap_event.gap_event.disconnect.reason = event->disconnect.reason;
        gap_event.gap_event.disconnect.role = p_role->role;

        BLE_HS_LOG(NOTICE, "BLE_GAP_EVENT_DISCONNECT(0x%02x)\n", event->disconnect.reason);
        break;

    case BLE_GAP_EVENT_DISC_COMPLETE: {
        BLE_HS_LOG(INFO, "BLE_GAP_EVENT_DISC_COMPLETE");
    } break;

    case BLE_GAP_EVENT_CONN_UPDATE: {
        struct ble_gap_conn_desc desc;

        gap_event.type = TKL_BLE_GAP_EVT_CONN_PARAM_UPDATE;
        gap_event.result = 0;
        gap_event.conn_handle = event->conn_update.conn_handle;
        ble_gap_conn_find(event->conn_update.conn_handle, &desc);

        gap_event.gap_event.conn_param.conn_interval_min = desc.conn_itvl;
        gap_event.gap_event.conn_param.conn_interval_max = desc.conn_itvl;
        gap_event.gap_event.conn_param.conn_latency = desc.conn_latency;
        gap_event.gap_event.conn_param.conn_sup_timeout = desc.supervision_timeout;
        BLE_HS_LOG(NOTICE, "BLE_GAP_EVENT_CONN_UPDATE,0x%x,0x%x,0x%x", desc.conn_itvl, desc.conn_latency,
                   desc.supervision_timeout);
    } break;
    case BLE_GAP_EVENT_CONN_UPDATE_REQ:
        BLE_HS_LOG(INFO, "BLE_GAP_EVENT_CONN_UPDATE_REQ");
        break;

    case BLE_GAP_EVENT_MTU:
        gatt_event.type = TKL_BLE_GATT_EVT_MTU_REQUEST;
        gatt_event.result = 0;
        gatt_event.conn_handle = event->mtu.conn_handle;

        gatt_event.gatt_event.exchange_mtu = event->mtu.value;
        BLE_HS_LOG(INFO, "mtu update event; conn_handle=0x%04x mtu=%d, channel id = %d\n", event->mtu.conn_handle,
                   event->mtu.value, event->mtu.channel_id);
        break;

    case BLE_GAP_EVENT_NOTIFY_TX:
        gatt_event.type = TKL_BLE_GATT_EVT_NOTIFY_TX;
        gatt_event.result = event->notify_tx.status;
        gatt_event.conn_handle = event->notify_tx.conn_handle;

        gatt_event.gatt_event.notify_result.char_handle = event->notify_tx.attr_handle;
        BLE_HS_LOG(INFO, "send notify ok");
        break;

    case BLE_GAP_EVENT_NOTIFY_RX:
        BLE_HS_LOG(INFO, "receive notify ok");
        break;
    case BLE_GAP_EVENT_SUBSCRIBE:
        gatt_event.type = TKL_BLE_GATT_EVT_SUBSCRIBE;
        gatt_event.result = 0;
        gatt_event.conn_handle = event->subscribe.conn_handle;
        gatt_event.gatt_event.subscribe.char_handle = event->subscribe.attr_handle;
        gatt_event.gatt_event.subscribe.reason = event->subscribe.reason;
        gatt_event.gatt_event.subscribe.prev_notify = event->subscribe.prev_notify;
        gatt_event.gatt_event.subscribe.cur_notify = event->subscribe.cur_notify;
        gatt_event.gatt_event.subscribe.prev_indicate = event->subscribe.prev_indicate;
        gatt_event.gatt_event.subscribe.cur_indicate = event->subscribe.cur_indicate;
        BLE_HS_LOG(INFO, "BLE_GAP_EVENT_SUBSCRIBE");
        break;
    default:
        BLE_HS_LOG(NOTICE, "Unknow Type = %d", event->type);
        return OPRT_OK;
    }

    if (tkl_bluetooth_gap_callback && gap_event.conn_handle != TKL_BLE_GATT_INVALID_HANDLE) {
        tkl_bluetooth_gap_callback(&gap_event);
    } else if (tkl_bluetooth_gatt_callback && gatt_event.conn_handle != TKL_BLE_GATT_INVALID_HANDLE) {
        tkl_bluetooth_gatt_callback(&gatt_event);
    }
    return OPRT_OK;
}

static int tuya_ble_host_write_callback(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt,
                                        void *arg)
{
    struct os_mbuf *om = ctxt->om;
    TKL_BLE_GATT_PARAMS_EVT_T gatt_event;

    switch (ctxt->op) {
        /*
        case BLE_GATT_ACCESS_OP_READ_CHR:
            BLE_HS_LOG(INFO, "Conn Handle(0x%02x), Read Char Handle(0x%02x), Heap:%d", conn_handle, attr_handle,
        tal_system_get_free_heap_size()); for(uint8_t i = 0; i < 1; i++) { if(tuya_ble_server.read_char[i].handle ==
        attr_handle) { if (tuya_ble_server.read_char[i].buffer != NULL) { os_mbuf_append(om,
        tuya_ble_server.read_char[i].buffer, tuya_ble_server.read_char[i].size);
                    }
                    return OPRT_OK;
                }
            }
            break;*/

    case BLE_GATT_ACCESS_OP_READ_CHR: {

        BLE_HS_LOG(INFO, "Conn Handle(0x%02x), Read Char Handle(0x%02x)", conn_handle, attr_handle);
        gatt_event.type = TKL_BLE_GATT_EVT_READ_CHAR_VALUE;
        gatt_event.conn_handle = conn_handle;
        gatt_event.gatt_event.char_read.char_handle = attr_handle;
        gatt_event.gatt_event.char_read.offset = 0;

        if (tkl_bluetooth_gatt_callback) {
            tkl_bluetooth_gatt_callback(&gatt_event);
        }

        for (uint8_t i = 0; i < 1; i++) {
            if (tuya_ble_server.read_char[i].handle == attr_handle) {
                if (tuya_ble_server.read_char[i].buffer != NULL) {
                    os_mbuf_append(om, tuya_ble_server.read_char[i].buffer, tuya_ble_server.read_char[i].size);
                }
                return OPRT_OK;
            }
        }
    } break;

    case BLE_GATT_ACCESS_OP_WRITE_CHR:
        while (om) {
            gatt_event.type = TKL_BLE_GATT_EVT_WRITE_REQ;
            gatt_event.conn_handle = conn_handle;
            gatt_event.gatt_event.write_report.char_handle = attr_handle;
            gatt_event.gatt_event.write_report.report.length = om->om_len;
            gatt_event.gatt_event.write_report.report.p_data = (uint8_t *)om->om_data;

            if (tkl_bluetooth_gatt_callback) {
                tkl_bluetooth_gatt_callback(&gatt_event);
            }
            om = SLIST_NEXT(om, om_next);
        }
        return OPRT_OK;
    default:
        BLE_HS_LOG(INFO, "Unknow Op = %d", ctxt->op);
        return BLE_ATT_ERR_UNLIKELY;
    }
    return 0;
}

static int tuya_ble_host_mtu_exchange_callback(uint16_t conn_handle, const struct ble_gatt_error *error, uint16_t mtu,
                                               void *arg)
{
    switch (error->status) {
    case 0:
        BLE_HS_LOG_INFO("mtu exchange complete: conn_handle=%d mtu=%d\n", conn_handle, mtu);
        break;
    }

    return OPRT_OK;
}

#if (TY_HS_BLE_ROLE_CENTRAL)
static int tuya_ble_svc_disc_callback(uint16_t conn_handle, const struct ble_gatt_error *error,
                                      const struct ble_gatt_svc *service, void *arg)
{
    TKL_BLE_GATT_PARAMS_EVT_T gatt_event;
    TKL_BLE_GATT_SVC_DISC_TYPE_T *p_service = &tuya_ble_client.service;
    uint8_t svc_index = p_service->svc_num;

    gatt_event.type = TKL_BLE_GATT_EVT_PRIM_SEV_DISCOVERY;
    gatt_event.conn_handle = conn_handle;
    switch (error->status) {
    case 0: {
        if (service->uuid.u.type == BLE_UUID_TYPE_16) {
            p_service->services[svc_index].uuid.uuid_type = TKL_BLE_UUID_TYPE_16;
            p_service->services[svc_index].uuid.uuid.uuid16 = service->uuid.u16.value;
        } else if (service->uuid.u.type == BLE_UUID_TYPE_32) {
            p_service->services[svc_index].uuid.uuid_type = TKL_BLE_UUID_TYPE_32;
            p_service->services[svc_index].uuid.uuid.uuid32 = service->uuid.u32.value;
        } else if (service->uuid.u.type == BLE_UUID_TYPE_128) {
            p_service->services[svc_index].uuid.uuid_type = TKL_BLE_UUID_TYPE_128;
            memcpy(p_service->services[svc_index].uuid.uuid.uuid128, service->uuid.u128.value, 16);
        }

        p_service->services[svc_index].start_handle = service->start_handle;
        p_service->services[svc_index].end_handle = service->end_handle;

        p_service->svc_num++;
        BLE_HS_LOG_DEBUG("Discovery Service, Service(0x%02x)", service->uuid.u16.value);
        BLE_HS_LOG_DEBUG("Start Handle = 0x%04x, End Handle = 0x%04x", service->start_handle, service->end_handle);
        return 0;
    }; // Need to return

    case BLE_HS_EDONE:
        memcpy(&gatt_event.gatt_event.svc_disc, p_service, sizeof(TKL_BLE_GATT_SVC_DISC_TYPE_T));
        memset(p_service, 0, sizeof(TKL_BLE_GATT_SVC_DISC_TYPE_T));

        gatt_event.result = OPRT_OK;
        /* All services discovered; start discovering characteristics. */
        BLE_HS_LOG_INFO("Finish Discovery Service, Success");
        break;

    default:
        memset(p_service, 0, sizeof(TKL_BLE_GATT_SVC_DISC_TYPE_T));
        gatt_event.result = OPRT_OS_ADAPTER_BLE_SVC_DISC_FAILED;

        BLE_HS_LOG_INFO("Finish Discovery Service, Fail, status(%d)", error->status);
        break;
    }

    if (tkl_bluetooth_gatt_callback) {
        tkl_bluetooth_gatt_callback(&gatt_event);
    }
    return OPRT_OK;
}

static int tuya_ble_chr_disc_callback(uint16_t conn_handle, const struct ble_gatt_error *error,
                                      const struct ble_gatt_chr *chr, void *arg)
{
    TKL_BLE_GATT_PARAMS_EVT_T gatt_event;
    TKL_BLE_GATT_CHAR_DISC_TYPE_T *p_char = &tuya_ble_client.char_disc;
    uint8_t char_index = p_char->char_num;

    gatt_event.type = TKL_BLE_GATT_EVT_CHAR_DISCOVERY;
    gatt_event.conn_handle = conn_handle;

    switch (error->status) {
    case 0: {
        if (chr->uuid.u.type == BLE_UUID_TYPE_16) {
            p_char->characteristics[char_index].uuid.uuid_type = TKL_BLE_UUID_TYPE_16;
            p_char->characteristics[char_index].uuid.uuid.uuid16 = chr->uuid.u16.value;
        } else if (chr->uuid.u.type == BLE_UUID_TYPE_32) {
            p_char->characteristics[char_index].uuid.uuid_type = TKL_BLE_UUID_TYPE_32;
            p_char->characteristics[char_index].uuid.uuid.uuid32 = chr->uuid.u32.value;
        } else if (chr->uuid.u.type == BLE_UUID_TYPE_128) {
            p_char->characteristics[char_index].uuid.uuid_type = TKL_BLE_UUID_TYPE_128;
            memcpy(p_char->characteristics[char_index].uuid.uuid.uuid128, chr->uuid.u128.value, 16);
        }

        p_char->characteristics[char_index].handle = chr->val_handle;

        BLE_HS_LOG_DEBUG("Discovery Characteristics, Value Handle = 0x%04x, UUID Value = 0x%04x", chr->val_handle,
                         chr->uuid.u16.value);
        return 0;
    };

    case BLE_HS_EDONE:
        /* All services discovered; start discovering characteristics. */
        memcpy(&gatt_event.gatt_event.char_disc, p_char, sizeof(TKL_BLE_GATT_SVC_DISC_TYPE_T));
        gatt_event.result = OPRT_OK;
        BLE_HS_LOG_INFO("Finish Discovery Characteristics");
        break;

    default:
        gatt_event.result = OPRT_OS_ADAPTER_BLE_CHAR_DISC_FAILED;
        break;
    }

    if (tkl_bluetooth_gatt_callback) {
        tkl_bluetooth_gatt_callback(&gatt_event);
    }
    return OPRT_OK;
}

static int tuya_ble_desc_disc_callback(uint16_t conn_handle, const struct ble_gatt_error *error,
                                       uint16_t chr_val_handle, const struct ble_gatt_dsc *dsc, void *arg)
{
    TKL_BLE_GATT_PARAMS_EVT_T gatt_event;

    gatt_event.type = TKL_BLE_GATT_EVT_CHAR_DESC_DISCOVERY;
    gatt_event.conn_handle = conn_handle;

    switch (error->status) {
    case 0:
        if (dsc->uuid.u16.value == BLE_GATT_DSC_CLT_CFG_UUID16) {
            tuya_ble_client.desc_disc.cccd_handle = dsc->handle;
        }
        BLE_HS_LOG_INFO("Discovery Characteristics Descriptor Handle = 0x%04x", dsc->handle);
        return 0;

    case BLE_HS_EDONE:
        /* All descriptors in this characteristic discovered; start discovering
         * descriptors in the next characteristic.
         */
        gatt_event.gatt_event.desc_disc.cccd_handle = tuya_ble_client.desc_disc.cccd_handle;
        gatt_event.result = OPRT_OK;
        BLE_HS_LOG_INFO("Finish Discovery Characteristics Descriptor (0x2902)");
        break;

    default:
        /* Error; abort discovery. */
        gatt_event.result = OPRT_OS_ADAPTER_BLE_DESC_DISC_FAILED;
        break;
    }

    if (tkl_bluetooth_gatt_callback) {
        tkl_bluetooth_gatt_callback(&gatt_event);
    }
    return OPRT_OK;
}
#endif

static void tuya_ble_stack_event_callback(TKL_BLE_GAP_EVT_TYPE_E type, int result)
{
    TKL_BLE_GAP_PARAMS_EVT_T init_event;

    init_event.type = type;
    init_event.result = result;
    BLE_HS_LOG_INFO("Init/Deinit Event");
    if (tkl_bluetooth_gap_callback) {
        tkl_bluetooth_gap_callback(&init_event);
    }
}

static void tuya_ble_host_stack_reset_callback(int reason)
{
    BLE_HS_LOG_INFO("Stack Reset,  reson = %d", reason);
}

static void tuya_ble_host_stack_sync_callback(void)
{
    BLE_HS_LOG_INFO("Stack sync");
    stack_sync_flag = 1;
}

/***************************************************** End
 * ************************************************************************************/
/**
 * @brief   Function for initializing the ble stack
 * @param   role                 Indicate the role for ble stack.
 *                               role = 1: ble peripheral
 *                               role = 2: ble central
 * @return  SUCCESS              Initialized successfully.
 *          ERROR
 * */
OPERATE_RET tkl_ble_stack_init(uint8_t role)
{
    if (ble_hs_is_enabled()) {
        BLE_HS_LOG_INFO("ble_stack already inited\r\n");
        tuya_ble_stack_event_callback(TKL_BLE_EVT_STACK_INIT, 0);
        return OPRT_OK;
    }

    if (!tkl_ble_stack_mutex)
        tkl_mutex_create_init(&tkl_ble_stack_mutex);
    tkl_mutex_lock(tkl_ble_stack_mutex);

    static int init_flag = 0;
    OPERATE_RET ret = OPRT_OK;

    if ((role & TKL_BLE_ROLE_SERVER) == TKL_BLE_ROLE_SERVER) {
        memset(&tuya_ble_server, 0, sizeof(TKL_BLUETOOTH_SERVER_PARAMS_T));
        tuya_ble_server.role = TKL_BLE_ROLE_SERVER;
        tuya_ble_server.read_char[0].buffer = NULL;
    }
#if (TY_HS_BLE_ROLE_CENTRAL)
    if ((role & TKL_BLE_ROLE_CLIENT) == TKL_BLE_ROLE_CLIENT) {
        memset(&tuya_ble_client, 0, sizeof(TKL_BLUETOOTH_CLIENT_PARAMS_T));
        tuya_ble_client.role = TKL_BLE_ROLE_CLIENT;
    }
#endif
    ret = tkl_hci_init();
    if (init_flag == 0) {
        // Hci Buf, Host Pre, Controller Init
        tuya_ble_pre_init();
        ble_svc_gap_init();
        ble_svc_gatt_init();
        init_flag = 1;
    }
    tuya_ble_host_main_run(NULL);
    stack_sync_flag = 0;
    ble_hs_sched_start();
    int num = 0;
    while ((!stack_sync_flag) && (num++ < 20)) {
        tuya_ble_time_delay(10);
    }
    tuya_ble_stack_event_callback(TKL_BLE_EVT_STACK_INIT, 0);
    tkl_mutex_unlock(tkl_ble_stack_mutex);
    return ret;
}

/**
 * @brief   Function for de-initializing the ble stack features
 * @param   role                 Indicate the role for ble stack.
 *                               role = 1: ble peripheral
 *                               role = 2: ble central
 * @return  SUCCESS              Deinitialized successfully.
 *          ERROR
 * */
OPERATE_RET tkl_ble_stack_deinit(uint8_t role)
{
    int i;

    if (!ble_hs_is_enabled()) {
        BLE_HS_LOG_INFO("ble_stack already deinited\r\n");
        tuya_ble_stack_event_callback(TKL_BLE_EVT_STACK_DEINIT, 0);
        return OPRT_OK;
    }

    if (!tkl_ble_stack_mutex)
        tkl_mutex_create_init(&tkl_ble_stack_mutex);
    tkl_mutex_lock(tkl_ble_stack_mutex);

    int loopnum = 0;
    ble_hs_shutdown(0);
    while (ble_gap_adv_active() || ble_gap_disc_active() || ble_gap_conn_active() || (loopnum < 10)) {
        tuya_ble_time_delay(20);
        loopnum++;
    }
    tuya_ble_host_main_exit();
    tkl_hci_deinit();
    tuya_ble_stack_event_callback(TKL_BLE_EVT_STACK_DEINIT, 0);

    for (i = 0; i < TKL_BLE_GATT_SERVICE_MAX_NUM; i++) {
        if (tuya_gatt_svcs[i].uuid) {
            tuya_ble_hs_free((void *)tuya_gatt_svcs[i].uuid);
        }
    }

    for (i = 0; i < TUYA_BLE_GATT_CHAR_MAX_NUM; i++) {
        if (tuya_gatt_chars[0][i].uuid) {
            tuya_ble_hs_free((void *)tuya_gatt_chars[0][i].uuid);
        }
        if (tuya_gatt_chars[1][i].uuid) {
            tuya_ble_hs_free((void *)tuya_gatt_chars[1][i].uuid);
        }
    }

    if (tuya_gatt_svcs) {
        tuya_ble_hs_free((void *)tuya_gatt_svcs);
    }
    // if (tuya_gatt_chars) {
    //     tuya_ble_hs_free((void *)tuya_gatt_chars);
    // }

    if (tuya_ble_server.read_char[0].buffer) {
        tuya_ble_hs_free(tuya_ble_server.read_char[0].buffer);
    }

    tkl_mutex_unlock(tkl_ble_stack_mutex);
    return OPRT_OK;
}

/**
 * @brief   Function for getting the GATT Link-Support.
 * @param   p_link              return gatt link
 * @return  SUCCESS             Support Gatt Link
 *          ERROR               Only Beacon or Mesh Beacon, Not Support Gatt Link.
 * */
OPERATE_RET tkl_ble_stack_gatt_link(uint16_t *p_link)
{
    *p_link = 4;

    return OPRT_OK;
}

/**
 * @brief   Register GAP Event Callback
 * @param   TKL_BLE_GAP_EVT_FUNC_CB Refer to @TKL_BLE_GAP_EVT_FUNC_CB
 * @return  SUCCESS              Register successfully.
 *          ERROR
 * */
OPERATE_RET tkl_ble_gap_callback_register(const TKL_BLE_GAP_EVT_FUNC_CB gap_evt)
{
    tkl_bluetooth_gap_callback = gap_evt;

    tuya_ble_hs_cfg.reset_cb = tuya_ble_host_stack_reset_callback;
    tuya_ble_hs_cfg.sync_cb = tuya_ble_host_stack_sync_callback;
    return OPRT_OK;
}

/**
 * @brief   Register GATT Event Callback
 * @param   TKL_BLE_GATT_EVT_FUNC_CB Refer to @TKL_BLE_GATT_EVT_FUNC_CB
 * @return  SUCCESS              Register successfully.
 *          ERROR
 * */
OPERATE_RET tkl_ble_gatt_callback_register(const TKL_BLE_GATT_EVT_FUNC_CB gatt_evt)
{
    // Register Gatt Callback
    tkl_bluetooth_gatt_callback = gatt_evt;
    return OPRT_OK;
}

/******************************************************************************************************************************/
/** @brief Define All GAP Interface
 */
/**
 * @brief   Set the local Bluetooth identity address.
 *          The local Bluetooth identity address is the address that identifies this device to other peers.
 *          The address type must be either @ref TKL_BLE_GAP_ADDR_TYPE_PUBLIC or @ref TKL_BLE_GAP_ADDR_TYPE_RANDOM.
 * @param   [in] p_peer_addr : pointer to local address parameters
 * @return  SUCCESS
 *          ERROR
 * */
OPERATE_RET tkl_ble_gap_addr_set(TKL_BLE_GAP_ADDR_T const *p_peer_addr)
{
    return OPRT_OK;
}

/**
 * @brief   Get the local Bluetooth identity address.
 * @param   [out] p_peer_addr: pointer to local address
 * @return  SUCCESS              Set Address successfully.
 *          ERROR
 * */
OPERATE_RET tkl_ble_gap_address_get(TKL_BLE_GAP_ADDR_T *p_peer_addr)
{
    return OPRT_OK;
}

/**
 * @brief   Start advertising
 * @param   [in] p_adv_params : pointer to advertising parameters
 * @return  SUCCESS
 *          ERROR
 * */
OPERATE_RET tkl_ble_gap_adv_start(TKL_BLE_GAP_ADV_PARAMS_T const *p_adv_params)
{
    if (!ble_hs_is_enabled()) {
        BLE_HS_LOG_INFO("bt_stack close,bt operation invalid.\n");
        return OPRT_OK;
    }

    struct ble_gap_adv_params adv_params;
    int rc;

    memset(&adv_params, 0, sizeof(adv_params));

    if (p_adv_params->adv_type == TKL_BLE_GAP_ADV_TYPE_CONN_SCANNABLE_UNDIRECTED) {
        adv_params.conn_mode = BLE_GAP_CONN_MODE_UND;
        adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;
    } else if (p_adv_params->adv_type == TKL_BLE_GAP_ADV_TYPE_NONCONN_SCANNABLE_UNDIRECTED) {
        adv_params.conn_mode = BLE_GAP_CONN_MODE_NON;
        adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;
    } else {
        return OPRT_INVALID_PARM; // Invalid Parameters
    }

    adv_params.itvl_min = BLE_GAP_ADV_ITVL_MS(p_adv_params->adv_interval_min);
    adv_params.itvl_max = BLE_GAP_ADV_ITVL_MS(p_adv_params->adv_interval_max);
    rc = ble_gap_adv_start(BLE_HCI_ADV_OWN_ADDR_PUBLIC, NULL, BLE_HS_FOREVER, &adv_params, tuya_ble_host_gap_event,
                           &tuya_ble_server);
    if (rc != 0) {
        BLE_HS_LOG(INFO, "error enabling advertisement; rc=%d\n", rc);
        return OPRT_OS_ADAPTER_BLE_ADV_START_FAILED;
    }

    return OPRT_OK;
}

/**
 * @brief   Stop advertising
 * @param   void
 * @return  SUCCESS
 *          ERROR
 * */
OPERATE_RET tkl_ble_gap_adv_stop(void)
{
    if (!ble_hs_is_enabled()) {
        BLE_HS_LOG_INFO("bt_stack close,bt operation invalid.\n");
        return OPRT_OK;
    }

    if (ble_gap_adv_stop() != 0) {
        return OPRT_OS_ADAPTER_BLE_ADV_STOP_FAILED;
    }

    return OPRT_OK;
}

/**
 * @brief   Setting advertising data
 * @param   [in] p_adv: Data to be used in advertisement packets, and include adv data len
 *          [in] p_scan_rsp: Data to be used in advertisement respond packets, and include rsp data len
 * @return  SUCCESS
 *          ERROR
 * */
OPERATE_RET tkl_ble_gap_adv_rsp_data_set(TKL_BLE_DATA_T const *p_adv, TKL_BLE_DATA_T const *p_scan_rsp)
{
    if (!ble_hs_is_enabled()) {
        BLE_HS_LOG_INFO("bt_stack close,bt operation invalid.\n");
        return OPRT_OK;
    }

    int rc;

    if (p_adv && p_adv->p_data != NULL) {
        rc = ble_gap_adv_set_data(p_adv->p_data, p_adv->length);
        if (rc != 0) {
            return OPRT_OS_ADAPTER_BLE_ADV_START_FAILED;
        }
    }

    if (p_scan_rsp && p_scan_rsp->p_data != NULL) {
        rc = ble_gap_adv_rsp_set_data(p_scan_rsp->p_data, p_scan_rsp->length);
        if (rc != 0) {
            return OPRT_OS_ADAPTER_BLE_ADV_START_FAILED;
        }
    }
    return OPRT_OK;
}

/**
 * @brief   Update advertising data
 * @param   [in] p_adv: Data to be used in advertisement packets, and include adv data len
 *          [in] p_scan_rsp: Data to be used in advertisement respond packets, and include rsp data len
 * @return  SUCCESS
 *          ERROR
 * */
OPERATE_RET tkl_ble_gap_adv_rsp_data_update(TKL_BLE_DATA_T const *p_adv, TKL_BLE_DATA_T const *p_scan_rsp)
{
    if (!ble_hs_is_enabled()) {
        BLE_HS_LOG_INFO("bt_stack close,bt operation invalid.\n");
        return OPRT_OK;
    }

    int rc;

    if (p_adv && p_adv->p_data != NULL) {
        rc = ble_gap_adv_set_data(p_adv->p_data, p_adv->length);
        if (rc != 0) {
            return OPRT_OS_ADAPTER_BLE_ADV_START_FAILED;
        }
    }

    if (p_scan_rsp && p_scan_rsp->p_data != NULL) {
        rc = ble_gap_adv_rsp_set_data(p_scan_rsp->p_data, p_scan_rsp->length);
        if (rc != 0) {
            return OPRT_OS_ADAPTER_BLE_ADV_START_FAILED;
        }
    }
    return OPRT_OK;
}

/**
 * @brief   Start scanning
 * @param   [in] scan_param: scan parameters including interval, windows
 * @return  SUCCESS
 *          ERROR
 * */
OPERATE_RET tkl_ble_gap_scan_start(TKL_BLE_GAP_SCAN_PARAMS_T const *p_scan_params)
{
    if (!ble_hs_is_enabled()) {
        BLE_HS_LOG_INFO("bt_stack close,bt operation invalid.\n");
        return OPRT_OK;
    }

    struct ble_gap_disc_params disc_params;
    int rc;

    /* Tell the controller to filter duplicates; we don't want to process
     * repeated advertisements from the same device.
     */
    disc_params.filter_duplicates = 0;

    /**
     * Perform a passive scan.  I.e., 1= don't send follow-up scan requests to
     * each advertiser. 0 = open scan rsp
     */
    disc_params.passive = p_scan_params->active ? 0 : 1;

    /* Use defaults for the rest of the parameters. */
    disc_params.itvl = BLE_GAP_SCAN_ITVL_MS(p_scan_params->interval);
    disc_params.window = BLE_GAP_SCAN_WIN_MS(p_scan_params->window);
    disc_params.filter_policy = 0;
    disc_params.limited = 0;

    // Need to combine with mesh stack, and need to adjust all of scan interface
    rc = ble_gap_disc(BLE_HCI_ADV_OWN_ADDR_PUBLIC, BLE_HS_FOREVER, &disc_params, NULL, NULL);
    // rc = ble_gap_disc(BLE_HCI_ADV_OWN_ADDR_PUBLIC, BLE_HS_FOREVER, &disc_params,tuya_ble_host_gap_event,
    // &tuya_ble_client);
    if (rc != 0) {
        BLE_HS_LOG(ERR, "Error initiating GAP discovery procedure; rc=%d\n", rc);
        return OPRT_OS_ADAPTER_BLE_SCAN_START_FAILED;
    }

    ble_gap_event_listener_register(&tuya_ble_event_listener, tuya_ble_host_scan_event, NULL);
    return OPRT_OK;
}

/**
 * @brief   Stop scanning
 * @param   void
 * @return  SUCCESS
 *          ERROR
 * */
OPERATE_RET tkl_ble_gap_scan_stop(void)
{
    if (!ble_hs_is_enabled()) {
        BLE_HS_LOG_INFO("bt_stack close,bt operation invalid.\n");
        return OPRT_OK;
    }

    if (ble_gap_disc_cancel() != 0) {
        BLE_HS_LOG(ERR, "Stop GAP discovery procedure fail\n");
        return OPRT_OS_ADAPTER_BLE_SCAN_STAOP_FAILED;
    }

    return OPRT_OK;
}

/**
 * @brief   Start connecting one peer
 * @param   [in] p_peer_addr: include address and address type
 *          [in] p_scan_params: scan parameters
 *          [in] p_conn_params: connection  parameters
 * @return  SUCCESS
 *          ERROR
 * */
#if (TY_HS_BLE_ROLE_CENTRAL)
OPERATE_RET tkl_ble_gap_connect(TKL_BLE_GAP_ADDR_T const *p_peer_addr, TKL_BLE_GAP_SCAN_PARAMS_T const *p_scan_params,
                                TKL_BLE_GAP_CONN_PARAMS_T const *p_conn_params)
{
    if (!ble_hs_is_enabled()) {
        BLE_HS_LOG_INFO("bt_stack close,bt operation invalid.\n");
        return OPRT_OK;
    }
    int rc, i;
    ble_addr_t conn_addr;
    struct ble_gap_conn_params conn_param;

    for (i = 0; i < 6; i++) {
        conn_addr.val[i] = p_peer_addr->addr[5 - i];
    }

    if (p_peer_addr->type == TKL_BLE_GAP_ADDR_TYPE_RANDOM) {
        conn_addr.type = BLE_ADDR_RANDOM;
    } else {
        conn_addr.type = BLE_ADDR_PUBLIC;
    }

    conn_param.itvl_min = p_conn_params->conn_interval_min;
    conn_param.itvl_max = p_conn_params->conn_interval_max;
    conn_param.latency = p_conn_params->conn_latency;
    conn_param.supervision_timeout = p_conn_params->conn_sup_timeout;

    conn_param.scan_itvl = p_scan_params->interval;
    conn_param.scan_window = p_scan_params->window;
    conn_param.min_ce_len = BLE_GAP_INITIAL_CONN_MIN_CE_LEN;
    conn_param.max_ce_len = BLE_GAP_INITIAL_CONN_MAX_CE_LEN;

    rc = ble_gap_connect(BLE_HCI_ADV_OWN_ADDR_PUBLIC, &conn_addr, p_conn_params->connection_timeout, &conn_param,
                         tuya_ble_host_gap_event, &tuya_ble_client);
    if (rc != 0) {
        BLE_HS_LOG(ERR, "Error: Failed to connect to device; addr_type=%d addr=0x%02x:0x%02x\n; rc=%d", conn_addr.type,
                   conn_addr.val[0], conn_addr.val[1], rc);
        return rc;
    }
    return OPRT_OK;
}
#endif
/**
 * @brief   Disconnect from peer
 * @param   [in] conn_handle: the connection handle
 *          [in] hci_reason: terminate reason
 * @return  SUCCESS
 *          ERROR
 * */
OPERATE_RET tkl_ble_gap_disconnect(uint16_t conn_handle, uint8_t hci_reason)
{
    if (!ble_hs_is_enabled()) {
        BLE_HS_LOG_INFO("bt_stack close,bt operation invalid.\n");
        return OPRT_OK;
    }

    int rc = 0;

    rc = ble_gap_terminate(conn_handle, BLE_ERR_REM_USER_CONN_TERM);
    if (rc != 0) {
        BLE_HS_LOG(ERR, "Failed to disconnect one device; rc=%d\n", rc);
        return OPRT_OS_ADAPTER_BLE_GATT_DISCONN_FAILED;
    }

    return OPRT_OK;
}

/**
 * @brief   Start to update connection parameters
 * @param   [in] conn_handle: connection handle
 *          [in] p_conn_params: connection  parameters
 * @return  SUCCESS
 *          ERROR
 * */
OPERATE_RET tkl_ble_gap_conn_param_update(uint16_t conn_handle, TKL_BLE_GAP_CONN_PARAMS_T const *p_conn_params)
{
    if (!ble_hs_is_enabled()) {
        BLE_HS_LOG_INFO("bt_stack close,bt operation invalid.\n");
        return OPRT_OK;
    }

    int rc = 0;
    struct ble_gap_upd_params conn_params;

    conn_params.itvl_min = p_conn_params->conn_interval_min;
    conn_params.itvl_max = p_conn_params->conn_interval_max;
    conn_params.latency = p_conn_params->conn_latency;
    conn_params.supervision_timeout = p_conn_params->conn_sup_timeout;

    rc = ble_gap_update_params(conn_handle, &conn_params);
    if (rc != 0) {
        BLE_HS_LOG(ERR, "Failed to update connect parameters; rc=%d\n", rc);
        return OPRT_OS_ADAPTER_BLE_CONN_PARAM_UPDATE_FAILED;
    }
    BLE_HS_LOG_INFO("+++++++++++++++++++++++sble_gap_update_params timeout %d\n", conn_params.supervision_timeout);

    return OPRT_OK;
}

/**
 * @brief   Set the radio's transmit power.
 * @param   [in] role: 0: Advertising Tx Power; 1: Scan Tx Power; 2: Connection Power
 *          [in] tx_power:      tx power:This value will be magnified 10 times.
 *                              If the tx_power value is -75, the real power is -7.5dB.(or 40 = 4dB)
 * @return  SUCCESS
 *          ERROR
 * */
OPERATE_RET tkl_ble_gap_tx_power_set(uint8_t role, int tx_power)
{
    return OPRT_NOT_SUPPORTED;
}

/**
 * @brief   Get the received signal strength for the last connection event.
 * @param   [in]conn_handle:    connection handle
 * @return  SUCCESS             Successfully read the RSSI.
 *          ERROR               No sample is available.
 * */
OPERATE_RET tkl_ble_gap_rssi_get(uint16_t conn_handle)
{
    if (!ble_hs_is_enabled()) {
        BLE_HS_LOG_INFO("bt_stack close,bt operation invalid.\n");
        return OPRT_OK;
    }

    int rc = 0;
    int8_t rssi;
    TKL_BLE_GAP_PARAMS_EVT_T gap_event;

    rc = ble_gap_conn_rssi(conn_handle, &rssi);
    if (rc != 0) {
        BLE_HS_LOG(ERR, "Failed to read connect rssi; rc=%d\n", rc);
        return OPRT_OS_ADAPTER_BLE_CONN_RSSI_GET_FAILED;
    }

    gap_event.type = TKL_BLE_GAP_EVT_CONN_RSSI;
    gap_event.result = 0;
    gap_event.conn_handle = conn_handle;
    gap_event.gap_event.link_rssi = rssi;

    if (tkl_bluetooth_gap_callback && gap_event.conn_handle != TKL_BLE_GATT_INVALID_HANDLE) {
        tkl_bluetooth_gap_callback(&gap_event);
    }

    return OPRT_OK;
}

/******************************************************************************************************************************/
/** @brief Define All Gatt Server Interface
 *
 *  Notes: notice the handle will be the one of signed point.
 */
/**
 * @brief   Add Ble Gatt Service
 * @param   [in] p_service: define the ble service
 *
 * @return  SUCCESS
 *          ERROR
 * */
OPERATE_RET tkl_ble_gatts_service_add(TKL_BLE_GATTS_PARAMS_T *p_service)
{
    if (gatts_service_flag == TRUE)
        return OPRT_OK;
    int rc;
    unsigned char i = 0, j = 0;
    unsigned char index = 0, type = 0;

    if (p_service->svc_num > TKL_BLE_GATT_SERVICE_MAX_NUM) {
        return OPRT_INVALID_PARM;
    }

    if (!tuya_gatt_svcs) {
        tuya_gatt_svcs = tuya_ble_hs_malloc(TKL_BLE_GATT_SERVICE_MAX_NUM * sizeof(struct ble_gatt_svc_def));
    }
    // if (!tuya_gatt_chars) {
    //     tuya_gatt_chars = tuya_ble_hs_malloc(TKL_BLE_GATT_CHAR_MAX_NUM * sizeof(struct ble_gatt_chr_def));
    // }
    memset(tuya_gatt_svcs, 0, TKL_BLE_GATT_SERVICE_MAX_NUM * sizeof(struct ble_gatt_svc_def));
    memset(tuya_gatt_chars, 0, 2 * TUYA_BLE_GATT_CHAR_MAX_NUM * sizeof(struct ble_gatt_chr_def));

    for (i = 0; i < p_service->svc_num; i++) {
        if (p_service->p_service[i].type == TKL_BLE_UUID_SERVICE_PRIMARY) {
            tuya_gatt_svcs[i].type = BLE_GATT_SVC_TYPE_PRIMARY;
        } else if (p_service->p_service[i].type == TKL_BLE_UUID_SERVICE_SECONDARY) {
            tuya_gatt_svcs[i].type = BLE_GATT_SVC_TYPE_SECONDARY;
        } else {
            return OPRT_INVALID_PARM;
        }

        // Add Service
        if (p_service->p_service[i].svc_uuid.uuid_type == TKL_BLE_UUID_TYPE_16) {
            tuya_gatt_svcs[i].uuid = (ble_uuid_t *)tal_malloc(sizeof(ble_uuid16_t));
            memcpy((uint8_t *)tuya_gatt_svcs[i].uuid,
                   (uint8_t *)BLE_UUID16_DECLARE(p_service->p_service[i].svc_uuid.uuid.uuid16),
                   (uint32_t)sizeof(ble_uuid16_t));
        } else if (p_service->p_service[i].svc_uuid.uuid_type == TKL_BLE_UUID_TYPE_32) {
            tuya_gatt_svcs[i].uuid = (ble_uuid_t *)tal_malloc(sizeof(ble_uuid32_t));
            memcpy((uint8_t *)tuya_gatt_svcs[i].uuid,
                   (uint8_t *)BLE_UUID32_DECLARE(p_service->p_service[i].svc_uuid.uuid.uuid32),
                   (uint32_t)sizeof(ble_uuid32_t));
        } else if (p_service->p_service[i].svc_uuid.uuid_type == TKL_BLE_UUID_TYPE_128) {
            tuya_gatt_svcs[i].uuid = (ble_uuid_t *)tal_malloc(sizeof(ble_uuid128_t));
            type = BLE_UUID_TYPE_128;
            memcpy((uint8_t *)tuya_gatt_svcs[i].uuid, &type, 1);
            memcpy((uint8_t *)tuya_gatt_svcs[i].uuid + 1, p_service->p_service[i].svc_uuid.uuid.uuid128,
                   (uint32_t)sizeof(ble_uuid128_t));
        }

        // Add characteristics
        TKL_BLE_CHAR_PARAMS_T *p_char = p_service->p_service[i].p_char;
        tuya_gatt_svcs[i].characteristics = tuya_gatt_chars[i];
        index += p_service->p_service[i].char_num;
        if (index > TKL_BLE_GATT_CHAR_MAX_NUM) {
            return OPRT_INVALID_PARM;
        }

        for (j = 0; j < p_service->p_service[i].char_num; j++) {
            if (p_char[j].char_uuid.uuid_type == TKL_BLE_UUID_TYPE_16) {
                tuya_gatt_svcs[i].characteristics[j].uuid = (ble_uuid_t *)tal_malloc(sizeof(ble_uuid16_t));
                memcpy((uint8_t *)tuya_gatt_svcs[i].characteristics[j].uuid,
                       (uint8_t *)BLE_UUID16_DECLARE(p_char[j].char_uuid.uuid.uuid16), (uint32_t)sizeof(ble_uuid16_t));
            } else if (p_char[j].char_uuid.uuid_type == TKL_BLE_UUID_TYPE_32) {
                tuya_gatt_svcs[i].characteristics[j].uuid = (ble_uuid_t *)tal_malloc(sizeof(ble_uuid32_t));
                memcpy((uint8_t *)tuya_gatt_svcs[i].characteristics[j].uuid,
                       (uint8_t *)BLE_UUID32_DECLARE(p_char[j].char_uuid.uuid.uuid32), (uint32_t)sizeof(ble_uuid32_t));
            } else if (p_char[j].char_uuid.uuid_type == TKL_BLE_UUID_TYPE_128) {
                tuya_gatt_svcs[i].characteristics[j].uuid = (ble_uuid_t *)tal_malloc(sizeof(ble_uuid128_t));
                type = BLE_UUID_TYPE_128;
                memcpy((uint8_t *)tuya_gatt_svcs[i].characteristics[j].uuid, &type, 1);
                memcpy((uint8_t *)tuya_gatt_svcs[i].characteristics[j].uuid + 1, p_char[j].char_uuid.uuid.uuid128,
                       (uint32_t)sizeof(ble_uuid128_t));
            }

            tuya_gatt_svcs[i].characteristics[j].access_cb = tuya_ble_host_write_callback;
            tuya_gatt_svcs[i].characteristics[j].val_handle = &p_char[j].handle; // is need alloc ram

            if ((p_char[j].property & TKL_BLE_GATT_CHAR_PROP_WRITE_NO_RSP) == TKL_BLE_GATT_CHAR_PROP_WRITE_NO_RSP) {
                tuya_gatt_svcs[i].characteristics[j].flags |= BLE_GATT_CHR_F_WRITE_NO_RSP;
            }
            if ((p_char[j].property & TKL_BLE_GATT_CHAR_PROP_WRITE) == TKL_BLE_GATT_CHAR_PROP_WRITE) {
                tuya_gatt_svcs[i].characteristics[j].flags |= BLE_GATT_CHR_F_WRITE;
            }
            if ((p_char[j].property & TKL_BLE_GATT_CHAR_PROP_NOTIFY) == TKL_BLE_GATT_CHAR_PROP_NOTIFY) {
                tuya_gatt_svcs[i].characteristics[j].flags |= BLE_GATT_CHR_F_NOTIFY;
            }
            if ((p_char[j].property & TKL_BLE_GATT_CHAR_PROP_INDICATE) == TKL_BLE_GATT_CHAR_PROP_INDICATE) {
                tuya_gatt_svcs[i].characteristics[j].flags |= BLE_GATT_CHR_F_INDICATE;
            }
            if ((p_char[j].property & TKL_BLE_GATT_CHAR_PROP_READ) == TKL_BLE_GATT_CHAR_PROP_READ) {
                tuya_gatt_svcs[i].characteristics[j].flags |= BLE_GATT_CHR_F_READ;
            }
            // BLE_HS_LOG(INFO, "Char Index = %d, Char Flag = 0x%04x,char(0x%04x)\n", j,
            // tuya_gatt_svcs[i].characteristics[j].flags,(int)
            // BLE_UUID16(tuya_gatt_svcs[i].characteristics[j].uuid)->value);
        }
        // BLE_HS_LOG(INFO, "Service Index = %d, type(0x%02x), service(0x%04x)\n", i, tuya_gatt_svcs[i].type,  (int)
        // BLE_UUID16(tuya_gatt_svcs[i].uuid)->value);
    }
    rc = ble_gatts_count_cfg(tuya_gatt_svcs);
    if (rc != 0) {
        BLE_HS_LOG(INFO, "rc = %d\n", rc);
        return OPRT_INVALID_PARM;
    }

    rc = ble_gatts_add_svcs(tuya_gatt_svcs);
    if (rc != 0) {
        BLE_HS_LOG(INFO, "rc = %d\n", rc);
        return OPRT_INVALID_PARM;
    }

    // FOR TEST
    // tuya_ble_test_main_run(NULL);
    gatts_service_flag = TRUE;
    return OPRT_OK;
}

/**
 * @brief   Set the value of a given attribute.
 * @param   [in] conn_handle  Connection handle.
 *          [in] char_handle  Attribute handle.
 *          [in,out] p_value  Attribute value information.
 * @return  SUCCESS
 *          ERROR
 *
 * @note Values other than system attributes can be set at any time, regardless of whether any active connections exist.
 * */
OPERATE_RET tkl_ble_gatts_value_set(uint16_t conn_handle, uint16_t char_handle, uint8_t *p_data, uint16_t length)
{
    if (!ble_hs_is_enabled()) {
        BLE_HS_LOG_INFO("bt_stack close,bt operation invalid.\n");
        return OPRT_OK;
    }
    if (tuya_ble_server.read_char[0].buffer == NULL) {
        tuya_ble_server.read_char[0].buffer = tuya_ble_hs_malloc(length);
    } else {
        tuya_ble_hs_free(tuya_ble_server.read_char[0].buffer);
        tuya_ble_server.read_char[0].buffer = tuya_ble_hs_malloc(length);
    }
    memcpy(tuya_ble_server.read_char[0].buffer, p_data, length);
    tuya_ble_server.read_char[0].size = length;
    tuya_ble_server.read_char[0].handle = char_handle;

    BLE_HS_LOG_INFO("char handle = 0x%02x", char_handle);

    return OPRT_OK;
}

/**
 * @brief   Get the value of a given attribute.
 * @param[in] conn_handle   Connection handle. Ignored if the value does not belong to a system attribute.
 * @param[in] char_handle   Attribute handle.
 * @return  SUCCESS
 *          ERROR
 * */
OPERATE_RET tkl_ble_gatts_value_get(uint16_t conn_handle, uint16_t char_handle, uint8_t *p_data, uint16_t length)
{
    return 0;
}

/**
 * @brief   Notify an attribute value.
 * @param   [in] conn_handle    Connection handle.
 * @param   [in] char_handle    Attribute handle.
 *          [in] p_data         Notify Values
 *          [in] length         Value Length
 * @return  SUCCESS
 *          ERROR
 * */
OPERATE_RET tkl_ble_gatts_value_notify(uint16_t conn_handle, uint16_t char_handle, uint8_t *p_data, uint16_t length)
{
    if (!ble_hs_is_enabled()) {
        BLE_HS_LOG_INFO("bt_stack close,bt operation invalid.\n");
        return OPRT_OK;
    }

    return tuya_ble_hs_notify(conn_handle, char_handle, p_data, length);
}

/**
 * @brief   Indicate an attribute value.
 * @param   [in] conn_handle    Connection handle.
 * @param   [in] char_handle    Attribute handle.
 *          [in] p_data         Notify Values
 *          [in] length         Value Length
 * @return  SUCCESS
 *          ERROR
 * */
OPERATE_RET tkl_ble_gatts_value_indicate(uint16_t conn_handle, uint16_t char_handle, uint8_t *p_data, uint16_t length)
{
    if (!ble_hs_is_enabled()) {
        BLE_HS_LOG_INFO("bt_stack close,bt operation invalid.\n");
        return OPRT_OK;
    }

    int rc = 0;
    struct os_mbuf *om;
    om = ble_hs_mbuf_from_flat(p_data, length);
    rc = ble_gattc_indicate_custom(conn_handle, char_handle, om);

    if (rc != 0) {
        return OPRT_OS_ADAPTER_BLE_INDICATE_FAILED;
    }
    return OPRT_OK;
}

/**
 * @brief   Reply to an ATT_MTU exchange request by sending an Exchange MTU Response to the client.
 * @param   [in] conn_handle    Connection handle.
 *          [in] server_rx_mtu  mtu size.
 * @return  SUCCESS
 *          ERROR
 * */
OPERATE_RET tkl_ble_gatts_exchange_mtu_reply(uint16_t conn_handle, uint16_t server_rx_mtu)
{
    if (!ble_hs_is_enabled()) {
        BLE_HS_LOG_INFO("bt_stack close,bt operation invalid.\n");
        return OPRT_OK;
    }
    int rc = 0;

    ble_att_set_preferred_mtu(server_rx_mtu);
    // Will Use Default MTU Size
    rc = ble_gattc_exchange_mtu(conn_handle, tuya_ble_host_mtu_exchange_callback, NULL);
    if (rc != 0) {
        return OPRT_OS_ADAPTER_BLE_MTU_REPLY_FAILED;
    }
    return OPRT_OK;
}

#if (TY_HS_BLE_ROLE_CENTRAL)
/******************************************************************************************************************************/
/** @brief Define All Gatt Client Interface, Refer to current ble gw and ble stack.
 *
 *  Notes: notice the handle will be the one of signed point.
 *  Discovery Operations belongs to GAP Interface, But declear here, because it will be used for the gatt client.
 */

/**
 * @brief   [Ble Central] Will Discovery All Service
 * @param   [in] conn_handle    Connection handle.
 * @return  SUCCESS
 *          ERROR
 * */
OPERATE_RET tkl_ble_gattc_all_service_discovery(uint16_t conn_handle)
{
    if (!ble_hs_is_enabled()) {
        BLE_HS_LOG_INFO("bt_stack close,bt operation invalid.\n");
        return OPRT_OK;
    }

    int rc = OPRT_COM_ERROR;

    rc = ble_gattc_disc_all_svcs(conn_handle, tuya_ble_svc_disc_callback, NULL);
    if (rc != 0) {
        return OPRT_OS_ADAPTER_BLE_SVC_DISC_FAILED;
    }

    return OPRT_OK;
}

/**
 * @brief   [Ble Central] Will Discovery All Characteristic
 * @param   [in] conn_handle    Connection handle.
 *          [in] start_handle   Handle of start
 *          [in] end_handle     Handle of End
 *          [in] p_uuid         char uuid
 * @return  SUCCESS
 *          ERROR
 * */
OPERATE_RET tkl_ble_gattc_all_char_discovery(uint16_t conn_handle, uint16_t start_handle, uint16_t end_handle)
{
    if (!ble_hs_is_enabled()) {
        BLE_HS_LOG_INFO("bt_stack close,bt operation invalid.\n");
        return OPRT_OK;
    }

    int rc = OPRT_COM_ERROR;

    BLE_HS_LOG_DEBUG("Discovery All Char, Start Handle = 0x%04x, End Handle = 0x%04x", start_handle, end_handle);
    rc = ble_gattc_disc_all_chrs(conn_handle, start_handle, end_handle, tuya_ble_chr_disc_callback, NULL);
    if (rc != 0) {
        BLE_HS_LOG_DEBUG("Discovery Char fail(%d)", rc);
        return OPRT_OS_ADAPTER_BLE_CHAR_DISC_FAILED;
    }

    return OPRT_OK;
}

/**
 * @brief   [Ble Central] Will Discovery All Descriptor of Characteristic
 * @param   [in] conn_handle    Connection handle.
 * @param   [in] conn_handle    Connection handle.
 *          [in] start_handle   Handle of start
 *          [in] end_handle     Handle of End
 *          [in] p_uuid         descriptor uuid
 * @return  SUCCESS
 *          ERROR
 * */
OPERATE_RET tkl_ble_gattc_char_desc_discovery(uint16_t conn_handle, uint16_t start_handle, uint16_t end_handle)
{
    if (!ble_hs_is_enabled()) {
        BLE_HS_LOG_INFO("bt_stack close,bt operation invalid.\n");
        return OPRT_OK;
    }

    int rc = OPRT_COM_ERROR;

    BLE_HS_LOG_DEBUG("Discovery All Descriptors, Start Handle = 0x%04x, End Handle = 0x%04x", start_handle, end_handle);
    rc = ble_gattc_disc_all_dscs(conn_handle, start_handle, end_handle, tuya_ble_desc_disc_callback, NULL);
    if (rc != 0) {
        BLE_HS_LOG_DEBUG("Discovery Char desc fail(%d)", rc);
        return OPRT_OS_ADAPTER_BLE_DESC_DISC_FAILED;
    }

    return OPRT_OK;
}

/**
 * @brief   [Ble Central] Write Data without Response
 * @param   [in] conn_handle    Connection handle.
 * @param   [in] char_handle    Attribute handle.
 *          [in] p_data         Write Values
 *          [in] length         Value Length
 * @return  SUCCESS
 *          ERROR
 * */
OPERATE_RET tkl_ble_gattc_write_without_rsp(uint16_t conn_handle, uint16_t char_handle, uint8_t *p_data,
                                            uint16_t length)
{
    if (!ble_hs_is_enabled()) {
        BLE_HS_LOG_INFO("bt_stack close,bt operation invalid.\n");
        return OPRT_OK;
    }

    int rc;

    rc = ble_gattc_write_no_rsp_flat(conn_handle, char_handle, p_data, length);
    if (rc != 0) {
        BLE_HS_LOG(ERR, "Error: Failed to write characteristic; rc=%d\n", rc);
        return OPRT_OS_ADAPTER_BLE_WRITE_FAILED;
    }

    return 0;
}

/**
 * @brief   [Ble Central] Write Data with response
 * @param   [in] conn_handle    Connection handle.
 * @param   [in] char_handle    Attribute handle.
 *          [in] p_data         Write Values
 *          [in] length         Value Length
 * @return  SUCCESS
 *          ERROR
 * */
OPERATE_RET tkl_ble_gattc_write(uint16_t conn_handle, uint16_t char_handle, uint8_t *p_data, uint16_t length)
{
    return 0;
}

/**
 * @brief   [Ble Central] Read Data
 * @param   [in] conn_handle    Connection handle.
 * @param   [in] char_handle    Attribute handle.
 * @return  SUCCESS
 *          ERROR
 * */
OPERATE_RET tkl_ble_gattc_read(uint16_t conn_handle, uint16_t char_handle)
{
    return 0;
}
#endif
/**
 * @brief   Start an ATT_MTU exchange by sending an Exchange MTU Request to the server.
 * @param   [in] conn_handle    Connection handle.
 *          [in] client_rx_mtu  mtu size.
 * @return  SUCCESS
 *          ERROR
 * */
OPERATE_RET tkl_ble_gattc_exchange_mtu_request(uint16_t conn_handle, uint16_t client_rx_mtu)
{
    if (!ble_hs_is_enabled()) {
        BLE_HS_LOG_INFO("bt_stack close,bt operation invalid.\n");
        return OPRT_OK;
    }

    int rc = 0;

    ble_att_set_preferred_mtu(client_rx_mtu);
    // Will Use Default MTU Size
    rc = ble_gattc_exchange_mtu(conn_handle, tuya_ble_host_mtu_exchange_callback, NULL);
    if (rc != 0) {
        return OPRT_OS_ADAPTER_BLE_MTU_REQ_FAILED;
    }

    return OPRT_OK;
}

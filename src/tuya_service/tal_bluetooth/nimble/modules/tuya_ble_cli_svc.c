
#include <stddef.h>
#include <stdlib.h>
// Host Include
#include "ble_hs.h"

// Configuration Include
#include "tuya_ble_cfg.h"
#include "tuya_hs_port.h"
#include "tuya_ble_os_adapter.h"
#include "ble_svc_gap.h"
#include "ble_svc_gatt.h"
#include "tal_system.h"

int tuya_ble_hs_notify(uint16_t conn_handle, uint16_t svc_handle, uint8_t *notify_data, uint16_t data_len)
{
    struct os_mbuf *om = NULL;

    om = ble_hs_mbuf_from_flat(notify_data, data_len);
    if (om == NULL) {
        tal_system_sleep(50);
        om = ble_hs_mbuf_from_flat(notify_data, data_len);
        if (om == NULL) {
            PR_ERR("OM BUF FAIL\r\n");
            return OPRT_OS_ADAPTER_BLE_NOTIFY_FAILED;
        }
    }

    if (om->om_omp->omp_pool->mp_num_free <= 2) { // (om->om_omp->omp_pool->mp_num_blocks / 2
        // Max Data Need Wait
        tal_system_sleep(50);
        PR_ERR("hs_notify wait:%d", om->om_omp->omp_pool->mp_num_free);
    }

    int rc = ble_gattc_notify_custom(conn_handle, svc_handle, om);
    if (rc != 0) {
        PR_ERR("HS_NOTIFY ERR:%x", rc);
        return OPRT_OS_ADAPTER_BLE_NOTIFY_FAILED;
    }
    return OPRT_OK;
}
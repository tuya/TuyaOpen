#include "tal_system.h"
#include "tuya_uf_db.h"
#include "tal_sw_timer.h"
#include "base_event.h"
#include "tuya_iot_wifi_api.h"
#include "uni_log.h"

#define RESET_NETCNT_NAME     "rst_cnt"
#define RESET_NETCNT_MAX      3


int reset_count_read(uint8_t *count)
{
    int      rt = OPRT_OK;
    uFILE   *fp = NULL;
    int     cnt = 0;

    fp = ufopen(RESET_NETCNT_NAME, "r+");
    if(NULL == fp) {
        PR_ERR("uf file %s can't open and read data!", RESET_NETCNT_NAME);
        return OPRT_NOT_EXIST;
    }

    PR_DEBUG("uf open OK");
    cnt = ufread(fp, count, 1);
    PR_DEBUG("uf file %s read data %d!", RESET_NETCNT_NAME, *count);

    rt = ufclose(fp);
    if(rt != OPRT_OK) {
        PR_ERR("uf file %s close error!", RESET_NETCNT_NAME);
        return rt;
    }
    
    return rt;
}


int reset_count_write(uint8_t count)
{
    int      rt = OPRT_OK;
    uFILE   *fp = NULL;
    int     cnt = 0;

    fp = ufopen(RESET_NETCNT_NAME, "a+");
    if(NULL == fp) {
        PR_ERR("uf file %s can't open and read data!", RESET_NETCNT_NAME);
        return OPRT_NOT_EXIST;
    }

    if (0 != ufseek(fp, 0, UF_SEEK_SET)) {
        ufclose(fp);
        PR_ERR("uf file %s Set file offset to 0 error!", RESET_NETCNT_NAME);
        return OPRT_NOT_EXIST;
    }

    cnt = ufwrite(fp, &count, 1);
    if(cnt != 1) {
        PR_ERR("uf file %s write data error!", RESET_NETCNT_NAME);
    }

    rt = ufclose(fp);
    if(rt != OPRT_OK) {
        PR_ERR("uf file %s close error!", RESET_NETCNT_NAME);
        return rt;
    }

    return rt;
}


STATIC VOID reset_netconfig_timer(VOID)
{
    reset_count_write(0);
    PR_DEBUG("reset cnt clear!");
}


int reset_netconfig_check(void *args)
{
    int rt;
    uint8_t rstcnt = 0;

    TUYA_CALL_ERR_LOG(reset_count_read(&rstcnt));
    if (rstcnt < RESET_NETCNT_MAX) {
        return OPRT_OK;
    }

    reset_count_write(0);

    PR_DEBUG("Reset ctrl data!");
    tuya_iot_wf_gw_fast_unactive(GWCM_OLD, WF_START_SMART_AP_CONCURRENT);
}


int reset_netconfig_start(VOID)
{
    int rt = OPRT_OK;
    uint8_t rstcnt = 0;

    // ty_subscribe_event
    ty_subscribe_event(EVENT_SDK_DB_INIT_OK, "reset", reset_netconfig_check, SUBSCRIBE_TYPE_ONETIME);

    // TUYA_RESET_REASON_E reason = tal_system_get_reset_reason(NULL);
    // PR_DEBUG("reset info -> reason is %d>>>", reason);
    // if(TUYA_RESET_REASON_POWERON != reason) {
    //     return OPRT_OK;
    // }
    TUYA_CALL_ERR_LOG(reset_count_read(&rstcnt));
    TUYA_CALL_ERR_LOG(reset_count_write(++rstcnt));

    PR_DEBUG("start reset cnt clear timer!!!!!");
    TIMER_ID rst_config_timer;
    tal_sw_timer_create(reset_netconfig_timer, NULL, &rst_config_timer);
    tal_sw_timer_start(rst_config_timer, 5000, TAL_TIMER_ONCE);

    return OPRT_OK;
}

int reset_netconfig_init(VOID)
{
    ty_subscribe_event(EVENT_SDK_EARLY_INIT_OK, "early_init", reset_netconfig_start, SUBSCRIBE_TYPE_ONETIME);

}

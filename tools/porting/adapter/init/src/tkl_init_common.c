/**
 * @file tkl_init_common.c
 * @brief Common process
 * @version 0.1
 * @date 2021-08-06
 *
 * @copyright Copyright 2020-2021 Tuya Inc. All Rights Reserved.
 *
 */

#include "tkl_init_common.h"

#define TKL_DESC_DEF(__type, __name, ...)                                                                              \
    const __type c_##__name = {__VA_ARGS__};                                                                           \
    __type *tkl_##__name##_get(void)                                                                                   \
    {                                                                                                                  \
        return (__type *)&c_##__name;                                                                                  \
    }

#define TKL_DESC_INIT(__key) .__key = NULL,
#define RTC_DESC_ITEM        TKL_DESC_INIT
#define FLASH_DESC_ITEM      TKL_DESC_INIT
#define WATCHDOG_DESC_ITEM   TKL_DESC_INIT

#if defined(ENABLE_RTC) && (ENABLE_RTC == 1)
#undef RTC_DESC_ITEM
#define RTC_DESC_ITEM(__key) .__key = tkl_rtc_##__key,

TKL_DESC_DEF(TKL_RTC_DESC_T, rtc_desc,
             RTC_DESC_ITEM(init) RTC_DESC_ITEM(deinit) RTC_DESC_ITEM(time_set) RTC_DESC_ITEM(time_get))
#endif

#if defined(ENABLE_WATCHDOG) && (ENABLE_WATCHDOG == 1)
#undef WATCHDOG_DESC_ITEM
#define WATCHDOG_DESC_ITEM(__key) .__key = tkl_watchdog_##__key,

TKL_DESC_DEF(TKL_WATCHDOG_DESC_T, watchdog_desc,
             WATCHDOG_DESC_ITEM(init) WATCHDOG_DESC_ITEM(deinit) WATCHDOG_DESC_ITEM(refresh))
#endif

#if defined(ENABLE_FLASH) && (ENABLE_FLASH == 1)
#undef FLASH_DESC_ITEM
#define FLASH_DESC_ITEM(__key) .__key = tkl_flash_##__key,

TKL_DESC_DEF(TKL_FLASH_DESC_T, flash_desc,
             FLASH_DESC_ITEM(read) FLASH_DESC_ITEM(write) FLASH_DESC_ITEM(erase) FLASH_DESC_ITEM(get_one_type_info))
#endif

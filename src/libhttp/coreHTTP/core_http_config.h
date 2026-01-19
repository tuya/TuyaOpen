/**
 * @file core_http_config_.h
 * @brief The default values for the configuration macros for the HTTP Client
 * library.
 *
 * @note This file SHOULD NOT be modified. If custom values are needed for
 * any configuration macro, a core_http_config.h file should be provided to
 * the HTTP Client library to override the default values defined in this file.
 * To use the custom config file, the HTTP_DO_NOT_USE_CUSTOM_CONFIG preprocessor
 * macro SHOULD NOT be set.
 */

#ifndef _CORE_HTTP_CONFIG_
#define _CORE_HTTP_CONFIG_

#include "tal_memory.h"

/**
 * @brief Maximum size, in bytes, of headers allowed from the server.
 *
 * If the total size in bytes of the headers received from the server exceeds
 * this configuration, then the status code
 * #HTTP_SECURITY_ALERT_RESPONSE_HEADERS_SIZE_LIMIT_EXCEEDED is returned from
 * #HTTPClient_Send.
 *
 * <b>Possible values:</b> Any positive 32 bit integer. <br>
 * <b>Default value:</b> `2048`
 */
#define HTTP_MAX_RESPONSE_HEADERS_SIZE_BYTES        4096U

#define HTTP_MAX_RESPONSE_CHUNK_SIZE_BYTES          (1024U * 16)

#define HTTP_MAX_RESPONSE_CHUNK_ONCE_BYTES          (2048U)


#if defined(ENABLE_EXT_RAM)&&(ENABLE_EXT_RAM==1)
#define HTTP_MALLOC                                 tal_psram_malloc
#define HTTP_FREE                                   tal_psram_free
#else
#define HTTP_MALLOC                                 tal_malloc
#define HTTP_FREE                                   tal_free
#endif
/**
 * @brief The HTTP header "User-Agent" value.
 *
 * The following header line is automatically written to
 * #HTTPRequestHeaders_t.pBuffer:
 * "User-Agent: my-platform-name\r\n"
 *
 * <b>Possible values:</b> Any string. <br>
 * <b>Default value:</b> `my-platform-name`
 */
#define HTTP_USER_AGENT_VALUE    "TUYA_IOT_SDK"

#include "tal_api.h"

#endif /* ifndef CORE_HTTP_CONFIG_DEFAULTS_ */

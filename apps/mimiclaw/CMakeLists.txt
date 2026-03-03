##
# @file CMakeLists.txt
# @brief mimiclaw app for TuyaOpen
#/

set(APP_PATH ${CMAKE_CURRENT_LIST_DIR})
get_filename_component(APP_NAME ${APP_PATH} NAME)

set(APP_SRCS
    ${APP_PATH}/mimi.c
    ${APP_PATH}/bus/message_bus.c
    ${APP_PATH}/wifi/wifi_manager.c
    ${APP_PATH}/channels/telegram_bot.c
    ${APP_PATH}/channels/discord_bot.c
    ${APP_PATH}/channels/feishu_bot.c
    ${APP_PATH}/llm/llm_proxy.c
    ${APP_PATH}/agent/agent_loop.c
    ${APP_PATH}/agent/context_builder.c
    ${APP_PATH}/memory/memory_store.c
    ${APP_PATH}/memory/session_mgr.c
    ${APP_PATH}/gateway/ws_server.c
    ${APP_PATH}/cli/serial_cli.c
    ${APP_PATH}/proxy/http_proxy.c
    ${APP_PATH}/cron/cron_service.c
    ${APP_PATH}/heartbeat/heartbeat.c
    ${APP_PATH}/tls_cert_bundle.c
    ${APP_PATH}/certs/ca_bundle_mini.c
    ${APP_PATH}/tools/tool_registry.c
    ${APP_PATH}/tools/tool_cron.c
    ${APP_PATH}/tools/tool_web_search.c
    ${APP_PATH}/tools/tool_web_search_baidu.c
    ${APP_PATH}/tools/tool_get_time.c
    ${APP_PATH}/tools/tool_files.c
    ${APP_PATH}/tools/tool_led.c
    ${APP_PATH}/skills/skill_loader.c
)

set(APP_INC
    ${APP_PATH}
)

add_library(${EXAMPLE_LIB})

target_sources(${EXAMPLE_LIB}
    PRIVATE
        ${APP_SRCS}
)

target_include_directories(${EXAMPLE_LIB}
    PRIVATE
        ${APP_INC}
)

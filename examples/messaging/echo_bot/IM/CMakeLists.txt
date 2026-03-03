##
# @file CMakeLists.txt
# @brief Echo bot IM component: message_bus + channels + proxy + certs.
#/

set(IM_PATH ${CMAKE_CURRENT_LIST_DIR})
# get_filename_component(APP_PATH ${IM_PATH}/.. ABSOLUTE)

set(IM_SRCS
    ${IM_PATH}/im_utils.c
    ${IM_PATH}/bus/message_bus.c
    ${IM_PATH}/channels/telegram_bot.c
    ${IM_PATH}/channels/discord_bot.c
    ${IM_PATH}/channels/feishu_bot.c
    ${IM_PATH}/proxy/http_proxy.c
    ${IM_PATH}/certs/tls_cert_bundle.c
    ${IM_PATH}/certs/ca_bundle_mini.c
)

set(IM_INC
    ${IM_PATH}
)

########################################
# Target Configure
########################################
target_sources(${EXAMPLE_LIB}
    PRIVATE
        ${IM_SRCS}
)

target_include_directories(${EXAMPLE_LIB}
    PRIVATE
        ${IM_INC}
)

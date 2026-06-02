##
# @file CMakeLists.txt
# @brief
#/
if (CONFIG_ENABLE_COMP_AI_DISPLAY STREQUAL "y")

set(COMP_MODULE_PATH ${CMAKE_CURRENT_LIST_DIR})

set(COMP_MODULE_SRCS)

# Common sources (non-recursive, top-level only)
aux_source_directory(${COMP_MODULE_PATH}/src COMP_MODULE_SRCS)

# UI variant sources based on Kconfig selection
if (CONFIG_ENABLE_AI_CHAT_GUI_WECHAT STREQUAL "y")
    file(GLOB WECHAT_SRCS ${COMP_MODULE_PATH}/src/wechat/*.c)
    list(APPEND COMP_MODULE_SRCS ${WECHAT_SRCS})
elseif (CONFIG_ENABLE_AI_CHAT_GUI_CHATBOT STREQUAL "y")
    file(GLOB CHATBOT_SRCS ${COMP_MODULE_PATH}/src/chatbot/*.c)
    list(APPEND COMP_MODULE_SRCS ${CHATBOT_SRCS})
elseif (CONFIG_ENABLE_AI_CHAT_GUI_OLED STREQUAL "y")
    file(GLOB OLED_SRCS ${COMP_MODULE_PATH}/src/oled/*.c)
    list(APPEND COMP_MODULE_SRCS ${OLED_SRCS})
endif()

# Font sources
aux_source_directory(${COMP_MODULE_PATH}/font FONT_SRCS)
aux_source_directory(${COMP_MODULE_PATH}/font/emoji EMOJI_SRCS)

# Icon sources: select version-specific subdirectory
if (CONFIG_LVGL_VERSION_9 STREQUAL "y")
    aux_source_directory(${COMP_MODULE_PATH}/icon/v9 ICON_SRCS)
else()
    aux_source_directory(${COMP_MODULE_PATH}/icon/v8 ICON_SRCS)
endif()

list(APPEND COMP_MODULE_SRCS
    ${FONT_SRCS}
    ${EMOJI_SRCS}
    ${ICON_SRCS}
)

# Include paths
set(COMP_MODULE_INC
    ${COMP_MODULE_PATH}/include
    ${COMP_MODULE_PATH}/font
)

if (CONFIG_ENABLE_AI_CHAT_GUI_WECHAT STREQUAL "y")
    list(APPEND COMP_MODULE_INC ${COMP_MODULE_PATH}/src/wechat)
endif()

########################################
# Target Configure
########################################
target_sources(${EXAMPLE_LIB}
    PRIVATE
        ${COMP_MODULE_SRCS}
    )

target_include_directories(${EXAMPLE_LIB}
    PRIVATE
        ${COMP_MODULE_INC}
    )

target_compile_options(${EXAMPLE_LIB}
    PRIVATE
        "-DLV_LVGL_H_INCLUDE_SIMPLE"
    )

endif()

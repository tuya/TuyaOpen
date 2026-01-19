##
# @file CMakeLists.txt
# @brief 
#/
set(APP_MODULE_PATH ${CMAKE_CURRENT_LIST_DIR})

set(APP_MODULE_SRCS)

aux_source_directory(${APP_MODULE_PATH}/ducky DUCKY_SRCS)
aux_source_directory(${APP_MODULE_PATH}/icons ICONS_SRCS)
aux_source_directory(${APP_MODULE_PATH}/logo LOGO_SRCS)
aux_source_directory(${APP_MODULE_PATH}/ui UI_SRCS)
aux_source_directory(${APP_MODULE_PATH}/fonts FONTS_SRCS)


list(APPEND APP_MODULE_SRCS
    ${DUCKY_SRCS}
    ${ICONS_SRCS}
    ${LOGO_SRCS}
    ${UI_SRCS}
    ${FONTS_SRCS}
)

set(APP_MODULE_INC 
    ${APP_MODULE_PATH}
    ${APP_MODULE_PATH}/ui
)

########################################
# Target Configure
########################################
target_sources(${EXAMPLE_LIB}
    PRIVATE
        ${APP_MODULE_SRCS}
    )

target_include_directories(${EXAMPLE_LIB}
    PRIVATE
        ${APP_MODULE_INC}
    )
##
# @file CMakeLists.txt
# @brief
#/

# APP_PATH
set(APP_PATH ${CMAKE_CURRENT_LIST_DIR})

# APP_NAME
get_filename_component(APP_NAME ${APP_PATH} NAME)

# APP_SRCS
aux_source_directory(${APP_PATH}/src APP_SRCS)
list(FILTER APP_SRCS EXCLUDE REGEX "portal_web_assets\\.c$")

set(PORTAL_WEB_ROOT ${APP_PATH}/web)
set(PORTAL_WEB_GEN_DIR ${APP_PATH}/src)
set(PORTAL_WEB_GEN_HEADER ${PORTAL_WEB_GEN_DIR}/portal_web_assets.h)
set(PORTAL_WEB_GEN_SOURCE ${PORTAL_WEB_GEN_DIR}/portal_web_assets.c)
set(PORTAL_WEB_EMBED_SCRIPT ${APP_PATH}/tools/embed_web_assets.py)

file(GLOB_RECURSE PORTAL_WEB_ASSET_FILES
    CONFIGURE_DEPENDS
    LIST_DIRECTORIES false
    ${PORTAL_WEB_ROOT}/*
)

if(DEFINED ENV{OPEN_SDK_PYTHON} AND NOT "$ENV{OPEN_SDK_PYTHON}" STREQUAL "")
    set(PORTAL_WEB_PYTHON $ENV{OPEN_SDK_PYTHON})
else()
    find_program(PORTAL_WEB_PYTHON python3)
endif()

if(PORTAL_WEB_PYTHON)
    add_custom_command(
        OUTPUT ${PORTAL_WEB_GEN_HEADER} ${PORTAL_WEB_GEN_SOURCE}
        COMMAND ${CMAKE_COMMAND} -E env PYTHONIOENCODING=utf-8 PYTHONUNBUFFERED=1
                ${PORTAL_WEB_PYTHON} ${PORTAL_WEB_EMBED_SCRIPT}
                --web-root ${PORTAL_WEB_ROOT}
                --out-dir ${PORTAL_WEB_GEN_DIR}
        DEPENDS ${PORTAL_WEB_EMBED_SCRIPT} ${PORTAL_WEB_ASSET_FILES}
        COMMENT "Embedding HTTP portal web assets into ${PORTAL_WEB_GEN_DIR}"
        VERBATIM
    )

    add_custom_target(portal_web_assets_gen
        DEPENDS ${PORTAL_WEB_GEN_HEADER} ${PORTAL_WEB_GEN_SOURCE}
    )
endif()

########################################
# Target Configure
########################################
add_library(${EXAMPLE_LIB})

target_sources(${EXAMPLE_LIB}
    PRIVATE
        ${APP_SRCS}
    )

target_include_directories(${EXAMPLE_LIB}
    PRIVATE
        ${APP_PATH}/src
    )

if(PORTAL_WEB_PYTHON)
    target_sources(${EXAMPLE_LIB}
        PRIVATE
            ${PORTAL_WEB_GEN_SOURCE}
    )

    add_dependencies(${EXAMPLE_LIB} portal_web_assets_gen)
endif()

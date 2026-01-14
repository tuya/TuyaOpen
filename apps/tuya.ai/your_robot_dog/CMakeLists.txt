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
aux_source_directory(${APP_PATH}/src/system_info APP_SRCS)
aux_source_directory(${APP_PATH}/src/servo_ctrl APP_SRCS)

# Exclude trivial wrapper source; we link LittleFS directly now
list(FILTER APP_SRCS EXCLUDE REGEX ".*/lfs_wrap\\.c$")

set(APP_INC 
            ${APP_PATH}/include
            ${APP_PATH}/src
            ${APP_PATH}/src/system_info
            ${APP_PATH}/src/servo_ctrl
)

# NOTE: Board-specific hardware is now provided by boards/T5AI/TUYA_T5AI_DOG.
# Do not compile or include the legacy app-local src/hardware (it may be removed).

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
        ${APP_INC}
    )

# Project-local build define for ai_components/ai_ui
# (Avoid patching shared ai_components headers.)
target_compile_definitions(${EXAMPLE_LIB} PRIVATE EMO_ICON_MAX_NUM=7)

target_compile_options(${EXAMPLE_LIB}
    PRIVATE
        "-DLV_LVGL_H_INCLUDE_SIMPLE"
    )

## Do not wrap lfs_mount; use platform LittleFS directly
## (removes need for a dedicated wrapper source file)

########################################
# Add subdirectory
########################################

if (CONFIG_ENABLE_COMP_AI_DISPLAY STREQUAL "y")
    add_subdirectory(${APP_PATH}/src/display)
endif()

add_subdirectory(${APP_PATH}/../ai_components)

########################################
# Project-local override (your_robot_dog only)
# Ensure only ONE LittleFS implementation/ABI in final firmware.
########################################

if(DEFINED TOS_PROJECT_NAME AND DEFINED TOS_PROJECT_PLATFORM)
    if(TOS_PROJECT_NAME STREQUAL "your_robot_dog" AND TOS_PROJECT_PLATFORM STREQUAL "T5AI")
        if(TARGET tal_kv)
            # 1) Remove tal_kv vendored littlefs sources from this project build.
            #    (Otherwise the firmware ends up with two LittleFS implementations/ABIs.)
            get_target_property(_tal_kv_srcs tal_kv SOURCES)
            if(_tal_kv_srcs)
                list(FILTER _tal_kv_srcs EXCLUDE REGEX ".*/src/tal_kv/littlefs/lfs\\.c$")
                list(FILTER _tal_kv_srcs EXCLUDE REGEX ".*/src/tal_kv/littlefs/lfs_util\\.c$")
                set_property(TARGET tal_kv PROPERTY SOURCES "${_tal_kv_srcs}")
            endif()

            # tal_kv sources have built-in guards to avoid including tal_kv's lfs_config.h.
            target_compile_definitions(tal_kv PRIVATE TAL_KV_USE_PLATFORM_LITTLEFS=1)

            # 2) tal_kv's directory-level add_definitions(-DLFS_CONFIG=...) would change LittleFS ABI.
            # Undefine it for this target so it uses platform LittleFS defaults.
            target_compile_options(tal_kv PRIVATE "-ULFS_CONFIG")

            # 3) Make tal_kv (and consumers via tal_kv.h) pick platform lfs.h/lfs_util.h first.
            set(_platform_lfs_inc "${CMAKE_SOURCE_DIR}/platform/T5AI/t5_os/ap/components/littlefs")
            target_include_directories(tal_kv BEFORE PUBLIC "${_platform_lfs_inc}")

            # 4) If tal_kv still includes "lfs_config.h", force it to use our no-op shim.
            target_include_directories(tal_kv BEFORE PUBLIC "${APP_PATH}/include")
        endif()
    endif()
endif()

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

list(APPEND media_srcs "${APP_PATH}/src/media")

list(APPEND APP_SRCS ${media_srcs})

set(APP_INC 
            ${APP_PATH}/assets
            ${APP_PATH}/include
            ${APP_PATH}/include/media
)

list(APPEND APP_INC "${APP_PATH}/assets")

########################################
# Generate language config header
########################################
set(LANG_SCRIPT "${APP_PATH}/script/gen_lang.py")
set(LANG_HEADER "${APP_PATH}/assets/lang_config.h")

# Select language JSON based on Kconfig
if(CONFIG_ENABLE_LANGUAGE_CHINESE STREQUAL "y")
    set(LANG_JSON "${APP_PATH}/assets/zh-CN/language.json")
    message(STATUS "Selected language: Chinese (zh-CN)")
elseif(CONFIG_ENABLE_LANGUAGE_ENGLISH STREQUAL "y")
    set(LANG_JSON "${APP_PATH}/assets/en-US/language.json")
    message(STATUS "Selected language: English (en-US)")
else()
    # Default to Chinese if not specified
    set(LANG_JSON "${APP_PATH}/assets/zh-CN/language.json")
    message(STATUS "Language not specified, defaulting to Chinese (zh-CN)")
endif()

# Get Python command from environment variable PYTHON_CMD
# Only execute if PYTHON_CMD is not empty
if(DEFINED ENV{OPEN_SDK_PYTHON} AND NOT "$ENV{OPEN_SDK_PYTHON}" STREQUAL "")
    set(PYTHON_CMD $ENV{OPEN_SDK_PYTHON})
    
    # Add custom command to generate language config header
    add_custom_command(
        OUTPUT ${LANG_HEADER}
        COMMAND ${CMAKE_COMMAND} -E env PYTHONIOENCODING=utf-8 PYTHONUNBUFFERED=1
                ${PYTHON_CMD} ${LANG_SCRIPT} --input ${LANG_JSON} --output ${LANG_HEADER}
        DEPENDS ${LANG_SCRIPT} ${LANG_JSON}
        COMMENT "Generating language config header from ${LANG_JSON}"
        VERBATIM
    )

    # Create a custom target for language generation (optional, for manual triggering)
    add_custom_target(generate_lang_config
        DEPENDS ${LANG_HEADER}
        COMMENT "Language config generation target"
    )
endif()

########################################
# Target Configure
########################################
add_library(${EXAMPLE_LIB})

# Ensure language config header is generated before building (only if PYTHON_CMD is set)
if(DEFINED ENV{OPEN_SDK_PYTHON} AND NOT "$ENV{OPEN_SDK_PYTHON}" STREQUAL "")
    add_dependencies(${EXAMPLE_LIB} generate_lang_config)
endif()

target_sources(${EXAMPLE_LIB}
    PRIVATE
        ${APP_SRCS}
    )

target_include_directories(${EXAMPLE_LIB}
    PRIVATE
        ${APP_INC}
    )

target_compile_options(${EXAMPLE_LIB}
    PRIVATE
        "-DLV_LVGL_H_INCLUDE_SIMPLE"
    )

########################################
# Add subdirectory
########################################

if (CONFIG_ENABLE_CHAT_DISPLAY STREQUAL "y")
        add_subdirectory(${APP_PATH}/src/display)
endif()

if (CONFIG_ENABLE_EX_MODULE_CAMERA STREQUAL "y")
    add_subdirectory(${APP_PATH}/src/camera)
endif()

# Add mcp subdirectory
add_subdirectory(${APP_PATH}/src/mcp)

add_subdirectory(${APP_PATH}/../ai_components/ai_audio)
target_include_directories(${EXAMPLE_LIB} PRIVATE ${APP_PATH}/../ai_components/ai_audio)

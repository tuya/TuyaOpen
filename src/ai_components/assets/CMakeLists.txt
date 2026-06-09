##
# @file CMakeLists.txt
# @brief
#/
set(MODULE_PATH ${CMAKE_CURRENT_LIST_DIR})
get_filename_component(MODULE_NAME ${MODULE_PATH} NAME)

file(GLOB_RECURSE LIB_SRCS ${MODULE_PATH}/src/*.c)
set(LIB_PUBLIC_INC ${MODULE_PATH}/include)

########################################
# Generate language config header
########################################
set(LANG_SCRIPT "${MODULE_PATH}/scripts/gen_lang.py")
set(LANG_HEADER "${MODULE_PATH}/include/lang_config.h")

# Select language JSON based on Kconfig
if(CONFIG_ENABLE_AI_LANGUAGE_CHINESE STREQUAL "y")
    set(LANG_JSON "${MODULE_PATH}/language/zh-CN/language.json")
    message(STATUS "Selected language: Chinese (zh-CN)")
elseif(CONFIG_ENABLE_AI_LANGUAGE_ENGLISH STREQUAL "y")
    set(LANG_JSON "${MODULE_PATH}/language/en-US/language.json")
    message(STATUS "Selected language: English (en-US)")
else()
    # Default to Chinese if not specified
    set(LANG_JSON "${MODULE_PATH}/language/zh-CN/language.json")
    message(STATUS "Language not specified, defaulting to Chinese (zh-CN)")
endif()

# Get Python command from environment variable OPEN_SDK_PYTHON.
# Only execute if it is not empty.
if(DEFINED ENV{OPEN_SDK_PYTHON} AND NOT "$ENV{OPEN_SDK_PYTHON}" STREQUAL "")
    set(PYTHON_CMD $ENV{OPEN_SDK_PYTHON})

    add_custom_command(
        OUTPUT ${LANG_HEADER}
        COMMAND ${CMAKE_COMMAND} -E env PYTHONIOENCODING=utf-8 PYTHONUNBUFFERED=1
                ${PYTHON_CMD} ${LANG_SCRIPT} --input ${LANG_JSON} --output ${LANG_HEADER}
        DEPENDS ${LANG_SCRIPT} ${LANG_JSON}
        COMMENT "Generating language config header from ${LANG_JSON}"
        VERBATIM
    )

    add_custom_target(generate_lang_config
        DEPENDS ${LANG_HEADER}
        COMMENT "Language config generation target"
    )
endif()

########################################
# Target Configure
########################################
add_library(${MODULE_NAME})
target_sources(${MODULE_NAME} PRIVATE ${LIB_SRCS})
target_include_directories(${MODULE_NAME} PUBLIC ${LIB_PUBLIC_INC})

# Ensure the language config header is generated before building (only if Python is available)
if(DEFINED ENV{OPEN_SDK_PYTHON} AND NOT "$ENV{OPEN_SDK_PYTHON}" STREQUAL "")
    add_dependencies(${MODULE_NAME} generate_lang_config)
endif()

list(APPEND COMPONENT_LIBS ${MODULE_NAME})
set(COMPONENT_LIBS "${COMPONENT_LIBS}" PARENT_SCOPE)
list(APPEND COMPONENT_PUBINC ${LIB_PUBLIC_INC})
set(COMPONENT_PUBINC "${COMPONENT_PUBINC}" PARENT_SCOPE)

##
# @file CMakeLists.txt
# @brief 
#/
set(COMP_MODULE_PATH ${CMAKE_CURRENT_LIST_DIR})

set(COMP_MODULE_SRCS)

file(GLOB_RECURSE COMP_MODULE_SRCS ${COMP_MODULE_PATH}/src/*.c) 

set(COMP_MODULE_INC 
    ${COMP_MODULE_PATH}/include
)

########################################
# Generate language config header
########################################
set(LANG_SCRIPT "${CMAKE_CURRENT_LIST_DIR}/scripts/gen_lang.py")
set(LANG_HEADER "${CMAKE_CURRENT_LIST_DIR}/include/lang_config.h")

# Select language JSON based on Kconfig
if(CONFIG_ENABLE_AI_LANGUAGE_CHINESE STREQUAL "y")
    set(LANG_JSON "${CMAKE_CURRENT_LIST_DIR}/language/zh-CN/language.json")
    message(STATUS "Selected language: Chinese (zh-CN)")
elseif(CONFIG_ENABLE_AI_LANGUAGE_ENGLISH STREQUAL "y")
    set(LANG_JSON "${CMAKE_CURRENT_LIST_DIR}/language/en-US/language.json")
    message(STATUS "Selected language: English (en-US)")
else()
    # Default to Chinese if not specified
    set(LANG_JSON "${CMAKE_CURRENT_LIST_DIR}/language/zh-CN/language.json")
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

# Ensure language config header is generated before building (only if PYTHON_CMD is set)
if(DEFINED ENV{OPEN_SDK_PYTHON} AND NOT "$ENV{OPEN_SDK_PYTHON}" STREQUAL "")
    add_dependencies(${EXAMPLE_LIB} generate_lang_config)
endif()

target_sources(${EXAMPLE_LIB}
    PRIVATE
        ${COMP_MODULE_SRCS}
    )

target_include_directories(${EXAMPLE_LIB}
    PRIVATE
        ${COMP_MODULE_INC}
    )

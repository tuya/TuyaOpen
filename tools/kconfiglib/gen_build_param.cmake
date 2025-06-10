##
# @file gen_build_param.cmake
# @brief Generate build parameters file
# @version 1.0.0
# @date 2025-05-08

execute_process(
    COMMAND
    ${CMAKE_COMMAND} -E make_directory ${BUILD_PARAM_DIR}

    COMMAND
    python ${KCONFIG_TOOLS}/conf2param.py -c "${DOT_CONFIG_DIR}/${DOT_CONFIG}" -p "${BUILD_PARAM_LIST}" -o "${BUILD_PARAM_DIR}/${BUILD_PARAM_FILE_NAME}"
)

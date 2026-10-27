##
# @file CMakeLists.txt
# @brief Opus encoding benchmark
#/
add_library(${EXAMPLE_LIB} STATIC "${CMAKE_CURRENT_LIST_DIR}/src/tuya_app_main.c")
if (CONFIG_ENABLE_TUYAOPEN_OPUS)
    if (NOT TARGET tuyaopen_opus)
        message(FATAL_ERROR "TuyaOpen Opus target is missing")
    endif()
    target_include_directories(${EXAMPLE_LIB} BEFORE PRIVATE
        "${TOP_SOURCE_DIR}/src/tuya_ai_service/thirdparty/opus/upstream/include")
    target_link_libraries(${EXAMPLE_LIB} tuyaopen_opus)
else()
    # Public Opus API headers already shipped with the SDK. The platform
    # supplies libopus.a during its final link; no source download is needed.
    target_include_directories(${EXAMPLE_LIB} BEFORE PRIVATE
        "${TOP_SOURCE_DIR}/src/tuya_ai_service/svc_ai_codec/src")
endif()
if (CONFIG_ENABLE_TUYAOPEN_OPUS_CORTEX_M3_MUL)
    target_sources(${EXAMPLE_LIB} PRIVATE
        "${TOP_SOURCE_DIR}/src/tuya_ai_service/thirdparty/opus/tests/cortex_m3_math_selftest.c")
    target_include_directories(${EXAMPLE_LIB} PRIVATE
        "${TOP_SOURCE_DIR}/src/tuya_ai_service/thirdparty/opus/upstream/include/opus"
        "${TOP_SOURCE_DIR}/src/tuya_ai_service/thirdparty/opus/upstream/celt"
        "${TOP_SOURCE_DIR}/src/tuya_ai_service/thirdparty/opus/upstream/silk")
endif()
if (CONFIG_OPUS_BENCH_PROFILE)
    target_include_directories(${EXAMPLE_LIB} PRIVATE
        "${TOP_SOURCE_DIR}/src/tuya_ai_service/thirdparty/opus/profile")
endif()

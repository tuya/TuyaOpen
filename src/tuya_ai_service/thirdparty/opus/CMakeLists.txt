##
# @file CMakeLists.txt
# @brief Third-party libraries used by the tuya AI service
#/

if (CONFIG_ENABLE_TUYAOPEN_OPUS)
    set(TUYAOPEN_OPUS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/upstream")
    set(TUYAOPEN_OPUS_REPO "https://github.com/xiph/opus.git")
    set(TUYAOPEN_OPUS_VERSION "v1.3.1")

    # Check source files, not just the directory, to detect incomplete installs.
    set(TUYAOPEN_OPUS_INSTALLED TRUE)
    foreach (opus_file
            include/opus/opus.h
            include/opus/opus_defines.h
            include/opus/opus_types.h
            src/opus_encoder.c
            src/opus_decoder.c
            celt/celt_encoder.c
            silk/NSQ.c
            COPYING)
        if (NOT EXISTS "${TUYAOPEN_OPUS_DIR}/${opus_file}")
            set(TUYAOPEN_OPUS_INSTALLED FALSE)
        endif()
    endforeach()

    if (NOT TUYAOPEN_OPUS_INSTALLED)
        find_package(Git REQUIRED)
        set(TUYAOPEN_OPUS_DOWNLOAD_DIR "${CMAKE_CURRENT_BINARY_DIR}/opus-download")
        file(REMOVE_RECURSE "${TUYAOPEN_OPUS_DOWNLOAD_DIR}")

        message(STATUS "Downloading TuyaOpen Opus ${TUYAOPEN_OPUS_VERSION}")
        execute_process(
            COMMAND "${CMAKE_COMMAND}" -E env GIT_TERMINAL_PROMPT=0
                    "${GIT_EXECUTABLE}" clone --depth 1
                    --branch "${TUYAOPEN_OPUS_VERSION}"
                    "${TUYAOPEN_OPUS_REPO}" "${TUYAOPEN_OPUS_DOWNLOAD_DIR}"
            RESULT_VARIABLE opus_download_result
            ERROR_VARIABLE opus_download_error
            TIMEOUT 300)
        if (NOT opus_download_result STREQUAL "0")
            file(REMOVE_RECURSE "${TUYAOPEN_OPUS_DOWNLOAD_DIR}")
            message(FATAL_ERROR
                "Failed to download TuyaOpen Opus: ${opus_download_result}\n"
                "${opus_download_error}\n"
                "Retry or disable CONFIG_ENABLE_TUYAOPEN_OPUS to use the platform libopus.a.")
        endif()

        # Keep the existing include/opus layout. Use CMake file operations for
        # Windows, Linux and macOS; do not configure upstream's desktop build.
        file(MAKE_DIRECTORY "${TUYAOPEN_OPUS_DIR}/include/opus")
        file(COPY "${TUYAOPEN_OPUS_DOWNLOAD_DIR}/include/"
             DESTINATION "${TUYAOPEN_OPUS_DIR}/include/opus"
             FILES_MATCHING PATTERN "*.h")
        file(COPY
             "${TUYAOPEN_OPUS_DOWNLOAD_DIR}/src"
             "${TUYAOPEN_OPUS_DOWNLOAD_DIR}/celt"
             "${TUYAOPEN_OPUS_DOWNLOAD_DIR}/silk"
             "${TUYAOPEN_OPUS_DOWNLOAD_DIR}/COPYING"
             DESTINATION "${TUYAOPEN_OPUS_DIR}")
        file(REMOVE_RECURSE "${TUYAOPEN_OPUS_DOWNLOAD_DIR}")
    endif()

    # Portable fixed-point sources, with optional compiler-checked ARM DSP arithmetic.
    file(GLOB OPUS_SOURCES
         "${TUYAOPEN_OPUS_DIR}/src/*.c"
         "${TUYAOPEN_OPUS_DIR}/celt/*.c"
         "${TUYAOPEN_OPUS_DIR}/silk/*.c"
         "${TUYAOPEN_OPUS_DIR}/silk/fixed/*.c")
    list(FILTER OPUS_SOURCES EXCLUDE REGEX
         "/(opus_demo|opus_compare|repacketizer_demo|opus_custom_demo|analysis|mlp|mlp_data)\\.c$")
    if (CONFIG_OPUS_BENCH_PROFILE)
        set(OPUS_PROFILE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/profile")
        if (NOT TOS_PYTHON)
            message(FATAL_ERROR "Opus profiling requires the SDK TOS_PYTHON interpreter")
        endif()
        set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
            "${OPUS_PROFILE_DIR}/instrument_silk.py")
        foreach (opus_profile_file fixed/encode_frame_FIX.c fixed/find_pred_coefs_FIX.c NSQ.c NSQ_del_dec.c)
            set(OPUS_PROFILE_INPUT "${TUYAOPEN_OPUS_DIR}/silk/${opus_profile_file}")
            set(OPUS_PROFILE_SOURCE "${CMAKE_CURRENT_BINARY_DIR}/opus-profile/${opus_profile_file}")
            set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${OPUS_PROFILE_INPUT}")
            execute_process(
                COMMAND "${TOS_PYTHON}" "${OPUS_PROFILE_DIR}/instrument_silk.py"
                    --input "${OPUS_PROFILE_INPUT}" --output "${OPUS_PROFILE_SOURCE}"
                RESULT_VARIABLE OPUS_PROFILE_RESULT)
            if (NOT OPUS_PROFILE_RESULT STREQUAL "0")
                message(FATAL_ERROR "Could not instrument ${opus_profile_file}")
            endif()
            list(REMOVE_ITEM OPUS_SOURCES "${OPUS_PROFILE_INPUT}")
            list(APPEND OPUS_SOURCES "${OPUS_PROFILE_SOURCE}")
        endforeach()
    endif()
    if (CONFIG_ENABLE_TUYAOPEN_OPUS_NSQ_CACHE OR CONFIG_ENABLE_TUYAOPEN_OPUS_NSQ_RAM)
        set(OPUS_NSQ_OPT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/optimization")
        if (NOT TOS_PYTHON)
            message(FATAL_ERROR "NSQ candidate requires the SDK TOS_PYTHON interpreter")
        endif()
        if (CONFIG_OPUS_BENCH_PROFILE)
            set(OPUS_NSQ_OPT_INPUT "${CMAKE_CURRENT_BINARY_DIR}/opus-profile/NSQ.c")
        else()
            set(OPUS_NSQ_OPT_INPUT "${TUYAOPEN_OPUS_DIR}/silk/NSQ.c")
        endif()
        set(OPUS_NSQ_OPT_SOURCE "${CMAKE_CURRENT_BINARY_DIR}/opus-nsq-opt/NSQ.c")
        set(OPUS_NSQ_OPT_ARGS)
        if (NOT CONFIG_ENABLE_TUYAOPEN_OPUS_NSQ_CACHE)
            list(APPEND OPUS_NSQ_OPT_ARGS --no-cache)
        elseif (CONFIG_PLATFORM_CHOICE STREQUAL "L511G_Y7PM")
            list(APPEND OPUS_NSQ_OPT_ARGS --cache-mode l511g)
        endif()
        if (CONFIG_ENABLE_TUYAOPEN_OPUS_NSQ_RAM)
            if (NOT CONFIG_PLATFORM_CHOICE STREQUAL "L511G_Y7PM" OR NOT CMAKE_C_COMPILER_ID STREQUAL "GNU")
                message(FATAL_ERROR "NSQ RAM placement requires the L511G_Y7PM GNU linker/loader")
            endif()
            list(APPEND OPUS_NSQ_OPT_ARGS --l511g-ram)
        endif()
        set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
            "${OPUS_NSQ_OPT_DIR}/prepare_nsq.py" "${TUYAOPEN_OPUS_DIR}/silk/NSQ.c")
        execute_process(
            COMMAND "${TOS_PYTHON}" "${OPUS_NSQ_OPT_DIR}/prepare_nsq.py"
                    --input "${OPUS_NSQ_OPT_INPUT}" --output "${OPUS_NSQ_OPT_SOURCE}" ${OPUS_NSQ_OPT_ARGS}
            RESULT_VARIABLE OPUS_NSQ_OPT_RESULT)
        if (NOT OPUS_NSQ_OPT_RESULT STREQUAL "0")
            message(FATAL_ERROR "Could not generate NSQ optimization candidate")
        endif()
        list(REMOVE_ITEM OPUS_SOURCES "${OPUS_NSQ_OPT_INPUT}")
        list(APPEND OPUS_SOURCES "${OPUS_NSQ_OPT_SOURCE}")
    endif()
    add_library(tuyaopen_opus STATIC ${OPUS_SOURCES})
    if (CONFIG_OPUS_BENCH_PROFILE)
        target_include_directories(tuyaopen_opus PRIVATE "${OPUS_PROFILE_DIR}")
    endif()
    target_include_directories(tuyaopen_opus BEFORE
        PUBLIC "${TUYAOPEN_OPUS_DIR}/include"
        PRIVATE "${TUYAOPEN_OPUS_DIR}/include/opus"
                "${TUYAOPEN_OPUS_DIR}/src" "${TUYAOPEN_OPUS_DIR}/celt"
                "${TUYAOPEN_OPUS_DIR}/silk" "${TUYAOPEN_OPUS_DIR}/silk/fixed")
    target_compile_definitions(tuyaopen_opus PRIVATE
        OPUS_BUILD FIXED_POINT DISABLE_FLOAT_API PACKAGE_VERSION="${TUYAOPEN_OPUS_VERSION}")
    # Only compile this probe: MCU toolchains cannot link host test programs.
    function(tuyaopen_opus_check_math)
        include(CheckSymbolExists)
        set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
        check_symbol_exists(lrintf "math.h" TUYAOPEN_OPUS_HAVE_LRINTF)
        if (TUYAOPEN_OPUS_HAVE_LRINTF)
            target_compile_definitions(tuyaopen_opus PRIVATE HAVE_LRINTF)
        endif()
    endfunction()
    tuyaopen_opus_check_math()

    if (CONFIG_ENABLE_TUYAOPEN_OPUS_ARM_DSP)
        function(tuyaopen_opus_check_arm_dsp)
            include(CheckCSourceCompiles)
            set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
            set(CMAKE_REQUIRED_INCLUDES
                "${TUYAOPEN_OPUS_DIR}/include/opus" "${TUYAOPEN_OPUS_DIR}/celt"
                "${TUYAOPEN_OPUS_DIR}/silk")
            set(CMAKE_REQUIRED_DEFINITIONS -DFIXED_POINT -DOPUS_ARM_INLINE_ASM -DOPUS_ARM_INLINE_EDSP)
            check_c_source_compiles("
                #if !defined(__arm__) || !defined(__ARM_FEATURE_DSP)
                #error ARM DSP instructions are required
                #endif
                #include \"SigProc_FIX.h\"
                int probe(int a, int b, int c) {
                    return silk_ADD_SAT32(silk_SMLAWB(a, b, c), silk_SMULWW(b, c));
                }" TUYAOPEN_OPUS_HAVE_ARM_DSP)
            if (NOT TUYAOPEN_OPUS_HAVE_ARM_DSP)
                message(FATAL_ERROR "Selected compiler cannot build Opus ARM DSP; disable CONFIG_ENABLE_TUYAOPEN_OPUS_ARM_DSP")
            endif()
        endfunction()
        tuyaopen_opus_check_arm_dsp()
        target_compile_definitions(tuyaopen_opus PRIVATE OPUS_ARM_INLINE_ASM OPUS_ARM_INLINE_EDSP)
    endif()

    if (CONFIG_ENABLE_TUYAOPEN_OPUS_CORTEX_M3_MUL)
        set(OPUS_M3_HEADER "${CMAKE_CURRENT_SOURCE_DIR}/optimization/opus_cortex_m3_macros.h")
        function(tuyaopen_opus_check_cortex_m3_mul)
            include(CheckCSourceCompiles)
            set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
            set(CMAKE_REQUIRED_INCLUDES
                "${TUYAOPEN_OPUS_DIR}/include/opus" "${TUYAOPEN_OPUS_DIR}/celt"
                "${TUYAOPEN_OPUS_DIR}/silk")
            set(CMAKE_REQUIRED_DEFINITIONS -DOPUS_BUILD -DFIXED_POINT)
            # Recheck when the candidate header changes, even on a warm build.
            unset(TUYAOPEN_OPUS_HAVE_CORTEX_M3_MUL CACHE)
            check_c_source_compiles("
                #include \"${OPUS_M3_HEADER}\"
                int probe(int a, int b, int c) {
                    return silk_SMLAWB(a, b, c) ^ silk_SMLAWT(a, b, c);
                }" TUYAOPEN_OPUS_HAVE_CORTEX_M3_MUL)
            if (NOT TUYAOPEN_OPUS_HAVE_CORTEX_M3_MUL)
                message(FATAL_ERROR
                    "Selected compiler cannot build Cortex-M3 Opus multiplication; disable CONFIG_ENABLE_TUYAOPEN_OPUS_CORTEX_M3_MUL")
            endif()
        endfunction()
        tuyaopen_opus_check_cortex_m3_mul()
        set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${OPUS_M3_HEADER}")
        target_compile_options(tuyaopen_opus PRIVATE "SHELL:-include \"${OPUS_M3_HEADER}\"")
    endif()

    if (MSVC)
        target_compile_definitions(tuyaopen_opus PRIVATE USE_ALLOCA)
        target_compile_options(tuyaopen_opus PRIVATE /O2)
    else()
        target_compile_features(tuyaopen_opus PRIVATE c_std_99)
        target_compile_definitions(tuyaopen_opus PRIVATE VAR_ARRAYS)
        if (CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
            target_compile_options(tuyaopen_opus PRIVATE -O3)
        endif()
        if (CMAKE_C_COMPILER_ID STREQUAL "GNU" AND CONFIG_PLATFORM_CHOICE STREQUAL "L511G_Y7PM")
            # L511G_Y7PM globally disables inlining. Re-enable it only for Opus so
            # its per-sample prediction/arithmetic helpers can be optimized.
            target_compile_options(tuyaopen_opus PRIVATE -finline)
        endif()
    endif()

    # SDK platform builds consume component objects through the tuyaos archive.
    list(APPEND COMPONENT_LIBS tuyaopen_opus)
    set(COMPONENT_LIBS "${COMPONENT_LIBS}" PARENT_SCOPE)
    list(APPEND COMPONENT_PUBINC "${TUYAOPEN_OPUS_DIR}/include")
    set(COMPONENT_PUBINC "${COMPONENT_PUBINC}" PARENT_SCOPE)
endif()

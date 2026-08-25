##
# @file CMakeLists.txt
# @brief Host-side (non-embedded) test suite for tal_image's dependency-free
# modules. Not part of the embedded build -- run directly with plain gcc/cmake
# on the dev machine:
#
#   cmake -S src/tal_image/test -B src/tal_image/test/.build
#   cmake --build src/tal_image/test/.build
#   ctest --test-dir src/tal_image/test/.build --output-on-failure
#/
cmake_minimum_required(VERSION 3.10)
project(tal_image_host_tests C)

enable_testing()

set(CMAKE_C_STANDARD 99)
set(TAL_IMAGE_SRC ${CMAKE_CURRENT_LIST_DIR}/../src)
set(TAL_IMAGE_INC ${CMAKE_CURRENT_LIST_DIR}/../include)

add_executable(test_dither_core
    test_dither_core.c
    ${TAL_IMAGE_SRC}/tal_image_dither_core.c
)
target_include_directories(test_dither_core PRIVATE ${TAL_IMAGE_INC})
add_test(NAME test_dither_core COMMAND test_dither_core)

# test_legacy_parity.c's legacy oracle transcribes the pre-unification gamma
# LUT with powf() directly (that's what it's proving the new code matches),
# so this target alone needs libm even though tal_image_dither_core.c itself
# no longer does.
add_executable(test_legacy_parity
    test_legacy_parity.c
    ${TAL_IMAGE_SRC}/tal_image_dither_core.c
)
target_include_directories(test_legacy_parity PRIVATE ${TAL_IMAGE_INC})
target_link_libraries(test_legacy_parity m)
add_test(NAME test_legacy_parity COMMAND test_legacy_parity)

add_executable(test_jpeg_noise_clamp
    test_jpeg_noise_clamp.c
    ${TAL_IMAGE_SRC}/tal_image_jpeg_noise_clamp.c
)
target_include_directories(test_jpeg_noise_clamp PRIVATE ${TAL_IMAGE_INC})
add_test(NAME test_jpeg_noise_clamp COMMAND test_jpeg_noise_clamp)

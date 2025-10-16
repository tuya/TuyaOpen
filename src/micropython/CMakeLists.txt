##
# @file CMakeLists.txt
# @brief MicroPython component for TuyaOpen
#/

if (CONFIG_ENABLE_MICROPYTHON STREQUAL "y")

# MODULE_PATH
set(MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR})

# MODULE_NAME
get_filename_component(MODULE_NAME ${MODULE_PATH} NAME)

# MicroPython directories
set(MPY_DIR ${MODULE_PATH}/mpy)
set(MPY_PY_DIR ${MPY_DIR}/py)
set(MPY_SHARED_DIR ${MPY_DIR}/shared)
set(MPY_EXTMOD_DIR ${MPY_DIR}/extmod)
set(MPY_LIB_DIR ${MPY_DIR}/lib)
set(GENHDR_INC ${MODULE_PATH}/genhdr)
set(GENHDR_DIR ${GENHDR_INC}/genhdr)

# Create generated header directory
file(MAKE_DIRECTORY ${GENHDR_DIR})

########################################
# Minimal Core Sources Selection
########################################
# Based on minimal port and py.mk

# Core VM sources (minimal set)
set(PY_CORE_SRCS
    # Memory management
    ${MPY_PY_DIR}/gc.c
    ${MPY_PY_DIR}/pystack.c
    ${MPY_PY_DIR}/qstr.c
    ${MPY_PY_DIR}/vstr.c
    ${MPY_PY_DIR}/mpprint.c  # Needed for print functions, mp_plat_print is in mpprint_port.c
    ${MPY_PY_DIR}/unicode.c
    ${MPY_PY_DIR}/mpz.c
    ${MPY_PY_DIR}/malloc.c
    ${MPY_PY_DIR}/mpstate.c

    # Lexer and parser
    ${MPY_PY_DIR}/reader.c
    ${MPY_PY_DIR}/lexer.c
    ${MPY_PY_DIR}/parse.c
    ${MPY_PY_DIR}/scope.c

    # Compiler
    ${MPY_PY_DIR}/compile.c
    ${MPY_PY_DIR}/emitcommon.c
    ${MPY_PY_DIR}/emitbc.c
    ${MPY_PY_DIR}/asmbase.c
    ${MPY_PY_DIR}/emitglue.c
    ${MPY_PY_DIR}/persistentcode.c

    # Runtime
    ${MPY_PY_DIR}/runtime.c
    ${MPY_PY_DIR}/runtime_utils.c
    ${MPY_PY_DIR}/scheduler.c
    ${MPY_PY_DIR}/nativeglue.c
    ${MPY_PY_DIR}/pairheap.c
    ${MPY_PY_DIR}/ringbuf.c
    ${MPY_PY_DIR}/stackctrl.c
    ${MPY_PY_DIR}/argcheck.c
    ${MPY_PY_DIR}/warning.c
    ${MPY_PY_DIR}/profile.c
    ${MPY_PY_DIR}/map.c

    # VM
    ${MPY_PY_DIR}/vm.c
    ${MPY_PY_DIR}/bc.c
    ${MPY_PY_DIR}/showbc.c
    ${MPY_PY_DIR}/repl.c
    ${MPY_PY_DIR}/smallint.c
    ${MPY_PY_DIR}/frozenmod.c

    # Objects (minimal set)
    ${MPY_PY_DIR}/obj.c
    ${MPY_PY_DIR}/objarray.c
    ${MPY_PY_DIR}/objbool.c
    ${MPY_PY_DIR}/objboundmeth.c
    ${MPY_PY_DIR}/objcell.c
    ${MPY_PY_DIR}/objclosure.c
    ${MPY_PY_DIR}/objdict.c
    ${MPY_PY_DIR}/objexcept.c
    ${MPY_PY_DIR}/objfun.c
    ${MPY_PY_DIR}/objgenerator.c
    ${MPY_PY_DIR}/objint.c
    ${MPY_PY_DIR}/objint_longlong.c
    ${MPY_PY_DIR}/objint_mpz.c
    ${MPY_PY_DIR}/objlist.c
    ${MPY_PY_DIR}/objmodule.c
    ${MPY_PY_DIR}/objobject.c
    ${MPY_PY_DIR}/objnone.c
    ${MPY_PY_DIR}/objrange.c
    ${MPY_PY_DIR}/objset.c
    ${MPY_PY_DIR}/objsingleton.c
    ${MPY_PY_DIR}/objslice.c
    ${MPY_PY_DIR}/objstr.c
    ${MPY_PY_DIR}/objstrunicode.c
    ${MPY_PY_DIR}/objstringio.c
    ${MPY_PY_DIR}/objtuple.c
    ${MPY_PY_DIR}/objtype.c
    ${MPY_PY_DIR}/objzip.c
    ${MPY_PY_DIR}/opmethods.c
    ${MPY_PY_DIR}/sequence.c
    ${MPY_PY_DIR}/stream.c
    ${MPY_PY_DIR}/binary.c
    ${MPY_PY_DIR}/builtinimport.c
    ${MPY_PY_DIR}/builtinevex.c
    ${MPY_PY_DIR}/builtinhelp.c

    # Iterator support (needed for CORE_FEATURES)
    ${MPY_PY_DIR}/objgetitemiter.c
    ${MPY_PY_DIR}/objpolyiter.c
    ${MPY_PY_DIR}/objmap.c
    ${MPY_PY_DIR}/objenumerate.c
    ${MPY_PY_DIR}/objfilter.c
    ${MPY_PY_DIR}/objreversed.c

    # Minimal modules - Only include enabled ones to avoid link errors
    # ${MPY_PY_DIR}/modarray.c        # Disabled - MICROPY_PY_ARRAY = 0
    ${MPY_PY_DIR}/modbuiltins.c      # Always needed
    # ${MPY_PY_DIR}/modcollections.c  # Disabled - MICROPY_PY_COLLECTIONS = 0
    ${MPY_PY_DIR}/modgc.c            # Enabled - MICROPY_PY_GC = 1
    # ${MPY_PY_DIR}/modio.c           # Disabled - MICROPY_PY_IO = 0
    # ${MPY_PY_DIR}/modmicropython.c  # Disabled - MICROPY_PY_MICROPYTHON = 0
    # ${MPY_PY_DIR}/modstruct.c       # Disabled - MICROPY_PY_STRUCT = 0
    # ${MPY_PY_DIR}/modsys.c          # Disabled - MICROPY_PY_SYS = 0
    ${MPY_PY_DIR}/moderrno.c         # Keep for basic error handling

    # Number parsing
    ${MPY_PY_DIR}/parsenumbase.c
    ${MPY_PY_DIR}/parsenum.c

    # NLR (exception handling) - platform specific
    ${MPY_PY_DIR}/nlr.c
    # For ARM Cortex-M (T5AI uses Cortex-M33)
    ${MPY_PY_DIR}/nlrthumb.c
)

# Optional: Math support (can be enabled later)
# list(APPEND PY_CORE_SRCS ${MPY_PY_DIR}/modmath.c)
# list(APPEND PY_CORE_SRCS ${MPY_PY_DIR}/objfloat.c)
# list(APPEND PY_CORE_SRCS ${MPY_PY_DIR}/formatfloat.c)

# Shared components
set(SHARED_SRCS
    ${MPY_SHARED_DIR}/readline/readline.c  # Required for REPL
    ${MPY_SHARED_DIR}/runtime/pyexec.c
    ${MPY_SHARED_DIR}/runtime/stdout_helpers.c
    ${MPY_SHARED_DIR}/runtime/sys_stdio_mphal.c
    ${MPY_SHARED_DIR}/runtime/interrupt_char.c
)

# Port specific sources
set(PORT_SRCS
    ${MODULE_PATH}/port/t5ai/main.c
    ${MODULE_PATH}/port/t5ai/mphalport.c
    ${MODULE_PATH}/port/t5ai/port_misc.c  # Misc port functions (gc_collect, nlr_jump_fail, etc.)
)

# Combine all sources
set(LIB_SRCS
    ${PY_CORE_SRCS}
    ${SHARED_SRCS}
    ${PORT_SRCS}
)

# Sources that need QSTR scanning
set(SRC_QSTR
    ${LIB_SRCS}
)

# Include pre-compilation preparation (QSTR generation, etc.)
# This must be after LIB_SRCS is defined
set(PORT_DIR ${MODULE_PATH}/port/t5ai)
include(${CMAKE_CURRENT_SOURCE_DIR}/mpy_prepare.cmake)

# LIB_PUBLIC_INC
set(LIB_PUBLIC_INC ${MODULE_PATH}/include)

# LIB_PRIVATE_INC
set(LIB_PRIVATE_INC
    ${MODULE_PATH}/port/t5ai
    ${MPY_DIR}
    ${MPY_PY_DIR}
    ${GENHDR_INC}
    # ${CMAKE_BINARY_DIR}  # For generated headers
)

# Compile definitions
add_definitions(-DMICROPYTHON_ENABLE=1)
add_definitions(-DMICROPYTHON_T5AI=1)

# MicroPython configuration
# These are now defined in mpconfigport.h to avoid conflicts
# add_definitions(-DMICROPY_CONFIG_ROM_LEVEL=MICROPY_CONFIG_ROM_LEVEL_MINIMUM)
# add_definitions(-DMICROPY_ENABLE_GC=1)
# add_definitions(-DMICROPY_HELPER_REPL=1)
# add_definitions(-DMICROPY_ENABLE_COMPILER=1)
# add_definitions(-DMICROPY_NLR_THUMB=1)  # For ARM Cortex-M
# add_definitions(-DMICROPY_LONGINT_IMPL=MICROPY_LONGINT_IMPL_MPZ)

########################################
# Target Configure
########################################
add_library(${MODULE_NAME})

target_sources(${MODULE_NAME}
    PRIVATE
        ${LIB_SRCS}
    )

target_include_directories(${MODULE_NAME}
    PRIVATE
        ${LIB_PRIVATE_INC}

    PUBLIC
        ${LIB_PUBLIC_INC}
    )

# Add dependency on generated headers
# This ensures all headers are generated before compilation
add_dependencies(${MODULE_NAME} mpy_generated_headers)


########################################
# Layer Configure
########################################
list(APPEND COMPONENT_LIBS ${MODULE_NAME})
set(COMPONENT_LIBS "${COMPONENT_LIBS}" PARENT_SCOPE)
list(APPEND COMPONENT_PUBINC ${LIB_PUBLIC_INC})
set(COMPONENT_PUBINC "${COMPONENT_PUBINC}" PARENT_SCOPE)

endif()

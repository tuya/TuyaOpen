/*
 * MicroPython Configuration for T5AI (BK7258)
 * Minimal configuration for initial build
 */

#ifndef MICROPYTHON_MPCONFIGPORT_H
#define MICROPYTHON_MPCONFIGPORT_H

/* Include necessary system headers */
#include <stdint.h>  /* For intptr_t, uintptr_t types */
#include <alloca.h>  /* For alloca() function */

/* MicroPython version and build information */
#define MICROPY_HW_BOARD_NAME       "TuyaOpen-T5AI"
#define MICROPY_HW_MCU_NAME         "BK7258"
#define MICROPY_PY_SYS_PLATFORM     "T5AI"

/* Python language features - minimal set */
#define MICROPY_ENABLE_COMPILER     (1)  /* Enable compiler for minimal functionality */
#define MICROPY_ENABLE_GC           (1)  /* Enable GC for memory management */
#define MICROPY_HELPER_REPL         (1)  /* Enable REPL for testing */
#define MICROPY_MODULE_FROZEN_MPY   (0)
#define MICROPY_QSTR_BYTES_IN_HASH  (2)  /* Use 2 bytes for hash to avoid overflow */

/* Core Python features - disabled for minimal build */
#define MICROPY_PY_BUILTINS_FLOAT   (0)
#define MICROPY_PY_BUILTINS_COMPLEX (0)
#define MICROPY_PY_BUILTINS_DICT    (0)
#define MICROPY_PY_BUILTINS_SET     (0)
#define MICROPY_PY_BUILTINS_FROZENSET (0)
#define MICROPY_PY_BUILTINS_MEMORYVIEW (0)
#define MICROPY_PY_BUILTINS_SLICE   (0)
#define MICROPY_PY_BUILTINS_PROPERTY (0)

/* Memory allocation - will be replaced with real implementation */
#define MICROPY_ALLOC_PATH_MAX      (128)

/* Error reporting */
#define MICROPY_ERROR_REPORTING     (MICROPY_ERROR_REPORTING_TERSE)

/* Optimizations */
#define MICROPY_OPT_COMPUTED_GOTO   (0)
#define MICROPY_OPT_MPZ_BITWISE     (0)

/* Platform specific - ARM Cortex-M33 */
#define MICROPY_NLR_THUMB           (1)
#define MICROPY_LONGINT_IMPL        (MICROPY_LONGINT_IMPL_MPZ)

/* Use core features configuration level for basic Python functionality */
#define MICROPY_CONFIG_ROM_LEVEL    (MICROPY_CONFIG_ROM_LEVEL_CORE_FEATURES)

/* Enable module registration (needed for MP_REGISTER_MODULE to work) */
#define MICROPY_MODULE_BUILTIN_INIT      (1)  /* Enable built-in module initialization */

/* Type definitions for T5AI */
typedef intptr_t mp_int_t;
typedef uintptr_t mp_uint_t;
typedef long mp_off_t;

/* We need to provide a type for the GC heap */
#define MICROPY_GCREGS_SETJMP       (0)

/* Minimal heap size for testing */
#define MICROPY_HEAP_SIZE           (8 * 1024)

/* Readline configuration */
#define MICROPY_PY_SYS_STDFILES     (0)
#define MICROPY_HELPER_READLINE     (1)  /* Enable readline for REPL */
#define MICROPY_HELPER_REPL         (1)  /* Enable REPL helper */
#define MICROPY_READLINE_HISTORY_SIZE (8)  /* Enable readline history with 8 entries */

/* Define global readline history array */
extern const char *readline_hist[8];

/* Define MP_STATE_PORT macro to access global variables */
#define MP_STATE_PORT(x) (x)

/* Port-specific state structure - empty for minimal build */
#define MICROPY_PORT_ROOT_POINTERS

/* Include HAL header */
#include "mphalport.h"

/* Use our custom print implementation */
#define MP_PLAT_PRINT_STRN(str, len) mp_hal_stdout_tx_strn_cooked(str, len)

/* Tell MicroPython we have our own mp_plat_print */
extern const struct _mp_print_t mp_plat_print;

/* Define some basic functions as macros for minimal build */
#define mp_type_print(x, y, z)
#define mp_type_make_new(x, y, z, w)

/* Miscellaneous settings */
#define MICROPY_KBD_EXCEPTION       (0)
#define MICROPY_HELPER_LEXER_UNIX   (0)
#define MICROPY_ENABLE_SOURCE_LINE  (0)
#define MICROPY_STREAMS_NON_BLOCK   (0)
#define MICROPY_MODULE_WEAK_LINKS   (0)
#define MICROPY_CAN_OVERRIDE_BUILTINS (0)
#define MICROPY_USE_INTERNAL_ERRNO  (0)
#define MICROPY_ENABLE_SCHEDULER    (0)

/* Module configuration - Only enable gc for Milestone 2 */
#define MICROPY_PY___FILE__         (0)
#define MICROPY_PY_GC               (1)  /* Enable gc module for memory management verification */
#define MICROPY_PY_MICROPYTHON      (0)  /* Disable for now - next milestone */
#define MICROPY_PY_ARRAY            (0)  /* Disable for now - next milestone */
#define MICROPY_PY_COLLECTIONS      (0)  /* Disable for now - next milestone */
#define MICROPY_PY_MATH             (0)  /* Disable for now - next milestone */
#define MICROPY_PY_IO               (0)  /* Disable for now - next milestone */
#define MICROPY_PY_STRUCT           (0)  /* Disable for now - next milestone */
#define MICROPY_PY_SYS              (0)  /* Disable for now - next milestone */
#define MICROPY_PY_UTIME            (0)

/* Machine module configuration (disabled for now) */
#define MICROPY_PY_MACHINE          (0)

/* Network module configuration (disabled for now) */
#define MICROPY_PY_NETWORK          (0)

/* Tuya module configuration (disabled for now) */
#define MICROPY_PY_TUYA             (0)

#endif /* MICROPYTHON_MPCONFIGPORT_H */

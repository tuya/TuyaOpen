/*
 * Minimal modplatform.h for T5AI port
 * This file provides platform information macros
 */

#ifndef MICROPY_INCLUDED_EXTMOD_MODPLATFORM_H
#define MICROPY_INCLUDED_EXTMOD_MODPLATFORM_H

// Platform name macros - these will be used by sys.platform
#ifndef MICROPY_PY_SYS_PLATFORM
#define MICROPY_PY_SYS_PLATFORM "T5AI"
#endif

#ifndef MICROPY_PLATFORM_ARCH
#define MICROPY_PLATFORM_ARCH "arm"
#endif

#endif // MICROPY_INCLUDED_EXTMOD_MODPLATFORM_H
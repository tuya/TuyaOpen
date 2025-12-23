#ifndef PLATFORM_COMPAT_H
#define PLATFORM_COMPAT_H

// Platform compatibility layer for embedded systems

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include "tkl_system.h"
#include "tal_memory.h"

// Memory allocation macros - use PSRAM for libnfc
#define malloc(size)       tal_psram_malloc(size)
#define free(ptr)          tal_psram_free(ptr)
#define calloc(n, size)    tal_psram_calloc(n, size)
#define realloc(ptr, size) tal_psram_realloc(ptr, size)

// Sleep function (usleep uses microseconds, we convert to milliseconds)
// Only define if not already defined by system headers
#ifndef usleep
#define usleep(us) tkl_system_sleep(((us) + 999) / 1000)
#endif

// Signal handling (dummy implementation for embedded)
#define SIGINT 2
typedef void (*sig_t)(int);
static inline sig_t signal(int sig, sig_t func)
{
    (void)sig;
    (void)func;
    return NULL;
}

// errno compatibility
#ifndef ENOSPC
#define ENOSPC 28
#endif

#ifndef ECONNABORTED
#define ECONNABORTED 103
#endif

#ifndef ENOTSUP
#define ENOTSUP 95
#endif

// Declare pipe function for embedded (will be defined in a .c file to avoid inline issues)
#ifndef pipe
int pipe(int pipefd[2]);
#endif

#endif // PLATFORM_COMPAT_H

#ifndef ERR_H
#define ERR_H

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include "tal_log.h"

// Simple implementation of BSD err.h functions for embedded systems
static inline void errx(int eval, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    char buffer[256];
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);
    PR_ERR("%s", buffer);
    // Don't actually exit in embedded system
    // exit(eval);
}

static inline void err(int eval, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    char buffer[256];
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);
    PR_ERR("%s", buffer);
    // Don't actually exit in embedded system
    // exit(eval);
}

static inline void warnx(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    char buffer[256];
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);
    PR_WARN("%s", buffer);
}

#endif // ERR_H

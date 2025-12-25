/*-
 * Free/Libre Near Field Communication (NFC) library
 *
 * Libnfc historical contributors:
 * Copyright (C) 2009      Roel Verdult
 * Copyright (C) 2009-2013 Romuald Conty
 * Copyright (C) 2010-2012 Romain Tartière
 * Copyright (C) 2010-2013 Philippe Teuwen
 * Copyright (C) 2012-2013 Ludovic Rousseau
 * See AUTHORS file for a more comprehensive list of contributors.
 * Additional contributors of this file:
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>
 */

#include "log.h"
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdarg.h>
#include <fcntl.h>

// Tuya platform log
#include "tal_log.h"

const char *log_priority_to_str(const int priority)
{
    switch (priority) {
    case NFC_LOG_PRIORITY_ERROR:
        return "error";
    case NFC_LOG_PRIORITY_INFO:
        return "info";
    case NFC_LOG_PRIORITY_DEBUG:
        return "debug";
    default:
        break;
    }
    return "unknown";
}

#ifdef LOG

void log_init(const nfc_context *context)
{
    (void)context;
    // Tuya log system is initialized by the platform
}

void log_exit(void)
{
    // Nothing to do for Tuya platform
}

void log_put(const uint8_t group, const char *category, const uint8_t priority, const char *format, ...)
{
    (void)group;

    char    buffer[256];
    va_list va;
    va_start(va, format);
    vsnprintf(buffer, sizeof(buffer), format, va);
    va_end(va);

    // Map NFC log priority to Tuya PR_xxx macros
    switch (priority) {
    case NFC_LOG_PRIORITY_ERROR:
        PR_ERR("[%s] %s", category, buffer);
        break;
    case NFC_LOG_PRIORITY_INFO:
        PR_INFO("[%s] %s", category, buffer);
        break;
    case NFC_LOG_PRIORITY_DEBUG:
    default:
#ifdef NFC_DEBUG
        PR_DEBUG("[%s] %s", category, buffer);
#endif
        break;
    }
}

#endif // LOG

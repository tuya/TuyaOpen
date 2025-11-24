/**
 * @file ai_log.c
 * @brief Implementation of AI log screen functions
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 *
 */
#include "ai_log.h"
#include <string.h>

#include "tal_api.h"

static void compute_next(const char *p, int *next)
{
    int m = (int)strlen(p);
    next[0] = 0;
    for (int i = 1, len = 0; i < m; ) {
        if (p[i] == p[len]) {
            ++len;
            next[i++] = len;
        } else if (len > 0) {
            len = next[len - 1];
        } else {
            next[i++] = 0;
        }
    }
}

int kmp_search(const char *s, const char *p)
{
    int n = (int)strlen(s);
    int m = (int)strlen(p);
    if (m == 0) return -1;
    if (n < m) return -1;

    int *next = tal_psram_malloc(m * sizeof(int));
    if (!next) return -1;
    compute_next(p, next);

    int i = 0, j = 0;
    while (i < n) {
        if (s[i] == p[j]) {
            ++i; ++j;
            if (j == m) {
                free(next);
                return i - j;
            }
        } else if (j > 0) {
            j = next[j - 1];
        } else {
            ++i;
        }
    }
    tal_psram_free(next);
    return -1;
}
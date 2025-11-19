/**
 * @file ai_log.h
 * @brief KMP substring search function declaration
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 *
 */
#ifndef __KMP_SEARCH_H__
#define __KMP_SEARCH_H__

#ifdef __cplusplus
extern "C" {
#endif

/*
* @brief Perform KMP substring search
* @param s The main string
* @param p The pattern string to search for
* @return The starting index of the first occurrence of p in s, or -1 if not found
*/  

int kmp_search(const char *s, const char *p);

#ifdef __cplusplus
}
#endif

#endif /*__KMP_SEARCH_H__*/
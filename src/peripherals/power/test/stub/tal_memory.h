/* host stub */
#ifndef __STUB_TAL_MEMORY_H__
#define __STUB_TAL_MEMORY_H__
#include <stdlib.h>
static inline void *tal_malloc(size_t s) { return malloc(s); }
static inline void  tal_free(void *p)    { free(p); }
#endif

/*
 * Port-specific miscellaneous functions for T5AI
 */

#include <stdio.h>
#include "py/runtime.h"
#include "py/gc.h"
#include "py/lexer.h"
#include "py/mperrno.h"
#include "py/builtin.h"  // For mp_import_stat_t
#include "tal_log.h"

/*
 * Garbage Collection
 */
void gc_collect(void) {
    // Start garbage collection
    gc_collect_start();

    // Collect root pointers from the stack
    // This is a simplified implementation
    // In a real implementation, you would scan the stack for pointers
    void *dummy;
    gc_collect_root(&dummy, ((mp_uint_t)MP_STATE_THREAD(stack_top) - (mp_uint_t)&dummy) / sizeof(mp_uint_t));

    // End garbage collection
    gc_collect_end();
}

/*
 * NLR (Non-Local Return) jump failure handler
 * This is called when an exception is not caught
 */
void nlr_jump_fail(void *val) {
    PR_ERR("FATAL: uncaught NLR %p", val);
    // In a real implementation, you might want to reset the system
    // For now, just enter an infinite loop
    for (;;) {
        __asm__("nop");
    }
}

/*
 * Create a lexer from a file
 * For now, we don't support file operations, so return NULL
 */
mp_lexer_t *mp_lexer_new_from_file(qstr filename) {
    (void)filename;
    mp_raise_OSError(MP_ENOENT);  // File not found
    return NULL;
}

/*
 * Import stat for modules (optional, but good to have)
 */
mp_import_stat_t mp_import_stat(const char *path) {
    (void)path;
    return MP_IMPORT_STAT_NO_EXIST;
}

/*
 * Fatal error handler
 */
void MP_WEAK __assert_func(const char *file, int line, const char *func, const char *expr) {
    PR_ERR("Assertion '%s' failed, at file %s:%d", expr, file, line);
    (void)func;
    for (;;) {
        __asm__("nop");
    }
}
/*
 * MicroPython main entry for T5AI (BK7258)
 * Minimal implementation for compilation test
 */

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "tal_log.h"
#include "tal_system.h"
#include "tal_thread.h"
#include "tal_uart.h"

/* MicroPython includes */
#include "py/compile.h"
#include "py/runtime.h"
#include "py/gc.h"
#include "py/mperrno.h"
#include "py/stackctrl.h"
#include "py/mphal.h"
#include "py/nlr.h"
#include "py/lexer.h"
#include "py/parse.h"
#include "shared/runtime/pyexec.h"

#ifdef ENABLE_MICROPYTHON

/* UART configuration for REPL */
extern TUYA_UART_NUM_E sg_repl_uart_num;

/* MicroPython task priority and stack size */
#define MP_TASK_PRIORITY    5
#define MP_TASK_STACK_SIZE  (8 * 1024)

/* MicroPython heap configuration */
#define MP_HEAP_SIZE        (32 * 1024)  /* 32KB heap for MicroPython GC */

/* MicroPython main task handle */
static THREAD_HANDLE sg_mp_thread = NULL;

/* Static heap for MicroPython GC */
static char mp_heap[MP_HEAP_SIZE] __attribute__((aligned(4)));

/**
 * @brief Execute a Python string
 * @param str Python code string to execute
 * @return 0 on success, -1 on error
 */
static int do_str(const char *str) {
    nlr_buf_t nlr;
    if (nlr_push(&nlr) == 0) {
        /* Parse and compile the Python code */
        mp_lexer_t *lex = mp_lexer_new_from_str_len(MP_QSTR__lt_stdin_gt_,
                                                     str, strlen(str), 0);
        qstr source_name = lex->source_name;
        mp_parse_tree_t parse_tree = mp_parse(lex, MP_PARSE_FILE_INPUT);

        /* Compile and execute */
        mp_obj_t module_fun = mp_compile(&parse_tree, source_name, true);
        mp_call_function_0(module_fun);

        nlr_pop();
        return 0;
    } else {
        /* Exception occurred */
        mp_obj_print_exception(&mp_plat_print, (mp_obj_t)nlr.ret_val);
        return -1;
    }
}

/**
 * @brief MicroPython main task
 */
static void micropython_task(void *arg)
{
    PR_NOTICE("MicroPython task started");

    /* Initialize HAL first */
    if (mp_hal_init() != 0) {
        PR_ERR("Failed to initialize HAL");
        return;
    }

    /* Initialize stack limit */
    mp_stack_ctrl_init();
    mp_stack_set_limit(MP_TASK_STACK_SIZE - 1024);

    /* Initialize MicroPython heap and GC */
    gc_init(mp_heap, mp_heap + MP_HEAP_SIZE);

    /* Initialize MicroPython runtime */
    mp_init();

    PR_NOTICE("MicroPython initialized, heap size: %d bytes", MP_HEAP_SIZE);

    /* Test basic Python execution */
    PR_NOTICE("Testing Python execution...");
    do_str("print('do_str() -> Hello from MicroPython on T5AI!')");

    /* Initialize REPL */
    PR_NOTICE("Starting MicroPython REPL...");

    /* Main REPL loop */
    for (;;) {
        if (pyexec_friendly_repl() != 0) {
            PR_DEBUG("REPL finished, restarting...");
        }
    }
}

/**
 * @brief Initialize MicroPython component
 * @return 0 on success, negative on error
 */
int micropython_init(void)
{
    OPERATE_RET rt = OPRT_OK;

    PR_NOTICE("Initializing MicroPython for T5AI...");

    /* Create MicroPython main thread */
    THREAD_CFG_T thread_cfg = {
        .priority = MP_TASK_PRIORITY,
        .stackDepth = MP_TASK_STACK_SIZE,
        .thrdname = "micropython"
    };

    rt = tal_thread_create_and_start(&sg_mp_thread,
                                      NULL,
                                      NULL,
                                      micropython_task,
                                      NULL,
                                      &thread_cfg);

    if (rt != OPRT_OK) {
        PR_ERR("Failed to create MicroPython thread: %d", rt);
        return -1;
    }

    PR_NOTICE("MicroPython initialized successfully");
    return 0;
}

/**
 * @brief Deinitialize MicroPython component
 */
void micropython_deinit(void)
{
    PR_NOTICE("Deinitializing MicroPython...");

    if (sg_mp_thread) {
        tal_thread_delete(sg_mp_thread);
        sg_mp_thread = NULL;
    }

    /* TODO: Clean up MicroPython resources */

    PR_NOTICE("MicroPython deinitialized");
}

#else /* ENABLE_MICROPYTHON */

int micropython_init(void)
{
    PR_NOTICE("MicroPython is disabled in configuration");
    return 0;
}

void micropython_deinit(void)
{
    /* Nothing to do */
}

#endif /* ENABLE_MICROPYTHON */

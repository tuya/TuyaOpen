/**
 * @file os_core_tuyaos.c
 * @brief pjlib OS core via TAL/TKL — unified for LINUX host and MCU
 * @version 1.1
 * @date 2026-08-06
 * @copyright Copyright (c) Tuya Inc. / based on pjlib os_core_unix
 *
 * One implementation for all platforms. LINUX: TAL → platform/LINUX TKL → POSIX.
 * MCU: TAL → platform TKL → RTOS.
 */
#include <pj/os.h>
#include <pj/assert.h>
#include <pj/pool.h>
#include <pj/log.h>
#include <pj/rand.h>
#include <pj/string.h>
#include <pj/guid.h>
#include <pj/except.h>
#include <pj/errno.h>
#include <pj/config.h>

#include "tal_mutex.h"
#include "tal_semaphore.h"
#include "tal_thread.h"
#include "tal_system.h"
#include "tkl_thread.h"
#include "tuya_cloud_types.h"

#include <errno.h>
#include <string.h>

#define THIS_FILE "os_core_tuyaos.c"
#define SIGNATURE1 0xDEAFBEEF
#define SIGNATURE2 0xDEADC0DE

#ifndef PJ_TUYAOS_THREAD_STACK
#define PJ_TUYAOS_THREAD_STACK (8 * 1024)
#endif
#ifndef PJ_TUYAOS_TLS_KEYS
#define PJ_TUYAOS_TLS_KEYS 32
#endif
#ifndef PJ_TUYAOS_TLS_ROWS
#define PJ_TUYAOS_TLS_ROWS 24
#endif

struct pj_thread_t {
    char obj_name[PJ_MAX_OBJ_NAME];
    THREAD_HANDLE thread;
    SEM_HANDLE join_sem;
    pj_thread_proc *proc;
    void *arg;
    pj_uint32_t signature1;
    pj_uint32_t signature2;
    pj_mutex_t *suspended_mutex;
    int joined;
};

struct pj_atomic_t {
    pj_mutex_t *mutex;
    pj_atomic_value_t value;
};

struct pj_mutex_t {
    MUTEX_HANDLE mutex;
    char obj_name[PJ_MAX_OBJ_NAME];
};

#if defined(PJ_HAS_SEMAPHORE) && PJ_HAS_SEMAPHORE != 0
struct pj_sem_t {
    SEM_HANDLE sem;
    char obj_name[PJ_MAX_OBJ_NAME];
};
#endif

typedef struct pj_tuyaos_tls_row {
    TKL_THREAD_HANDLE task; /* FreeRTOS task handle from tkl_thread_get_id() */
    int in_use;
    void *vals[PJ_TUYAOS_TLS_KEYS];
} pj_tuyaos_tls_row;

static int initialized;
static pj_thread_t main_thread;
static long thread_tls_id = -1;
static pj_mutex_t critical_section;
static unsigned atexit_count;
static void (*atexit_func[32])(void);

static pj_tuyaos_tls_row s_tls_rows[PJ_TUYAOS_TLS_ROWS];
static MUTEX_HANDLE s_tls_lock;
static int s_tls_key_used[PJ_TUYAOS_TLS_KEYS];
static pj_thread_t *s_current_hint;

static pj_status_t init_mutex(pj_mutex_t *mutex, const char *name, int type);
pj_status_t pj_thread_init(void);

static pj_status_t map_oprt(OPERATE_RET rt)
{
    return (rt == OPRT_OK) ? PJ_SUCCESS : PJ_EUNKNOWN;
}

/**
 * @brief Current OS task id (same identity OS pthread shim uses)
 * @return task handle, or NULL on failure
 */
static TKL_THREAD_HANDLE __tls_self_task(void)
{
    TKL_THREAD_HANDLE id = NULL;

    (void)tkl_thread_get_id(&id);
    return id;
}

/**
 * @brief Find or allocate TLS row for the calling OS task
 * @param[in] create allocate a free row when not found
 * @return row pointer or NULL
 * @note Match by tkl_thread_get_id() only. Never share a "main" row across tasks
 *       (that caused pj_thread_register2 on every sendto and heap thrashing).
 */
static pj_tuyaos_tls_row *__tls_row_current(int create)
{
    int i;
    TKL_THREAD_HANDLE self = __tls_self_task();

    if (s_tls_lock) {
        tal_mutex_lock(s_tls_lock);
    }
    for (i = 0; i < PJ_TUYAOS_TLS_ROWS; i++) {
        if (s_tls_rows[i].in_use && s_tls_rows[i].task == self) {
            if (s_tls_lock) {
                tal_mutex_unlock(s_tls_lock);
            }
            return &s_tls_rows[i];
        }
    }
    if (!create) {
        if (s_tls_lock) {
            tal_mutex_unlock(s_tls_lock);
        }
        return NULL;
    }
    for (i = 0; i < PJ_TUYAOS_TLS_ROWS; i++) {
        if (!s_tls_rows[i].in_use) {
            memset(&s_tls_rows[i], 0, sizeof(s_tls_rows[i]));
            s_tls_rows[i].in_use = 1;
            s_tls_rows[i].task = self;
            if (s_tls_lock) {
                tal_mutex_unlock(s_tls_lock);
            }
            return &s_tls_rows[i];
        }
    }
    if (s_tls_lock) {
        tal_mutex_unlock(s_tls_lock);
    }
    return NULL;
}

PJ_DEF(pj_status_t) pj_init(void)
{
    char dummy_guid[PJ_GUID_MAX_LENGTH];
    pj_str_t guid;
    pj_status_t rc;

    if (initialized) {
        ++initialized;
        return PJ_SUCCESS;
    }

    pj_log_init();

    if (tal_mutex_create_init(&s_tls_lock) != OPRT_OK) {
        return PJ_ENOMEM;
    }
    memset(s_tls_rows, 0, sizeof(s_tls_rows));
    memset(s_tls_key_used, 0, sizeof(s_tls_key_used));

#if PJ_HAS_THREADS
    rc = pj_thread_init();
    if (rc != PJ_SUCCESS) {
        return rc;
    }
    rc = init_mutex(&critical_section, "critsec", PJ_MUTEX_RECURSE);
    if (rc != PJ_SUCCESS) {
        return rc;
    }
#endif

    rc = pj_exception_id_alloc("PJLIB/No memory", &PJ_NO_MEMORY_EXCEPTION);
    if (rc != PJ_SUCCESS) {
        return rc;
    }

    guid.ptr = dummy_guid;
    pj_generate_unique_string(&guid);

#if defined(PJ_HAS_HIGH_RES_TIMER) && PJ_HAS_HIGH_RES_TIMER != 0
    {
        pj_timestamp dummy_ts;
        if ((rc = pj_get_timestamp(&dummy_ts)) != 0) {
            return rc;
        }
    }
#endif

    ++initialized;
    pj_assert(initialized == 1);
    PJ_LOG(4, (THIS_FILE, "pjlib %s for TuyaOS/TAL initialized", PJ_VERSION));
    return PJ_SUCCESS;
}

PJ_DEF(pj_status_t) pj_atexit(void (*func)(void))
{
    if (atexit_count >= PJ_ARRAY_SIZE(atexit_func)) {
        return PJ_ETOOMANY;
    }
    atexit_func[atexit_count++] = func;
    return PJ_SUCCESS;
}

PJ_DEF(void) pj_shutdown(void)
{
    int i;

    pj_assert(initialized > 0);
    if (--initialized != 0) {
        return;
    }

    for (i = (int)atexit_count - 1; i >= 0; --i) {
        (*atexit_func[i])();
    }
    atexit_count = 0;

    if (PJ_NO_MEMORY_EXCEPTION != -1) {
        pj_exception_id_free(PJ_NO_MEMORY_EXCEPTION);
        PJ_NO_MEMORY_EXCEPTION = -1;
    }

#if PJ_HAS_THREADS
    pj_mutex_destroy(&critical_section);
    if (thread_tls_id != -1) {
        pj_thread_local_free(thread_tls_id);
        thread_tls_id = -1;
    }
    pj_bzero(&main_thread, sizeof(main_thread));
#endif
    if (s_tls_lock) {
        tal_mutex_release(s_tls_lock);
        s_tls_lock = NULL;
    }
    pj_errno_clear_handlers();
}

PJ_DEF(pj_uint32_t) pj_getpid(void)
{
    PJ_CHECK_STACK();
    return 1;
}

PJ_DEF(pj_bool_t) pj_thread_is_registered(void)
{
#if PJ_HAS_THREADS
    return pj_thread_local_get(thread_tls_id) != 0;
#else
    return PJ_TRUE;
#endif
}

PJ_DEF(int) pj_thread_get_prio(pj_thread_t *thread)
{
    PJ_UNUSED_ARG(thread);
    return 0;
}

PJ_DEF(pj_status_t) pj_thread_set_prio(pj_thread_t *thread, int prio)
{
    PJ_UNUSED_ARG(thread);
    PJ_UNUSED_ARG(prio);
    return PJ_SUCCESS;
}

PJ_DEF(int) pj_thread_get_prio_min(pj_thread_t *thread)
{
    PJ_UNUSED_ARG(thread);
    return 0;
}

PJ_DEF(int) pj_thread_get_prio_max(pj_thread_t *thread)
{
    PJ_UNUSED_ARG(thread);
    return 0;
}

PJ_DEF(void *) pj_thread_get_os_handle(pj_thread_t *thread)
{
    PJ_ASSERT_RETURN(thread, NULL);
    return thread->thread;
}

PJ_DEF(pj_status_t) pj_thread_register(const char *cstr_thread_name, pj_thread_desc desc, pj_thread_t **ptr_thread)
{
#if PJ_HAS_THREADS
    pj_status_t rc;
    pj_thread_t *thread = (pj_thread_t *)desc;
    pj_str_t thread_name = pj_str((char *)cstr_thread_name);

    if (sizeof(pj_thread_desc) < sizeof(pj_thread_t)) {
        pj_assert(!"Not enough pj_thread_desc size!");
        return PJ_EBUG;
    }

    pj_bzero(desc, sizeof(struct pj_thread_t));
    thread->thread = NULL;
    thread->signature1 = SIGNATURE1;
    thread->signature2 = SIGNATURE2;

    if (cstr_thread_name && pj_strlen(&thread_name) < sizeof(thread->obj_name) - 1) {
        pj_ansi_snprintf(thread->obj_name, sizeof(thread->obj_name), "%s", cstr_thread_name);
    } else {
        pj_ansi_snprintf(thread->obj_name, sizeof(thread->obj_name), "thr%p", (void *)thread);
    }

    rc = pj_thread_local_set(thread_tls_id, thread);
    if (rc != PJ_SUCCESS) {
        pj_bzero(desc, sizeof(struct pj_thread_t));
        return rc;
    }
    s_current_hint = thread;
    *ptr_thread = thread;
    return PJ_SUCCESS;
#else
    PJ_UNUSED_ARG(cstr_thread_name);
    *ptr_thread = (pj_thread_t *)desc;
    return PJ_SUCCESS;
#endif
}

pj_status_t pj_thread_init(void)
{
#if PJ_HAS_THREADS
    pj_status_t rc;
    pj_thread_t *dummy;

    rc = pj_thread_local_alloc(&thread_tls_id);
    if (rc != PJ_SUCCESS) {
        return rc;
    }
    return pj_thread_register("thr%p", (long *)&main_thread, &dummy);
#else
    return PJ_EINVALIDOP;
#endif
}

#if PJ_HAS_THREADS
/**
 * @brief TAL thread trampoline for pj threads
 */
static VOID_T __pj_thread_entry(VOID_T *param)
{
    pj_thread_t *rec = (pj_thread_t *)param;

    (void)pj_thread_local_set(thread_tls_id, rec);
    s_current_hint = rec;

    if (rec->suspended_mutex) {
        pj_mutex_lock(rec->suspended_mutex);
        pj_mutex_unlock(rec->suspended_mutex);
    }

    PJ_LOG(6, (rec->obj_name, "Thread started"));
    (void)(*rec->proc)(rec->arg);
    PJ_LOG(6, (rec->obj_name, "Thread quitting"));

    if (rec->join_sem) {
        tal_semaphore_post(rec->join_sem);
    }
}
#endif

PJ_DEF(pj_status_t)
pj_thread_create(pj_pool_t *pool, const char *thread_name, pj_thread_proc *proc, void *arg, pj_size_t stack_size,
                 unsigned flags, pj_thread_t **ptr_thread)
{
#if PJ_HAS_THREADS
    pj_thread_t *rec;
    THREAD_CFG_T cfg;
    OPERATE_RET ort;
    pj_status_t rc;

    PJ_CHECK_STACK();
    PJ_ASSERT_RETURN(pool && proc && ptr_thread, PJ_EINVAL);

    rec = (struct pj_thread_t *)pj_pool_zalloc(pool, sizeof(pj_thread_t));
    PJ_ASSERT_RETURN(rec, PJ_ENOMEM);

    if (!thread_name) {
        thread_name = "thr%p";
    }
    if (strchr(thread_name, '%')) {
        pj_ansi_snprintf(rec->obj_name, PJ_MAX_OBJ_NAME, thread_name, rec);
    } else {
        strncpy(rec->obj_name, thread_name, PJ_MAX_OBJ_NAME);
        rec->obj_name[PJ_MAX_OBJ_NAME - 1] = '\0';
    }

    if (stack_size == 0) {
        stack_size = PJ_THREAD_DEFAULT_STACK_SIZE;
    }
    if (stack_size < PJ_TUYAOS_THREAD_STACK) {
        stack_size = PJ_TUYAOS_THREAD_STACK;
    }

    if (flags & PJ_THREAD_SUSPENDED) {
        rc = pj_mutex_create_simple(pool, NULL, &rec->suspended_mutex);
        if (rc != PJ_SUCCESS) {
            return rc;
        }
        pj_mutex_lock(rec->suspended_mutex);
    }

    if (tal_semaphore_create_init(&rec->join_sem, 0, 1) != OPRT_OK) {
        return PJ_ENOMEM;
    }

    rec->proc = proc;
    rec->arg = arg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.stackDepth = (uint32_t)stack_size;
    cfg.priority = THREAD_PRIO_2;
    cfg.thrdname = rec->obj_name;
#if defined(ENABLE_EXT_RAM) && (ENABLE_EXT_RAM == 1)
    /* Align OS tuya_p2p_lib_pthread_create → tkl_thread_create_in_psram */
    cfg.psram_mode = 1;
#endif

    ort = tal_thread_create_and_start(&rec->thread, NULL, NULL, __pj_thread_entry, rec, &cfg);
    if (ort != OPRT_OK) {
        tal_semaphore_release(rec->join_sem);
        rec->join_sem = NULL;
        return map_oprt(ort);
    }

    *ptr_thread = rec;
    PJ_LOG(6, (rec->obj_name, "Thread created"));
    return PJ_SUCCESS;
#else
    PJ_UNUSED_ARG(pool);
    PJ_UNUSED_ARG(thread_name);
    PJ_UNUSED_ARG(proc);
    PJ_UNUSED_ARG(arg);
    PJ_UNUSED_ARG(stack_size);
    PJ_UNUSED_ARG(flags);
    PJ_UNUSED_ARG(ptr_thread);
    return PJ_EINVALIDOP;
#endif
}

PJ_DEF(const char *) pj_thread_get_name(pj_thread_t *p)
{
#if PJ_HAS_THREADS
    PJ_CHECK_STACK();
    PJ_ASSERT_RETURN(p, "");
    return p->obj_name;
#else
    PJ_UNUSED_ARG(p);
    return "";
#endif
}

PJ_DEF(pj_status_t) pj_thread_resume(pj_thread_t *p)
{
#if PJ_HAS_THREADS
    PJ_ASSERT_RETURN(p, PJ_EINVAL);
    if (p->suspended_mutex) {
        return pj_mutex_unlock(p->suspended_mutex);
    }
    return PJ_SUCCESS;
#else
    PJ_UNUSED_ARG(p);
    return PJ_EINVALIDOP;
#endif
}

PJ_DEF(pj_thread_t *) pj_thread_this(void)
{
#if PJ_HAS_THREADS
    pj_thread_t *rec = (pj_thread_t *)pj_thread_local_get(thread_tls_id);
    if (rec == NULL) {
        rec = s_current_hint;
    }
    pj_assert(rec != NULL);
    return rec;
#else
    pj_assert(!"Not supported");
    return NULL;
#endif
}

PJ_DEF(pj_status_t) pj_thread_join(pj_thread_t *p)
{
#if PJ_HAS_THREADS
    PJ_CHECK_STACK();
    PJ_ASSERT_RETURN(p, PJ_EINVAL);
    if (p->joined) {
        return PJ_SUCCESS;
    }
    if (p->join_sem) {
        tal_semaphore_wait(p->join_sem, SEM_WAIT_FOREVER);
    }
    p->joined = 1;
    return PJ_SUCCESS;
#else
    PJ_UNUSED_ARG(p);
    return PJ_SUCCESS;
#endif
}

PJ_DEF(pj_status_t) pj_thread_destroy(pj_thread_t *p)
{
#if PJ_HAS_THREADS
    PJ_ASSERT_RETURN(p, PJ_EINVAL);
    if (!p->joined) {
        pj_thread_join(p);
    }
    if (p->join_sem) {
        tal_semaphore_release(p->join_sem);
        p->join_sem = NULL;
    }
    if (p->suspended_mutex) {
        pj_mutex_destroy(p->suspended_mutex);
        p->suspended_mutex = NULL;
    }
    /* THREAD_HANDLE lifetime owned by TAL after exit; do not delete forcibly */
    p->thread = NULL;
    return PJ_SUCCESS;
#else
    PJ_UNUSED_ARG(p);
    return PJ_SUCCESS;
#endif
}

PJ_DEF(pj_status_t) pj_thread_sleep(unsigned msec)
{
    PJ_CHECK_STACK();
    if (msec == 0) {
        msec = 1;
    }
    tal_system_sleep(msec);
    return PJ_SUCCESS;
}

#if defined(PJ_OS_HAS_CHECK_STACK) && PJ_OS_HAS_CHECK_STACK != 0
PJ_DEF(void) pj_thread_check_stack(const char *file, int line)
{
    PJ_UNUSED_ARG(file);
    PJ_UNUSED_ARG(line);
}

PJ_DEF(pj_uint32_t) pj_thread_get_stack_max_usage(pj_thread_t *thread)
{
    PJ_UNUSED_ARG(thread);
    return 0;
}

PJ_DEF(pj_status_t) pj_thread_get_stack_info(pj_thread_t *thread, const char **file, int *line)
{
    PJ_UNUSED_ARG(thread);
    if (file) {
        *file = "";
    }
    if (line) {
        *line = 0;
    }
    return PJ_SUCCESS;
}
#endif /* PJ_OS_HAS_CHECK_STACK */

PJ_DEF(pj_status_t) pj_atomic_create(pj_pool_t *pool, pj_atomic_value_t initial, pj_atomic_t **ptr_atomic)
{
    pj_status_t rc;
    pj_atomic_t *atomic_var;

    PJ_ASSERT_RETURN(pool && ptr_atomic, PJ_EINVAL);
    atomic_var = PJ_POOL_ALLOC_T(pool, pj_atomic_t);
    PJ_ASSERT_RETURN(atomic_var, PJ_ENOMEM);
    rc = pj_mutex_create_simple(pool, "atm%p", &atomic_var->mutex);
    if (rc != PJ_SUCCESS) {
        return rc;
    }
    atomic_var->value = initial;
    *ptr_atomic = atomic_var;
    return PJ_SUCCESS;
}

PJ_DEF(pj_status_t) pj_atomic_destroy(pj_atomic_t *atomic_var)
{
    PJ_ASSERT_RETURN(atomic_var, PJ_EINVAL);
    return pj_mutex_destroy(atomic_var->mutex);
}

PJ_DEF(void) pj_atomic_set(pj_atomic_t *atomic_var, pj_atomic_value_t value)
{
    PJ_ASSERT_ON_FAIL(atomic_var, return );
    pj_mutex_lock(atomic_var->mutex);
    atomic_var->value = value;
    pj_mutex_unlock(atomic_var->mutex);
}

PJ_DEF(pj_atomic_value_t) pj_atomic_get(pj_atomic_t *atomic_var)
{
    pj_atomic_value_t old_val;
    PJ_ASSERT_RETURN(atomic_var, 0);
    pj_mutex_lock(atomic_var->mutex);
    old_val = atomic_var->value;
    pj_mutex_unlock(atomic_var->mutex);
    return old_val;
}

PJ_DEF(pj_atomic_value_t) pj_atomic_inc_and_get(pj_atomic_t *atomic_var)
{
    pj_atomic_value_t new_val;
    PJ_ASSERT_RETURN(atomic_var, 0);
    pj_mutex_lock(atomic_var->mutex);
    new_val = ++atomic_var->value;
    pj_mutex_unlock(atomic_var->mutex);
    return new_val;
}

PJ_DEF(void) pj_atomic_inc(pj_atomic_t *atomic_var)
{
    pj_atomic_inc_and_get(atomic_var);
}

PJ_DEF(pj_atomic_value_t) pj_atomic_dec_and_get(pj_atomic_t *atomic_var)
{
    pj_atomic_value_t new_val;
    PJ_ASSERT_RETURN(atomic_var, 0);
    pj_mutex_lock(atomic_var->mutex);
    new_val = --atomic_var->value;
    pj_mutex_unlock(atomic_var->mutex);
    return new_val;
}

PJ_DEF(void) pj_atomic_dec(pj_atomic_t *atomic_var)
{
    pj_atomic_dec_and_get(atomic_var);
}

PJ_DEF(pj_atomic_value_t) pj_atomic_add_and_get(pj_atomic_t *atomic_var, pj_atomic_value_t value)
{
    pj_atomic_value_t new_val;
    PJ_ASSERT_RETURN(atomic_var, 0);
    pj_mutex_lock(atomic_var->mutex);
    atomic_var->value += value;
    new_val = atomic_var->value;
    pj_mutex_unlock(atomic_var->mutex);
    return new_val;
}

PJ_DEF(void) pj_atomic_add(pj_atomic_t *atomic_var, pj_atomic_value_t value)
{
    pj_atomic_add_and_get(atomic_var, value);
}

PJ_DEF(pj_status_t) pj_thread_local_alloc(long *p_index)
{
    int i;
    PJ_ASSERT_RETURN(p_index != NULL, PJ_EINVAL);
    if (s_tls_lock) {
        tal_mutex_lock(s_tls_lock);
    }
    for (i = 0; i < PJ_TUYAOS_TLS_KEYS; i++) {
        if (!s_tls_key_used[i]) {
            s_tls_key_used[i] = 1;
            *p_index = i;
            if (s_tls_lock) {
                tal_mutex_unlock(s_tls_lock);
            }
            return PJ_SUCCESS;
        }
    }
    if (s_tls_lock) {
        tal_mutex_unlock(s_tls_lock);
    }
    return PJ_ETOOMANY;
}

PJ_DEF(void) pj_thread_local_free(long index)
{
    PJ_ASSERT_ON_FAIL(index >= 0 && index < PJ_TUYAOS_TLS_KEYS, return );
    if (s_tls_lock) {
        tal_mutex_lock(s_tls_lock);
    }
    s_tls_key_used[index] = 0;
    if (s_tls_lock) {
        tal_mutex_unlock(s_tls_lock);
    }
}

PJ_DEF(pj_status_t) pj_thread_local_set(long index, void *value)
{
    pj_tuyaos_tls_row *row;
    PJ_ASSERT_RETURN(index >= 0 && index < PJ_TUYAOS_TLS_KEYS, PJ_EINVAL);
    row = __tls_row_current(1);
    if (row == NULL) {
        return PJ_ENOMEM;
    }
    row->vals[index] = value;
    return PJ_SUCCESS;
}

PJ_DEF(void *) pj_thread_local_get(long index)
{
    pj_tuyaos_tls_row *row;
    if (index < 0 || index >= PJ_TUYAOS_TLS_KEYS) {
        return NULL;
    }
    row = __tls_row_current(0);
    if (row == NULL) {
        return NULL;
    }
    return row->vals[index];
}

PJ_DEF(void) pj_enter_critical_section(void)
{
#if PJ_HAS_THREADS
    pj_mutex_lock(&critical_section);
#endif
}

PJ_DEF(void) pj_leave_critical_section(void)
{
#if PJ_HAS_THREADS
    pj_mutex_unlock(&critical_section);
#endif
}

static pj_status_t init_mutex(pj_mutex_t *mutex, const char *name, int type)
{
#if PJ_HAS_THREADS
    PJ_UNUSED_ARG(type);
    PJ_CHECK_STACK();
    PJ_ASSERT_RETURN(mutex, PJ_EINVAL);
    memset(mutex, 0, sizeof(*mutex));
    if (tal_mutex_create_init(&mutex->mutex) != OPRT_OK) {
        return PJ_ENOMEM;
    }
    if (!name) {
        name = "mtx%p";
    }
    if (strchr(name, '%')) {
        pj_ansi_snprintf(mutex->obj_name, PJ_MAX_OBJ_NAME, name, mutex);
    } else {
        strncpy(mutex->obj_name, name, PJ_MAX_OBJ_NAME);
        mutex->obj_name[PJ_MAX_OBJ_NAME - 1] = '\0';
    }
    return PJ_SUCCESS;
#else
    PJ_UNUSED_ARG(mutex);
    PJ_UNUSED_ARG(name);
    PJ_UNUSED_ARG(type);
    return PJ_SUCCESS;
#endif
}

PJ_DEF(pj_status_t) pj_mutex_create(pj_pool_t *pool, const char *name, int type, pj_mutex_t **ptr_mutex)
{
#if PJ_HAS_THREADS
    pj_mutex_t *mutex;
    pj_status_t rc;

    PJ_ASSERT_RETURN(pool && ptr_mutex, PJ_EINVAL);
    mutex = PJ_POOL_ALLOC_T(pool, pj_mutex_t);
    PJ_ASSERT_RETURN(mutex, PJ_ENOMEM);
    rc = init_mutex(mutex, name, type);
    if (rc != PJ_SUCCESS) {
        return rc;
    }
    *ptr_mutex = mutex;
    return PJ_SUCCESS;
#else
    PJ_UNUSED_ARG(pool);
    PJ_UNUSED_ARG(name);
    PJ_UNUSED_ARG(type);
    *ptr_mutex = (pj_mutex_t *)1;
    return PJ_SUCCESS;
#endif
}

PJ_DEF(pj_status_t) pj_mutex_create_simple(pj_pool_t *pool, const char *name, pj_mutex_t **mutex)
{
    return pj_mutex_create(pool, name, PJ_MUTEX_SIMPLE, mutex);
}

PJ_DEF(pj_status_t) pj_mutex_create_recursive(pj_pool_t *pool, const char *name, pj_mutex_t **mutex)
{
    return pj_mutex_create(pool, name, PJ_MUTEX_RECURSE, mutex);
}

PJ_DEF(pj_status_t) pj_mutex_lock(pj_mutex_t *mutex)
{
#if PJ_HAS_THREADS
    PJ_ASSERT_RETURN(mutex && mutex->mutex, PJ_EINVAL);
    return map_oprt(tal_mutex_lock(mutex->mutex));
#else
    PJ_UNUSED_ARG(mutex);
    return PJ_SUCCESS;
#endif
}

PJ_DEF(pj_status_t) pj_mutex_unlock(pj_mutex_t *mutex)
{
#if PJ_HAS_THREADS
    PJ_ASSERT_RETURN(mutex && mutex->mutex, PJ_EINVAL);
    return map_oprt(tal_mutex_unlock(mutex->mutex));
#else
    PJ_UNUSED_ARG(mutex);
    return PJ_SUCCESS;
#endif
}

PJ_DEF(pj_status_t) pj_mutex_trylock(pj_mutex_t *mutex)
{
    /* TAL has no trylock — best-effort blocking lock (same as prior shim) */
    return pj_mutex_lock(mutex);
}

PJ_DEF(pj_status_t) pj_mutex_destroy(pj_mutex_t *mutex)
{
#if PJ_HAS_THREADS
    PJ_ASSERT_RETURN(mutex, PJ_EINVAL);
    if (mutex->mutex) {
        tal_mutex_release(mutex->mutex);
        mutex->mutex = NULL;
    }
    return PJ_SUCCESS;
#else
    PJ_UNUSED_ARG(mutex);
    return PJ_SUCCESS;
#endif
}

PJ_DEF(pj_bool_t) pj_mutex_is_locked(pj_mutex_t *mutex)
{
    PJ_UNUSED_ARG(mutex);
    return PJ_TRUE;
}

#if defined(PJ_HAS_SEMAPHORE) && PJ_HAS_SEMAPHORE != 0

PJ_DEF(pj_status_t) pj_sem_create(pj_pool_t *pool, const char *name, unsigned initial, unsigned max, pj_sem_t **ptr_sem)
{
#if PJ_HAS_THREADS
    pj_sem_t *sem;
    unsigned sem_max;

    PJ_CHECK_STACK();
    PJ_ASSERT_RETURN(pool != NULL && ptr_sem != NULL, PJ_EINVAL);

    sem = PJ_POOL_ALLOC_T(pool, pj_sem_t);
    PJ_ASSERT_RETURN(sem, PJ_ENOMEM);
    sem_max = (max > 0) ? max : ((initial > 0) ? initial : 1);
    if (sem_max < 64) {
        sem_max = 64;
    }
    if (tal_semaphore_create_init(&sem->sem, initial, sem_max) != OPRT_OK) {
        return PJ_ENOMEM;
    }
    if (!name) {
        name = "sem%p";
    }
    if (strchr(name, '%')) {
        pj_ansi_snprintf(sem->obj_name, PJ_MAX_OBJ_NAME, name, sem);
    } else {
        strncpy(sem->obj_name, name, PJ_MAX_OBJ_NAME);
        sem->obj_name[PJ_MAX_OBJ_NAME - 1] = '\0';
    }
    *ptr_sem = sem;
    return PJ_SUCCESS;
#else
    PJ_UNUSED_ARG(pool);
    PJ_UNUSED_ARG(name);
    PJ_UNUSED_ARG(initial);
    PJ_UNUSED_ARG(max);
    *ptr_sem = (pj_sem_t *)1;
    return PJ_SUCCESS;
#endif
}

PJ_DEF(pj_status_t) pj_sem_wait(pj_sem_t *sem)
{
#if PJ_HAS_THREADS
    PJ_ASSERT_RETURN(sem && sem->sem, PJ_EINVAL);
    return map_oprt(tal_semaphore_wait(sem->sem, SEM_WAIT_FOREVER));
#else
    PJ_UNUSED_ARG(sem);
    return PJ_SUCCESS;
#endif
}

PJ_DEF(pj_status_t) pj_sem_trywait(pj_sem_t *sem)
{
#if PJ_HAS_THREADS
    PJ_ASSERT_RETURN(sem && sem->sem, PJ_EINVAL);
    if (tal_semaphore_wait(sem->sem, 0) != OPRT_OK) {
        return PJ_EGONE;
    }
    return PJ_SUCCESS;
#else
    PJ_UNUSED_ARG(sem);
    return PJ_SUCCESS;
#endif
}

PJ_DEF(pj_status_t) pj_sem_post(pj_sem_t *sem)
{
#if PJ_HAS_THREADS
    PJ_ASSERT_RETURN(sem && sem->sem, PJ_EINVAL);
    return map_oprt(tal_semaphore_post(sem->sem));
#else
    PJ_UNUSED_ARG(sem);
    return PJ_SUCCESS;
#endif
}

PJ_DEF(pj_status_t) pj_sem_destroy(pj_sem_t *sem)
{
#if PJ_HAS_THREADS
    PJ_ASSERT_RETURN(sem, PJ_EINVAL);
    if (sem->sem) {
        tal_semaphore_release(sem->sem);
        sem->sem = NULL;
    }
    return PJ_SUCCESS;
#else
    PJ_UNUSED_ARG(sem);
    return PJ_SUCCESS;
#endif
}

#endif /* PJ_HAS_SEMAPHORE */

PJ_DEF(pj_status_t) pj_term_set_color(pj_color_t color)
{
    PJ_UNUSED_ARG(color);
    return PJ_SUCCESS;
}

PJ_DEF(pj_color_t) pj_term_get_color(void)
{
    return 0;
}

PJ_DEF(int) pj_run_app(pj_main_func_ptr main_func, int argc, char *argv[], unsigned flags)
{
    PJ_UNUSED_ARG(flags);
    return (*main_func)(argc, argv);
}

#if defined(PJ_EMULATE_RWMUTEX) && PJ_EMULATE_RWMUTEX != 0
#if !defined(PJ_HAS_SEMAPHORE) || PJ_HAS_SEMAPHORE == 0
#error "Semaphore support needs to be enabled to emulate rwmutex"
#endif
#include "os_rwmutex.c"
#endif /* PJ_EMULATE_RWMUTEX */

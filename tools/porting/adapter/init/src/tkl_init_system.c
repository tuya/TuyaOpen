/**
 * @file tkl_init_system.c
 * @brief Common process - tkl init system object descption
 * @version 0.1
 * @date 2021-08-06
 *
 * @copyright Copyright 2020-2021 Tuya Inc. All Rights Reserved.
 *
 */

#include "tkl_init_system.h"

const TKL_OS_T TKL_OS = {
    //! system
    .reset = tkl_system_reset,
    .get_free_heap_size = tkl_system_get_free_heap_size,
    .get_tick_count = tkl_system_get_tick_count,
    .get_millisecond = tkl_system_get_millisecond,
    .get_random = tkl_system_get_random,
    .get_reset_reason = tkl_system_get_reset_reason,
    .sleep = tkl_system_sleep,
    .get_cpu_info = tkl_system_get_cpu_info,
    .cpu_sleep_mode_set = tkl_cpu_sleep_mode_set,
    //! log output
    .log_output = tkl_log_output,
    .log_close = tkl_log_close,
    .log_open = tkl_log_open,
    //! ota
    .ota_get_ability = tkl_ota_get_ability,
    .ota_start = tkl_ota_start_notify,
    .ota_process = tkl_ota_data_process,
    .ota_end = tkl_ota_end_notify,
    //! memory
    .malloc = tkl_system_malloc,
    .free = tkl_system_free,
    .calloc = tkl_system_calloc,
    .realloc = tkl_system_realloc,
    //! thread
    .thread_create = tkl_thread_create,
    .thread_release = tkl_thread_release,
    .thread_set_name = tkl_thread_set_self_name,
    .thread_get_watermark = tkl_thread_get_watermark,
    .thread_get_id = tkl_thread_get_id,
    .thread_is_self = tkl_thread_is_self,
    .thread_diagnose = tkl_thread_diagnose,
    //! mutex
    .mutex_init = tkl_mutex_create_init,
    .mutex_release = tkl_mutex_release,
    .mutex_lock = tkl_mutex_lock,
    .mutex_trylock = tkl_mutex_trylock,
    .mutex_unlock = tkl_mutex_unlock,
    //! semaphore
    .sem_init = tkl_semaphore_create_init,
    .sem_release = tkl_semaphore_release,
    .sem_wait = tkl_semaphore_wait,
    .sem_post = tkl_semaphore_post,
    //! queue
    .queue_init = tkl_queue_create_init,
    .queue_free = tkl_queue_free,
    .queue_post = tkl_queue_post,
    .queue_fetch = tkl_queue_fetch,
};

/**
 * @brief get system os description
 *
 * @return TKL_OS instance
 */
TUYA_WEAK_ATTRIBUTE TKL_OS_T *tkl_os_desc_get()
{
    return (TKL_OS_T *)&TKL_OS;
}

#ifdef ENABLE_FILE_SYSTEM
const TKL_FS_T TKL_FS = {
    .fs_mkdir = tkl_fs_mkdir,
    .fs_remove = tkl_fs_remove,
    .fs_mode = tkl_fs_mode,
    .fs_is_exist = tkl_fs_is_exist,
    .fs_rename = tkl_fs_rename,
    .dir_open = tkl_dir_open,
    .dir_close = tkl_dir_close,
    .dir_read = tkl_dir_read,
    .dir_name = tkl_dir_name,
    .dir_is_dir = tkl_dir_is_directory,
    .dir_is_regular = tkl_dir_is_regular,
    .fopen = tkl_fopen,
    .fclose = tkl_fclose,
    .fread = tkl_fread,
    .fwrite = tkl_fwrite,
    .fgets = tkl_fgets,
    .feof = tkl_feof,
    .fseek = tkl_fseek,
    .ftell = tkl_ftell,
    .fflush = tkl_fflush,
    .fileno = tkl_fileno,
    .fsync = tkl_fsync,
    .faccess = tkl_faccess,
    .fgetsize = tkl_fgetsize,
    .fgetc = tkl_fgetc,
    .ftruncate = tkl_ftruncate,
};

/**
 * @brief get file system description
 *
 * @return TKL_FS instance
 */
TUYA_WEAK_ATTRIBUTE TKL_FS_T *tkl_fs_desc_get()
{
    return (TKL_FS_T *)&TKL_FS;
}
#endif

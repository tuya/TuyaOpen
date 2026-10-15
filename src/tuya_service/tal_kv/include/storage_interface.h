/**
 * @file storage_interface.h
 * @brief Defines the interface for local storage operations.
 *
 * This header file declares the interface for basic local storage operations,
 * including setting, getting, and deleting data. It abstracts the underlying
 * storage mechanism (e.g., POSIX file system) to provide a unified API for
 * storage operations across different platforms. The file includes
 * platform-specific headers as needed and maps the generic local storage
 * operations to their corresponding platform-specific implementations.
 *
 * @note The actual implementation of these operations must be provided by the
 * platform-specific storage library.
 *
 * @copyright Copyright (c) 2021-2024 Tuya Inc. All Rights Reserved.
 *
 */
#ifndef __STORAGE_INTERFACE_H_
#define __STORAGE_INTERFACE_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>

#define local_storage_set posix_storage_set
#define local_storage_get posix_storage_get
#define local_storage_del posix_storage_del

#ifdef __cplusplus
}
#endif

#endif //__STORAGE_INTERFACE_H_

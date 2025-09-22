/**
 * @file tkl_storage.h
 * @brief Common process - storage: sdCard U-Disk HDD SSD etc.
 * @version 0.1
 * @date 2021-08-31
 *
 * @copyright Copyright 2019-2021 Tuya Inc. All Rights Reserved.
 *
 */

#ifndef __TKL_STORAGE_H__
#define __TKL_STORAGE_H__

#include "tuya_cloud_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* common filesystem name list */
#define FSTP_FAT32 "vfat" // fat32
#define FSTP_EXT34 "ext4"
#define FSTP_XFS   "xfs"

/* common storage bus type list */
#define BUSTP_USB  "usb"
#define BUSTP_SATA "sata"
#define BUSTP_SDIO "sdcard"

#define MAX_PATH_LEN   256
#define MAX_PART_NUM   10
#define MAX_DEVICE_NUM 10
#define MAX_FSTP_LEN   16

/* for disk partition add or delete */
#define PART_ADD 1
#define PART_DEL 2

#define DEVICE_HOTPLUG_ADD 1
#define DEVICE_HOTPLUG_DEL 0

typedef struct {
    char dev_name[MAX_PATH_LEN];   /* device node name, eg. /dev/sda1 */
    char fs_type[MAX_FSTP_LEN];    /* filesystem type. refer to common filesystem name list */
    uint64_t size;                 /* total KB */
    uint64_t used;                 /* used KB */
    uint64_t avail;                /* available KB */
    char mounted_on[MAX_PATH_LEN]; /* mount point */
} TKL_STORAGE_DF_T;

typedef struct {
    char dev_name[MAX_PATH_LEN]; /* device node name, eg. /dev/sda1 */
    uint32_t part_id;            /* partition index */
    uint64_t start;              /* start position, KB, unsigned long long */
    uint64_t end;                /* end position, KB */
    uint64_t size;               /* partition size, KB */
    char fs_type[MAX_FSTP_LEN];  /* filesystem type, eg. ext4 */
    char part_label[16];         /* partition label */
    char flags[16];              /* partition flags, eg. bios_grub */
} TKL_STORAGE_PART_INFO_T;

typedef struct {
    uint32_t slot;               /* hardware slot number */
    uint32_t major;              /* device major id */
    uint32_t minor;              /* device minor id */
    char bus_type[8];            /* interface type, eg. usb sata sd */
    char model[16];              /* storage model, optional */
    char dev_name[MAX_PATH_LEN]; /* device node, eg. /dev/sda */
    uint64_t capacity;           /* total size, KB */
    uint32_t sect_size;          /* sector size, bytes */
    char part_table_type[16];    /* partition table type, eg. gpt mbr private */
    char dev_id[64];             /* disk identifier, eg. 330BD448-E0FC-46ED-BC7E-67D75DDA5572 */
} TKL_STORAGE_DEVICE_INFO_T;

typedef enum {
    EVT_HOTPLUG,
    EVT_FS_ERR, // eg. ro
} TKL_STORAGE_EVT_MSGTP;

typedef struct {
    char bus_type[8];
    char dev_name[MAX_PATH_LEN];
    TKL_STORAGE_EVT_MSGTP type;
    union {
        int i;
        char s[64];
    } msg;
} TKL_STORAGE_EVENT_MSG;

/* progress bar in case of long time operations */
typedef void (*TKL_STORAGE_PROGRESS_CB)(int progress, void *user_ctx);

/* common storage device event */
typedef void (*TKL_STORAGE_EVENT_CB)(TKL_STORAGE_EVENT_MSG *msg, void *user_ctx);

/* return badblock index in blk_idx  */
typedef void (*TKL_STORAGE_BADBLK_CB)(uint32_t blk_idx, void *user_ctx);

/**
 * @brief get internal r/w dir
 *
 * @param path absolute path returned on success
 * @return OPERATE_RET 0 on success. A negative error code on error.
 */
OPERATE_RET tkl_storage_get_internal_dir(char path[MAX_PATH_LEN]);

/**
 * @brief report filesystem disk space usage
 *
 * @param mount_path mount point
 * @param df fs state loaded on success
 * @return OPERATE_RET 0 on success. A negative error code on error.
 */
OPERATE_RET tkl_storage_df(char *mount_point, TKL_STORAGE_DF_T *df);

/**
 * @brief get one device info
 *
 * @param list device list loaded on success
 * @return OPERATE_RET return list number on success. A negative error code on error.
 */
OPERATE_RET tkl_storage_get_device_info(char *dev_name, TKL_STORAGE_DEVICE_INFO_T *list);

/**
 * @brief get block device list
 *
 * @param list device list loaded on success
 * @param num list number
 * @return OPERATE_RET return list number on success. A negative error code on error.
 */
OPERATE_RET tkl_storage_get_device_list(TKL_STORAGE_DEVICE_INFO_T *list, int num);

/**
 * @brief get partition info
 *
 * @param dev_name device node wanted to list
 * @param list partition list loaded on success
 * @param num list number
 * @return OPERATE_RET return list number on success. A negative error code on error.
 */
OPERATE_RET tkl_storage_get_part_list(char *dev_name, TKL_STORAGE_PART_INFO_T *list, int num);

/**
 * @brief make partition. refer to parted/fdisk
 *
 * @param part_name partition label, optional
 * @param part_type partition type, eg. primary logical extended
 * @param fs_type filesystem type, eg. ext4 xfs, optional
 * @param isadd part is add or delet, 1:add, 2:delete
 * @param part_index means part number, eg. primary[1-4] logical [5 or over]
 * @param start partition start position, KB
 * @param end partition end position, KB
 * @return OPERATE_RET 0 on success. A negative error code on error.
 */
OPERATE_RET tkl_storage_make_part(char *part_name, char *part_type, char *fs_type, BOOL_T isadd, uint32_t part_index,
                                  LONG_T start, LONG_T end);

/**
 * @brief partiton format, build filesystem
 *
 * @param fs_type filesystem type, eg. ext4 fat32
 * @param fs_dev device node, eg. /dev/sda1
 * @param parm mkfs parame, eg. mkfs.vfat -F 32 -n "name" /dev/sda1
 * @param cb progress callback, optional
 * @param user_ctx caller's private context, optional
 * @return OPERATE_RET 0 on success. A negative error code on error.
 */
OPERATE_RET tkl_storage_mkfs(char *fs_type, char *fs_dev, char *parm, TKL_STORAGE_PROGRESS_CB cb, void *user_ctx);

/**
 * @brief mount a filesystem partition. refer to mount system call
 *
 * @param source device node
 * @param target dir wanted mount to
 * @param fs_type filesystem type
 * @param flags filesystem r/w/a flags, optional
 * @param data filesystem-specific params, optional
 * @return OPERATE_RET 0 on success. A negative error code on error.
 */
OPERATE_RET tkl_storage_mount(char *source, char *target, char *fs_type, uint32_t flags, void *data);

/**
 * @brief umount a filesystem partition. refer to umount system call
 *
 * @param target dir fs mounted
 * @param flags reserved flags
 * @return OPERATE_RET 0 on success. A negative error code on error.
 */
OPERATE_RET tkl_storage_umount(char *target, int flags);

/**
 * @brief loop storage event, eg. hotplug
 * will not return except an error occured.
 *
 * @param event_cb callback handle storage event
 * @param user_ctx user private data, optional
 * @return OPERATE_RET 0 on success. A negative error code on error.
 */
OPERATE_RET tkl_storage_loop_event(TKL_STORAGE_EVENT_CB event_cb, void *user_ctx);

/**
 * @brief check and repair a filesystem
 *
 * @param blk_dev device node
 * @param fs_type filesystem type
 * @param parm fsck parame, eg. fsck.vfat -f /dev/sda1
 * @param flags reserved flags
 * @param cb progress callback, optional
 * @param user_ctx caller's private context, optional
 * @return OPERATE_RET 0 on success. A negative error code on error.
 */
OPERATE_RET tkl_storage_fsck(char *blk_dev, char *fs_type, char *parm, int flags, TKL_STORAGE_PROGRESS_CB cb,
                             void *user_ctx);

/**
 * @brief scsi generic ioctl
 *
 * @param dev_name device node name
 * @param request request. TODO: copy scsi header files for request
 * @param args args
 * @return OPERATE_RET 0 on success. A negative error code on error.
 */
OPERATE_RET tkl_storage_ioctl(char *dev_name, uint32_t request, void *args);

/* medium-specific interfaces */

/**
 * @brief check bad blocks
 *
 * @param dev_name device node name
 * @param cb callback to report bad blocks
 * @param user_ctx caller's private context, optional
 * @return OPERATE_RET 0 on success. A negative error code on error.
 */
OPERATE_RET tkl_storage_check_badblocks(char *dev_name, TKL_STORAGE_BADBLK_CB cb, void *user_ctx);

#ifdef __cplusplus
}
#endif

#endif

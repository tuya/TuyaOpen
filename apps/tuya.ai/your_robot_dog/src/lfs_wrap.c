#include "sdkconfig.h"

#include "lfs.h"

#ifdef CONFIG_LFS_THREADSAFE
// Provided by platform littlefs (platform/T5AI/t5_os/ap/components/littlefs/lfs_flashbd.c)
extern int lfs_lock_init(void);
extern int lfs_lock(void);
extern int lfs_unlock(void);
#endif

// Linker wrap: -Wl,--wrap=lfs_mount
int __real_lfs_mount(lfs_t *lfs, const struct lfs_config *cfg);
int __wrap_lfs_mount(lfs_t *lfs, const struct lfs_config *cfg)
{
#ifdef CONFIG_LFS_THREADSAFE
    if (cfg && (cfg->lock == NULL || cfg->unlock == NULL)) {
        (void)lfs_lock_init();

        // lfs treats cfg as const, but the cfg object is owned by caller.
        // We patch lock/unlock once to avoid null function pointer calls.
        struct lfs_config *mutable_cfg = (struct lfs_config *)cfg;
        mutable_cfg->lock = (int (*)(const struct lfs_config *c))lfs_lock;
        mutable_cfg->unlock = (int (*)(const struct lfs_config *c))lfs_unlock;
    }
#endif

    return __real_lfs_mount(lfs, cfg);
}

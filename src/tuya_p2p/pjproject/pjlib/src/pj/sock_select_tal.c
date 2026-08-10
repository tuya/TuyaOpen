/**
 * @file sock_select_tal.c
 * @brief pj_sock_select() on TuyaOpen TAL — replaces sock_select.c
 *
 * pj_fd_set_t carries an opaque buffer; sock_select.c stored a native fd_set
 * there, this stores a TUYA_FD_SET_T so no BSD socket headers are needed.
 * Slot 0 keeps the descriptor count, exactly as upstream does.
 *
 * @copyright Copyright (c) Tuya Inc.
 */
#include <pj/sock_select.h>
#include <pj/assert.h>
#include <pj/errno.h>
#include <pj/os.h>
#include <pj/string.h>

#include "tal_api.h"
#include "tal_network.h"

#define PART_FDSET(ps)         ((TUYA_FD_SET_T *)&(ps)->data[1])
#define PART_FDSET_OR_NULL(ps) ((ps) ? PART_FDSET(ps) : NULL)
#define PART_COUNT(ps)         ((ps)->data[0])

/* The opaque area after the count must be able to hold a TUYA_FD_SET_T. */
#define TAL_FDSET_FITS (sizeof(pj_fd_set_t) - sizeof(pj_sock_t) >= sizeof(TUYA_FD_SET_T))

PJ_DEF(void) PJ_FD_ZERO(pj_fd_set_t *fdsetp)
{
    PJ_CHECK_STACK();
    pj_assert(TAL_FDSET_FITS);

    tal_net_fd_zero(PART_FDSET(fdsetp));
    PART_COUNT(fdsetp) = 0;
}

PJ_DEF(void) PJ_FD_SET(pj_sock_t fd, pj_fd_set_t *fdsetp)
{
    PJ_CHECK_STACK();
    pj_assert(TAL_FDSET_FITS);

    if (!PJ_FD_ISSET(fd, fdsetp))
        ++PART_COUNT(fdsetp);

    tal_net_fd_set((int)fd, PART_FDSET(fdsetp));
}

PJ_DEF(void) PJ_FD_CLR(pj_sock_t fd, pj_fd_set_t *fdsetp)
{
    PJ_CHECK_STACK();
    pj_assert(TAL_FDSET_FITS);

    if (PJ_FD_ISSET(fd, fdsetp))
        --PART_COUNT(fdsetp);

    tal_net_fd_clear((int)fd, PART_FDSET(fdsetp));
}

PJ_DEF(pj_bool_t) PJ_FD_ISSET(pj_sock_t fd, const pj_fd_set_t *fdsetp)
{
    PJ_CHECK_STACK();
    PJ_ASSERT_RETURN(TAL_FDSET_FITS, 0);

    return tal_net_fd_isset((int)fd, PART_FDSET((pj_fd_set_t *)fdsetp)) ? PJ_TRUE : PJ_FALSE;
}

PJ_DEF(pj_size_t) PJ_FD_COUNT(const pj_fd_set_t *fdsetp)
{
    return PART_COUNT(fdsetp);
}

PJ_DEF(int)
pj_sock_select(int n, pj_fd_set_t *readfds, pj_fd_set_t *writefds, pj_fd_set_t *exceptfds, const pj_time_val *timeout)
{
    uint32_t ms_timeout;

    PJ_CHECK_STACK();
    PJ_ASSERT_RETURN(TAL_FDSET_FITS, PJ_EBUG);

    /*
     * tal_net_select() spells "wait forever" as a zero timeout, so a NULL
     * pj timeout maps to 0. A pj timeout that rounds down to 0 ms would then
     * be indistinguishable from that, so it becomes 1 ms — a poll returns
     * promptly either way, and this can never turn a poll into a hang.
     */
    if (timeout) {
        ms_timeout = (uint32_t)(timeout->sec * 1000 + timeout->msec);
        if (ms_timeout == 0)
            ms_timeout = 1;
    } else {
        ms_timeout = 0;
    }

    return tal_net_select(n, PART_FDSET_OR_NULL(readfds), PART_FDSET_OR_NULL(writefds), PART_FDSET_OR_NULL(exceptfds),
                          ms_timeout);
}

#include <stdio.h>
#include <assert.h>

//#include "ble.h"
#include "hci_common.h"
#include "tuya_ble_mbuf.h"
#include "tuya_ble_mempool.h"
#include "tuya_ble_linkqueue.h"
#include "tuya_ble_os_adapter.h" // we will use same operations for controlling the socket message
#include "tuya_ble_hci.h"
#include "tuya_ble_os_adapter.h"

#if defined(TUYA_USE_DYNA_RAM) && (TUYA_USE_DYNA_RAM == 0)
// [Tuya[TimCheng] Start to allocate the memory for all hci operations.
/* Buffers for HCI commands data */
static struct os_mempool ble_hci_ram_cmd_pool;
static os_membuf_t ble_hci_ram_cmd_buf[OS_MEMPOOL_SIZE(
    (TUYA_BLE_HCI_CMD_BUF_COUNT),
    TUYA_BLE_HCI_CMD_SZ)]; // change into 2, because of double malloc for seperating the different layers

/* Buffers for HCI events data */
static struct os_mempool ble_hci_ram_evt_hi_pool;
static os_membuf_t
    ble_hci_ram_evt_hi_buf[OS_MEMPOOL_SIZE((TUYA_BLE_HCI_EVT_HI_BUF_COUNT), (TUYA_BLE_HCI_EVT_BUF_SIZE))];

static struct os_mempool ble_hci_ram_evt_lo_pool;
static os_membuf_t
    ble_hci_ram_evt_lo_buf[OS_MEMPOOL_SIZE((TUYA_BLE_HCI_EVT_LO_BUF_COUNT), (TUYA_BLE_HCI_EVT_BUF_SIZE))];
#endif

#define TUYA_BLE_ACL_BLOCK_SIZE                                                                                        \
    OS_ALIGN(TUYA_BLE_ACL_BUF_SIZE + sizeof(struct os_mbuf) + sizeof(struct os_mbuf_pkthdr), OS_ALIGNMENT)

#if defined(TUYA_USE_DYNA_RAM) && (TUYA_USE_DYNA_RAM == 0)
static struct os_mempool ble_hci_ram_acl_pool;
static struct os_mbuf_pool ble_hci_ram_acl_mbuf_pool;

/*
 * The MBUF payload size must accommodate the HCI data header size plus the
 * maximum ACL data packet length. The ACL block size is the size of the
 * mbufs we will allocate.
 */
static os_membuf_t ble_hci_ram_acl_buf[OS_MEMPOOL_SIZE(TUYA_BLE_ACL_BUF_COUNT, TUYA_BLE_ACL_BLOCK_SIZE)];

#else
// dynamic ram allocation
static struct os_mempool ble_hci_dyna_ram_cmd_pool;
static struct os_mempool ble_hci_dyna_ram_evt_pool;
// dynamic acl ram allocation
static struct os_mbuf_pool ble_hci_ram_acl_mbuf_pool; // ble_hci_dyna_ram_acl_mbuf_pool;
#if defined(TY_HS_BLE_HS_FLOW_CTRL) && (TY_HS_BLE_HS_FLOW_CTRL == 0)
struct os_mempool ble_hci_dyna_ram_acl_pool;
#else
// for ext ram use
static struct os_mempool_ext ble_hci_ext_ram_acl_pool;
#endif

void *tuya_ble_hci_dyna_buf_alloc(int type, struct os_mempool *dynapool);
void tuya_ble_hci_dyna_buf_free(uint8_t *buf);
#endif

/*
 * Called by tuya ble host to allocate buffer for HCI Command packet.
 * Called by HCI transport to allocate buffer for HCI Event packet.
 */
/* Both of Host and Controller use*/
#if defined(TUYA_USE_DYNA_RAM) && (TUYA_USE_DYNA_RAM == 0)
uint8_t *tuya_ble_hci_buf_alloc(int type)
{
    uint8_t *buf;

    switch (type) {
    case TUYA_BLE_HCI_BUF_CMD:
        buf = os_memblock_get(&ble_hci_ram_cmd_pool);
        break;

    case TUYA_BLE_HCI_BUF_EVT_HI:
        buf = os_memblock_get(&ble_hci_ram_evt_hi_pool);
        if (buf == NULL) {
            /* If no high-priority event buffers remain, try to grab a
             * low-priority one.
             */
            buf = tuya_ble_hci_buf_alloc(TUYA_BLE_HCI_BUF_EVT_LO);
        }
        break;

    case TUYA_BLE_HCI_BUF_EVT_LO:
        buf = os_memblock_get(&ble_hci_ram_evt_lo_pool);
        break;

    default:
        TUYA_HS_ASSERT(0);
        buf = NULL;
    }

    return buf;
}

/*
 * Called by tuya ble host to free buffer allocated for HCI Event packet.
 * Called by HCI transport to free buffer allocated for HCI Command packet.
 */
/* Both of Host and Controller use*/
void tuya_ble_hci_buf_free(uint8_t *buf)
{
    int rc;

    /* XXX: this may look a bit odd, but the controller uses the command
     * buffer to send back the command complete/status as an immediate
     * response to the command. This was done to insure that the controller
     * could always send back one of these events when a command was received.
     * Thus, we check to see which pool the buffer came from so we can free
     * it to the appropriate pool
     */

    if (os_memblock_from(&ble_hci_ram_evt_hi_pool, buf)) {
        rc = os_memblock_put(&ble_hci_ram_evt_hi_pool, buf);
        TUYA_HS_ASSERT(rc == 0);
    } else if (os_memblock_from(&ble_hci_ram_evt_lo_pool, buf)) {
        rc = os_memblock_put(&ble_hci_ram_evt_lo_pool, buf);
        TUYA_HS_ASSERT(rc == 0);
    } else {
        TUYA_HS_ASSERT(os_memblock_from(&ble_hci_ram_cmd_pool, buf));
        rc = os_memblock_put(&ble_hci_ram_cmd_pool, buf);
        TUYA_HS_ASSERT(rc == 0);
    }
}

#else
uint8_t *tuya_ble_hci_buf_alloc(int type)
{
    uint8_t *buf;

    switch (type) {
    case TUYA_BLE_HCI_BUF_CMD:
        buf = tuya_ble_hci_dyna_buf_alloc(TUYA_BLE_HCI_BUF_CMD, &ble_hci_dyna_ram_cmd_pool);
        break;

    case TUYA_BLE_HCI_BUF_EVT_HI:
    case TUYA_BLE_HCI_BUF_EVT_LO:
        buf = tuya_ble_hci_dyna_buf_alloc(TUYA_BLE_HCI_BUF_EVT, &ble_hci_dyna_ram_evt_pool);
        break;

    default:
        TUYA_HS_ASSERT(0);
        buf = NULL;
    }

    return buf;
}

/*
 * Called by tuya ble host to free buffer allocated for HCI Event packet.
 * Called by HCI transport to free buffer allocated for HCI Command packet.
 */
/* Both of Host and Controller use*/
void tuya_ble_hci_buf_free(int type, uint8_t *buf)
{
    // tuya_ble_hci_dyna_buf_free(buf);
    switch (type) {
    case TUYA_BLE_HCI_BUF_CMD:
        tuya_ble_hci_mp_num_buf_free(&ble_hci_dyna_ram_cmd_pool, buf);
        break;

    case TUYA_BLE_HCI_BUF_EVT_HI:
    case TUYA_BLE_HCI_BUF_EVT_LO:
    case TUYA_BLE_HCI_BUF_EVT:
        tuya_ble_hci_mp_num_buf_free(&ble_hci_dyna_ram_evt_pool, buf);
        break;
    default:
        TUYA_HS_ASSERT(0);
        buf = NULL;
    }
}

int os_dyna_memblock_put(struct os_mempool *pool, void *buf)
{
    struct os_mempool_ext *mpe;
    stats_error_t ret;

    if ((pool == NULL) || (buf == NULL)) {
        PR_ERR("FREE POLL OR BUF NULL");
        return 0;
    }
    /* If this is an extended mempool with a put callback, call the callback
     * instead of freeing the block directly.
     */
    if (pool->mp_flags & OS_MEMPOOL_F_EXT) {
        mpe = (struct os_mempool_ext *)pool;
        if (mpe->mpe_put_cb != NULL) {
            ret = mpe->mpe_put_cb(mpe, buf, mpe->mpe_put_arg);
            return ret;
        }
    }

#if defined(TUYA_DYNA_ALLOCATION_LIMIT) && (TUYA_DYNA_ALLOCATION_LIMIT == 1)
    if (pool->mp_num_free >= pool->mp_num_blocks) {
        PR_WARN("FREE NUM ERR, NAME:%s,MAX NUM:%d, FREE NUM:%d", pool->name, pool->mp_num_blocks, pool->mp_num_free);
    }
#endif
    tuya_ble_hs_enter_critical();
    pool->mp_num_free++;
    tuya_ble_hs_exit_critical();
    tuya_ble_hci_dyna_buf_free(buf);
    return 0;
}

int tuya_ble_hci_mp_num_buf_free(struct os_mempool *pool, uint8_t *buf)
{
    if ((pool == NULL) || (buf == NULL)) {
        PR_ERR("FREE POLL OR BUF NULL");
        return 0;
    }
#if defined(TUYA_DYNA_ALLOCATION_LIMIT) && (TUYA_DYNA_ALLOCATION_LIMIT == 1)
    if (pool->mp_num_free >= pool->mp_num_blocks) {
        PR_WARN("FREE NUM ERR, NAME:%s,MAX NUM:%d, FREE NUM:%d", pool->name, pool->mp_num_blocks, pool->mp_num_free);
    }
#endif
    tuya_ble_hs_enter_critical();
    pool->mp_num_free++;
    tuya_ble_hs_exit_critical();
    tuya_ble_hci_dyna_buf_free(buf);
    return 0;
}

void *tuya_ble_hci_dyna_buf_alloc(int type, struct os_mempool *dynapool)
{
    uint8_t *buf = NULL;
    uint32_t mem_size = dynapool->mp_block_size;
#if defined(TUYA_DYNA_ALLOCATION_LIMIT) && (TUYA_DYNA_ALLOCATION_LIMIT == 1)
    if (dynapool->mp_num_free == 0) {
        PR_WARN("OUT OF MAX ALLOCATION NUM, NAME:%s,MAX NUM:%d", dynapool->name, dynapool->mp_num_blocks);
        return NULL;
    }
#endif
    buf = tuya_ble_hs_malloc(mem_size);
    if (!buf) {
        return NULL;
    }

    // not cmd or event
    //  if(type == 0) {
    tuya_ble_hs_enter_critical();
    dynapool->mp_num_free--;
    tuya_ble_hs_exit_critical();
    // }
    // PR_DEBUG("++mlc,%d, %d %d, %p ,%s\r\n",type, dynapool->mp_num_free, mem_size, buf, dynapool->name);
    return buf;
}

void tuya_ble_hci_dyna_buf_free(uint8_t *buf)
{
    // PR_DEBUG("--free %p \r\n",buf);
    if (buf) {
        tuya_ble_hs_free(buf);
        buf = NULL;
    }
    // dynapool.mp_num_free++;
}

/**
 * Unsupported; the RAM transport does not have a dedicated ACL data packet
 * pool.
 */
int tuya_ble_hci_set_acl_free_cb(os_mempool_put_fn *cb, void *arg)
{
#if defined(TY_HS_BLE_HS_FLOW_CTRL) && (TY_HS_BLE_HS_FLOW_CTRL == 1)
    ble_hci_ext_ram_acl_pool.mpe_put_cb = cb;
    ble_hci_ext_ram_acl_pool.mpe_put_arg = arg;
#endif
    return 0;
}

#endif
/**
 * Allocates a buffer (mbuf) for ACL operation.
 *
 * @return                      The allocated buffer on success;
 *                              NULL on buffer exhaustion.
 */
struct os_mbuf *tuya_ble_hci_acl_buf_alloc(void)
{
    struct os_mbuf *m;
#if defined(TUYA_USE_DYNA_RAM) && (TUYA_USE_DYNA_RAM == 0)
    m = os_mbuf_get_pkthdr(&ble_hci_ram_acl_mbuf_pool, 0);
#else
    m = os_dyna_mbuf_get_pkthdr(&ble_hci_ram_acl_mbuf_pool, 0);
#endif
    // m = os_msys_get_pkthdr(0,0);  need close flow ctrl
    return m;
}

void tuya_ble_hci_buf_init(void)
{
    int rc;
    /*
     * Create memory pool of HCI command buffers. NOTE: we currently dont
     * allow this to be configured. The controller will only allow one
     * outstanding command. We decided to keep this a pool in case we allow
     * allow the controller to handle more than one outstanding command.
     */
#if defined(TUYA_USE_DYNA_RAM) && (TUYA_USE_DYNA_RAM == 0)
    rc = os_mempool_init(&ble_hci_ram_cmd_pool, TUYA_BLE_HCI_CMD_BUF_COUNT, TUYA_BLE_HCI_CMD_SZ, ble_hci_ram_cmd_buf,
                         "ble_hci_ram_cmd_pool");
    TUYA_HS_ASSERT(rc == 0);

    // All the memory except the ADV Data, And will be set into Hight Priority
    rc = os_mempool_init(&ble_hci_ram_evt_hi_pool, (TUYA_BLE_HCI_EVT_HI_BUF_COUNT), (TUYA_BLE_HCI_EVT_BUF_SIZE),
                         ble_hci_ram_evt_hi_buf, "ble_hci_ram_evt_hi_pool");
    TUYA_HS_ASSERT(rc == 0);

    // Allocate the memory for ADV Data.
    rc = os_mempool_init(&ble_hci_ram_evt_lo_pool, (TUYA_BLE_HCI_EVT_LO_BUF_COUNT), (TUYA_BLE_HCI_EVT_BUF_SIZE),
                         ble_hci_ram_evt_lo_buf, "ble_hci_ram_evt_lo_pool");
    TUYA_HS_ASSERT(rc == 0);

    rc = os_mempool_init(&ble_hci_ram_acl_pool, TUYA_BLE_ACL_BUF_COUNT, TUYA_BLE_ACL_BLOCK_SIZE, ble_hci_ram_acl_buf,
                         "ble_hci_sock_acl_pool");
    TUYA_HS_ASSERT(rc == 0);

    rc = os_mbuf_pool_init(&ble_hci_ram_acl_mbuf_pool, &ble_hci_ram_acl_pool, TUYA_BLE_ACL_BLOCK_SIZE,
                           TUYA_BLE_ACL_BUF_COUNT);
    TUYA_HS_ASSERT(rc == 0);
#else
    rc = os_dynamempool_init(&ble_hci_dyna_ram_cmd_pool, TUYA_BLE_HCI_CMD_BUF_COUNT, TUYA_BLE_HCI_CMD_SZ,
                             "ble_hci_dyna_ram_cmd_pool", TUYA_USE_DYNA_RAM_FLAG);
    TUYA_HS_ASSERT(rc == 0);

    rc =
        os_dynamempool_init(&ble_hci_dyna_ram_evt_pool, (TUYA_BLE_HCI_EVT_HI_BUF_COUNT + TUYA_BLE_HCI_EVT_LO_BUF_COUNT),
                            TUYA_BLE_HCI_EVT_BUF_SIZE, "ble_hci_dyna_ram_evt_pool", TUYA_USE_DYNA_RAM_FLAG);
    TUYA_HS_ASSERT(rc == 0);

#if defined(TY_HS_BLE_HS_FLOW_CTRL) && (TY_HS_BLE_HS_FLOW_CTRL == 0)
    rc = os_dynamempool_init(&ble_hci_dyna_ram_acl_pool, TUYA_BLE_ACL_BUF_COUNT, TUYA_BLE_ACL_BLOCK_SIZE,
                             "ble_hci_dyna_ram_acl_pool", TUYA_USE_DYNA_RAM_FLAG);
    TUYA_HS_ASSERT(rc == 0);

    rc = os_dyna_mbuf_pool_init(&ble_hci_ram_acl_mbuf_pool, &ble_hci_dyna_ram_acl_pool, TUYA_BLE_ACL_BLOCK_SIZE,
                                TUYA_BLE_ACL_BUF_COUNT);
    TUYA_HS_ASSERT(rc == 0);
#else
    // for ext ram use
    rc = os_mempool_ext_init(&ble_hci_ext_ram_acl_pool, TUYA_BLE_ACL_BUF_COUNT, TUYA_BLE_ACL_BLOCK_SIZE, NULL,
                             "ble_hci_ext_ram_acl_pool");
    TUYA_HS_ASSERT(rc == 0);

    rc = os_dyna_mbuf_pool_init(&ble_hci_ram_acl_mbuf_pool, &ble_hci_ext_ram_acl_pool.mpe_mp, TUYA_BLE_ACL_BLOCK_SIZE,
                                TUYA_BLE_ACL_BUF_COUNT);
    TUYA_HS_ASSERT(rc == 0);
#endif
#endif
}

/* Sends ACL data from host to controller. */
int tuya_ble_hs_acl_tx(struct os_mbuf *acl_pkt)
{
    static uint8_t acl_data[TUYA_BLE_ACL_BUF_SIZE + TUYA_BLE_HCI_DATA_HDR_SZ] = {0};
    static uint8_t acl_wait = 0;
    static uint16_t acl_wait_len = 0;
    static uint16_t payload_len = 0;
    struct os_mbuf *acl_ll = acl_pkt;
    int rc = 0;
    extern OPERATE_RET tkl_hci_acl_packet_send(uint8_t * p_buf, uint16_t buf_len);

    while (acl_ll) {
        switch (acl_wait) {
        case 0:
            payload_len = get_le16(acl_ll->om_data + 2); // the payload
            if (payload_len > TUYA_BLE_ACL_BUF_SIZE || acl_ll->om_len < TUYA_BLE_HCI_DATA_HDR_SZ) {
                os_mbuf_free_chain(acl_ll);
                break;
            }
            if ((acl_ll->om_len - TUYA_BLE_HCI_DATA_HDR_SZ) >= payload_len) {
                memcpy(acl_data, acl_ll->om_data, payload_len + TUYA_BLE_HCI_DATA_HDR_SZ); // only get the verified data
                rc = tkl_hci_acl_packet_send(acl_data, payload_len + TUYA_BLE_HCI_DATA_HDR_SZ);
                acl_wait = 0;
            } else if ((acl_ll->om_len - TUYA_BLE_HCI_DATA_HDR_SZ) < payload_len) {
                memcpy(acl_data, acl_ll->om_data, acl_ll->om_len);
                acl_wait_len = payload_len + TUYA_BLE_HCI_DATA_HDR_SZ - acl_ll->om_len; // get the reset len
                acl_wait = 1;
            }
            break;
        case 1:
            if (acl_wait_len <= acl_ll->om_len) {
                memcpy(&acl_data[payload_len + TUYA_BLE_HCI_DATA_HDR_SZ - acl_wait_len], acl_ll->om_data, acl_wait_len);
                rc = tkl_hci_acl_packet_send(acl_data, payload_len + TUYA_BLE_HCI_DATA_HDR_SZ);
                memset(acl_data, 0, TUYA_BLE_ACL_BUF_SIZE);
                acl_wait_len = 0;
                payload_len = 0;
                acl_wait = 0;
            } else {
                memcpy(&acl_data[payload_len + TUYA_BLE_HCI_DATA_HDR_SZ - acl_wait_len], acl_ll->om_data,
                       acl_ll->om_len);
                acl_wait_len = acl_wait_len - acl_ll->om_len;
                rc = 0;
            }
            break;
        }

        if (rc < 0) {
            break;
        }
        acl_ll = SLIST_NEXT(acl_ll, om_next);
    }

    os_mbuf_free_chain(acl_pkt); // Must Do Free
    return rc;
}

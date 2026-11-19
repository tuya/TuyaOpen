/*
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

#include "ble_hs_priv.h"

int ble_mqueue_init(struct ble_mqueue *mq, tuya_ble_event_fn *ev_fn, void *ev_arg)
{
    STAILQ_INIT(&mq->head);

    tuya_ble_event_set_ev(&mq->ev, ev_fn, ev_arg);

    return (0);
}

struct os_mbuf *ble_mqueue_get(struct ble_mqueue *mq)
{
    struct os_mbuf_pkthdr *mp;
    struct os_mbuf        *om;

    tuya_ble_hs_enter_critical();
    mp = STAILQ_FIRST(&mq->head);
    if (mp) {
        STAILQ_REMOVE_HEAD(&mq->head, omp_next);
    }
    tuya_ble_hs_exit_critical();

    if (mp) {
        om = OS_MBUF_PKTHDR_TO_MBUF(mp);
    } else {
        om = NULL;
    }

    return (om);
}

int ble_mqueue_put(struct ble_mqueue *mq, tuya_ble_eventq *evq, struct os_mbuf *om)
{
    struct os_mbuf_pkthdr *mp;
    int                    rc;

    /* Can only place the head of a chained mbuf on the queue. */
    if (!OS_MBUF_IS_PKTHDR(om)) {
        rc = OS_EINVAL;
        PR_ERR("PKTHDR ERR %d", om->om_pkthdr_len);
        goto err;
    }

    mp = OS_MBUF_PKTHDR(om);

    tuya_ble_hs_enter_critical();
    STAILQ_INSERT_TAIL(&mq->head, mp, omp_next);
    tuya_ble_hs_exit_critical();

    /* Only post an event to the queue if its specified */
    if (evq) {
        tuya_ble_eventq_put(evq, &mq->ev);
    }

    return (0);
err:
    return (rc);
}

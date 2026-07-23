/*
 * TPC-C benchmark harness implementation.
 *
 * Weighted random dispatch to the five TPC-C transaction types.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "tpcc_harness.h"

#include <stdlib.h>

#include "tpcc_new_order.h"
#include "tpcc_payment.h"
#include "tpcc_procedures.h"
#include "tpcc_read_txns.h"
#include "utility.h"

void tpcc_harness_dispatch(h2o_req_t *req, fdb_thread_state_t *fdb_state,
                           unsigned int seed)
{
    /* Weighted random: 0-44 = NewOrder, 45-87 = Payment,
     * 88-91 = OrderStatus, 92-95 = Delivery, 96-99 = StockLevel */
    uint32_t r = get_random_number(99, &seed);

    if (r < NEW_ORDER_WEIGHT) {
        tpcc_new_order_execute(req, fdb_state, seed);
    } else if (r < NEW_ORDER_WEIGHT + PAYMENT_WEIGHT) {
        tpcc_payment_execute(req, fdb_state, seed);
    } else if (r < NEW_ORDER_WEIGHT + PAYMENT_WEIGHT + ORDER_STATUS_WEIGHT) {
        tpcc_order_status_execute(req, fdb_state, seed);
    } else if (r < NEW_ORDER_WEIGHT + PAYMENT_WEIGHT + ORDER_STATUS_WEIGHT + DELIVERY_WEIGHT) {
        tpcc_delivery_execute(req, fdb_state, seed);
    } else {
        tpcc_stock_level_execute(req, fdb_state, seed);
    }
}

#ifndef TPCC_NEW_ORDER_H_
#define TPCC_NEW_ORDER_H_

#include <h2o.h>
#include <stdint.h>
#include <foundationdb/fdb_c.h>

#include "fdb_database.h"
#include "global_data.h"

/*
 * TPC-C NewOrder transaction (45% of transaction mix).
 *
 * Reads: warehouse (tax), district (next_o_id, tax), customer (discount),
 *        stock (5-15 items), item (5-15 items)
 * Writes: district (next_o_id++), oorder, new_order, order_line (5-15),
 *         stock (quantity, order_cnt, ytd)
 *
 * Async flow (callback chain):
 *   1. fdb_transaction_get(district) -> callback reads next_o_id
 *   2. fdb_transaction_get(customer) -> callback reads discount
 *   3. For each order line: fdb_transaction_get(stock) -> callback updates stock
 *   4. fdb_transaction_set for oorder, new_order, all order_lines, updated stocks
 *   5. fdb_transaction_commit -> callback sends HTTP response
 *
 * On conflict: fdb_transaction_on_error -> retry from step 1
 */

#define MAX_ORDER_LINES 15
#define MIN_ORDER_LINES 5

/* Context that flows through the callback chain. */
typedef struct new_order_ctx {
    FDBTransaction *tr;
    h2o_req_t *req;
    fdb_thread_state_t *fdb_state;
    unsigned int seed;

    /* Transaction parameters */
    uint32_t w_id;
    uint32_t d_id;
    uint32_t c_id;
    uint8_t ol_cnt;           /* 5-15 */
    uint8_t ol_supply_w[MAX_ORDER_LINES];
    uint32_t ol_i_id[MAX_ORDER_LINES];
    uint8_t ol_quantity[MAX_ORDER_LINES];
    bool all_local;

    /* State read from DB */
    int32_t w_tax;
    int32_t d_tax;
    int32_t d_next_o_id;
    int32_t c_discount;

    /* Stock state */
    int32_t s_quantity[MAX_ORDER_LINES];
    int32_t s_amount[MAX_ORDER_LINES];

    /* Current step in the callback chain */
    uint8_t current_ol;
    bool committed;
} new_order_ctx_t;

/* Entry point: called from HTTP handler. */
void tpcc_new_order_execute(h2o_req_t *req, fdb_thread_state_t *fdb_state,
                            unsigned int seed);

#endif

#ifndef TPCC_PAYMENT_H_
#define TPCC_PAYMENT_H_

#include <h2o.h>
#include <stdint.h>
#include <foundationdb/fdb_c.h>

#include "fdb_database.h"
#include "global_data.h"

/*
 * TPC-C Payment transaction (43% of transaction mix).
 *
 * Reads: warehouse (ytd, tax), district (ytd, tax), customer (balance, payment_cnt)
 * Writes: warehouse (ytd+amt), district (ytd+amt), customer (balance-amt,
 *         payment_cnt+1), history (new row)
 *
 * 60% payment by customer ID, 40% by customer last name (NURand).
 */

typedef struct payment_ctx {
    FDBTransaction *tr;
    h2o_req_t *req;
    fdb_thread_state_t *fdb_state;
    unsigned int seed;

    uint32_t w_id;
    uint32_t d_id;
    uint32_t c_w_id;
    uint32_t c_d_id;
    uint32_t c_id;
    char c_last[16];
    bool by_name;
    int32_t h_amount;  /* 1.00 to 5000.00 */

    /* State */
    int64_t w_ytd;
    int64_t d_ytd;
    int64_t c_balance;
    int16_t c_payment_cnt;
} payment_ctx_t;

void tpcc_payment_execute(h2o_req_t *req, fdb_thread_state_t *fdb_state,
                          unsigned int seed);

#endif

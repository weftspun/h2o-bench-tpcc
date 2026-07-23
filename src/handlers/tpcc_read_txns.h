#ifndef TPCC_READ_TXNS_H_
#define TPCC_READ_TXNS_H_

#include <h2o.h>
#include "fdb_database.h"

/* Defined in tpcc_read_txns.c */

void tpcc_order_status_execute(h2o_req_t *req, fdb_thread_state_t *fdb_state,
                               unsigned int seed);
void tpcc_delivery_execute(h2o_req_t *req, fdb_thread_state_t *fdb_state,
                          unsigned int seed);
void tpcc_stock_level_execute(h2o_req_t *req, fdb_thread_state_t *fdb_state,
                              unsigned int seed);

#endif

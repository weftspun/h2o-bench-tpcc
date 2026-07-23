#ifndef TPCC_HARNESS_H_
#define TPCC_HARNESS_H_

#include <h2o.h>
#include "fdb_database.h"
#include "global_data.h"

/*
 * TPC-C benchmark harness.
 *
 * The /tpcc/run endpoint dispatches a weighted-random transaction:
 *   NewOrder      45%
 *   Payment       43%
 *   OrderStatus    4%
 *   Delivery       4%
 *   StockLevel     4%
 *
 * Each call to the harness executes exactly one transaction and returns
 * its result as JSON. This is the endpoint wrk/bombardier hit during
 * benchmark runs.
 */

void tpcc_harness_dispatch(h2o_req_t *req, fdb_thread_state_t *fdb_state,
                           unsigned int seed);

#endif

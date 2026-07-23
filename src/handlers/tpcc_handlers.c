/*
 * TPC-C handler registration.
 *
 * Wires up all five TPC-C transaction endpoints plus the harness endpoint.
 */

#include <h2o.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "error.h"
#include "global_data.h"
#include "list.h"
#include "thread.h"
#include "tpcc_handler_data.h"
#include "tpcc_harness.h"
#include "tpcc_new_order.h"
#include "tpcc_payment.h"
#include "tpcc_procedures.h"

/* Per-request context for the HTTP handler (carries FDB state + seed) */
typedef struct {
    fdb_thread_state_t *fdb_state;
    unsigned int seed;
} handler_state_t;

static handler_state_t g_handler_state;

static int tpcc_new_order_handler(struct st_h2o_handler_t *self, h2o_req_t *req)
{
    (void)self;
    tpcc_new_order_execute(req, g_handler_state.fdb_state, g_handler_state.seed++);
    return 0;
}

static int tpcc_payment_handler(struct st_h2o_handler_t *self, h2o_req_t *req)
{
    (void)self;
    tpcc_payment_execute(req, g_handler_state.fdb_state, g_handler_state.seed++);
    return 0;
}

static int tpcc_order_status_handler(struct st_h2o_handler_t *self, h2o_req_t *req)
{
    (void)self;
    tpcc_order_status_execute(req, g_handler_state.fdb_state, g_handler_state.seed++);
    return 0;
}

static int tpcc_delivery_handler(struct st_h2o_handler_t *self, h2o_req_t *req)
{
    (void)self;
    tpcc_delivery_execute(req, g_handler_state.fdb_state, g_handler_state.seed++);
    return 0;
}

static int tpcc_stock_level_handler(struct st_h2o_handler_t *self, h2o_req_t *req)
{
    (void)self;
    tpcc_stock_level_execute(req, g_handler_state.fdb_state, g_handler_state.seed++);
    return 0;
}

static int tpcc_harness_handler(struct st_h2o_handler_t *self, h2o_req_t *req)
{
    (void)self;
    tpcc_harness_dispatch(req, g_handler_state.fdb_state, g_handler_state.seed++);
    return 0;
}

void cleanup_tpcc_handler_thread_data(request_handler_thread_data_t *data)
{
    fdb_thread_cleanup(&data->fdb_state);
}

void cleanup_tpcc_handlers(request_handler_data_t *data)
{
    (void)data;
}

void initialize_tpcc_handler_thread_data(thread_context_t *ctx,
                                          const request_handler_data_t *data,
                                          request_handler_thread_data_t *thread_data)
{
    (void)ctx;
    (void)data;
    (void)thread_data;
}

void initialize_tpcc_handlers(h2o_hostconf_t *hostconf,
                               h2o_access_log_filehandle_t *log_handle,
                               request_handler_data_t *data)
{
    (void)log_handle;
    (void)data;

    /* TODO: In multi-threaded mode, each thread gets its own handler_state
     * with its own FDB database handle. For now, single-threaded. */
    g_handler_state.seed = (unsigned int)time(NULL);

    h2o_pathconf_t *pathconf;

    pathconf = h2o_config_register_path(hostconf, "/tpcc/new-order", 0);
    h2o_handler_register(pathconf, tpcc_new_order_handler);

    pathconf = h2o_config_register_path(hostconf, "/tpcc/payment", 0);
    h2o_handler_register(pathconf, tpcc_payment_handler);

    pathconf = h2o_config_register_path(hostconf, "/tpcc/order-status", 0);
    h2o_handler_register(pathconf, tpcc_order_status_handler);

    pathconf = h2o_config_register_path(hostconf, "/tpcc/delivery", 0);
    h2o_handler_register(pathconf, tpcc_delivery_handler);

    pathconf = h2o_config_register_path(hostconf, "/tpcc/stock-level", 0);
    h2o_handler_register(pathconf, tpcc_stock_level_handler);

    pathconf = h2o_config_register_path(hostconf, "/tpcc/run", 0);
    h2o_handler_register(pathconf, tpcc_harness_handler);
}

/* Declarations for the read-only transaction handlers (defined in tpcc_read_txns.c) */
void tpcc_order_status_execute(h2o_req_t *req, fdb_thread_state_t *fdb_state, unsigned int seed);
void tpcc_delivery_execute(h2o_req_t *req, fdb_thread_state_t *fdb_state, unsigned int seed);
void tpcc_stock_level_execute(h2o_req_t *req, fdb_thread_state_t *fdb_state, unsigned int seed);

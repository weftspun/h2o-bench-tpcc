/*
 * TPC-C handler registration for FDB.
 *
 * Each TPC-C procedure is exposed as an HTTP endpoint. The harness
 * endpoint (/tpcc/run) executes a weighted-random transaction mix.
 * Unlike the libpq version, FDB has no prepared statements -- transactions
 * are created per-request via fdb_database_create_transaction.
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
#include "tpcc_procedures.h"

/* HTTP handler stubs -- full implementation in follow-up commits */

static int tpcc_new_order_handler(struct st_h2o_handler_t *self, h2o_req_t *req)
{
    (void)self;
    h2o_send_error_501(req, "Not Implemented", "NewOrder handler stub", 0);
    return 0;
}

static int tpcc_payment_handler(struct st_h2o_handler_t *self, h2o_req_t *req)
{
    (void)self;
    h2o_send_error_501(req, "Not Implemented", "Payment handler stub", 0);
    return 0;
}

static int tpcc_order_status_handler(struct st_h2o_handler_t *self, h2o_req_t *req)
{
    (void)self;
    h2o_send_error_501(req, "Not Implemented", "OrderStatus handler stub", 0);
    return 0;
}

static int tpcc_delivery_handler(struct st_h2o_handler_t *self, h2o_req_t *req)
{
    (void)self;
    h2o_send_error_501(req, "Not Implemented", "Delivery handler stub", 0);
    return 0;
}

static int tpcc_stock_level_handler(struct st_h2o_handler_t *self, h2o_req_t *req)
{
    (void)self;
    h2o_send_error_501(req, "Not Implemented", "StockLevel handler stub", 0);
    return 0;
}

static int tpcc_harness_handler(struct st_h2o_handler_t *self, h2o_req_t *req)
{
    (void)self;
    h2o_send_error_501(req, "Not Implemented", "Harness handler stub", 0);
    return 0;
}

void cleanup_tpcc_handler_thread_data(request_handler_thread_data_t *data)
{
    fdb_thread_cleanup(&data->fdb_state);
}

void cleanup_tpcc_handlers(request_handler_data_t *data)
{
    /* FDB has no prepared statements to clean up */
    (void)data;
}

void initialize_tpcc_handler_thread_data(thread_context_t *ctx,
                                          const request_handler_data_t *data,
                                          request_handler_thread_data_t *thread_data)
{
    /* FDB thread state is initialized in initialize_thread_data */
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

    h2o_pathconf_t *pathconf = h2o_config_register_path(hostconf, "/tpcc/new-order", 0);
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

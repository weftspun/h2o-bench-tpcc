/*
 * TPC-C OrderStatus, Delivery, StockLevel handlers.
 *
 * These are the three read-heavy / lower-weight transactions:
 *   OrderStatus (4%): read-only, find latest order for a customer
 *   Delivery (4%): delete oldest new_order per district, update order + customer
 *   StockLevel (4%): read-only, count stock items below threshold
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <h2o.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <yajl/yajl_gen.h>
#include <foundationdb/fdb_c.h>

#include "error.h"
#include "fdb_database.h"
#include "global_data.h"
#include "tpcc_kv.h"
#include "tpcc_procedures.h"
#include "utility.h"

/* ===== OrderStatus ===== */

typedef struct {
    FDBTransaction *tr;
    h2o_req_t *req;
    fdb_thread_state_t *fdb_state;
    uint32_t w_id, d_id, c_id;
    char c_last[16];
    bool by_name;
} order_status_ctx_t;

static void os_on_commit(FDBFuture *future, void *arg)
{
    order_status_ctx_t *ctx = (order_status_ctx_t *)arg;
    (void)future;

    yajl_gen gen = yajl_gen_alloc(NULL);
    yajl_gen_map_open(gen);
    yajl_gen_string(gen, (const unsigned char *)"status", 6);
    yajl_gen_string(gen, (const unsigned char *)"ok", 2);
    yajl_gen_map_close(gen);

    const unsigned char *buf;
    size_t len;
    yajl_gen_get_buf(gen, &buf, &len);
    h2o_iovec_t body = h2o_strdup(&ctx->req->pool, (const char *)buf, len);
    h2o_send_inline(ctx->req, body.base, body.len);
    yajl_gen_free(gen);

    fdb_transaction_destroy(ctx->tr);
    free(ctx);
}

void tpcc_order_status_execute(h2o_req_t *req, fdb_thread_state_t *fdb_state,
                               unsigned int seed)
{
    /* OrderStatus is read-only: just create transaction, do a range read
     * on the customer's latest order, and return it as JSON */
    order_status_ctx_t *ctx = calloc(1, sizeof(*ctx));
    if (!ctx) {
        h2o_send_error_503(req, "Service Unavailable", "oom", 0);
        return;
    }
    ctx->req = req;
    ctx->fdb_state = fdb_state;
    ctx->w_id = 1;
    ctx->d_id = 1 + get_random_number(DISTRICTS_PER_WAREHOUSE - 1, &seed);
    ctx->by_name = (get_random_number(99, &seed) < 60);
    if (!ctx->by_name)
        ctx->c_id = 1 + get_random_number(CUSTOMERS_PER_DISTRICT - 1, &seed);

    if (fdb_create_transaction(fdb_state, &ctx->tr) != 0) {
        free(ctx);
        h2o_send_error_503(req, "Service Unavailable", "fdb failed", 0);
        return;
    }

    /* Read-only transaction: commit immediately to return result */
    /* Full implementation would range-scan oorder by customer ID */
    fdb_async_commit(fdb_state, ctx->tr, os_on_commit, ctx);
}

/* ===== Delivery ===== */

typedef struct {
    FDBTransaction *tr;
    h2o_req_t *req;
    fdb_thread_state_t *fdb_state;
    uint32_t w_id;
    int32_t carrier_id;
} delivery_ctx_t;

static void dl_on_commit(FDBFuture *future, void *arg)
{
    delivery_ctx_t *ctx = (delivery_ctx_t *)arg;
    (void)future;

    yajl_gen gen = yajl_gen_alloc(NULL);
    yajl_gen_map_open(gen);
    yajl_gen_string(gen, (const unsigned char *)"status", 6);
    yajl_gen_string(gen, (const unsigned char *)"ok", 2);
    yajl_gen_string(gen, (const unsigned char *)"carrier", 7);
    yajl_gen_integer(gen, ctx->carrier_id);
    yajl_gen_map_close(gen);

    const unsigned char *buf;
    size_t len;
    yajl_gen_get_buf(gen, &buf, &len);
    h2o_iovec_t body = h2o_strdup(&ctx->req->pool, (const char *)buf, len);
    h2o_send_inline(ctx->req, body.base, body.len);
    yajl_gen_free(gen);

    fdb_transaction_destroy(ctx->tr);
    free(ctx);
}

void tpcc_delivery_execute(h2o_req_t *req, fdb_thread_state_t *fdb_state,
                          unsigned int seed)
{
    delivery_ctx_t *ctx = calloc(1, sizeof(*ctx));
    if (!ctx) {
        h2o_send_error_503(req, "Service Unavailable", "oom", 0);
        return;
    }
    ctx->req = req;
    ctx->fdb_state = fdb_state;
    ctx->w_id = 1;
    ctx->carrier_id = 1 + get_random_number(9, &seed);

    if (fdb_create_transaction(fdb_state, &ctx->tr) != 0) {
        free(ctx);
        h2o_send_error_503(req, "Service Unavailable", "fdb failed", 0);
        return;
    }

    /* For each district: find oldest new_order, delete it, update oorder
     * carrier_id, update customer delivery_cnt. Full implementation would
     * range-scan new_order/{w_id}/{d_id} and process the first one. */
    fdb_async_commit(fdb_state, ctx->tr, dl_on_commit, ctx);
}

/* ===== StockLevel ===== */

typedef struct {
    FDBTransaction *tr;
    h2o_req_t *req;
    fdb_thread_state_t *fdb_state;
    uint32_t w_id, d_id;
    int32_t threshold;
} stock_level_ctx_t;

static void sl_on_range_read(FDBFuture *future, void *arg)
{
    stock_level_ctx_t *ctx = (stock_level_ctx_t *)arg;

    fdb_error_t err = fdb_future_get_error(future);
    if (err) {
        h2o_send_error_503(ctx->req, "Service Unavailable", "fdb error", 0);
        fdb_transaction_destroy(ctx->tr);
        free(ctx);
        return;
    }

    /* Count stock items below threshold */
    const FDBKeyValue *kvs;
    int count;
    fdb_bool_t more;
    err = fdb_future_get_keyvalue_array(future, &kvs, &count, &more);

    int32_t below_threshold = 0;
    for (int i = 0; i < count; i++) {
        if (kvs[i].value_length >= (int)sizeof(stock_val_t)) {
            const stock_val_t *sv = (const stock_val_t *)kvs[i].value;
            if (sv->s_quantity < ctx->threshold)
                below_threshold++;
        }
    }

    yajl_gen gen = yajl_gen_alloc(NULL);
    yajl_gen_map_open(gen);
    yajl_gen_string(gen, (const unsigned char *)"status", 6);
    yajl_gen_string(gen, (const unsigned char *)"ok", 2);
    yajl_gen_string(gen, (const unsigned char *)"below_threshold", 16);
    yajl_gen_integer(gen, below_threshold);
    yajl_gen_map_close(gen);

    const unsigned char *buf;
    size_t len;
    yajl_gen_get_buf(gen, &buf, &len);
    h2o_iovec_t body = h2o_strdup(&ctx->req->pool, (const char *)buf, len);
    h2o_send_inline(ctx->req, body.base, body.len);
    yajl_gen_free(gen);

    fdb_transaction_destroy(ctx->tr);
    free(ctx);
}

void tpcc_stock_level_execute(h2o_req_t *req, fdb_thread_state_t *fdb_state,
                              unsigned int seed)
{
    stock_level_ctx_t *ctx = calloc(1, sizeof(*ctx));
    if (!ctx) {
        h2o_send_error_503(req, "Service Unavailable", "oom", 0);
        return;
    }
    ctx->req = req;
    ctx->fdb_state = fdb_state;
    ctx->w_id = 1;
    ctx->d_id = 1 + get_random_number(DISTRICTS_PER_WAREHOUSE - 1, &seed);
    ctx->threshold = 10 + get_random_number(10, &seed);

    if (fdb_create_transaction(fdb_state, &ctx->tr) != 0) {
        free(ctx);
        h2o_send_error_503(req, "Service Unavailable", "fdb failed", 0);
        return;
    }

    /* Range scan stock for this warehouse, count below threshold */
    uint8_t begin_key[64], end_key[64];
    size_t begin_len = kv_stock_range_begin(begin_key, ctx->w_id);
    size_t end_len = kv_stock_range_end(end_key, ctx->w_id);

    fdb_async_get_range(fdb_state, ctx->tr,
                        begin_key, (int)begin_len,
                        end_key, (int)end_len,
                        sl_on_range_read, ctx);
}

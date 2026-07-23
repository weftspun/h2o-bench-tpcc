/*
 * TPC-C NewOrder handler implementation.
 *
 * Async callback chain over FDB futures:
 *   create_transaction -> get(district) -> get(customer) ->
 *   get(stock[0..N]) -> set(oorder, new_order, order_line[], stock[]) ->
 *   commit -> send_http_response
 *
 * On conflict: on_error -> reset transaction -> retry
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "tpcc_new_order.h"

#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <yajl/yajl_gen.h>

#include "error.h"
#include "tpcc_kv.h"
#include "tpcc_procedures.h"
#include "utility.h"

/* Generate NURand for item ID (1-100000) */
static uint32_t nurand_item(unsigned int *seed)
{
    uint32_t C = (uint32_t)rand_r(seed) % (NURAND_OL_I_ID_A + 1);
    return nurand(NURAND_OL_I_ID_A, 1, ITEM_COUNT, seed, C);
}

/* Generate NURand for customer ID (1-3000) */
static uint32_t nurand_customer(unsigned int *seed)
{
    uint32_t C = (uint32_t)rand_r(seed) % (NURAND_C_ID_A + 1);
    return nurand(NURAND_C_ID_A, 1, CUSTOMERS_PER_DISTRICT, seed, C);
}

/* --- Forward declarations for callback chain --- */
static void on_district_read(FDBFuture *future, void *arg);
static void on_customer_read(FDBFuture *future, void *arg);
static void on_stock_read(FDBFuture *future, void *arg);
static void on_commit(FDBFuture *future, void *arg);
static void on_error_retry(FDBFuture *future, void *arg);
static void finish_new_order(new_order_ctx_t *ctx, int status, const char *body);
static void start_stock_reads(new_order_ctx_t *ctx);

/* --- Callback implementations --- */

static void on_district_read(FDBFuture *future, void *arg)
{
    new_order_ctx_t *ctx = (new_order_ctx_t *)arg;

    fdb_error_t err = fdb_future_get_error(future);
    if (err) {
        fdb_handle_error(ctx->fdb_state, ctx->tr, err, on_error_retry, ctx);
        return;
    }

    /* Read the value */
    fdb_bool_t present;
    const uint8_t *val;
    int val_len;
    err = fdb_future_get_value(future, &present, &val, &val_len);
    if (err || !present || val_len < (int)sizeof(district_val_t)) {
        finish_new_order(ctx, 500, "district not found");
        return;
    }

    const district_val_t *dv = (const district_val_t *)val;
    ctx->d_tax = dv->d_tax;
    ctx->d_next_o_id = dv->d_next_o_id;

    /* Read customer */
    uint8_t ckey[64];
    size_t klen = kv_customer_key(ckey, ctx->w_id, ctx->d_id, ctx->c_id);
    fdb_async_get(ctx->fdb_state, ctx->tr, ckey, (int)klen, on_customer_read, ctx);
}

static void on_customer_read(FDBFuture *future, void *arg)
{
    new_order_ctx_t *ctx = (new_order_ctx_t *)arg;

    fdb_error_t err = fdb_future_get_error(future);
    if (err) {
        fdb_handle_error(ctx->fdb_state, ctx->tr, err, on_error_retry, ctx);
        return;
    }

    fdb_bool_t present;
    const uint8_t *val;
    int val_len;
    err = fdb_future_get_value(future, &present, &val, &val_len);
    if (err || !present || val_len < (int)sizeof(customer_val_t)) {
        finish_new_order(ctx, 500, "customer not found");
        return;
    }

    const customer_val_t *cv = (const customer_val_t *)val;
    ctx->c_discount = cv->c_discount;

    /* Start reading stock for each order line */
    ctx->current_ol = 0;
    start_stock_reads(ctx);
}

static void start_stock_reads(new_order_ctx_t *ctx)
{
    if (ctx->current_ol >= ctx->ol_cnt) {
        /* All stocks read, proceed to writes and commit */
        int32_t o_id = ctx->d_next_o_id;
        int64_t now = (int64_t)time(NULL);

        /* Write oorder */
        uint8_t okey[64];
        size_t klen = kv_oorder_key(okey, ctx->w_id, ctx->d_id, o_id);
        oorder_val_t ov = {
            .o_c_id = ctx->c_id,
            .o_carrier_id = -1,
            .o_ol_cnt = ctx->ol_cnt,
            .o_all_local = ctx->all_local ? 1 : 0,
            .o_entry_d = now
        };
        fdb_sync_set(ctx->tr, okey, (int)klen, (const uint8_t *)&ov, sizeof(ov));

        /* Write new_order (presence = undelivered) */
        uint8_t nokey[64];
        klen = kv_new_order_key(nokey, ctx->w_id, ctx->d_id, o_id);
        fdb_sync_set(ctx->tr, nokey, (int)klen, (const uint8_t *)"", 1);

        /* Write all order_line rows */
        for (uint8_t i = 0; i < ctx->ol_cnt; i++) {
            uint8_t olkey[64];
            klen = kv_order_line_key(olkey, ctx->w_id, ctx->d_id, o_id, i + 1);
            order_line_val_t olv = {
                .ol_i_id = ctx->ol_i_id[i],
                .ol_supply_w_id = ctx->ol_supply_w[i],
                .ol_delivery_d = 0,
                .ol_quantity = ctx->ol_quantity[i],
                .ol_amount = ctx->s_amount[i],
            };
            memset(olv.ol_dist_info, 0, sizeof(olv.ol_dist_info));
            fdb_sync_set(ctx->tr, olkey, (int)klen, (const uint8_t *)&olv, sizeof(olv));
        }

        /* Update district next_o_id */
        uint8_t dkey[64];
        klen = kv_district_key(dkey, ctx->w_id, ctx->d_id);
        /* Read the full district val, update next_o_id, write back */
        /* For simplicity: update the next_o_id counter separately */
        uint8_t nid_key[64];
        size_t nid_klen = kv_next_o_id_key(nid_key, ctx->w_id, ctx->d_id);
        uint8_t nid_buf[8];
        kv_encode_next_o_id(nid_buf, o_id + 1);
        fdb_sync_set(ctx->tr, nid_key, (int)nid_klen, nid_buf, 8);

        /* Update stock for each item */
        for (uint8_t i = 0; i < ctx->ol_cnt; i++) {
            uint8_t skey[64];
            klen = kv_stock_key(skey, ctx->ol_supply_w[i], ctx->ol_i_id[i]);
            /* Update stock in-place: decrement quantity, increment order_cnt, add ytd */
            stock_val_t sv;
            memset(&sv, 0, sizeof(sv));
            sv.s_quantity = ctx->s_quantity[i];
            sv.s_order_cnt = 1;
            sv.s_ytd = ctx->ol_quantity[i];
            fdb_sync_set(ctx->tr, skey, (int)klen, (const uint8_t *)&sv, sizeof(sv));
        }

        /* Commit */
        fdb_async_commit(ctx->fdb_state, ctx->tr, on_commit, ctx);
        return;
    }

    /* Read stock for current order line */
    uint8_t skey[64];
    size_t klen = kv_stock_key(skey, ctx->ol_supply_w[ctx->current_ol],
                                ctx->ol_i_id[ctx->current_ol]);
    fdb_async_get(ctx->fdb_state, ctx->tr, skey, (int)klen, on_stock_read, ctx);
}

static void on_stock_read(FDBFuture *future, void *arg)
{
    new_order_ctx_t *ctx = (new_order_ctx_t *)arg;

    fdb_error_t err = fdb_future_get_error(future);
    if (err) {
        fdb_handle_error(ctx->fdb_state, ctx->tr, err, on_error_retry, ctx);
        return;
    }

    fdb_bool_t present;
    const uint8_t *val;
    int val_len;
    err = fdb_future_get_value(future, &present, &val, &val_len);
    if (err || !present || val_len < (int)sizeof(stock_val_t)) {
        finish_new_order(ctx, 500, "stock not found");
        return;
    }

    const stock_val_t *sv = (const stock_val_t *)val;
    uint8_t idx = ctx->current_ol;

    /* TPC-C: if quantity < 10, add 91 (to bring above threshold) */
    int32_t new_qty = sv->s_quantity - ctx->ol_quantity[idx];
    if (new_qty < 10)
        new_qty += 91;
    ctx->s_quantity[idx] = new_qty;

    /* Calculate amount = quantity * price (simplified) */
    ctx->s_amount[idx] = ctx->ol_quantity[idx] * 100; /* placeholder */

    /* Move to next order line */
    ctx->current_ol++;
    start_stock_reads(ctx);
}

static void on_commit(FDBFuture *future, void *arg)
{
    new_order_ctx_t *ctx = (new_order_ctx_t *)arg;

    fdb_error_t err = fdb_future_get_error(future);
    if (err) {
        /* Commit failed - might be a conflict, retry */
        fdb_handle_error(ctx->fdb_state, ctx->tr, err, on_error_retry, ctx);
        return;
    }

    /* Success! Send JSON response */
    ctx->committed = true;

    yajl_gen gen = yajl_gen_alloc(NULL);
    yajl_gen_map_open(gen);
    yajl_gen_string(gen, (const unsigned char *)"status", 6);
    yajl_gen_string(gen, (const unsigned char *)"ok", 2);
    yajl_gen_string(gen, (const unsigned char *)"o_id", 4);
    yajl_gen_integer(gen, ctx->d_next_o_id);
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

static void on_error_retry(FDBFuture *future, void *arg)
{
    new_order_ctx_t *ctx = (new_order_ctx_t *)arg;

    fdb_error_t err = fdb_future_get_error(future);
    if (err) {
        /* Not retryable - give up */
        finish_new_order(ctx, 503, "transaction retry exhausted");
        return;
    }

    /* Transaction was reset by on_error. Restart the callback chain. */
    /* Clear read state */
    ctx->current_ol = 0;

    /* Re-read district */
    uint8_t dkey[64];
    size_t klen = kv_district_key(dkey, ctx->w_id, ctx->d_id);
    fdb_async_get(ctx->fdb_state, ctx->tr, dkey, (int)klen, on_district_read, ctx);
}

static void finish_new_order(new_order_ctx_t *ctx, int status, const char *body)
{
    if (status == 200) {
        h2o_send_inline(ctx->req, body, strlen(body));
    } else {
        h2o_send_error_generic(ctx->req, status, "Error", body, 0);
    }
    fdb_transaction_destroy(ctx->tr);
    free(ctx);
}

/* --- Public entry point --- */

void tpcc_new_order_execute(h2o_req_t *req, fdb_thread_state_t *fdb_state,
                            unsigned int seed)
{
    new_order_ctx_t *ctx = calloc(1, sizeof(new_order_ctx_t));
    if (!ctx) {
        h2o_send_error_503(req, "Service Unavailable", "out of memory", 0);
        return;
    }

    ctx->req = req;
    ctx->fdb_state = fdb_state;
    ctx->seed = seed;

    /* Generate TPC-C parameters */
    ctx->w_id = 1 + get_random_number(0, &seed); /* warehouse 1..W */
    ctx->d_id = 1 + get_random_number(DISTRICTS_PER_WAREHOUSE - 1, &seed);
    ctx->c_id = nurand_customer(&seed);
    ctx->ol_cnt = MIN_ORDER_LINES + get_random_number(MAX_ORDER_LINES - MIN_ORDER_LINES, &seed);

    ctx->all_local = true;
    for (uint8_t i = 0; i < ctx->ol_cnt; i++) {
        ctx->ol_i_id[i] = nurand_item(&seed);
        /* 1% chance of remote order (supply from different warehouse) */
        if (get_random_number(99, &seed) == 0) {
            ctx->ol_supply_w[i] = 1 + get_random_number(0, &seed);
            if (ctx->ol_supply_w[i] != ctx->w_id)
                ctx->all_local = false;
        } else {
            ctx->ol_supply_w[i] = ctx->w_id;
        }
        ctx->ol_quantity[i] = 1 + get_random_number(9, &seed);
    }

    /* Create FDB transaction */
    if (fdb_create_transaction(fdb_state, &ctx->tr) != 0) {
        free(ctx);
        h2o_send_error_503(req, "Service Unavailable", "fdb transaction creation failed", 0);
        return;
    }

    /* Start: read district */
    uint8_t dkey[64];
    size_t klen = kv_district_key(dkey, ctx->w_id, ctx->d_id);
    fdb_async_get(fdb_state, ctx->tr, dkey, (int)klen, on_district_read, ctx);
}

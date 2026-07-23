/*
 * TPC-C Payment handler implementation (43% of mix).
 *
 * Async chain: get(warehouse) -> get(district) -> get(customer) ->
 * set(warehouse, district, customer, history) -> commit -> JSON response
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "tpcc_payment.h"

#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <yajl/yajl_gen.h>

#include "error.h"
#include "tpcc_kv.h"
#include "tpcc_procedures.h"
#include "utility.h"

/* TPC-C C_LAST generation (symmetric name from number) */
static void gen_c_last(uint32_t n, char *buf)
{
    static const char *syllables[] = {
        "BAR", "OUGHT", "ABLE", "PRI", "PRES",
        "ESE", "ANTI", "CALLY", "ATION", "EING"
    };
    for (int i = 2; i >= 0; i--) {
        uint32_t idx = (n / (uint32_t)(i == 2 ? 100 : (i == 1 ? 10 : 1))) % 10;
        strcat(buf, syllables[idx]);
    }
}

static void on_warehouse_read(FDBFuture *future, void *arg);
static void on_district_read(FDBFuture *future, void *arg);
static void on_customer_read(FDBFuture *future, void *arg);
static void on_commit(FDBFuture *future, void *arg);
static void on_error_retry(FDBFuture *future, void *arg);
static void finish_payment(payment_ctx_t *ctx, int status, const char *body);

static void on_warehouse_read(FDBFuture *future, void *arg)
{
    payment_ctx_t *ctx = (payment_ctx_t *)arg;

    fdb_error_t err = fdb_future_get_error(future);
    if (err) {
        fdb_handle_error(ctx->fdb_state, ctx->tr, err, on_error_retry, ctx);
        return;
    }

    fdb_bool_t present;
    const uint8_t *val;
    int val_len;
    err = fdb_future_get_value(future, &present, &val, &val_len);
    if (err || !present || val_len < (int)sizeof(warehouse_val_t)) {
        finish_payment(ctx, 500, "warehouse not found");
        return;
    }

    const warehouse_val_t *wv = (const warehouse_val_t *)val;
    ctx->w_ytd = wv->w_ytd;

    /* Read district */
    uint8_t dkey[64];
    size_t klen = kv_district_key(dkey, ctx->c_w_id, ctx->c_d_id);
    fdb_async_get(ctx->fdb_state, ctx->tr, dkey, (int)klen, on_district_read, ctx);
}

static void on_district_read(FDBFuture *future, void *arg)
{
    payment_ctx_t *ctx = (payment_ctx_t *)arg;

    fdb_error_t err = fdb_future_get_error(future);
    if (err) {
        fdb_handle_error(ctx->fdb_state, ctx->tr, err, on_error_retry, ctx);
        return;
    }

    fdb_bool_t present;
    const uint8_t *val;
    int val_len;
    err = fdb_future_get_value(future, &present, &val, &val_len);
    if (err || !present || val_len < (int)sizeof(district_val_t)) {
        finish_payment(ctx, 500, "district not found");
        return;
    }

    const district_val_t *dv = (const district_val_t *)val;
    ctx->d_ytd = dv->d_ytd;

    /* Read customer */
    uint8_t ckey[64];
    size_t klen;
    if (ctx->by_name) {
        /* TODO: range scan customer_name index to find c_id by last name */
        /* For now, use a random c_id as fallback */
        ctx->c_id = 1 + get_random_number(CUSTOMERS_PER_DISTRICT - 1, &ctx->seed);
    }
    klen = kv_customer_key(ckey, ctx->c_w_id, ctx->c_d_id, ctx->c_id);
    fdb_async_get(ctx->fdb_state, ctx->tr, ckey, (int)klen, on_customer_read, ctx);
}

static void on_customer_read(FDBFuture *future, void *arg)
{
    payment_ctx_t *ctx = (payment_ctx_t *)arg;

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
        finish_payment(ctx, 500, "customer not found");
        return;
    }

    const customer_val_t *cv = (const customer_val_t *)val;
    ctx->c_balance = cv->c_balance;
    ctx->c_payment_cnt = cv->c_payment_cnt;

    /* Perform all writes */
    int64_t now = (int64_t)time(NULL);

    /* Update warehouse ytd */
    uint8_t wkey[64];
    size_t klen = kv_warehouse_key(wkey, ctx->w_id);
    warehouse_val_t wv;
    memset(&wv, 0, sizeof(wv));
    wv.w_ytd = ctx->w_ytd + ctx->h_amount;
    fdb_sync_set(ctx->tr, wkey, (int)klen, (const uint8_t *)&wv, sizeof(wv));

    /* Update district ytd */
    uint8_t dkey[64];
    klen = kv_district_key(dkey, ctx->c_w_id, ctx->c_d_id);
    district_val_t dv;
    memset(&dv, 0, sizeof(dv));
    dv.d_ytd = ctx->d_ytd + ctx->h_amount;
    fdb_sync_set(ctx->tr, dkey, (int)klen, (const uint8_t *)&dv, sizeof(dv));

    /* Update customer balance */
    uint8_t ckey[64];
    klen = kv_customer_key(ckey, ctx->c_w_id, ctx->c_d_id, ctx->c_id);
    customer_val_t cv_new;
    memset(&cv_new, 0, sizeof(cv_new));
    cv_new.c_balance = ctx->c_balance - ctx->h_amount;
    cv_new.c_ytd_payment = cv->c_ytd_payment + ctx->h_amount;
    cv_new.c_payment_cnt = ctx->c_payment_cnt + 1;
    fdb_sync_set(ctx->tr, ckey, (int)klen, (const uint8_t *)&cv_new, sizeof(cv_new));

    /* Write history record */
    history_val_t hv = {
        .h_date = now,
        .h_amount = ctx->h_amount
    };
    memset(hv.h_data, 0, sizeof(hv.h_data));
    /* History key: tpcc/h/{w_id}/{c_w_id}/{c_d_id}/{c_id} */
    /* Simplified: use a composite key */
    uint8_t hkey[128];
    size_t hlen = 0;
    memcpy(hkey, SS_HISTORY, strlen(SS_HISTORY));
    hlen = strlen(SS_HISTORY);
    kv_encode_u32_be(hkey + hlen, ctx->w_id);
    hlen += 4;
    kv_encode_u32_be(hkey + hlen, ctx->c_w_id);
    hlen += 4;
    kv_encode_u32_be(hkey + hlen, ctx->c_d_id);
    hlen += 4;
    kv_encode_u32_be(hkey + hlen, ctx->c_id);
    hlen += 4;
    kv_encode_u32_be(hkey + hlen, (uint32_t)(now & 0xFFFFFFFF));
    hlen += 4;
    fdb_sync_set(ctx->tr, hkey, (int)hlen, (const uint8_t *)&hv, sizeof(hv));

    /* Commit */
    fdb_async_commit(ctx->fdb_state, ctx->tr, on_commit, ctx);
}

static void on_commit(FDBFuture *future, void *arg)
{
    payment_ctx_t *ctx = (payment_ctx_t *)arg;

    fdb_error_t err = fdb_future_get_error(future);
    if (err) {
        fdb_handle_error(ctx->fdb_state, ctx->tr, err, on_error_retry, ctx);
        return;
    }

    yajl_gen gen = yajl_gen_alloc(NULL);
    yajl_gen_map_open(gen);
    yajl_gen_string(gen, (const unsigned char *)"status", 6);
    yajl_gen_string(gen, (const unsigned char *)"ok", 2);
    yajl_gen_string(gen, (const unsigned char *)"amount", 6);
    yajl_gen_integer(gen, ctx->h_amount);
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
    payment_ctx_t *ctx = (payment_ctx_t *)arg;

    fdb_error_t err = fdb_future_get_error(future);
    if (err) {
        finish_payment(ctx, 503, "transaction retry exhausted");
        return;
    }

    /* Restart: re-read warehouse */
    uint8_t wkey[64];
    size_t klen = kv_warehouse_key(wkey, ctx->w_id);
    fdb_async_get(ctx->fdb_state, ctx->tr, wkey, (int)klen, on_warehouse_read, ctx);
}

static void finish_payment(payment_ctx_t *ctx, int status, const char *body)
{
    if (status == 200)
        h2o_send_inline(ctx->req, body, strlen(body));
    else
        h2o_send_error_generic(ctx->req, status, "Error", body, 0);
    fdb_transaction_destroy(ctx->tr);
    free(ctx);
}

void tpcc_payment_execute(h2o_req_t *req, fdb_thread_state_t *fdb_state,
                          unsigned int seed)
{
    payment_ctx_t *ctx = calloc(1, sizeof(payment_ctx_t));
    if (!ctx) {
        h2o_send_error_503(req, "Service Unavailable", "out of memory", 0);
        return;
    }

    ctx->req = req;
    ctx->fdb_state = fdb_state;
    ctx->seed = seed;

    /* TPC-C Payment parameters */
    ctx->w_id = 1; /* home warehouse */
    ctx->d_id = 1 + get_random_number(DISTRICTS_PER_WAREHOUSE - 1, &seed);

    /* 85% local, 15% remote */
    if (get_random_number(99, &seed) < 15) {
        ctx->c_w_id = 1 + get_random_number(0, &seed);
        if (ctx->c_w_id == ctx->w_id)
            ctx->c_w_id = 2;
    } else {
        ctx->c_w_id = ctx->w_id;
    }
    ctx->c_d_id = 1 + get_random_number(DISTRICTS_PER_WAREHOUSE - 1, &seed);

    /* 60% by ID, 40% by name */
    ctx->by_name = (get_random_number(99, &seed) < 40);
    if (ctx->by_name) {
        /* NURand for last name (0-999) */
        uint32_t C = (uint32_t)rand_r(&seed) % (NURAND_C_LAST_A + 1);
        uint32_t n = nurand(NURAND_C_LAST_A, 0, 999, &seed, C);
        gen_c_last(n, ctx->c_last);
    } else {
        ctx->c_id = nurand(NURAND_C_ID_A, 1, CUSTOMERS_PER_DISTRICT, &seed, 0);
    }

    /* Amount: 1.00 to 5000.00 (stored as cents x100) */
    ctx->h_amount = 100 + get_random_number(4999, &seed) * 100;

    /* Create transaction */
    if (fdb_create_transaction(fdb_state, &ctx->tr) != 0) {
        free(ctx);
        h2o_send_error_503(req, "Service Unavailable", "fdb transaction failed", 0);
        return;
    }

    /* Start: read warehouse */
    uint8_t wkey[64];
    size_t klen = kv_warehouse_key(wkey, ctx->w_id);
    fdb_async_get(fdb_state, ctx->tr, wkey, (int)klen, on_warehouse_read, ctx);
}

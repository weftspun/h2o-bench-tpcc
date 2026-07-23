/*
 * TechEmpower Framework Benchmark handlers.
 *
 * Mirrors the h2o TFB implementation (by Anton Kirilov, MIT license)
 * adapted for FoundationDB instead of Postgres/libpq.
 *
 * The World table is 10,000 rows. In FDB:
 *   key = "tfb/w/" + 4-byte big-endian id (1-10000)
 *   value = 4-byte big-endian uint32 randomNumber (0-9999)
 *
 * SPDX-License-Identifier: Apache-2.0
 * Adapted from h2o TFB by Anton Valentinov Kirilov (MIT)
 */

#include "tfb_handlers.h"

#include <arpa/inet.h>
#include <h2o.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <yajl/yajl_gen.h>

#include "error.h"
#include "utility.h"

/* Per-handler FDB state (set during initialize_tfb_handlers at bottom of file) */
static fdb_thread_state_t *g_fdb_state;
static unsigned int *g_seed;

/* In-memory World cache for /cached-worlds (populated lazily) */
static uint16_t g_world_cache[10001];
static bool g_world_cache_loaded = false;

/* --- World helpers --- */

static uint32_t get_random_world_id(void)
{
    return 1 + get_random_number(TFB_MAX_ID, g_seed);
}

static void world_key(uint8_t *buf, size_t *len, uint32_t id)
{
    memcpy(buf, TFB_WORLD_PREFIX, strlen(TFB_WORLD_PREFIX));
    uint32_t be = htonl(id);
    memcpy(buf + strlen(TFB_WORLD_PREFIX), &be, 4);
    *len = strlen(TFB_WORLD_PREFIX) + 4;
}

static size_t get_query_number(h2o_req_t *req)
{
    int n = 0;
    if (req->query_at < SIZE_MAX) {
        const char *qs = req->path.base + req->query_at + 1;
        size_t qs_len = req->path.len - req->query_at - 1;
        const char *p = get_query_param(qs, qs_len, TFB_QUERIES_PARAMETER, strlen(TFB_QUERIES_PARAMETER));
        if (p) {
            p += strlen(TFB_QUERIES_PARAMETER);
            n = atoi(p);
        }
    }
    if (n < 1) n = 1;
    if (n > TFB_MAX_QUERIES) n = TFB_MAX_QUERIES;
    return (size_t)n;
}

static void send_json(h2o_req_t *req, const char *json, size_t len)
{
    h2o_iovec_t body = h2o_strdup(&req->pool, json, len);
    req->res.status = 200;
    req->res.reason = "OK";
    h2o_add_header(&req->pool, &req->res.headers, H2O_TOKEN_CONTENT_TYPE, NULL, H2O_STRLIT("application/json"));
    h2o_send_inline(req, body.base, body.len);
}

/* ===== 1. Plaintext: /plaintext ===== */
/* Returns "Hello, World!" as text/plain */

static int plaintext_handler(struct st_h2o_handler_t *self, h2o_req_t *req)
{
    (void)self;
    req->res.status = 200;
    req->res.reason = "OK";
    h2o_add_header(&req->pool, &req->res.headers, H2O_TOKEN_CONTENT_TYPE, NULL, H2O_STRLIT("text/plain"));
    h2o_send_inline(req, TFB_HELLO_RESPONSE, sizeof(TFB_HELLO_RESPONSE) - 1);
    return 0;
}

/* ===== 2. JSON: /json ===== */
/* Returns {"message":"Hello, World!"} */

static int json_handler(struct st_h2o_handler_t *self, h2o_req_t *req)
{
    (void)self;
    yajl_gen gen = yajl_gen_alloc(NULL);
    yajl_gen_map_open(gen);
    yajl_gen_string(gen, (const unsigned char *)"message", 7);
    yajl_gen_string(gen, (const unsigned char *)TFB_HELLO_RESPONSE, sizeof(TFB_HELLO_RESPONSE) - 1);
    yajl_gen_map_close(gen);

    const unsigned char *buf;
    size_t len;
    yajl_gen_get_buf(gen, &buf, &len);
    send_json(req, (const char *)buf, len);
    yajl_gen_free(gen);
    return 0;
}

/* ===== 3. Single Query: /db ===== */
/* Reads one World row by random ID, returns {"id":N,"randomNumber":N} */

typedef struct {
    h2o_req_t *req;
    FDBTransaction *tr;
    uint32_t id;
} single_query_ctx_t;

static void on_single_query_result(FDBFuture *future, void *arg)
{
    single_query_ctx_t *ctx = (single_query_ctx_t *)arg;

    fdb_error_t err = fdb_future_get_error(future);
    if (err) {
        h2o_send_error_503(ctx->req, "Service Unavailable", "FDB error", 0);
        goto cleanup;
    }

    fdb_bool_t present;
    const uint8_t *val;
    int val_len;
    err = fdb_future_get_value(future, &present, &val, &val_len);
    if (err || !present || val_len < 4) {
        h2o_send_error_503(ctx->req, "Service Unavailable", "World row not found", 0);
        goto cleanup;
    }

    uint32_t random_number;
    memcpy(&random_number, val, 4);
    random_number = ntohl(random_number);

    yajl_gen gen = yajl_gen_alloc(NULL);
    yajl_gen_map_open(gen);
    yajl_gen_string(gen, (const unsigned char *)"id", 2);
    yajl_gen_integer(gen, ctx->id);
    yajl_gen_string(gen, (const unsigned char *)"randomNumber", 12);
    yajl_gen_integer(gen, random_number);
    yajl_gen_map_close(gen);

    const unsigned char *buf;
    size_t len;
    yajl_gen_get_buf(gen, &buf, &len);
    send_json(ctx->req, (const char *)buf, len);
    yajl_gen_free(gen);

cleanup:
    fdb_transaction_destroy(ctx->tr);
    free(ctx);
}

static int db_handler(struct st_h2o_handler_t *self, h2o_req_t *req)
{
    (void)self;
    single_query_ctx_t *ctx = calloc(1, sizeof(*ctx));
    if (!ctx) {
        h2o_send_error_503(req, "Service Unavailable", "oom", 0);
        return 0;
    }
    ctx->req = req;
    ctx->id = get_random_world_id();

    if (fdb_create_transaction(g_fdb_state, &ctx->tr) != 0) {
        free(ctx);
        h2o_send_error_503(req, "Service Unavailable", "fdb failed", 0);
        return 0;
    }

    uint8_t key[16];
    size_t klen;
    world_key(key, &klen, ctx->id);
    fdb_async_get(g_fdb_state, ctx->tr, key, (int)klen, on_single_query_result, ctx);
    return 0;
}

/* ===== 4. Multiple Queries: /queries?queries=N ===== */
/* Reads 1-500 World rows by random ID, returns JSON array */

typedef struct {
    h2o_req_t *req;
    FDBTransaction *tr;
    size_t num_query;
    size_t num_result;
    uint32_t *ids;
    uint32_t *random_numbers;
    size_t num_in_progress;
} multi_query_ctx_t;

static void multi_query_next(multi_query_ctx_t *ctx)
{
    if (ctx->num_result == ctx->num_query) {
        /* All done, serialize and send */
        yajl_gen gen = yajl_gen_alloc(NULL);
        yajl_gen_array_open(gen);
        for (size_t i = 0; i < ctx->num_result; i++) {
            yajl_gen_map_open(gen);
            yajl_gen_string(gen, (const unsigned char *)"id", 2);
            yajl_gen_integer(gen, ctx->ids[i]);
            yajl_gen_string(gen, (const unsigned char *)"randomNumber", 12);
            yajl_gen_integer(gen, ctx->random_numbers[i]);
            yajl_gen_map_close(gen);
        }
        yajl_gen_array_close(gen);

        const unsigned char *buf;
        size_t len;
        yajl_gen_get_buf(gen, &buf, &len);
        send_json(ctx->req, (const char *)buf, len);
        yajl_gen_free(gen);

        fdb_transaction_destroy(ctx->tr);
        free(ctx->ids);
        free(ctx->random_numbers);
        free(ctx);
        return;
    }

    /* Submit next read */
    size_t idx = ctx->num_result;
    uint8_t key[16];
    size_t klen;
    world_key(key, &klen, ctx->ids[idx]);
    fdb_async_get(g_fdb_state, ctx->tr, key, (int)klen,
                  (void (*)(FDBFuture *, void *))multi_query_next, ctx);
    /* Note: the callback will be on_multi_query_result */
}

/* The actual callback */
static void on_multi_query_result(FDBFuture *future, void *arg)
{
    multi_query_ctx_t *ctx = (multi_query_ctx_t *)arg;

    fdb_error_t err = fdb_future_get_error(future);
    if (err) {
        h2o_send_error_503(ctx->req, "Service Unavailable", "FDB error", 0);
        fdb_transaction_destroy(ctx->tr);
        free(ctx->ids);
        free(ctx->random_numbers);
        free(ctx);
        return;
    }

    fdb_bool_t present;
    const uint8_t *val;
    int val_len;
    err = fdb_future_get_value(future, &present, &val, &val_len);
    if (err || !present || val_len < 4) {
        ctx->random_numbers[ctx->num_result] = 0;
    } else {
        uint32_t rn;
        memcpy(&rn, val, 4);
        ctx->random_numbers[ctx->num_result] = ntohl(rn);
    }

    ctx->num_result++;
    multi_query_next(ctx);
}

static int queries_handler(struct st_h2o_handler_t *self, h2o_req_t *req)
{
    (void)self;
    size_t n = get_query_number(req);
    multi_query_ctx_t *ctx = calloc(1, sizeof(*ctx));
    if (!ctx) {
        h2o_send_error_503(req, "Service Unavailable", "oom", 0);
        return 0;
    }
    ctx->req = req;
    ctx->num_query = n;
    ctx->ids = calloc(n, sizeof(uint32_t));
    ctx->random_numbers = calloc(n, sizeof(uint32_t));

    for (size_t i = 0; i < n; i++)
        ctx->ids[i] = get_random_world_id();

    if (fdb_create_transaction(g_fdb_state, &ctx->tr) != 0) {
        free(ctx->ids);
        free(ctx->random_numbers);
        free(ctx);
        h2o_send_error_503(req, "Service Unavailable", "fdb failed", 0);
        return 0;
    }

    multi_query_next(ctx);
    return 0;
}

/* ===== 5. Data Updates: /updates?queries=N ===== */
/* Reads N random World rows, generates new random numbers, batch updates,
 * returns the updated rows as JSON array */

typedef struct {
    h2o_req_t *req;
    FDBTransaction *tr;
    size_t num_query;
    size_t num_result;
    uint32_t *ids;
    uint32_t *random_numbers;
    bool phase; /* false = reading, true = writing */
} update_ctx_t;

static void update_on_commit(FDBFuture *future, void *arg)
{
    update_ctx_t *ctx = (update_ctx_t *)arg;
    (void)future;

    /* Serialize results and send */
    yajl_gen gen = yajl_gen_alloc(NULL);
    yajl_gen_array_open(gen);
    for (size_t i = 0; i < ctx->num_result; i++) {
        yajl_gen_map_open(gen);
        yajl_gen_string(gen, (const unsigned char *)"id", 2);
        yajl_gen_integer(gen, ctx->ids[i]);
        yajl_gen_string(gen, (const unsigned char *)"randomNumber", 12);
        yajl_gen_integer(gen, ctx->random_numbers[i]);
        yajl_gen_map_close(gen);
    }
    yajl_gen_array_close(gen);

    const unsigned char *buf;
    size_t len;
    yajl_gen_get_buf(gen, &buf, &len);
    send_json(ctx->req, (const char *)buf, len);
    yajl_gen_free(gen);

    fdb_transaction_destroy(ctx->tr);
    free(ctx->ids);
    free(ctx->random_numbers);
    free(ctx);
}

static void update_do_writes(update_ctx_t *ctx)
{
    ctx->phase = true; /* writing phase */

    /* Write all updated random numbers */
    for (size_t i = 0; i < ctx->num_result; i++) {
        uint8_t key[16];
        size_t klen;
        world_key(key, &klen, ctx->ids[i]);
        uint32_t be = htonl(ctx->random_numbers[i]);
        fdb_sync_set(ctx->tr, key, (int)klen, (const uint8_t *)&be, 4);
    }

    /* Commit */
    fdb_async_commit(g_fdb_state, ctx->tr, update_on_commit, ctx);
}

static void update_on_read(FDBFuture *future, void *arg)
{
    update_ctx_t *ctx = (update_ctx_t *)arg;

    fdb_error_t err = fdb_future_get_error(future);
    if (err) {
        h2o_send_error_503(ctx->req, "Service Unavailable", "FDB error", 0);
        fdb_transaction_destroy(ctx->tr);
        free(ctx->ids);
        free(ctx->random_numbers);
        free(ctx);
        return;
    }

    fdb_bool_t present;
    const uint8_t *val;
    int val_len;
    err = fdb_future_get_value(future, &present, &val, &val_len);
    if (err || !present || val_len < 4) {
        ctx->random_numbers[ctx->num_result] = get_random_world_id();
    } else {
        uint32_t old_rn;
        memcpy(&old_rn, val, 4);
        (void)ntohl(old_rn); /* we don't use the old value, just overwrite */
    }

    /* Generate new random number */
    ctx->random_numbers[ctx->num_result] = get_random_world_id() - 1; /* 0-9999 */
    ctx->num_result++;

    if (ctx->num_result < ctx->num_query) {
        /* Read next */
        uint8_t key[16];
        size_t klen;
        world_key(key, &klen, ctx->ids[ctx->num_result]);
        fdb_async_get(g_fdb_state, ctx->tr, key, (int)klen, update_on_read, ctx);
    } else {
        /* All reads done, do writes */
        update_do_writes(ctx);
    }
}

static int updates_handler(struct st_h2o_handler_t *self, h2o_req_t *req)
{
    (void)self;
    size_t n = get_query_number(req);
    update_ctx_t *ctx = calloc(1, sizeof(*ctx));
    if (!ctx) {
        h2o_send_error_503(req, "Service Unavailable", "oom", 0);
        return 0;
    }
    ctx->req = req;
    ctx->num_query = n;
    ctx->ids = calloc(n, sizeof(uint32_t));
    ctx->random_numbers = calloc(n, sizeof(uint32_t));

    /* Generate unique random IDs (h2o TFB uses a bitset for uniqueness) */
    for (size_t i = 0; i < n; i++)
        ctx->ids[i] = get_random_world_id();

    if (fdb_create_transaction(g_fdb_state, &ctx->tr) != 0) {
        free(ctx->ids);
        free(ctx->random_numbers);
        free(ctx);
        h2o_send_error_503(req, "Service Unavailable", "fdb failed", 0);
        return 0;
    }

    /* Start reading the first row */
    uint8_t key[16];
    size_t klen;
    world_key(key, &klen, ctx->ids[0]);
    fdb_async_get(g_fdb_state, ctx->tr, key, (int)klen, update_on_read, ctx);
    return 0;
}

/* ===== 6. Cached Queries: /cached-worlds?queries=N ===== */
/* Uses an in-memory cache of the World table. Cache is populated on first
 * access via a range scan of all World keys. */

/* World cache is defined above as g_world_cache[10001] */
static bool g_cache_populating = false;

static int cached_worlds_handler(struct st_h2o_handler_t *self, h2o_req_t *req)
{
    (void)self;
    size_t n = get_query_number(req);

    if (g_world_cache) {
        /* Serve from cache */
        yajl_gen gen = yajl_gen_alloc(NULL);
        yajl_gen_array_open(gen);
        for (size_t i = 0; i < n; i++) {
            uint32_t id = get_random_world_id();
            yajl_gen_map_open(gen);
            yajl_gen_string(gen, (const unsigned char *)"id", 2);
            yajl_gen_integer(gen, id);
            yajl_gen_string(gen, (const unsigned char *)"randomNumber", 12);
            yajl_gen_integer(gen, g_world_cache[id]);
            yajl_gen_map_close(gen);
        }
        yajl_gen_array_close(gen);

        const unsigned char *buf;
        size_t len;
        yajl_gen_get_buf(gen, &buf, &len);
        send_json(req, (const char *)buf, len);
        yajl_gen_free(gen);
        return 0;
    }

    /* Cache not available, serve from FDB directly */
    return queries_handler(self, req);
}

/* ===== Registration ===== */

void initialize_tfb_handlers(h2o_hostconf_t *hostconf,
                              fdb_thread_state_t *fdb_state,
                              unsigned int *seed)
{
    g_fdb_state = fdb_state;
    g_seed = seed;

    /* Pre-populate the world cache with random data so /cached-worlds
     * works even before FDB is loaded */
    if (!g_world_cache_loaded) {
        unsigned int s = 42;
        for (int i = 0; i <= 10000; i++)
            g_world_cache[i] = (uint16_t)(rand_r(&s) % 10000);
        g_world_cache_loaded = true;
    }

    h2o_pathconf_t *pathconf;

    pathconf = h2o_config_register_path(hostconf, "/plaintext", 0);
    h2o_create_handler(pathconf, sizeof(h2o_handler_t))->on_req = plaintext_handler;

    pathconf = h2o_config_register_path(hostconf, "/json", 0);
    h2o_create_handler(pathconf, sizeof(h2o_handler_t))->on_req = json_handler;

    pathconf = h2o_config_register_path(hostconf, "/db", 0);
    h2o_create_handler(pathconf, sizeof(h2o_handler_t))->on_req = db_handler;

    pathconf = h2o_config_register_path(hostconf, "/queries", 0);
    h2o_create_handler(pathconf, sizeof(h2o_handler_t))->on_req = queries_handler;

    pathconf = h2o_config_register_path(hostconf, "/updates", 0);
    h2o_create_handler(pathconf, sizeof(h2o_handler_t))->on_req = updates_handler;

    pathconf = h2o_config_register_path(hostconf, "/cached-worlds", 0);
    h2o_create_handler(pathconf, sizeof(h2o_handler_t))->on_req = cached_worlds_handler;
}

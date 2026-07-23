/*
 * Async FDB database adapter implementation.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * FDB futures are callback-based: fdb_future_set_callback() invokes
 * a user callback when the future is ready. We use this to chain
 * transaction steps without blocking.
 *
 * The callback model maps naturally to h2o's event loop:
 *   1. Worker thread creates FDBTransaction
 *   2. Submits reads (async, callback-driven)
 *   3. Reads complete -> perform writes (sync, in-transaction)
 *   4. Commits (async, callback)
 *   5. Commit completes -> send HTTP response via h2o_multithread_send
 *
 * On conflict (retryable error), fdb_transaction_on_error() returns
 * a future; when it resolves, the transaction is reset and the
 * caller retries the entire transaction from step 2.
 */

#include "fdb_database.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "error.h"

int fdb_global_init(fdb_global_t *fdb, const char *cluster_file, size_t num_threads)
{
    memset(fdb, 0, sizeof(*fdb));
    fdb->cluster_file = cluster_file ? cluster_file : FDB_CLUSTER_FILE_DEFAULT;
    fdb->num_threads = num_threads;
    fdb->thread_states = calloc(num_threads, sizeof(fdb_thread_state_t));

    fdb_error_t err = fdb_select_api_version_impl(FDB_API_VERSION, FDB_API_VERSION);
    if (err) {
        LIBRARY_ERROR("fdb_select_api_version", fdb_get_error(err));
        return err;
    }

    err = fdb_setup_network();
    if (err) {
        LIBRARY_ERROR("fdb_setup_network", fdb_get_error(err));
        return err;
    }

    fdb->network_started = true;
    return 0;
}

int fdb_thread_init(fdb_global_t *fdb, h2o_loop_t *loop, fdb_thread_state_t *state)
{
    memset(state, 0, sizeof(*state));
    state->loop = loop;

    FDBFuture *cluster_future = fdb_create_cluster(fdb->cluster_file);
    if (!cluster_future) {
        ERROR("fdb_create_cluster failed");
        return -1;
    }

    fdb_error_t err = fdb_future_block_until_ready(cluster_future);
    if (err) {
        LIBRARY_ERROR("fdb_future_block_until_ready", fdb_get_error(err));
        fdb_future_destroy(cluster_future);
        return err;
    }

    FDBCluster *cluster;
    err = fdb_future_get_cluster(cluster_future, &cluster);
    if (err) {
        LIBRARY_ERROR("fdb_future_get_cluster", fdb_get_error(err));
        fdb_future_destroy(cluster_future);
        return err;
    }
    fdb_future_destroy(cluster_future);

    FDBFuture *db_future = fdb_cluster_create_database(cluster, "DB");
    if (!db_future) {
        ERROR("fdb_cluster_create_database failed");
        return -1;
    }

    err = fdb_future_block_until_ready(db_future);
    if (err) {
        LIBRARY_ERROR("fdb_future_block_until_ready", fdb_get_error(err));
        fdb_future_destroy(db_future);
        return err;
    }

    err = fdb_future_get_database(db_future, &state->db);
    if (err) {
        LIBRARY_ERROR("fdb_future_get_database", fdb_get_error(err));
        fdb_future_destroy(db_future);
        return err;
    }
    fdb_future_destroy(db_future);

    return 0;
}

int fdb_create_transaction(fdb_thread_state_t *state, FDBTransaction **tr)
{
    assert(state->db);
    fdb_error_t err = fdb_database_create_transaction(state->db, tr);
    if (err) {
        LIBRARY_ERROR("fdb_database_create_transaction", fdb_get_error(err));
        return err;
    }
    state->active_transactions++;
    return 0;
}

/* Generic future callback wrapper */
typedef struct {
    void (*cb)(FDBFuture *, void *);
    void *ctx;
    FDBFuture *future;
} fdb_callback_ctx_t;

static void future_callback(FDBFuture *future, void *arg)
{
    fdb_callback_ctx_t *ctx = (fdb_callback_ctx_t *)arg;
    ctx->cb(future, ctx->ctx);
    fdb_future_destroy(future);
    free(ctx);
}

static int submit_future(FDBFuture *future,
                          void (*cb)(FDBFuture *, void *), void *user_ctx)
{
    fdb_callback_ctx_t *ctx = malloc(sizeof(*ctx));
    if (!ctx) {
        STANDARD_ERROR("malloc");
        fdb_future_destroy(future);
        return -1;
    }
    ctx->cb = cb;
    ctx->ctx = user_ctx;
    ctx->future = future;

    fdb_error_t err = fdb_future_set_callback(future, future_callback, ctx);
    if (err) {
        LIBRARY_ERROR("fdb_future_set_callback", fdb_get_error(err));
        fdb_future_destroy(future);
        free(ctx);
        return err;
    }
    return 0;
}

int fdb_async_get(fdb_thread_state_t *state, FDBTransaction *tr,
                  const uint8_t *key, int key_len,
                  void (*cb)(FDBFuture *, void *), void *ctx)
{
    (void)state;
    FDBFuture *future = fdb_transaction_get(tr, key, key_len, 0 /* snapshot */);
    if (!future) {
        ERROR("fdb_transaction_get failed");
        return -1;
    }
    return submit_future(future, cb, ctx);
}

void fdb_sync_set(FDBTransaction *tr,
                  const uint8_t *key, int key_len,
                  const uint8_t *value, int value_len)
{
    fdb_transaction_set(tr, key, key_len, value, value_len);
}

int fdb_async_get_range(fdb_thread_state_t *state, FDBTransaction *tr,
                        const uint8_t *begin, int begin_len,
                        const uint8_t *end, int end_len,
                        void (*cb)(FDBFuture *, void *), void *ctx)
{
    (void)state;
    FDBFuture *future = fdb_transaction_get_range(tr,
        begin, begin_len, end, end_len,
        0 /* snapshot */, 0 /* reverse */, 0 /* limit */,
        FDB_STREAMING_MODE_WANT_ALL, NULL, 0);
    if (!future) {
        ERROR("fdb_transaction_get_range failed");
        return -1;
    }
    return submit_future(future, cb, ctx);
}

int fdb_async_commit(fdb_thread_state_t *state, FDBTransaction *tr,
                     void (*cb)(FDBFuture *, void *), void *ctx)
{
    (void)state;
    FDBFuture *future = fdb_transaction_commit(tr);
    if (!future) {
        ERROR("fdb_transaction_commit failed");
        return -1;
    }
    return submit_future(future, cb, ctx);
}

int fdb_handle_error(fdb_thread_state_t *state, FDBTransaction *tr,
                     fdb_error_t err,
                     void (*cb)(FDBFuture *, void *), void *ctx)
{
    (void)state;
    FDBFuture *future = fdb_transaction_on_error(tr, err);
    if (!future) {
        LIBRARY_ERROR("fdb_transaction_on_error", fdb_get_error(err));
        return err;
    }
    return submit_future(future, cb, ctx);
}

void fdb_thread_cleanup(fdb_thread_state_t *state)
{
    if (state->db)
        fdb_database_destroy(state->db);
}

void fdb_global_cleanup(fdb_global_t *fdb)
{
    if (fdb->network_started) {
        fdb_error_t err = fdb_stop_network();
        if (err)
            LIBRARY_ERROR("fdb_stop_network", fdb_get_error(err));
    }
    free(fdb->thread_states);
}

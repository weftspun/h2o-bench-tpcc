#include <h2o.h>
#include <stdlib.h>
#include <string.h>

#include "fdb_database.h"
#include "global_data.h"
#include "thread.h"

static fdb_global_t fdb_global;

void cleanup_thread_data(thread_context_t *ctx)
{
    fdb_thread_cleanup(&ctx->request_handler_data.fdb_state);
    h2o_context_dispose(&ctx->event_loop);
}

void initialize_thread_data(global_thread_data_t *global_data, thread_context_t *ctx)
{
    memset(ctx, 0, sizeof(*ctx));
    ctx->global_thread_data = global_data;
    ctx->random_seed = (unsigned int)(uintptr_t)ctx;
    h2o_linklist_init(&ctx->local_messages);
    h2o_context_init(&ctx->event_loop, global_data->config->h2o_config, h2o_evloop_create());

    fdb_thread_init(&fdb_global, ctx->event_loop.loop, &ctx->request_handler_data.fdb_state);
}

void *event_loop_thread(void *arg)
{
    thread_context_t *ctx = arg;
    while (h2o_evloop_run(ctx->event_loop.loop, INT32_MAX) == 0)
        ;
    return NULL;
}

/* Called once at startup before threads are created */
int init_fdb_global(config_t *config, size_t num_threads)
{
    return fdb_global_init(&fdb_global, config->fdb_cluster_file, num_threads);
}

void cleanup_fdb_global(void)
{
    fdb_global_cleanup(&fdb_global);
}

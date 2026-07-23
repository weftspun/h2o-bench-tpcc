/*
 * h2o-bench-tpcc main entry point.
 *
 * Usage:
 *   h2o-bench-tpcc -a<thread_count> -d<conninfo> -w<warehouses> [-p<port>] [-t<duration_ms>]
 *
 * Based on the h2o TFB implementation's argument parsing and h2o context
 * setup. HTTP/3 enabled via h2o's built-in quic stack (same as weft-warp-loop).
 */

#include <h2o.h>
#include <pthread.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "database.h"
#include "error.h"
#include "global_data.h"
#include "thread.h"
#include "handlers/tpcc_handler_data.h"

static void usage(const char *prog)
{
    fprintf(stderr,
            "Usage: %s -a<thread_count> -d<conninfo> -w<warehouses> [-p<port>] [-t<duration_ms>]\n"
            "\n"
            "  -a  Number of worker threads (also max DB conns per thread)\n"
            "  -d  libpq connection string (e.g. dbname=tpcc host=localhost port=26257)\n"
            "  -w  Warehouse count (TPC-C scale factor)\n"
            "  -p  HTTP port (default 8080)\n"
            "  -t  Benchmark duration in ms (default 0 = run until killed)\n",
            prog);
}

int main(int argc, char *argv[])
{
    config_t config = {0};
    config.db_host = "localhost";
    config.warehouses = 1;
    config.worker_count = 1;
    config.duration_ms = 0;
    config.db_config.max_db_conn_num = 1;
    config.db_config.max_pipeline_query_num = 8;
    config.db_config.max_query_num = config.db_config.max_db_conn_num * config.db_config.max_pipeline_query_num;
    config.db_config.db_timeout = 30;
    int port = DEFAULT_PORT;

    int opt;
    while ((opt = getopt(argc, argv, "a:d:w:p:t:h")) != -1) {
        switch (opt) {
        case 'a':
            config.worker_count = (size_t)atoi(optarg);
            config.db_config.max_db_conn_num = config.worker_count;
            config.db_config.max_query_num = config.db_config.max_db_conn_num * config.db_config.max_pipeline_query_num;
            break;
        case 'd':
            config.db_conninfo = optarg;
            break;
        case 'w':
            config.warehouses = (size_t)atoi(optarg);
            break;
        case 'p':
            port = atoi(optarg);
            break;
        case 't':
            config.duration_ms = (size_t)atoi(optarg);
            break;
        case 'h':
        default:
            usage(argv[0]);
            return opt == 'h' ? 0 : 1;
        }
    }

    if (!config.db_conninfo) {
        config.db_conninfo = "dbname=tpcc host=tfb-database port=26257 sslmode=disable user=benchmarkdbuser";
    }

    signal(SIGPIPE, SIG_IGN);

    h2o_config_init(&config.h2o_config);
    /* HTTP/3 support — same quic stack as weft-warp-loop */
    config.h2o_config.http3.enabled = 1;
    config.h2o_config.http3.max_concurrent_streams = 256;
    h2o_hostconf_t *hostconf = h2o_config_register_host(&config.h2o_config, h2o_iovec_init(H2O_STRLIT("*")), port);

    request_handler_data_t handler_data = {0};
    initialize_tpcc_handlers(hostconf, NULL, &handler_data);

    global_thread_data_t global_data = {
        .config = &config,
        .request_handler_data = handler_data,
    };

    thread_context_t *threads = calloc(config.worker_count, sizeof(*threads));
    pthread_t *thread_ids = calloc(config.worker_count, sizeof(*thread_ids));

    for (size_t i = 0; i < config.worker_count; i++) {
        initialize_thread_data(&global_data, &threads[i]);
        pthread_create(&thread_ids[i], NULL, event_loop_thread, &threads[i]);
    }

    fprintf(stderr, "h2o-bench-tpcc: %zu worker(s), %zu warehouse(s), port %d (HTTP/3 enabled)\n",
            config.worker_count, config.warehouses, port);

    for (size_t i = 0; i < config.worker_count; i++)
        pthread_join(thread_ids[i], NULL);

    for (size_t i = 0; i < config.worker_count; i++)
        cleanup_thread_data(&threads[i]);

    free(threads);
    free(thread_ids);
    cleanup_tpcc_handlers(&handler_data);

    return 0;
}

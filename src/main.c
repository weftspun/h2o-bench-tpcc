/*
 * h2o-bench-tpcc main entry point.
 *
 * Usage:
 *   h2o-bench-tpcc -a<thread_count> -c<cluster_file> -w<warehouses> [-p<port>]
 */

#include <h2o.h>
#include <h2o/http1.h>
#include <h2o/http2.h>

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "error.h"
#include "global_data.h"
#include "thread.h"
#include "handlers/tpcc_handler_data.h"
#include "handlers/tfb_handlers.h"

#define DEFAULT_PORT 8080

typedef struct {
    h2o_accept_ctx_t accept_ctx;
    h2o_context_t h2o_ctx;
    h2o_loop_t *loop;
    int listen_fd;
    fdb_thread_state_t fdb_state;
    pthread_t tid;
    config_t *config;
    bool running;
} thread_ctx_t;

static void usage(const char *prog)
{
    fprintf(stderr,
            "Usage: %s -a<thread_count> -c<cluster_file> -w<warehouses> [-p<port>]\n"
            "\n"
            "  -a  Number of worker threads\n"
            "  -c  FoundationDB cluster file path\n"
            "  -w  Warehouse count (TPC-C scale factor)\n"
            "  -p  HTTP port (default 8080)\n",
            prog);
}

static int create_listening_socket(int port)
{
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) { perror("socket"); return -1; }

    int opt = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
#ifdef SO_REUSEPORT
    setsockopt(fd, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt));
#endif

    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(port);

    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind");
        close(fd);
        return -1;
    }
    if (listen(fd, 4096) < 0) {
        perror("listen");
        close(fd);
        return -1;
    }
    return fd;
}

static void on_accept(h2o_socket_t *listener, const char *err)
{
    thread_ctx_t *tctx = (thread_ctx_t *)listener->data;
    h2o_socket_t *sock;

    if (err != NULL)
        return;

    while ((sock = h2o_evloop_socket_accept(listener)) != NULL) {
        struct timeval connected_at = {0};
        gettimeofday(&connected_at, NULL);
        h2o_accept(&tctx->accept_ctx, sock);
    }
}

static void *worker_main(void *arg)
{
    thread_ctx_t *tctx = (thread_ctx_t *)arg;

    h2o_socket_t *sock = h2o_evloop_socket_create(tctx->loop, tctx->listen_fd,
                                                    H2O_SOCKET_FLAG_DONT_READ);
    sock->data = tctx;
    h2o_socket_read_start(sock, on_accept);

    while (tctx->running) {
        h2o_evloop_run(tctx->loop, INT32_MAX);
    }

    h2o_socket_read_stop(sock);
    h2o_socket_close(sock);
    return NULL;
}

static fdb_global_t fdb_global;

int main(int argc, char *argv[])
{
    config_t config = {0};
    config.fdb_cluster_file = "/etc/foundationdb/fdb.cluster";
    config.warehouses = 1;
    config.worker_count = 1;
    int port = DEFAULT_PORT;

    int opt;
    while ((opt = getopt(argc, argv, "a:c:w:p:h")) != -1) {
        switch (opt) {
        case 'a': config.worker_count = (size_t)atoi(optarg); break;
        case 'c': config.fdb_cluster_file = optarg; break;
        case 'w': config.warehouses = (size_t)atoi(optarg); break;
        case 'p': port = atoi(optarg); break;
        case 'h':
        default: usage(argv[0]); return opt == 'h' ? 0 : 1;
        }
    }

    signal(SIGPIPE, SIG_IGN);

    /* Initialize FDB */
    if (fdb_global_init(&fdb_global, config.fdb_cluster_file, config.worker_count)) {
        fprintf(stderr, "Failed to initialize FoundationDB\n");
        return 1;
    }

    /* Initialize H2O global config */
    h2o_config_init(&config.h2o_config);
    h2o_hostconf_t *hostconf = h2o_config_register_host(&config.h2o_config,
        h2o_iovec_init(H2O_STRLIT("*")), port);

    /* Register TFB routes */
    unsigned int tfb_seed = (unsigned int)time(NULL);
    initialize_tfb_handlers(hostconf, NULL, &tfb_seed);

    /* Register TPC-C routes */
    request_handler_data_t handler_data = {0};
    initialize_tpcc_handlers(hostconf, NULL, &handler_data);

    /* Create listening socket */
    int listen_fd = create_listening_socket(port);
    if (listen_fd < 0) {
        fprintf(stderr, "Failed to create listening socket on port %d\n", port);
        return 1;
    }

    /* Create per-thread contexts */
    thread_ctx_t *threads = calloc(config.worker_count, sizeof(*threads));

    for (size_t i = 0; i < config.worker_count; i++) {
        thread_ctx_t *t = &threads[i];
        t->listen_fd = listen_fd;
        t->config = &config;
        t->running = true;
        t->loop = h2o_evloop_create();

        h2o_context_init(&t->h2o_ctx, t->loop, &config.h2o_config);

        t->accept_ctx.ctx = &t->h2o_ctx;
        t->accept_ctx.hosts = config.h2o_config.hosts;
        t->accept_ctx.ssl_ctx = NULL;
        t->accept_ctx.expect_proxy_line = 0;
        t->accept_ctx.http2_origin_frame = NULL;

        /* Initialize per-thread FDB state */
        fdb_thread_init(&fdb_global, t->loop, &t->fdb_state);

        pthread_create(&t->tid, NULL, worker_main, t);
    }

    fprintf(stderr, "h2o-bench-tpcc: %zu worker(s), %zu warehouse(s), port %d\n",
            config.worker_count, config.warehouses, port);

    /* Run FDB network on main thread (blocks) */
    fdb_run_network();

    /* Cleanup */
    for (size_t i = 0; i < config.worker_count; i++) {
        threads[i].running = false;
        pthread_join(threads[i].tid, NULL);
        h2o_context_dispose(&threads[i].h2o_ctx);
        fdb_thread_cleanup(&threads[i].fdb_state);
    }

    free(threads);
    close(listen_fd);
    cleanup_tpcc_handlers(&handler_data);
    cleanup_fdb_global();

    return 0;
}

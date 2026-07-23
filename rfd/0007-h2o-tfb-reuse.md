# RFD 7: h2o TFB reference implementation reuse

**State:** accepted

## Decision

Reuse the async libpq connection pool and event loop integration from
the h2o TechEmpower FrameworkBenchmarks implementation as the foundation
for h2o-bench-tpcc.

## Source

`frameworks/C/h2o/` in TechEmpower/FrameworkBenchmarks, authored by
Anton Valentinov Kirilov (MIT license).

## What was reused

- `database.c` — async libpq with `PQconnectStart`/`PQconnectPoll`,
  `PQenterPipelineMode`, `PQsendQueryPrepared`/`PQsendPipelineSync`,
  `PQconsumeInput`/`PQisBusy`/`PQgetResult`. Non-blocking, integrated
  into h2o's event loop via `h2o_socket_t`.
- Connection pool pattern: `-m` (max conns per thread), `-e` (max
  pipelined queries per conn).
- Binary protocol (format=1, network byte order) for params/results.
- `db_query_param_t` callback structure: `on_result`, `on_error`,
  `on_timeout`.

## What was stripped

- BPF socket load balancer (`src/bpf/`)
- NUMA support (`libnuma`)
- mustache-c templates
- The World/Fortune/Plaintext/JSON handlers (replaced with TPC-C handlers)

## What was replaced

- `database.c` (libpq async pipeline pool) -> `fdb_database.c` (FDB C API,
  `fdb_future_set_callback` for async). The callback model maps to the
  same `on_result`/`on_error`/`on_timeout` pattern but via FDB futures
  instead of libpq pipeline results.

## What was added

- SPSC lock-free ring buffer (`spsc_ring.c`) for worker dispatch
- Bespoke worker pool (`worker_pool.c`) with round-robin dispatch
- TPC-C schema, prepared statements, and handler stubs
- HTTP/3 enabled in h2o config

## License

The reused code retains its MIT license. h2o-bench-tpcc is Apache-2.0.

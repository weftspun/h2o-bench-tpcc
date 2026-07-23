# h2o-bench-tpcc

TPC-C-style benchmark harness for [libh2o](https://h2o.examp1e.net/), built
directly on top of `libh2o`'s event loop with the [FoundationDB C
API](https://github.com/apple/foundationdb/tree/main/bindings/c) as the
storage engine.

## Why this exists

`ecto-bench-tpcc` measures TPC-C throughput through Elixir/Ecto. This repo
measures the same workload through a C HTTP server with direct FDB key-value
operations — no ORM, no VM, no SQL layer. The framework itself is the
variable; the database is held constant (FoundationDB, actively maintained by
Apple).

## Architecture

```
HTTP client (wrk)
    │
    ▼
h2o-bench-tpcc (C, libh2o + libfdb_c)
    │
    │  SPSC ring → worker → FDB async callbacks
    ▼
FoundationDB (libfdb_c, pure C, no JVM)
```

### Actor-lite worker pool

Three components (see [RFD 0005](rfd/0005-actor-lite-architecture.md)):

1. **SPSC lock-free ring buffer** — `spsc_ring.c`, no mutexes on hot path
2. **Worker threads** — bare pthreads, each owns an FDB database handle
3. **Return path** — `h2o_multithread_send()` back to the H2O event loop

Verified by CBMC (C invariants), Lean 4 (specification), and
plausible-witness-dag (TPC-C semantics). See [RFD
0008](rfd/0008-verification-strategy.md).

## Scenarios

| RFD | Scenario | Scale knob |
|-----|----------|-----------|
| 0001 | tpcc (standard TPC-C) | warehouses (W) |
| 0002 | zonefabric | zones |

See `rfd/` for exact scaling formulas and architectural decisions.

## License

Apache-2.0

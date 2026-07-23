# RFD 6: CockroachDB selection over FRL/FDB

**State:** accepted

## Decision

Use CockroachDB (v-sekai fork) as the database for h2o-bench-tpcc, not
Apple FoundationDB Relational Layer (FRL).

## Rationale

The benchmark measures h2o's HTTP-to-DB pipeline throughput. The database
is held constant so the framework is the variable.

CockroachDB speaks the Postgres wire protocol natively on port 26257.
This means `libpq` links directly into h2o with no JVM, no gRPC, no JNI
bridge. A "raw SQL driver" is just the Postgres wire protocol, which
libpq already implements.

FRL requires either a running `fdb-relational-server` JVM process
(gRPC on 8123) or an embedded JNI classpath pointing at a 100MB+ jar.
Either path breaks the pure-C constraint and adds latency that
contaminates the measurement.

| | CockroachDB | FRL |
|---|---|---|
| C integration | libpq (native C) | gRPC-C or JNI bridge |
| External deps | 1 binary | JVM + FDB + relational-server jar |
| SQL support | Full SQL | Relational layer (some gaps) |
| Raw driver effort | libpq (or wrap it) | gRPC protobuf client |
| Write throughput | Good (Raft) | Excellent (FDB underneath) |
| Best for | Framework benchmarking | DB benchmarking |

## When to revisit

If the goal shifts from framework benchmarking to database write
throughput comparison (FDB vs CockroachDB), add FRL as a second adapter.
The Postgres wire protocol path stays as the primary.

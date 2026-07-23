# RFD 15: TechEmpower benchmark implementation

**State:** accepted

## Decision

Implement the 6 standard TechEmpower Framework Benchmark tests exactly
as the h2o TFB entry, adapted for FoundationDB instead of Postgres/libpq.

## Test implementation

| Test | Endpoint | What it does | FDB operations |
|------|----------|-------------|----------------|
| Plaintext | `/plaintext` | Return "Hello, World!" | none |
| JSON | `/json` | Return `{"message":"Hello, World!"}` | none |
| Single Query | `/db` | Read 1 random World row | 1 get |
| Multiple Queries | `/queries?queries=N` | Read 1-500 World rows | N gets |
| Data Updates | `/updates?queries=N` | Read N rows, batch update | N gets + N sets + 1 commit |
| Cached Queries | `/cached-worlds?queries=N` | Read from in-memory cache | 0 (or 1 range scan to populate) |

## World table in FDB

```
key:   "tfb/w/" + 4-byte big-endian id (1-10000)
value: 4-byte big-endian uint32 randomNumber (0-9999)
```

10,000 rows. Loaded by `tpcc_loader`.

## Differences from h2o TFB reference

| Aspect | h2o TFB (libpq) | h2o-bench-tpcc (FDB) |
|--------|-----------------|---------------------|
| DB driver | libpq async pipeline | libfdb_c async callbacks |
| Connection pool | max_db_conn_num conns | 1 FDBDatabase per thread |
| Batch update | CASE WHEN ... SQL | N sets in one transaction |
| Prepared statements | PQsendQueryPrepared | none (direct KV) |
| Sort to avoid deadlock | qsort by id | not needed (FDB handles conflicts) |
| Binary protocol | format=1 (PQgetvalue) | direct struct cast |
| Cache | h2o_cache_t (131072 capacity) | uint16_t[10001] flat array |

## What we do NOT implement

- **Fortunes test** (`/fortunes`). Requires HTML templating (mustache).
  Future work: add a minimal fortune handler.
- **BPF socket load balancer**. h2o TFB uses BPF for connection
  distribution. Not needed with FDB's single connection per thread.
- **NUMA awareness**. h2o TFB uses libnuma. Not implemented.

## Performance expectations

The h2o TFB entry scored 1,226,814 RPS on the data update test
(R23, rank #2). Our implementation uses FDB instead of Postgres, so
direct comparison is not possible until we run on identical hardware.

FDB advantages for this workload:
- No SQL parsing (direct KV get/set)
- No pipeline mode overhead (native async)
- Single connection per thread (no pool management)

FDB risks:
- Network round-trip to FDB cluster (if not co-located)
- Transaction conflict on concurrent updates to same key
- No prepared statement caching (not needed for KV)

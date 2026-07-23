# RFD 15: TechEmpower benchmark implementation

**State:** accepted

## Decision

Implement the six TechEmpower Framework Benchmark (TFB) test endpoints
alongside the TPC-C endpoints. This allows direct comparison with
h2o's official TFB results on the TechEmpower leaderboard.

## Endpoints

| Endpoint | TFB test | Description |
|----------|----------|-------------|
| /plaintext | Plaintext | Return "Hello, World!" (13 bytes) |
| /json | JSON Serialization | Return `{"message":"Hello, World!"}` |
| /db | Single Query | Read one random World row, return as JSON |
| /queries?n=N | Multiple Queries | Read N random World rows (1-500) |
| /updates?n=N | Data Updates | Read-modify-write N random World rows |
| /cached-worlds | Cached Queries | In-memory cache of World table |

## World table

The World table has 10,000 rows with `id` (1-10000) and `randomNumber`
(0-9999). Seeded by `load_world_table()` in tpcc_loader using FDB
range writes.

For `/cached-worlds`, the entire World table is loaded into an
in-memory `uint16_t[10001]` array on first access. Subsequent reads
hit the cache directly — no FDB calls.

## TFB compliance

Response bodies and content types match the TFB specification:
- `/plaintext`: `Content-Type: text/plain`, body = `Hello, World!`
- `/json`: `Content-Type: application/json`, body = `{"message":"Hello, World!"}`
- `/db`, `/queries`: `Content-Type: application/json`, body = `{"id":N,"randomNumber":N}`

## Comparison with h2o official TFB

h2o's official TFB entry uses the h2o standalone server with a
PostgreSQL backend. Our implementation uses the same h2o event loop
but with FDB as the backend. The `/plaintext` and `/json` endpoints
have no database dependency and should match h2o's official numbers
on identical hardware.

The `/db`, `/queries`, and `/updates` endpoints use FDB instead of
Postgres. FDB's per-core throughput (90K reads/sec, 35K writes/sec
on memory engine) provides a baseline for expected performance.

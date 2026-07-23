# RFD 7: h2o TFB reference implementation reuse

**State:** accepted

## Decision

Reuse h2o's TechEmpower Framework Benchmark (TFB) reference
implementation as the HTTP layer for h2o-bench-tpcc. The TFB handlers
(/plaintext, /json, /db, /queries, /updates, /cached-worlds) are
adapted from h2o's official TFB entry.

## Rationale

h2o is rank #2 on TechEmpower R23 plaintext (7M RPS) and rank #2 on
data updates (1.2M RPS). By reusing h2o's own handler code, we inherit
its proven HTTP/1 + HTTP/2 event loop, connection management, and
response sending — without reimplementing them.

## What we reuse

- h2o event loop (`h2o_evloop_run`)
- h2o accept socket (`h2o_evloop_socket_create` + `h2o_socket_read_start`)
- h2o HTTP/1 parser (`h2o_http1_accept`)
- h2o response sending (`h2o_send_inline`, `h2o_add_header`)
- h2o multithread queue (`h2o_multithread_send_message`)

## What we replace

- TFB's Postgres/MySQL handlers → FDB async callback handlers
- TFB's JSON serializer (varies) → yajl (C JSON library)
- TFB's connection pool → FDB database handles (one per thread)

## TFB endpoints mapped to our handlers

| TFB endpoint | Our handler | Storage |
|---|---|---|
| /plaintext | plaintext_handler | none (hardcoded) |
| /json | json_handler | none (yajl) |
| /db | db_handler | FDB point read (world table) |
| /queries?n=N | queries_handler | FDB N point reads |
| /updates?n=N | updates_handler | FDB N read-modify-write |
| /cached-worlds | cached_worlds_handler | in-memory cache |

## Verification

TFB specification compliance: response bodies match the TFB spec
exactly. World table has 10,000 rows, random IDs 1-10000, values
0-9999. JSON response format matches `{"id":N,"randomNumber":N}`.

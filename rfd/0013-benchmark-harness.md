# RFD 13: Benchmark harness endpoint and transaction mix

**State:** accepted

## Decision

Expose a `/tpcc/run` endpoint that executes a mixed TPC-C transaction
stream according to the official TPC-C specification weights. This is
the primary benchmark endpoint, complementing the TFB-style endpoints.

## Transaction mix

| Transaction | Weight | Endpoint |
|-------------|--------|----------|
| NewOrder | 45% | /tpcc/new-order |
| Payment | 43% | /tpcc/payment |
| OrderStatus | 4% | /tpcc/order-status |
| Delivery | 4% | /tpcc/delivery |
| StockLevel | 4% | /tpcc/stock-level |

Each call to `/tpcc/run` randomly selects a transaction based on these
weights, executes it against FDB, and returns the result as JSON.

## Individual endpoints

Each transaction is also exposed as its own endpoint for isolated
benchmarking:

- `/tpcc/new-order` — full NewOrder transaction (5-15 order lines)
- `/tpcc/payment` — Payment transaction (update customer balance)
- `/tpcc/order-status` — OrderStatus (read-only)
- `/tpcc/delivery` — Delivery (batch update)
- `/tpcc/stock-level` — StockLevel (range scan, read-only)

## Response format

All TPC-C endpoints return JSON with a `status` field and transaction-
specific result fields:

```json
{"status":"ok","txn":"new-order","w_id":1,"d_id":5,"o_id":3001,"items":8}
```

Errors return HTTP 503 with the error message:
```json
{"status":"error","txn":"new-order","error":"conflict"}
```

## wrk benchmark command

```bash
wrk -t4 -c256 -d10s http://localhost:8080/tpcc/run
```

For individual transaction benchmarks:
```bash
wrk -t4 -c256 -d10s http://localhost:8080/tpcc/new-order
```

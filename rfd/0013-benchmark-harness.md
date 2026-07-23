# RFD 13: Benchmark harness endpoint and transaction mix

**State:** accepted

## Decision

Expose a single `/tpcc/run` HTTP endpoint that dispatches a
weighted-random TPC-C transaction on each request.

## Transaction mix

Per TPC-C spec section 5.2.2:

| Transaction | Weight | Probability |
|-------------|--------|-------------|
| NewOrder | 45 | 45% |
| Payment | 43 | 43% |
| OrderStatus | 4 | 4% |
| Delivery | 4 | 4% |
| StockLevel | 4 | 4% |

The harness generates a random number 0-99 and selects the transaction
based on cumulative thresholds. Each request executes exactly one
transaction and returns its result as JSON.

## Endpoints

| Endpoint | Transaction | Method |
|----------|-------------|--------|
| `/tpcc/run` | Weighted random | harness dispatch |
| `/tpcc/new-order` | NewOrder only | direct |
| `/tpcc/payment` | Payment only | direct |
| `/tpcc/order-status` | OrderStatus only | direct |
| `/tpcc/delivery` | Delivery only | direct |
| `/tpcc/stock-level` | StockLevel only | direct |

Direct endpoints are for debugging and per-transaction benchmarking.
The harness endpoint is what wrk/bombardier hits during full benchmark runs.

## Response format

All endpoints return JSON:

```json
{"status": "ok", ...}
```

On error:

```json
{"status": "error", "message": "..."}
```

## What the harness does NOT do

- **Terminal simulation.** TPC-C spec defines 10 terminals per warehouse,
   each pinned to one district. The harness does not simulate terminals;
   each request is independent with random warehouse/district selection.
- **Think time.** TPC-C spec includes keyed think time between
   transactions. The harness has no think time (maximum throughput).
- **Logging.** No per-transaction logging on the hot path. Metrics are
   collected externally by wrk/bombardier.

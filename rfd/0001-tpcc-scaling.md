# RFD 1: TPC-C scaling

**State:** discussion  
**Scale knob:** warehouse count (W)  
**Source:** TPC-C specification v5.11

Standard TPC-C benchmark with W warehouses. Each warehouse contains
10 districts, 3,000 customers per district, 100,000 items, and 100,000
stock rows. The scale factor is W — more warehouses means more data and
more concurrent terminals.

## Row counts

| Table | Rows | Formula |
|-------|------|---------|
| WAREHOUSE | W | 1 per warehouse |
| DISTRICT | 10W | 10 per warehouse |
| CUSTOMER | 30,000W | 3,000 per district |
| HISTORY | 30,000W | 1 per customer |
| ITEM | 100,000 | fixed |
| STOCK | 100,000W | 100,000 per warehouse |
| OORDER | 30,000W | 3,000 per district |
| NEW_ORDER | 9,000W | 900 per district (undelivered) |
| ORDER_LINE | ~300,000W | 5-15 per order |

## Transaction mix (TPC-C spec)

| Transaction | Weight | Reads | Writes |
|-------------|--------|-------|--------|
| NewOrder | 45% | warehouse, district, stock (5-15), customer | district, oorder, new_order, order_line, stock |
| Payment | 43% | warehouse, district, customer | warehouse, district, customer, history |
| OrderStatus | 4% | customer, oorder, order_line | none |
| Delivery | 4% | new_order, oorder, customer | new_order, oorder, customer |
| StockLevel | 4% | stock (range scan) | none |

88% of transactions perform writes. This is a write-heavy workload.

## Comparison baseline

TechEmpower R23 data update test (closest TFB test to TPC-C):

| Rank | Framework | RPS | Lang |
|------|-----------|-----|------|
| 1 | may-minihttp | 1,327,378 | Rust |
| 2 | h2o | 1,226,814 | C |
| 3 | ntex [sailfish] | 1,210,348 | Rust |

All top 10 use Postgres with raw SQL. h2o-bench-tpcc uses FDB instead
of Postgres, so numbers are not directly comparable. The goal is to
demonstrate that FDB's native C API can match or exceed Postgres
throughput for the same transaction profile.

## FDB keyspace mapping

See RFD 0009 for the FDB subspace design. Each TPC-C table maps to an
FDB key prefix. Values are packed binary structs (RFD 0010).
Transactions are async callback chains (RFD 0011).

## Verification

TPC-C invariants verified by plausible-witness-dag (RFD 0008):
- NewOrder atomicity (all order lines written or none)
- Stock non-negative (s_quantity >= 0 after decrement)
- District next_o_id monotonic increasing
- Delivery correctness (oldest new_order delivered first)

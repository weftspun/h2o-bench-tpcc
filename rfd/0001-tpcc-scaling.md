# RFD 1: TPC-C scaling

**State:** discussion  
**Scale knob:** W (warehouse count)

TPC-C scales by a single integer W. Every table cardinality is a fixed function of W except ITEM (100,000 rows, fixed).

| Table | Rows | Formula |
|---|---|---|
| WAREHOUSE | W | 1 per warehouse |
| DISTRICT | 10W | 10 districts per warehouse |
| CUSTOMER | 30,000W | 3,000 per district |
| HISTORY | 30,000W | 1 per customer at load |
| OORDER | 30,000W | 1 per customer at load |
| NEW_ORDER | 9,000W | 900 undelivered per district |
| ORDER_LINE | ~300,000W | 5-15 lines per order |
| ITEM | 100,000 | fixed, not scaled |
| STOCK | 100,000W | 1 per item per warehouse |

NURand (non-uniform random) is mandated for C_LAST (A=255), C_ID (A=1023), OL_I_ID (A=8191). Per-run constant C chosen once, held fixed. Transaction mix: NewOrder 45%, Payment 43%, OrderStatus 4%, Delivery 4%, StockLevel 4%. Terminal binding: 10 terminals/warehouse, each pinned to one warehouse/district pair.

# RFD 12: TPC-C data loader design

**State:** discussion

## Decision

A separate `tpcc_loader` binary populates the FDB keyspace with TPC-C
data before the benchmark server starts.

## Usage

```
tpcc_loader -c<cluster_file> -w<warehouses>
```

## Batch commit strategy

FDB transactions have a 10MB write limit. The loader commits in
batches of 1000 rows to stay well under this limit.

| Table | Rows per batch | Batches for W=1 | Total writes for W=1 |
|-------|---------------|-----------------|---------------------|
| item | 1000 | 100 | 100,000 |
| warehouse | all | 1 | W |
| district | all | 1 | 10*W |
| customer | 1000 | 30*W | 30,000*W |
| stock | 1000 | 100*W | 100,000*W |
| oorder | 1000 | 30*W | 30,000*W |
| order_line | 1000 | ~300*W | ~300,000*W |
| new_order | 1000 | 9*W | 9,000*W |

## Current state

- Items, warehouses, districts: fully implemented
- Customers, orders, stock: stub (full implementation pending)

## What the loader does NOT do

- **Index population.** The customer_name index is not populated at load
   time. Payment-by-name transactions fall back to random customer ID.
- **Data validation.** No post-load checksums. Verification is handled
   by plausible-witness-dag (RFD 0008) at runtime.

# RFD 12: TPC-C data loader design

**State:** discussion

## Decision

Implement `tpcc_loader` as a standalone C binary that loads TPC-C data
into FDB before the benchmark server starts. It is invoked by
`docker-entrypoint.sh` before `h2o-bench-tpcc` is launched.

## Loader flow

```
tpcc_loader -c<cluster_file> -w<warehouses>
  1. fdb_select_api_version_impl(730, 730)
  2. fdb_setup_network()
  3. Spawn fdb_run_network() on background pthread
  4. fdb_create_database(cluster_file, &db)
  5. load_items(db)           — 100,000 item rows
  6. load_warehouse(db)        — W warehouse rows
  7. load_districts(db)        — 10*W district rows
  8. load_customers(db)        — 30,000*W customer + history rows
  9. load_orders(db)           — 30,000*W order + order_line + new_order rows
  10. load_stock(db)           — 100,000*W stock rows
  11. load_world_table(db)     — 10,000 World table rows (for TFB /db test)
  12. fdb_database_destroy(db)
  13. fdb_stop_network()
  14. pthread_join(fdb_thread)
```

## Batching strategy

Each load function batches rows into FDB transactions of 1,000 rows
per commit. FDB transactions have a 10MB value size limit and a
recommended 1,000 key per transaction for optimal throughput.

For 100,000 items: 100 transactions of 1,000 items each.
For 100,000*W stock: 100*W transactions.

## NURand generation

Customer IDs and item IDs use TPC-C's NURand (Non-Uniform Random)
function to generate realistic skewed access patterns. The NURand
constants C_LAST_A=255, C_ID_A=1023, OL_I_ID_A=8191 are defined in
tpcc_procedures.h and verified by CBMC.

## FDB network thread

The loader spawns `fdb_run_network()` on a background pthread because
FDB's C API requires the network thread to be running before any
database operations can complete. `fdb_future_block_until_ready()`
blocks until the future resolves, which requires the network thread
to process the request.

## Shutdown order

1. `fdb_database_destroy(db)` — close database handle
2. `fdb_stop_network()` — signal network thread to stop
3. `pthread_join(fdb_thread)` — wait for network thread to exit

This order is critical: `fdb_stop_network()` unblocks
`fdb_run_network()`, allowing the pthread to exit. Calling
`pthread_join` before `fdb_stop_network()` deadlocks.

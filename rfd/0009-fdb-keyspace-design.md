# RFD 9: TPC-C keyspace design for FoundationDB

**State:** discussion

## Overview

TPC-C defines 9 tables with SQL. FDB is a key-value store. This RFD
maps each table to FDB subspace (directory layer + tuple encoding).

## Key encoding

All keys use FDB tuple layer encoding. The top-level subspace is
`tpcc` (or `tpcc/<warehouse_id>` for per-warehouse isolation).

```
tpcc/warehouse/{w_id}                        -> serialized warehouse record
tpcc/district/{w_id}/{d_id}                   -> serialized district record
tpcc/customer/{w_id}/{d_id}/{c_id}            -> serialized customer record
tpcc/customer_name/{w_id}/{d_id}/{c_last}/{c_first}/{c_id}  -> customer ID index
tpcc/history/{w_w_id}/{c_w_id}/{c_d_id}/{c_id}  -> serialized history record
tpcc/item/{i_id}                              -> serialized item record
tpcc/stock/{w_id}/{i_id}                      -> serialized stock record
tpcc/oorder/{w_id}/{d_id}/{o_id}              -> serialized order record
tpcc/oorder_c_id/{w_id}/{d_id}/{c_id}/{o_id}  -> order ID by customer
tpcc/new_order/{w_id}/{d_id}/{o_id}           -> empty value (presence = undelivered)
tpcc/order_line/{w_id}/{d_id}/{o_id}/{ol_number}  -> serialized order line record
tpcc/next_o_id/{w_id}/{d_id}                  -> 8-byte big-endian integer
```

## Value encoding

Values are fixed-layout binary structs (no protobuf, no JSON). Each
table row is a packed C struct serialized with explicit byte order
(network byte order for all integers).

## Transaction mapping

| TPC-C procedure | FDB transaction reads | FDB transaction writes |
|-----------------|----------------------|------------------------|
| NewOrder | warehouse, district (next_o_id), stock (5-15), customer | district (next_o_id++), oorder, new_order, order_line (5-15), stock (qty-1, order_cnt+1) |
| Payment | warehouse, district, customer | warehouse (ytd+amt), district (ytd+amt), customer (balance-amt, payment_cnt+1), history |
| OrderStatus | customer (by name or ID), oorder (latest), order_line (range scan) | none |
| Delivery | new_order (oldest per district), oorder, customer | new_order (delete), oorder (carrier_id), customer (delivery_cnt+1) |
| StockLevel | stock (range scan where s_quantity < threshold) | none |

## Read-write conflict ranges

FDB requires explicit conflict ranges for serializable isolation. The
C API provides `fdb_transaction_add_conflict_range()` for this. Each
procedure must declare its read and write ranges so FDB can detect
conflicts and retry.

Example: NewOrder reads warehouse (read conflict on warehouse key) and
writes district next_o_id (write conflict on district key + next_o_id).
If two concurrent NewOrder transactions target the same district, FDB
will retry one of them.

## StockLevel without SQL COUNT

StockLevel counts items with `s_quantity < threshold` for a given
warehouse. In FDB, this is a range scan:
```
fdb_transaction_get_range(tr, stock/{w_id}/\x00, stock/{w_id}/\xff, ...)
```
Then count locally in C. Since there are 100,000 stock rows per
warehouse, this is a single range read. FDB returns a contiguous array
of `FDBKeyValue` pairs, so the count is a tight loop over the array.

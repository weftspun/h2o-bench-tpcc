#ifndef TPCC_KV_H_
#define TPCC_KV_H_

#define FDB_API_VERSION 730

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <foundationdb/fdb_c.h>

/*
 * TPC-C key-value encoding layer.
 *
 * Encodes/decodes TPC-C rows to/from FDB keys and values.
 * Keys use prefix + big-endian integer encoding for correct
 * lexicographic ordering. Values are packed binary structs.
 *
 * See rfd/0009-fdb-keyspace-design.md for the full keyspace layout.
 */

/* Subspace prefixes */
#define SS_WAREHOUSE   "tpcc/w/"
#define SS_DISTRICT    "tpcc/d/"
#define SS_CUSTOMER    "tpcc/c/"
#define SS_CUST_NAME   "tpcc/cn/"
#define SS_HISTORY     "tpcc/h/"
#define SS_ITEM        "tpcc/i/"
#define SS_STOCK       "tpcc/s/"
#define SS_OORDER      "tpcc/o/"
#define SS_OORDER_CID  "tpcc/oc/"
#define SS_NEW_ORDER   "tpcc/no/"
#define SS_ORDER_LINE  "tpcc/ol/"
#define SS_NEXT_O_ID   "tpcc/nid/"

/* --- Key builders --- */
size_t kv_warehouse_key(uint8_t *buf, uint32_t w_id);
size_t kv_district_key(uint8_t *buf, uint32_t w_id, uint32_t d_id);
size_t kv_customer_key(uint8_t *buf, uint32_t w_id, uint32_t d_id, uint32_t c_id);
size_t kv_customer_name_key(uint8_t *buf, uint32_t w_id, uint32_t d_id,
                             const char *c_last, const char *c_first, uint32_t c_id);
size_t kv_item_key(uint8_t *buf, uint32_t i_id);
size_t kv_stock_key(uint8_t *buf, uint32_t w_id, uint32_t i_id);
size_t kv_oorder_key(uint8_t *buf, uint32_t w_id, uint32_t d_id, uint32_t o_id);
size_t kv_new_order_key(uint8_t *buf, uint32_t w_id, uint32_t d_id, uint32_t o_id);
size_t kv_order_line_key(uint8_t *buf, uint32_t w_id, uint32_t d_id,
                          uint32_t o_id, uint32_t ol_number);
size_t kv_next_o_id_key(uint8_t *buf, uint32_t w_id, uint32_t d_id);

/* --- Range builders --- */
size_t kv_stock_range_begin(uint8_t *buf, uint32_t w_id);
size_t kv_stock_range_end(uint8_t *buf, uint32_t w_id);
size_t kv_new_order_range_begin(uint8_t *buf, uint32_t w_id, uint32_t d_id);
size_t kv_new_order_range_end(uint8_t *buf, uint32_t w_id, uint32_t d_id);
size_t kv_order_line_range_begin(uint8_t *buf, uint32_t w_id, uint32_t d_id, uint32_t o_id);
size_t kv_order_line_range_end(uint8_t *buf, uint32_t w_id, uint32_t d_id, uint32_t o_id);

/* --- Value structs (packed binary, network byte order) --- */
#pragma pack(push, 1)

typedef struct {
    char     w_name[10];
    char     w_street_1[20];
    char     w_street_2[20];
    char     w_city[20];
    char     w_state[2];
    char     w_zip[9];
    int32_t  w_tax;
    int64_t  w_ytd;
} warehouse_val_t;

typedef struct {
    char     d_name[10];
    char     d_street_1[20];
    char     d_street_2[20];
    char     d_city[20];
    char     d_state[2];
    char     d_zip[9];
    int32_t  d_tax;
    int64_t  d_ytd;
    int32_t  d_next_o_id;
} district_val_t;

typedef struct {
    char     c_first[16];
    char     c_middle[2];
    char     c_last[16];
    char     c_street_1[20];
    char     c_street_2[20];
    char     c_city[20];
    char     c_state[2];
    char     c_zip[9];
    char     c_phone[16];
    int64_t  c_since;
    char     c_credit[2];
    int64_t  c_credit_lim;
    int32_t  c_discount;
    int64_t  c_balance;
    int64_t  c_ytd_payment;
    int16_t  c_payment_cnt;
    int16_t  c_delivery_cnt;
    char     c_data[500];
} customer_val_t;

typedef struct {
    int64_t  h_date;
    int32_t  h_amount;
    char     h_data[24];
} history_val_t;

typedef struct {
    char     i_name[24];
    int32_t  i_price;
    char     i_data[50];
    int32_t  i_im_id;
} item_val_t;

typedef struct {
    int32_t  s_quantity;
    char     s_dist[10][24];
    int32_t  s_ytd;
    int32_t  s_order_cnt;
    int32_t  s_remote_cnt;
    char     s_data[50];
} stock_val_t;

typedef struct {
    uint32_t o_c_id;
    int32_t  o_carrier_id;
    int16_t  o_ol_cnt;
    int16_t  o_all_local;
    int64_t  o_entry_d;
} oorder_val_t;

typedef struct {
    uint32_t ol_i_id;
    uint32_t ol_supply_w_id;
    int64_t  ol_delivery_d;
    int16_t  ol_quantity;
    int32_t  ol_amount;
    char     ol_dist_info[24];
} order_line_val_t;

#pragma pack(pop)

/* --- Next_o_id helpers --- */
void kv_encode_next_o_id(uint8_t *buf, int32_t next_o_id);
int32_t kv_decode_next_o_id(const uint8_t *buf, int len);

/* --- Big-endian integer encoding for keys --- */
void kv_encode_u32_be(uint8_t *buf, uint32_t val);
uint32_t kv_decode_u32_be(const uint8_t *buf);

#endif

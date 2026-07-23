#ifndef TPCC_PROCEDURES_H_
#define TPCC_PROCEDURES_H_

#include <h2o.h>
#include <stdint.h>

#include "fdb_database.h"
#include "global_data.h"

/* TPC-C transaction mix (official spec weights) */
#define NEW_ORDER_WEIGHT 45
#define PAYMENT_WEIGHT 43
#define ORDER_STATUS_WEIGHT 4
#define DELIVERY_WEIGHT 4
#define STOCK_LEVEL_WEIGHT 4

/* NURand constants (TPC-C spec) */
#define NURAND_C_LAST_A 255
#define NURAND_C_ID_A 1023
#define NURAND_OL_I_ID_A 8191

/* TPC-C fixed constants */
#define DISTRICTS_PER_WAREHOUSE 10
#define CUSTOMERS_PER_DISTRICT 3000
#define ITEM_COUNT 100000

/* FDB subspace prefixes for TPC-C tables */
#define FDB_SS_WAREHOUSE   "tpcc/w/"
#define FDB_SS_DISTRICT    "tpcc/d/"
#define FDB_SS_CUSTOMER    "tpcc/c/"
#define FDB_SS_CUST_NAME   "tpcc/cn/"
#define FDB_SS_HISTORY     "tpcc/h/"
#define FDB_SS_ITEM        "tpcc/i/"
#define FDB_SS_STOCK       "tpcc/s/"
#define FDB_SS_OORDER      "tpcc/o/"
#define FDB_SS_OORDER_CID  "tpcc/oc/"
#define FDB_SS_NEW_ORDER   "tpcc/no/"
#define FDB_SS_ORDER_LINE  "tpcc/ol/"
#define FDB_SS_NEXT_O_ID   "tpcc/nid/"

uint32_t nurand(uint32_t A, uint32_t x, uint32_t y, unsigned int *seed, uint32_t C);
void tpcc_send_error(int status_code, const char *error_msg, h2o_req_t *req);
void send_json_str(const char *json, size_t len, h2o_req_t *req);

#endif

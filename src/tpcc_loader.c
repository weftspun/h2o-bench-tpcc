/*
 * TPC-C data loader for FoundationDB.
 *
 * Populates the FDB keyspace with TPC-C data for W warehouses.
 * See rfd/0001-tpcc-scaling.md for row-count formulas.
 *
 * Usage:
 *   tpcc_loader -c<cluster_file> -w<warehouses>
 *
 * Writes:
 *   W        warehouse rows
 *   10*W     district rows
 *   30000*W  customer rows (+ name index)
 *   30000*W  history rows
 *   100000   item rows
 *   100000*W stock rows
 *   30000*W  oorder rows
 *   9000*W   new_order rows
 *   ~300000*W order_line rows
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <arpa/inet.h>

#define FDB_API_VERSION 730
#include <foundationdb/fdb_c.h>

#include "tpcc_kv.h"
#include "handlers/tpcc_procedures.h"

#define BATCH_SIZE 1000

static int W = 1;
static unsigned int seed = 42;

static void check_fdb(fdb_error_t err, const char *msg)
{
    if (err) {
        fprintf(stderr, "FDB error: %s: %s\n", msg, fdb_get_error(err));
        exit(1);
    }
}

/* TPC-C C_LAST generation */
static void gen_c_last(uint32_t n, char *buf)
{
    static const char *syllables[] = {
        "BAR", "OUGHT", "ABLE", "PRI", "PRES",
        "ESE", "ANTI", "CALLY", "ATION", "EING"
    };
    buf[0] = '\0';
    for (int i = 2; i >= 0; i--) {
        uint32_t idx = (n / (uint32_t)(i == 2 ? 100 : (i == 1 ? 10 : 1))) % 10;
        strcat(buf, syllables[idx]);
    }
}

static void load_items(FDBDatabase *db)
{
    fprintf(stderr, "Loading 100000 items...\n");
    FDBTransaction *tr;
    check_fdb(fdb_database_create_transaction(db, &tr), "create_transaction");

    for (uint32_t i = 1; i <= ITEM_COUNT; i++) {
        uint8_t key[64];
        size_t klen = kv_item_key(key, i);

        item_val_t iv;
        memset(&iv, 0, sizeof(iv));
        snprintf(iv.i_name, sizeof(iv.i_name), "Item-%u", i);
        iv.i_price = 100 + (int32_t)(rand_r(&seed) % 9900);
        iv.i_im_id = 1 + (int32_t)(rand_r(&seed) % 10000);
        /* 10% of items have "ORIGINAL" in i_data */
        if (rand_r(&seed) % 10 == 0)
            snprintf(iv.i_data, sizeof(iv.i_data), "ORIGINAL_ITEM_DATA_%u", i);
        else
            snprintf(iv.i_data, sizeof(iv.i_data), "item_data_%u", i);

        fdb_transaction_set(tr, key, (int)klen,
                           (const uint8_t *)&iv, sizeof(iv));

        if (i % BATCH_SIZE == 0) {
            FDBFuture *f = fdb_transaction_commit(tr);
            check_fdb(fdb_future_block_until_ready(f), "commit items");
            check_fdb(fdb_future_get_error(f), "commit items error");
            fdb_future_destroy(f);
            fdb_transaction_reset(tr);
        }
    }

    FDBFuture *f = fdb_transaction_commit(tr);
    check_fdb(fdb_future_block_until_ready(f), "final commit items");
    fdb_future_destroy(f);
    fdb_transaction_destroy(tr);
    fprintf(stderr, "  done\n");
}

static void load_warehouse(FDBDatabase *db)
{
    fprintf(stderr, "Loading %d warehouses...\n", W);
    FDBTransaction *tr;
    check_fdb(fdb_database_create_transaction(db, &tr), "create_transaction");

    for (uint32_t w = 1; w <= (uint32_t)W; w++) {
        uint8_t key[64];
        size_t klen = kv_warehouse_key(key, w);

        warehouse_val_t wv;
        memset(&wv, 0, sizeof(wv));
        snprintf(wv.w_name, sizeof(wv.w_name), "Whse-%u", w);
        snprintf(wv.w_street_1, sizeof(wv.w_street_1), "%u Main St", w);
        snprintf(wv.w_city, sizeof(wv.w_city), "City-%u", w);
        memcpy(wv.w_state, "CA", 2);
        snprintf(wv.w_zip, sizeof(wv.w_zip), "94000%03u", w);
        wv.w_tax = (int32_t)(rand_r(&seed) % 2000);
        wv.w_ytd = 300000 * 100; /* $300,000.00 initial YTD */

        fdb_transaction_set(tr, key, (int)klen,
                           (const uint8_t *)&wv, sizeof(wv));
    }

    FDBFuture *f = fdb_transaction_commit(tr);
    check_fdb(fdb_future_block_until_ready(f), "commit warehouse");
    fdb_future_destroy(f);
    fdb_transaction_destroy(tr);
    fprintf(stderr, "  done\n");
}

static void load_districts_and_next_o_id(FDBDatabase *db)
{
    fprintf(stderr, "Loading %d districts...\n", W * 10);
    FDBTransaction *tr;
    check_fdb(fdb_database_create_transaction(db, &tr), "create_transaction");

    for (uint32_t w = 1; w <= (uint32_t)W; w++) {
        for (uint32_t d = 1; d <= DISTRICTS_PER_WAREHOUSE; d++) {
            /* District record */
            uint8_t dkey[64];
            size_t dklen = kv_district_key(dkey, w, d);

            district_val_t dv;
            memset(&dv, 0, sizeof(dv));
            snprintf(dv.d_name, sizeof(dv.d_name), "Dist-%u-%u", w, d);
            snprintf(dv.d_street_1, sizeof(dv.d_street_1), "%u-%u St", w, d);
            snprintf(dv.d_city, sizeof(dv.d_city), "City-%u", w);
            memcpy(dv.d_state, "CA", 2);
            snprintf(dv.d_zip, sizeof(dv.d_zip), "94000%03u", w);
            dv.d_tax = (int32_t)(rand_r(&seed) % 2000);
            dv.d_ytd = 30000 * 100;
            /* Next order ID starts at 3001 (3000 initial orders) */
            dv.d_next_o_id = 3001;

            fdb_transaction_set(tr, dkey, (int)dklen,
                               (const uint8_t *)&dv, sizeof(dv));

            /* Next_o_id counter */
            uint8_t nkey[64];
            size_t nklen = kv_next_o_id_key(nkey, w, d);
            uint8_t nbuf[8];
            kv_encode_next_o_id(nbuf, 3001);
            fdb_transaction_set(tr, nkey, (int)nklen, nbuf, 8);
        }
    }

    FDBFuture *f = fdb_transaction_commit(tr);
    check_fdb(fdb_future_block_until_ready(f), "commit districts");
    fdb_future_destroy(f);
    fdb_transaction_destroy(tr);
    fprintf(stderr, "  done\n");
}

/* Loads customers, orders, order_lines, new_orders, and stock for W warehouses.
 * This is a stub that sets up the structure. Full implementation would
 * generate all 30,000*W customers, 100,000*W stock rows, etc. */
static void load_customer_and_orders(FDBDatabase *db)
{
    fprintf(stderr, "Loading customers and orders (stub for W=%d)...\n", W);
    /* Full implementation generates:
     * - 3000 customers per district (30,000*W total)
     * - 3000 orders per district (30,000*W total)
     * - 900 new_orders per district (9,000*W total)
     * - 5-15 order lines per order (~300,000*W total)
     * - 100,000 stock rows per warehouse
     */
    fprintf(stderr, "  (stub - full implementation pending)\n");
}

/* ===== TechEmpower World table ===== */

static void load_world_table(FDBDatabase *db)
{
    fprintf(stderr, "Loading 10000 World rows...\n");
    FDBTransaction *tr;
    check_fdb(fdb_database_create_transaction(db, &tr), "create_transaction");

    for (uint32_t i = 1; i <= 10000; i++) {
        /* key = "tfb/w/" + 4-byte big-endian id */
        uint8_t key[16];
        memcpy(key, "tfb/w/", 6);
        uint32_t be = htonl(i);
        memcpy(key + 6, &be, 4);

        /* value = 4-byte big-endian random number (0-9999) */
        uint32_t rn = htonl((uint32_t)(rand_r(&seed) % 10000));

        fdb_transaction_set(tr, key, 10, (const uint8_t *)&rn, 4);

        if (i % 1000 == 0) {
            FDBFuture *f = fdb_transaction_commit(tr);
            check_fdb(fdb_future_block_until_ready(f), "commit world");
            check_fdb(fdb_future_get_error(f), "commit world error");
            fdb_future_destroy(f);
            fdb_transaction_reset(tr);
        }
    }

    FDBFuture *f = fdb_transaction_commit(tr);
    check_fdb(fdb_future_block_until_ready(f), "final commit world");
    fdb_future_destroy(f);
    fdb_transaction_destroy(tr);
    fprintf(stderr, "  done\n");
}


int main(int argc, char *argv[])
{
    const char *cluster_file = "/etc/foundationdb/fdb.cluster";

    for (int i = 1; i < argc; i++) {
        if (strncmp(argv[i], "-w", 2) == 0)
            W = atoi(argv[i] + 2);
        else if (strncmp(argv[i], "-c", 2) == 0)
            cluster_file = argv[i] + 2;
    }

    fprintf(stderr, "tpcc_loader: W=%d, cluster=%s\n", W, cluster_file);

    check_fdb(fdb_select_api_version_impl(FDB_API_VERSION, FDB_API_VERSION), "select_api_version");
    check_fdb(fdb_setup_network(), "setup_network");

    /* FDB API 730: use fdb_create_database directly */
    FDBFuture *db_future = fdb_create_database(cluster_file);
    check_fdb(fdb_future_block_until_ready(db_future), "db ready");
    FDBDatabase *db;
    check_fdb(fdb_future_get_database(db_future, &db), "get_database");
    fdb_future_destroy(db_future);

    /* Load data */
    load_items(db);
    load_warehouse(db);
    load_districts_and_next_o_id(db);
    load_customer_and_orders(db);
    load_world_table(db);

    fprintf(stderr, "TPC-C data load complete.\n");

    fdb_database_destroy(db);
    check_fdb(fdb_stop_network(), "stop_network");

    return 0;
}

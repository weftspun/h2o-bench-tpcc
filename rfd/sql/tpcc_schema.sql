-- TPC-C schema (PostgreSQL / CockroachDB compatible)
-- Based on TPC-C specification, scaled by warehouse count (W).
-- See rfd/0001-tpcc-scaling.md for exact row-count formulas.
-- FDB keyspace mapping: see rfd/0009-fdb-keyspace-design.md.

-- WAREHOUSE: W rows (1 per warehouse)
CREATE TABLE IF NOT EXISTS warehouse (
    w_id       INTEGER PRIMARY KEY,
    w_name     VARCHAR(10),
    w_street_1 VARCHAR(20),
    w_street_2 VARCHAR(20),
    w_city     VARCHAR(20),
    w_state    CHAR(2),
    w_zip      CHAR(9),
    w_tax      DECIMAL(4,4),
    w_ytd      DECIMAL(12,2)
);

-- DISTRICT: 10*W rows (10 districts per warehouse)
CREATE TABLE IF NOT EXISTS district (
    d_id        INTEGER,
    d_w_id      INTEGER,
    d_name      VARCHAR(10),
    d_street_1  VARCHAR(20),
    d_street_2  VARCHAR(20),
    d_city      VARCHAR(20),
    d_state     CHAR(2),
    d_zip       CHAR(9),
    d_tax       DECIMAL(4,4),
    d_ytd       DECIMAL(12,2),
    d_next_o_id INTEGER,
    PRIMARY KEY (d_w_id, d_id),
    FOREIGN KEY (d_w_id) REFERENCES warehouse(w_id)
);

-- CUSTOMER: 30,000*W rows (3,000 per district)
CREATE TABLE IF NOT EXISTS customer (
    c_id           INTEGER,
    c_d_id         INTEGER,
    c_w_id         INTEGER,
    c_first        VARCHAR(16),
    c_middle       CHAR(2),
    c_last         VARCHAR(16),
    c_street_1     VARCHAR(20),
    c_street_2     VARCHAR(20),
    c_city         VARCHAR(20),
    c_state        CHAR(2),
    c_zip          CHAR(9),
    c_phone        VARCHAR(16),
    c_since        TIMESTAMP,
    c_credit       CHAR(2),
    c_credit_lim   DECIMAL(12,2),
    c_discount     DECIMAL(4,4),
    c_balance      DECIMAL(12,2),
    c_ytd_payment  DECIMAL(12,2),
    c_payment_cnt  INTEGER,
    c_delivery_cnt INTEGER,
    c_data         VARCHAR(500),
    PRIMARY KEY (c_w_id, c_d_id, c_id),
    FOREIGN KEY (c_w_id, c_d_id) REFERENCES district(d_w_id, d_id)
);

-- HISTORY: 30,000*W initial rows, grows during run
CREATE TABLE IF NOT EXISTS history (
    h_c_id   INTEGER,
    h_c_d_id INTEGER,
    h_c_w_id INTEGER,
    h_d_id   INTEGER,
    h_w_id   INTEGER,
    h_date   TIMESTAMP,
    h_amount DECIMAL(6,2),
    h_data   VARCHAR(24)
);

-- ITEM: 100,000 rows (fixed, not scaled by W)
CREATE TABLE IF NOT EXISTS item (
    i_id    INTEGER PRIMARY KEY,
    i_name  VARCHAR(24),
    i_price DECIMAL(5,2),
    i_data  VARCHAR(50),
    i_im_id INTEGER
);

-- STOCK: 100,000*W rows (one per item per warehouse)
CREATE TABLE IF NOT EXISTS stock (
    s_i_id       INTEGER,
    s_w_id       INTEGER,
    s_quantity   INTEGER,
    s_dist_01    CHAR(24),
    s_dist_02    CHAR(24),
    s_dist_03    CHAR(24),
    s_dist_04    CHAR(24),
    s_dist_05    CHAR(24),
    s_dist_06    CHAR(24),
    s_dist_07    CHAR(24),
    s_dist_08    CHAR(24),
    s_dist_09    CHAR(24),
    s_dist_10    CHAR(24),
    s_ytd        INTEGER,
    s_order_cnt  INTEGER,
    s_remote_cnt INTEGER,
    s_data       VARCHAR(50),
    PRIMARY KEY (s_w_id, s_i_id),
    FOREIGN KEY (s_i_id) REFERENCES item(i_id),
    FOREIGN KEY (s_w_id) REFERENCES warehouse(w_id)
);

-- OORDER: 30,000*W initial rows, grows during run
CREATE TABLE IF NOT EXISTS oorder (
    o_id         INTEGER,
    o_w_id       INTEGER,
    o_d_id       INTEGER,
    o_c_id       INTEGER,
    o_carrier_id INTEGER,
    o_ol_cnt     INTEGER,
    o_all_local  INTEGER,
    o_entry_d    TIMESTAMP,
    PRIMARY KEY (o_w_id, o_d_id, o_id),
    UNIQUE (o_w_id, o_d_id, o_c_id, o_id),
    FOREIGN KEY (o_w_id, o_d_id) REFERENCES district(d_w_id, d_id),
    FOREIGN KEY (o_w_id, o_d_id, o_c_id) REFERENCES customer(c_w_id, c_d_id, c_id)
);

-- NEW_ORDER: 9,000*W initial rows (undelivered orders)
CREATE TABLE IF NOT EXISTS new_order (
    no_o_id INTEGER,
    no_w_id INTEGER,
    no_d_id INTEGER,
    PRIMARY KEY (no_w_id, no_d_id, no_o_id),
    FOREIGN KEY (no_w_id, no_d_id, no_o_id) REFERENCES oorder(o_w_id, o_d_id, o_id)
);

-- ORDER_LINE: ~300,000*W initial rows (5-15 per order)
CREATE TABLE IF NOT EXISTS order_line (
    ol_o_id        INTEGER,
    ol_w_id        INTEGER,
    ol_d_id        INTEGER,
    ol_number      INTEGER,
    ol_i_id        INTEGER,
    ol_supply_w_id INTEGER,
    ol_delivery_d  TIMESTAMP,
    ol_quantity    INTEGER,
    ol_amount      DECIMAL(6,2),
    ol_dist_info   CHAR(24),
    PRIMARY KEY (ol_w_id, ol_d_id, ol_o_id, ol_number),
    FOREIGN KEY (ol_w_id, ol_d_id, ol_o_id) REFERENCES oorder(o_w_id, o_d_id, o_id),
    FOREIGN KEY (ol_supply_w_id, ol_i_id) REFERENCES stock(s_w_id, s_i_id)
);

-- Indexes for TPC-C workload patterns
CREATE INDEX IF NOT EXISTS idx_customer_name ON customer (c_w_id, c_d_id, c_last, c_first);
CREATE INDEX IF NOT EXISTS idx_order_line_i_id ON order_line (ol_w_id, ol_d_id, ol_o_id);
CREATE INDEX IF NOT EXISTS idx_history ON history (h_c_w_id, h_c_d_id, h_c_id);

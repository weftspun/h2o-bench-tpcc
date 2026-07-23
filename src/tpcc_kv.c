/*
 * TPC-C key-value encoding layer for FoundationDB.
 *
 * Keys: prefix + big-endian uint32 components.
 * Values: packed structs with network byte order integers.
 *
 * See rfd/0009-fdb-keyspace-design.md.
 */

#include "tpcc_kv.h"

#include <string.h>

void kv_encode_u32_be(uint8_t *buf, uint32_t val)
{
    buf[0] = (val >> 24) & 0xFF;
    buf[1] = (val >> 16) & 0xFF;
    buf[2] = (val >> 8) & 0xFF;
    buf[3] = val & 0xFF;
}

uint32_t kv_decode_u32_be(const uint8_t *buf)
{
    return ((uint32_t)buf[0] << 24) | ((uint32_t)buf[1] << 16) |
           ((uint32_t)buf[2] << 8) | (uint32_t)buf[3];
}

void kv_encode_next_o_id(uint8_t *buf, int32_t next_o_id)
{
    int64_t val = next_o_id;
    buf[0] = (val >> 56) & 0xFF;
    buf[1] = (val >> 48) & 0xFF;
    buf[2] = (val >> 40) & 0xFF;
    buf[3] = (val >> 32) & 0xFF;
    buf[4] = (val >> 24) & 0xFF;
    buf[5] = (val >> 16) & 0xFF;
    buf[6] = (val >> 8) & 0xFF;
    buf[7] = val & 0xFF;
}

int32_t kv_decode_next_o_id(const uint8_t *buf, int len)
{
    if (len < 8)
        return 0;
    int64_t val = ((int64_t)buf[0] << 56) | ((int64_t)buf[1] << 48) |
                  ((int64_t)buf[2] << 40) | ((int64_t)buf[3] << 32) |
                  ((int64_t)buf[4] << 24) | ((int64_t)buf[5] << 16) |
                  ((int64_t)buf[6] << 8) | (int64_t)buf[7];
    return (int32_t)val;
}

static size_t append_prefix(uint8_t *buf, const char *prefix)
{
    size_t len = strlen(prefix);
    memcpy(buf, prefix, len);
    return len;
}

size_t kv_warehouse_key(uint8_t *buf, uint32_t w_id)
{
    size_t off = append_prefix(buf, SS_WAREHOUSE);
    kv_encode_u32_be(buf + off, w_id);
    return off + 4;
}

size_t kv_district_key(uint8_t *buf, uint32_t w_id, uint32_t d_id)
{
    size_t off = append_prefix(buf, SS_DISTRICT);
    kv_encode_u32_be(buf + off, w_id);
    off += 4;
    kv_encode_u32_be(buf + off, d_id);
    return off + 4;
}

size_t kv_customer_key(uint8_t *buf, uint32_t w_id, uint32_t d_id, uint32_t c_id)
{
    size_t off = append_prefix(buf, SS_CUSTOMER);
    kv_encode_u32_be(buf + off, w_id);
    off += 4;
    kv_encode_u32_be(buf + off, d_id);
    off += 4;
    kv_encode_u32_be(buf + off, c_id);
    return off + 4;
}

size_t kv_customer_name_key(uint8_t *buf, uint32_t w_id, uint32_t d_id,
                             const char *c_last, const char *c_first, uint32_t c_id)
{
    size_t off = append_prefix(buf, SS_CUST_NAME);
    kv_encode_u32_be(buf + off, w_id);
    off += 4;
    kv_encode_u32_be(buf + off, d_id);
    off += 4;
    size_t slen = strlen(c_last);
    memcpy(buf + off, c_last, slen);
    off += slen;
    buf[off++] = 0;
    slen = strlen(c_first);
    memcpy(buf + off, c_first, slen);
    off += slen;
    buf[off++] = 0;
    kv_encode_u32_be(buf + off, c_id);
    return off + 4;
}

size_t kv_item_key(uint8_t *buf, uint32_t i_id)
{
    size_t off = append_prefix(buf, SS_ITEM);
    kv_encode_u32_be(buf + off, i_id);
    return off + 4;
}

size_t kv_stock_key(uint8_t *buf, uint32_t w_id, uint32_t i_id)
{
    size_t off = append_prefix(buf, SS_STOCK);
    kv_encode_u32_be(buf + off, w_id);
    off += 4;
    kv_encode_u32_be(buf + off, i_id);
    return off + 4;
}

size_t kv_oorder_key(uint8_t *buf, uint32_t w_id, uint32_t d_id, uint32_t o_id)
{
    size_t off = append_prefix(buf, SS_OORDER);
    kv_encode_u32_be(buf + off, w_id);
    off += 4;
    kv_encode_u32_be(buf + off, d_id);
    off += 4;
    kv_encode_u32_be(buf + off, o_id);
    return off + 4;
}

size_t kv_new_order_key(uint8_t *buf, uint32_t w_id, uint32_t d_id, uint32_t o_id)
{
    size_t off = append_prefix(buf, SS_NEW_ORDER);
    kv_encode_u32_be(buf + off, w_id);
    off += 4;
    kv_encode_u32_be(buf + off, d_id);
    off += 4;
    kv_encode_u32_be(buf + off, o_id);
    return off + 4;
}

size_t kv_order_line_key(uint8_t *buf, uint32_t w_id, uint32_t d_id,
                          uint32_t o_id, uint32_t ol_number)
{
    size_t off = append_prefix(buf, SS_ORDER_LINE);
    kv_encode_u32_be(buf + off, w_id);
    off += 4;
    kv_encode_u32_be(buf + off, d_id);
    off += 4;
    kv_encode_u32_be(buf + off, o_id);
    off += 4;
    kv_encode_u32_be(buf + off, ol_number);
    return off + 4;
}

size_t kv_next_o_id_key(uint8_t *buf, uint32_t w_id, uint32_t d_id)
{
    size_t off = append_prefix(buf, SS_NEXT_O_ID);
    kv_encode_u32_be(buf + off, w_id);
    off += 4;
    kv_encode_u32_be(buf + off, d_id);
    return off + 4;
}

size_t kv_stock_range_begin(uint8_t *buf, uint32_t w_id)
{
    size_t off = append_prefix(buf, SS_STOCK);
    kv_encode_u32_be(buf + off, w_id);
    off += 4;
    kv_encode_u32_be(buf + off, 0);
    return off + 4;
}

size_t kv_stock_range_end(uint8_t *buf, uint32_t w_id)
{
    size_t off = append_prefix(buf, SS_STOCK);
    kv_encode_u32_be(buf + off, w_id);
    off += 4;
    kv_encode_u32_be(buf + off, 0xFFFFFFFF);
    return off + 4;
}

size_t kv_new_order_range_begin(uint8_t *buf, uint32_t w_id, uint32_t d_id)
{
    size_t off = append_prefix(buf, SS_NEW_ORDER);
    kv_encode_u32_be(buf + off, w_id);
    off += 4;
    kv_encode_u32_be(buf + off, d_id);
    off += 4;
    kv_encode_u32_be(buf + off, 0);
    return off + 4;
}

size_t kv_new_order_range_end(uint8_t *buf, uint32_t w_id, uint32_t d_id)
{
    size_t off = append_prefix(buf, SS_NEW_ORDER);
    kv_encode_u32_be(buf + off, w_id);
    off += 4;
    kv_encode_u32_be(buf + off, d_id);
    off += 4;
    kv_encode_u32_be(buf + off, 0xFFFFFFFF);
    return off + 4;
}

size_t kv_order_line_range_begin(uint8_t *buf, uint32_t w_id, uint32_t d_id, uint32_t o_id)
{
    size_t off = append_prefix(buf, SS_ORDER_LINE);
    kv_encode_u32_be(buf + off, w_id);
    off += 4;
    kv_encode_u32_be(buf + off, d_id);
    off += 4;
    kv_encode_u32_be(buf + off, o_id);
    off += 4;
    kv_encode_u32_be(buf + off, 0);
    return off + 4;
}

size_t kv_order_line_range_end(uint8_t *buf, uint32_t w_id, uint32_t d_id, uint32_t o_id)
{
    size_t off = append_prefix(buf, SS_ORDER_LINE);
    kv_encode_u32_be(buf + off, w_id);
    off += 4;
    kv_encode_u32_be(buf + off, d_id);
    off += 4;
    kv_encode_u32_be(buf + off, o_id);
    off += 4;
    kv_encode_u32_be(buf + off, 0xFFFFFFFF);
    return off + 4;
}

#include <h2o.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <yajl/yajl_gen.h>
#include <foundationdb/fdb_c.h>

#include "fdb_database.h"
#include "error.h"
#include "global_data.h"
#include "tpcc_procedures.h"

uint32_t nurand(uint32_t A, uint32_t x, uint32_t y, unsigned int *seed, uint32_t C)
{
    const uint32_t r1 = (uint32_t)rand_r(seed) % (A + 1);
    const uint32_t r2 = x + (uint32_t)rand_r(seed) % (y - x + 1);
    return ((r1 | r2) + C) % (y - x + 1) + x;
}

void send_error(int status_code, const char *error_msg, h2o_req_t *req)
{
    h2o_send_error_503(req, "Service Unavailable", error_msg, 0);
}

void send_json_response(const char *json, size_t len, h2o_req_t *req)
{
    h2o_iovec_t body = h2o_strdup(&req->pool, json, len);
    h2o_send_inline(req, body.base, body.len);
}

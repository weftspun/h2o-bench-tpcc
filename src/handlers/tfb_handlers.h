#ifndef TFB_HANDLERS_H_
#define TFB_HANDLERS_H_

#include <h2o.h>
#include <stdint.h>
#include <foundationdb/fdb_c.h>

#include "fdb_database.h"

/*
 * TechEmpower Framework Benchmark handlers.
 *
 * Implements the 6 standard TFB tests exactly as the h2o TFB entry does,
 * adapted to use FoundationDB instead of Postgres.
 *
 * World table: 10,000 rows, each with id (1-10000) and randomNumber (0-9999).
 * In FDB: key = "tfb/w/{id_be32}", value = 4-byte BE uint32 randomNumber.
 */

#define TFB_MAX_ID 10000
#define TFB_MAX_QUERIES 500
#define TFB_WORLD_PREFIX "tfb/w/"
#define TFB_HELLO_RESPONSE "Hello, World!"
#define TFB_QUERIES_PARAMETER "queries="

/* Initialize all TFB routes on the given host config. */
void initialize_tfb_handlers(h2o_hostconf_t *hostconf,
                              fdb_thread_state_t *fdb_state,
                              unsigned int *seed);

#endif

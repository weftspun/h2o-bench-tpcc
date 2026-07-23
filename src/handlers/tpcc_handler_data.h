#ifndef TPCC_HANDLER_DATA_H_
#define TPCC_HANDLER_DATA_H_

#include "fdb_database.h"
#include "global_data.h"
#include "list.h"

void cleanup_tpcc_handler_thread_data(request_handler_thread_data_t *data);
void cleanup_tpcc_handlers(request_handler_data_t *data);
void initialize_tpcc_handler_thread_data(thread_context_t *ctx,
                                          const request_handler_data_t *data,
                                          request_handler_thread_data_t *thread_data);
void initialize_tpcc_handlers(h2o_hostconf_t *hostconf,
                               h2o_access_log_filehandle_t *log_handle,
                               request_handler_data_t *data);

#endif

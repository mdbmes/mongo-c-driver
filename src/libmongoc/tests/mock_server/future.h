#ifndef FUTURE_H
#define FUTURE_H

#include <bson/bson.h>

#include "future-value.h"
#include "mongoc/mongoc-thread-private.h"

/**************************************************
 *
 * Generated by build/generate-future-functions.py.
 *
 * DO NOT EDIT THIS FILE.
 *
 *************************************************/
/* clang-format off */

typedef struct
{
   bool             resolved;
   bool             awaited;
   future_value_t   return_value;
   int              argc;
   future_value_t  *argv;
   mongoc_cond_t    cond;
   bson_mutex_t     mutex;
   bson_thread_t    thread;
} future_t;

future_t *future_new (future_value_type_t return_type, int argc);

future_value_t *future_get_param (future_t *future, int i);

void future_start (future_t *future,
                   BSON_THREAD_FUN_TYPE (start_routine));

void future_resolve (future_t *future, future_value_t return_value);

bool future_wait (future_t *future);
bool future_wait_max (future_t *future, int64_t timeout_ms);

void future_get_void (future_t *future);


bool
future_get_bool (future_t *future);

char_ptr
future_get_char_ptr (future_t *future);

char_ptr_ptr
future_get_char_ptr_ptr (future_t *future);

int
future_get_int (future_t *future);

int64_t
future_get_int64_t (future_t *future);

size_t
future_get_size_t (future_t *future);

ssize_t
future_get_ssize_t (future_t *future);

uint32_t
future_get_uint32_t (future_t *future);

void_ptr
future_get_void_ptr (future_t *future);

const_char_ptr
future_get_const_char_ptr (future_t *future);

bool_ptr
future_get_bool_ptr (future_t *future);

bson_error_ptr
future_get_bson_error_ptr (future_t *future);

bson_ptr
future_get_bson_ptr (future_t *future);

const_bson_ptr
future_get_const_bson_ptr (future_t *future);

const_bson_ptr_ptr
future_get_const_bson_ptr_ptr (future_t *future);

mongoc_async_ptr
future_get_mongoc_async_ptr (future_t *future);

mongoc_bulk_operation_ptr
future_get_mongoc_bulk_operation_ptr (future_t *future);

mongoc_client_ptr
future_get_mongoc_client_ptr (future_t *future);

mongoc_client_pool_ptr
future_get_mongoc_client_pool_ptr (future_t *future);

mongoc_collection_ptr
future_get_mongoc_collection_ptr (future_t *future);

mongoc_cluster_ptr
future_get_mongoc_cluster_ptr (future_t *future);

mongoc_cmd_parts_ptr
future_get_mongoc_cmd_parts_ptr (future_t *future);

mongoc_cursor_ptr
future_get_mongoc_cursor_ptr (future_t *future);

mongoc_database_ptr
future_get_mongoc_database_ptr (future_t *future);

mongoc_gridfs_file_ptr
future_get_mongoc_gridfs_file_ptr (future_t *future);

mongoc_gridfs_ptr
future_get_mongoc_gridfs_ptr (future_t *future);

mongoc_insert_flags_t
future_get_mongoc_insert_flags_t (future_t *future);

mongoc_iovec_ptr
future_get_mongoc_iovec_ptr (future_t *future);

mongoc_server_stream_ptr
future_get_mongoc_server_stream_ptr (future_t *future);

mongoc_query_flags_t
future_get_mongoc_query_flags_t (future_t *future);

const_mongoc_index_opt_t
future_get_const_mongoc_index_opt_t (future_t *future);

mongoc_server_description_ptr
future_get_mongoc_server_description_ptr (future_t *future);

mongoc_ss_optype_t
future_get_mongoc_ss_optype_t (future_t *future);

mongoc_topology_ptr
future_get_mongoc_topology_ptr (future_t *future);

mongoc_write_concern_ptr
future_get_mongoc_write_concern_ptr (future_t *future);

mongoc_change_stream_ptr
future_get_mongoc_change_stream_ptr (future_t *future);

mongoc_remove_flags_t
future_get_mongoc_remove_flags_t (future_t *future);

const_mongoc_find_and_modify_opts_ptr
future_get_const_mongoc_find_and_modify_opts_ptr (future_t *future);

const_mongoc_iovec_ptr
future_get_const_mongoc_iovec_ptr (future_t *future);

const_mongoc_read_prefs_ptr
future_get_const_mongoc_read_prefs_ptr (future_t *future);

const_mongoc_write_concern_ptr
future_get_const_mongoc_write_concern_ptr (future_t *future);

const_mongoc_ss_log_context_ptr
future_get_const_mongoc_ss_log_context_ptr (future_t *future);


void future_destroy (future_t *future);

#endif /* FUTURE_H */


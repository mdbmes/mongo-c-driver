/*
 * Copyright 2020 MongoDB, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "mongoc-prelude.h"

#ifndef MONGOC_STRUCTURED_LOG_COMMAND_PRIVATE_H
#define MONGOC_STRUCTURED_LOG_COMMAND_PRIVATE_H

#include "mongoc-structured-log.h"
#include "mongoc-cmd-private.h"

typedef struct {
   const char *command_name;
   const char *db_name;
   const bson_t *command;
   const bson_t *reply;
   const bson_error_t *error;
   int64_t duration;
   int64_t operation_id;
   uint32_t request_id;
   uint32_t driver_connection_id;
   uint32_t server_connection_id;
   bool explicit_session;
} _mongoc_structured_log_command_t;

void
mongoc_structured_log_command_started (const bson_t *command,
                                       const char *command_name,
                                       const char *db_name,
                                       int64_t operation_id,
                                       uint32_t request_id,
                                       uint32_t driver_connection_id,
                                       uint32_t server_connection_id,
                                       bool explicit_session);

void
mongoc_structured_log_command_success (const char *command_name,
                                       int64_t operation_id,
                                       const bson_t *reply,
                                       uint64_t duration,
                                       uint32_t request_id,
                                       uint32_t driver_connection_id,
                                       uint32_t server_connection_id,
                                       bool explicit_session);

void
mongoc_structured_log_command_failure (const char *command_name,
                                       int64_t operation_id,
                                       const bson_t *reply,
                                       const bson_error_t *error,
                                       uint32_t request_id,
                                       uint32_t driver_connection_id,
                                       uint32_t server_connection_id,
                                       bool explicit_session);

#endif /* MONGOC_STRUCTURED_LOG_COMMAND_PRIVATE_H */

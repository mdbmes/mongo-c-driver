/*
 * Copyright 2009-present MongoDB, Inc.
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


#include "mongoc-log-and-monitor-private.h"
#include "mongoc-structured-log-private.h"
#include "common-atomic-private.h"

static mongoc_log_and_monitor_serial_t
_mongoc_log_and_monitor_instance_new_serial (void)
{
   static int64_t serial_number_atomic = 0;
   mongoc_log_and_monitor_serial_t result =
      1 + mcommon_atomic_int64_fetch_add (&serial_number_atomic, 1, mcommon_memory_order_seq_cst);
   BSON_ASSERT (result);
   return result;
}

/**
 * @brief Initializes the contents of a just-allocated mongoc_log_and_monitor_instance_t
 *
 * Captures default structured log options from the environment
 */
void
mongoc_log_and_monitor_instance_init (mongoc_log_and_monitor_instance_t *new_instance)
{
   BSON_ASSERT_PARAM (new_instance);

   // Init apm_callbacks and serial
   mongoc_log_and_monitor_instance_set_apm_callbacks (new_instance, NULL, NULL);

   // Note: For now this apm_mutex is quite limited in scope, held only during callbacks from the server_monitor
   bson_mutex_init (&new_instance->apm_mutex);

   mongoc_structured_log_opts_t *structured_log_opts = mongoc_structured_log_opts_new ();
   new_instance->structured_log = mongoc_structured_log_instance_new (structured_log_opts);
   mongoc_structured_log_opts_destroy (structured_log_opts);
}

/**
 * @brief Destroy the contents of a mongoc_log_and_monitor_instance_t
 *
 * Does not try to free the outer memory allocation; it will be part of another object.
 * There must not be any other threads using the instance concurrently.
 */
void
mongoc_log_and_monitor_instance_destroy_contents (mongoc_log_and_monitor_instance_t *instance)
{
   BSON_ASSERT_PARAM (instance);

   BSON_ASSERT (instance->structured_log);
   mongoc_structured_log_instance_destroy (instance->structured_log);
   instance->structured_log = NULL;

   bson_mutex_destroy (&instance->apm_mutex);
}

/**
 * @brief Set the APM callbacks in a mongoc_log_and_monitor_instance_t
 *
 * There must not be any other threads using the instance concurrently.
 * In single threaded mode, this is only valid on the thread that owns the
 * client. In pooled mode, it's only valid prior to the first client init.
 */
void
mongoc_log_and_monitor_instance_set_apm_callbacks (mongoc_log_and_monitor_instance_t *instance,
                                                   mongoc_apm_callbacks_t *callbacks,
                                                   void *context)
{
   BSON_ASSERT_PARAM (instance);
   instance->apm_callbacks = callbacks ? *callbacks : (mongoc_apm_callbacks_t){0};
   instance->apm_context = context;
   instance->serial = _mongoc_log_and_monitor_instance_new_serial ();
}

/**
 * @brief Set the structured log options in a mongoc_log_and_monitor_instance_t
 *
 * Replace the instance's structured logging options. Options are copied.
 *
 * There must not be any other threads using the instance concurrently.
 * In single threaded mode, this is only valid on the thread that owns the
 * client. In pooled mode, it's only valid prior to the first client init.
 */
void
mongoc_log_and_monitor_instance_set_structured_log_opts (mongoc_log_and_monitor_instance_t *instance,
                                                         const mongoc_structured_log_opts_t *opts)
{
   BSON_ASSERT_PARAM (instance);
   mongoc_structured_log_instance_destroy (instance->structured_log);
   instance->structured_log = mongoc_structured_log_instance_new (opts);
   instance->serial = _mongoc_log_and_monitor_instance_new_serial ();
}

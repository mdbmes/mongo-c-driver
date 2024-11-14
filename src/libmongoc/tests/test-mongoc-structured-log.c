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

#include <mongoc/mongoc.h>

#include "mongoc/mongoc-structured-log-private.h"
#include "TestSuite.h"

typedef struct log_assumption {
   mongoc_structured_log_envelope_t expected_envelope;
   bson_t *expected_bson;
   int expected_calls;
   int calls;
} log_assumption;

typedef struct structured_log_state {
   mongoc_structured_log_func_t handler;
   void *data;
} structured_log_state;

static BSON_INLINE structured_log_state
save_state (void)
{
   structured_log_state state;
   mongoc_structured_log_get_handler (&state.handler, &state.data);
   return state;
}

static BSON_INLINE void
restore_state (structured_log_state state)
{
   mongoc_structured_log_set_handler (state.handler, state.data);
}

static void
structured_log_func (const mongoc_structured_log_entry_t *entry, void *user_data)
{
   struct log_assumption *assumption = (struct log_assumption *) user_data;

   assumption->calls++;

   ASSERT_CMPINT (assumption->calls, <=, assumption->expected_calls);

   ASSERT_CMPINT (entry->envelope.level, ==, assumption->expected_envelope.level);
   ASSERT_CMPINT (entry->envelope.component, ==, assumption->expected_envelope.component);
   ASSERT_CMPSTR (entry->envelope.message, assumption->expected_envelope.message);

   ASSERT_CMPINT (entry->envelope.level, ==, mongoc_structured_log_entry_get_level (entry));
   ASSERT_CMPINT (entry->envelope.component, ==, mongoc_structured_log_entry_get_component (entry));

   // Each call to message_as_bson allocates an identical copy
   bson_t *bson_1 = mongoc_structured_log_entry_message_as_bson (entry);
   bson_t *bson_2 = mongoc_structured_log_entry_message_as_bson (entry);

   // Compare for exact bson equality *after* comparing json strings, to give a more user friendly error on most
   // failures
   char *json_actual = bson_as_relaxed_extended_json (bson_1, NULL);
   char *json_expected = bson_as_relaxed_extended_json (assumption->expected_bson, NULL);
   ASSERT_CMPSTR (json_actual, json_expected);

   ASSERT (bson_equal (bson_1, assumption->expected_bson));
   ASSERT (bson_equal (bson_2, assumption->expected_bson));
   bson_destroy (bson_2);
   bson_destroy (bson_1);
   bson_free (json_actual);
   bson_free (json_expected);
}

void
test_structured_log_plain (void)
{
   struct log_assumption assumption = {
      .expected_envelope.level = MONGOC_STRUCTURED_LOG_LEVEL_WARNING,
      .expected_envelope.component = MONGOC_STRUCTURED_LOG_COMPONENT_COMMAND,
      .expected_envelope.message = "Plain log entry",
      .expected_bson = BCON_NEW ("message", BCON_UTF8 ("Plain log entry")),
      .expected_calls = 1,
   };

   structured_log_state old_state = save_state ();
   mongoc_structured_log_set_handler (structured_log_func, &assumption);

   mongoc_structured_log (
      MONGOC_STRUCTURED_LOG_LEVEL_WARNING, MONGOC_STRUCTURED_LOG_COMPONENT_COMMAND, "Plain log entry");

   ASSERT_CMPINT (assumption.calls, ==, 1);
   restore_state (old_state);
   bson_destroy (assumption.expected_bson);
}

void
test_structured_log_plain_with_extra_data (void)
{
   struct log_assumption assumption = {
      .expected_envelope.level = MONGOC_STRUCTURED_LOG_LEVEL_WARNING,
      .expected_envelope.component = MONGOC_STRUCTURED_LOG_COMPONENT_COMMAND,
      .expected_envelope.message = "Plain log entry with extra data",
      .expected_bson = BCON_NEW ("message", BCON_UTF8 ("Plain log entry with extra data"), "extra", BCON_INT32 (1)),
      .expected_calls = 1,
   };

   structured_log_state old_state = save_state ();
   mongoc_structured_log_set_handler (structured_log_func, &assumption);

   mongoc_structured_log (MONGOC_STRUCTURED_LOG_LEVEL_WARNING,
                          MONGOC_STRUCTURED_LOG_COMPONENT_COMMAND,
                          "Plain log entry with extra data",
                          int32 ("extra", 1));

   ASSERT_CMPINT (assumption.calls, ==, 1);
   restore_state (old_state);
   bson_destroy (assumption.expected_bson);
}

void
test_structured_log_basic_data_types (void)
{
   const char non_terminated_test_string[] = {0, 1, 2, 3, 'a', '\\'};
   bson_t *bson_str_n = bson_new ();
   bson_append_utf8 (bson_str_n, "kStrN1", -1, non_terminated_test_string, sizeof non_terminated_test_string);
   bson_append_utf8 (bson_str_n, "kStrN2", -1, non_terminated_test_string, sizeof non_terminated_test_string);

   struct log_assumption assumption = {
      .expected_envelope.level = MONGOC_STRUCTURED_LOG_LEVEL_WARNING,
      .expected_envelope.component = MONGOC_STRUCTURED_LOG_COMPONENT_COMMAND,
      .expected_envelope.message = "Log entry with all basic data types",
      .expected_bson = BCON_NEW ("message",
                                 BCON_UTF8 ("Log entry with all basic data types"),
                                 "kStr",
                                 BCON_UTF8 ("string value"),
                                 "kNullStr",
                                 BCON_NULL,
                                 BCON (bson_str_n),
                                 "kNullStrN1",
                                 BCON_NULL,
                                 "kNullStrN2",
                                 BCON_NULL,
                                 "kNullStrN3",
                                 BCON_NULL,
                                 "kInt32",
                                 BCON_INT32 (-12345),
                                 "kInt64",
                                 BCON_INT64 (0x76543210aabbccdd),
                                 "kTrue",
                                 BCON_BOOL (true),
                                 "kFalse",
                                 BCON_BOOL (false)),
      .expected_calls = 1,
   };

   structured_log_state old_state = save_state ();
   mongoc_structured_log_set_handler (structured_log_func, &assumption);

   mongoc_structured_log (MONGOC_STRUCTURED_LOG_LEVEL_WARNING,
                          MONGOC_STRUCTURED_LOG_COMPONENT_COMMAND,
                          "Log entry with all basic data types",
                          utf8 ("kStr", "string value"),
                          utf8 ("kNullStr", NULL),
                          utf8 (NULL, NULL),
                          utf8_nn ("kStrN1ZZZ", 6, non_terminated_test_string, sizeof non_terminated_test_string),
                          utf8_n ("kStrN2", non_terminated_test_string, sizeof non_terminated_test_string),
                          utf8_nn ("kNullStrN1ZZZ", 10, NULL, 12345),
                          utf8_nn ("kNullStrN2", -1, NULL, 12345),
                          utf8_nn (NULL, 999, NULL, 999),
                          utf8_n ("kNullStrN3", NULL, 12345),
                          int32 ("kInt32", -12345),
                          int32 (NULL, 9999),
                          int64 ("kInt64", 0x76543210aabbccdd),
                          int64 (NULL, -1),
                          boolean ("kTrue", true),
                          boolean ("kFalse", false),
                          boolean (NULL, true));

   ASSERT_CMPINT (assumption.calls, ==, 1);
   restore_state (old_state);
   bson_destroy (assumption.expected_bson);
   bson_destroy (bson_str_n);
}

void
test_structured_log_json (void)
{
   struct log_assumption assumption = {
      .expected_envelope.level = MONGOC_STRUCTURED_LOG_LEVEL_WARNING,
      .expected_envelope.component = MONGOC_STRUCTURED_LOG_COMPONENT_COMMAND,
      .expected_envelope.message = "Log entry with deferred BSON-to-JSON",
      .expected_bson = BCON_NEW (
         "message", BCON_UTF8 ("Log entry with deferred BSON-to-JSON"), "kJSON", BCON_UTF8 ("{ \"k\" : \"v\" }")),
      .expected_calls = 1,
   };

   bson_t *json_doc = BCON_NEW ("k", BCON_UTF8 ("v"));

   structured_log_state old_state = save_state ();
   mongoc_structured_log_set_handler (structured_log_func, &assumption);

   mongoc_structured_log (MONGOC_STRUCTURED_LOG_LEVEL_WARNING,
                          MONGOC_STRUCTURED_LOG_COMPONENT_COMMAND,
                          "Log entry with deferred BSON-to-JSON",
                          bson_as_json ("kJSON", json_doc),
                          bson_as_json (NULL, NULL));

   ASSERT_CMPINT (assumption.calls, ==, 1);
   restore_state (old_state);
   bson_destroy (assumption.expected_bson);
   bson_destroy (json_doc);
}

void
test_structured_log_oid (void)
{
   struct log_assumption assumption = {
      .expected_envelope.level = MONGOC_STRUCTURED_LOG_LEVEL_WARNING,
      .expected_envelope.component = MONGOC_STRUCTURED_LOG_COMPONENT_COMMAND,
      .expected_envelope.message = "Log entry with deferred OID-to-hex conversion",
      .expected_bson = BCON_NEW ("message",
                                 BCON_UTF8 ("Log entry with deferred OID-to-hex conversion"),
                                 "kOID",
                                 BCON_UTF8 ("112233445566778899aabbcc")),
      .expected_calls = 1,
   };

   bson_oid_t oid;
   bson_oid_init_from_string (&oid, "112233445566778899aabbcc");

   structured_log_state old_state = save_state ();
   mongoc_structured_log_set_handler (structured_log_func, &assumption);

   mongoc_structured_log (MONGOC_STRUCTURED_LOG_LEVEL_WARNING,
                          MONGOC_STRUCTURED_LOG_COMPONENT_COMMAND,
                          "Log entry with deferred OID-to-hex conversion",
                          oid_as_hex ("kOID", &oid),
                          oid_as_hex (NULL, NULL));

   ASSERT_CMPINT (assumption.calls, ==, 1);
   restore_state (old_state);
   bson_destroy (assumption.expected_bson);
}

void
test_structured_log_server_description (void)
{
   struct log_assumption assumption = {
      .expected_envelope.level = MONGOC_STRUCTURED_LOG_LEVEL_WARNING,
      .expected_envelope.component = MONGOC_STRUCTURED_LOG_COMPONENT_COMMAND,
      .expected_envelope.message = "Log entry with server description",
      .expected_bson = BCON_NEW ("message",
                                 BCON_UTF8 ("Log entry with server description"),
                                 "serverHost",
                                 BCON_UTF8 ("db1.example.com"),
                                 "serverHost",
                                 BCON_UTF8 ("db2.example.com"),
                                 "serverPort",
                                 BCON_INT32 (2340),
                                 "serverConnectionId",
                                 BCON_INT64 (0x3deeff00112233f0),
                                 "serverHost",
                                 BCON_UTF8 ("db1.example.com"),
                                 "serverPort",
                                 BCON_INT32 (2340),
                                 "serverHost",
                                 BCON_UTF8 ("db1.example.com"),
                                 "serverPort",
                                 BCON_INT32 (2340),
                                 "serverConnectionId",
                                 BCON_INT64 (0x3deeff00112233f0),
                                 "serviceId",
                                 BCON_UTF8 ("2233445566778899aabbccdd"),
                                 "serverHost",
                                 BCON_UTF8 ("db2.example.com"),
                                 "serverPort",
                                 BCON_INT32 (2341),
                                 "serverConnectionId",
                                 BCON_INT64 (0x3deeff00112233f1)),
      .expected_calls = 1,
   };

   mongoc_server_description_t server_description_1 = {
      .host.host = "db1.example.com",
      .host.port = 2340,
      .server_connection_id = 0x3deeff00112233f0,
   };
   bson_oid_init_from_string (&server_description_1.service_id, "2233445566778899aabbccdd");

   mongoc_server_description_t server_description_2 = {
      .host.host = "db2.example.com",
      .host.port = 2341,
      .server_connection_id = 0x3deeff00112233f1,
      .service_id = {{0}},
   };

   structured_log_state old_state = save_state ();
   mongoc_structured_log_set_handler (structured_log_func, &assumption);

   mongoc_structured_log (
      MONGOC_STRUCTURED_LOG_LEVEL_WARNING,
      MONGOC_STRUCTURED_LOG_COMPONENT_COMMAND,
      "Log entry with server description",
      server_description (&server_description_1, SERVER_HOST),
      server_description (&server_description_2, SERVICE_ID),
      server_description (&server_description_2, SERVER_HOST),
      server_description (&server_description_1, SERVER_PORT),
      server_description (&server_description_1, SERVER_CONNECTION_ID),
      server_description (&server_description_1, SERVER_HOST, SERVER_PORT),
      server_description (&server_description_1, SERVER_HOST, SERVER_PORT, SERVER_CONNECTION_ID, SERVICE_ID),
      server_description (&server_description_2, SERVER_HOST, SERVER_PORT, SERVER_CONNECTION_ID, SERVICE_ID));

   ASSERT_CMPINT (assumption.calls, ==, 1);
   restore_state (old_state);
   bson_destroy (assumption.expected_bson);
}

void
test_structured_log_command (void)
{
   struct log_assumption assumption = {
      .expected_envelope.level = MONGOC_STRUCTURED_LOG_LEVEL_WARNING,
      .expected_envelope.component = MONGOC_STRUCTURED_LOG_COMPONENT_COMMAND,
      .expected_envelope.message = "Log entry with command and reply fields",
      .expected_bson = BCON_NEW ("message",
                                 BCON_UTF8 ("Log entry with command and reply fields"),
                                 "commandName",
                                 BCON_UTF8 ("Not a command"),
                                 "databaseName",
                                 BCON_UTF8 ("Some database"),
                                 "commandName",
                                 BCON_UTF8 ("Not a command"),
                                 "operationId",
                                 BCON_INT64 (0x12345678eeff0011),
                                 "command",
                                 BCON_UTF8 ("{ \"c\" : \"d\" }"),
                                 "reply", // Un-redacted successful reply
                                 BCON_UTF8 ("{ \"r\" : \"s\", \"code\" : 1 }"),
                                 "reply", // Redacted successful reply
                                 BCON_UTF8 ("{}"),
                                 "failure", // Un-redacted server side error
                                 "{",
                                 "r",
                                 BCON_UTF8 ("s"),
                                 "code",
                                 BCON_INT32 (1),
                                 "}",
                                 "failure", // Redacted server side error
                                 "{",
                                 "code",
                                 BCON_INT32 (1),
                                 "}",
                                 "failure", // Client side error
                                 "{",
                                 "code",
                                 BCON_INT32 (123),
                                 "domain",
                                 BCON_INT32 (456),
                                 "message",
                                 BCON_UTF8 ("oh no"),
                                 "}"),
      .expected_calls = 1,
   };

   bson_t *cmd_doc = BCON_NEW ("c", BCON_UTF8 ("d"));
   bson_t *reply_doc = BCON_NEW ("r", BCON_UTF8 ("s"), "code", BCON_INT32 (1));

   const bson_error_t server_error = {
      .domain = MONGOC_ERROR_SERVER,
      .code = 99,
      .message = "unused",
   };
   const bson_error_t client_error = {
      .domain = 456,
      .code = 123,
      .message = "oh no",
   };

   mongoc_cmd_t cmd = {
      .db_name = "Some database",
      .command_name = "Not a command",
      .operation_id = 0x12345678eeff0011,
      .command = cmd_doc,
   };

   structured_log_state old_state = save_state ();
   mongoc_structured_log_set_handler (structured_log_func, &assumption);

   mongoc_structured_log (MONGOC_STRUCTURED_LOG_LEVEL_WARNING,
                          MONGOC_STRUCTURED_LOG_COMPONENT_COMMAND,
                          "Log entry with command and reply fields",
                          cmd (&cmd, COMMAND_NAME),
                          cmd (&cmd, DATABASE_NAME, COMMAND_NAME, OPERATION_ID, COMMAND),
                          cmd_reply ("ping", reply_doc),
                          cmd_reply ("authenticate", reply_doc),
                          cmd_failure ("ping", reply_doc, &server_error),
                          cmd_failure ("authenticate", reply_doc, &server_error),
                          cmd_failure ("authenticate", reply_doc, &client_error));

   ASSERT_CMPINT (assumption.calls, ==, 1);
   restore_state (old_state);
   bson_destroy (assumption.expected_bson);
   bson_destroy (cmd_doc);
   bson_destroy (reply_doc);
}

void
test_structured_log_duration (void)
{
   struct log_assumption assumption = {
      .expected_envelope.level = MONGOC_STRUCTURED_LOG_LEVEL_WARNING,
      .expected_envelope.component = MONGOC_STRUCTURED_LOG_COMPONENT_COMMAND,
      .expected_envelope.message = "Log entry with duration",
      .expected_bson = BCON_NEW ("message",
                                 BCON_UTF8 ("Log entry with duration"),
                                 "durationMS",
                                 BCON_INT32 (1),
                                 "durationMicros",
                                 BCON_INT64 (1999),
                                 "durationMS",
                                 BCON_INT32 (0),
                                 "durationMicros",
                                 BCON_INT64 (10),
                                 "durationMS",
                                 BCON_INT32 (10000000),
                                 "durationMicros",
                                 BCON_INT64 (10000000999)),
      .expected_calls = 1,
   };

   structured_log_state old_state = save_state ();
   mongoc_structured_log_set_handler (structured_log_func, &assumption);

   mongoc_structured_log (MONGOC_STRUCTURED_LOG_LEVEL_WARNING,
                          MONGOC_STRUCTURED_LOG_COMPONENT_COMMAND,
                          "Log entry with duration",
                          monotonic_time_duration (1999),
                          monotonic_time_duration (10),
                          monotonic_time_duration (10000000999));

   ASSERT_CMPINT (assumption.calls, ==, 1);
   restore_state (old_state);
   bson_destroy (assumption.expected_bson);
}

void
test_structured_log_install (TestSuite *suite)
{
   TestSuite_Add (suite, "/structured_log/plain", test_structured_log_plain);
   TestSuite_Add (suite, "/structured_log/plain_with_extra_data", test_structured_log_plain_with_extra_data);
   TestSuite_Add (suite, "/structured_log/basic_data_types", test_structured_log_basic_data_types);
   TestSuite_Add (suite, "/structured_log/json", test_structured_log_json);
   TestSuite_Add (suite, "/structured_log/oid", test_structured_log_oid);
   TestSuite_Add (suite, "/structured_log/server_description", test_structured_log_server_description);
   TestSuite_Add (suite, "/structured_log/command", test_structured_log_command);
   TestSuite_Add (suite, "/structured_log/duration", test_structured_log_duration);
}

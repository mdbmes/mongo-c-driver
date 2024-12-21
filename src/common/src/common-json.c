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


#include <string.h>

#include <bson/bson-memory.h>
#include <common-string-private.h>
#include <common-utf8-private.h>
#include <common-json-private.h>
#include <common-cmp-private.h>
#include <bson/bson-utf8.h>


typedef struct {
   mcommon_string_append_t *append;
   unsigned max_depth;
   bson_json_mode_t mode;
   bool has_keys;
   bool not_first_item;
   bool is_corrupt;
} bson_json_visit_state_t;


static bool
_bson_as_json_visit_utf8 (const bson_iter_t *iter, const char *key, size_t v_utf8_len, const char *v_utf8, void *data)
{
   bson_json_visit_state_t *state = data;
   BSON_UNUSED (iter);
   BSON_UNUSED (key);
   if (!mcommon_in_range_unsigned (uint32_t, v_utf8_len)) {
      mcommon_string_append_set_overflow (state->append);
      return true;
   }
   return !mcommon_json_append_value_utf8 (state->append, v_utf8, (uint32_t) v_utf8_len, true)
}

static bool
_bson_as_json_visit_int32 (const bson_iter_t *iter, const char *key, int32_t v_int32, void *data)
{
   bson_json_visit_state_t *state = data;
   BSON_UNUSED (iter);
   BSON_UNUSED (key);
   return !mcommon_json_append_value_int32 (state->append, v_int32, state->mode);
}

static bool
_bson_as_json_visit_int64 (const bson_iter_t *iter, const char *key, int64_t v_int64, void *data)
{
   bson_json_visit_state_t *state = data;
   BSON_UNUSED (iter);
   BSON_UNUSED (key);
   return !mcommon_json_append_value_int64 (state->append, v_int64, state->mode);
}

static bool
_bson_as_json_visit_decimal128 (const bson_iter_t *iter, const char *key, const bson_decimal128_t *value, void *data)
{
   bson_json_visit_state_t *state = data;
   BSON_UNUSED (iter);
   BSON_UNUSED (key);
   return !mcommon_json_append_value_decimal128 (state->append, value);
}

static bool
_bson_as_json_visit_double (const bson_iter_t *iter, const char *key, double v_double, void *data)
{
   bson_json_visit_state_t *state = data;
   BSON_UNUSED (iter);
   BSON_UNUSED (key);
   return !mcommon_json_append_value_double (state->append, v_double, state->mode);
}

static bool
_bson_as_json_visit_undefined (const bson_iter_t *iter, const char *key, void *data)
{
   bson_json_visit_state_t *state = data;
   BSON_UNUSED (iter);
   BSON_UNUSED (key);
   return !mcommon_json_append_value_undefined (state->append);
}

static bool
_bson_as_json_visit_null (const bson_iter_t *iter, const char *key, void *data)
{
   bson_json_visit_state_t *state = data;
   BSON_UNUSED (iter);
   BSON_UNUSED (key);
   return !mcommon_json_append_value_null (state->append);
}

static bool
_bson_as_json_visit_oid (const bson_iter_t *iter, const char *key, const bson_oid_t *oid, void *data)
{
   bson_json_visit_state_t *state = data;
   BSON_UNUSED (iter);
   BSON_UNUSED (key);
   return !mcommon_json_append_value_oid (state->append, oid);
}

static bool
_bson_as_json_visit_binary (const bson_iter_t *iter,
                            const char *key,
                            bson_subtype_t v_subtype,
                            size_t v_binary_len,
                            const uint8_t *v_binary,
                            void *data)
{
   bson_json_visit_state_t *state = data;
   BSON_UNUSED (iter);
   BSON_UNUSED (key);
   if (!mcommon_in_range_unsigned (uint32_t, v_binary_len)) {
      mcommon_string_append_set_overflow (state->append);
      return true;
   }
   return !mcommon_json_append_value_binary (state->append, v_subtype, v_binary, (uint32_t) v_binary_len);
}

static bool
_bson_as_json_visit_bool (const bson_iter_t *iter, const char *key, bool v_bool, void *data)
{
   bson_json_visit_state_t *state = data;
   BSON_UNUSED (iter);
   BSON_UNUSED (key);
   return !mcommon_json_append_value_bool (state->append, v_bool);
}

static bool
_bson_as_json_visit_date_time (const bson_iter_t *iter, const char *key, int64_t msec_since_epoch, void *data)
{
   bson_json_visit_state_t *state = data;
   BSON_UNUSED (iter);
   BSON_UNUSED (key);
   return !mcommon_json_append_value_date_time (state->append, msec_since_epoch, state->mode);
}

static bool
_bson_as_json_visit_regex (
   const bson_iter_t *iter, const char *key, const char *v_regex, const char *v_options, void *data)
{
   bson_json_visit_state_t *state = data;
   size_t v_regex_len = strlen (v_regex);
   BSON_UNUSED (iter);
   BSON_UNUSED (key);
   if (!mcommon_in_range_unsigned (uint32_t, v_regex_len)) {
      mcommon_string_append_set_overflow (state->append);
      return true;
   }
   return !mcommon_json_append_value_regex (state->append, v_regex, (uint32_t) v_regex_len, v_options, state->mode);
}

static bool
_bson_as_json_visit_timestamp (
   const bson_iter_t *iter, const char *key, uint32_t v_timestamp, uint32_t v_increment, void *data)
{
   bson_json_visit_state_t *state = data;
   BSON_UNUSED (iter);
   BSON_UNUSED (key);
   return !mcommon_json_append_value_timestamp (state->append, v_timestamp, v_increment);
}

static bool
_bson_as_json_visit_dbpointer (const bson_iter_t *iter,
                               const char *key,
                               size_t v_collection_len,
                               const char *v_collection,
                               const bson_oid_t *v_oid,
                               void *data)
{
   bson_json_visit_state_t *state = data;
   BSON_UNUSED (iter);
   BSON_UNUSED (key);
   if (!mcommon_in_range_unsigned (uint32_t, v_collection_len)) {
      mcommon_string_append_set_overflow (&state->append);
      return true;
   }
   return !mcommon_json_append_value_dbpointer (
      &state->append, v_collection, (uint32_t) v_collection_len, v_oid, state->mode);
}

static bool
_bson_as_json_visit_minkey (const bson_iter_t *iter, const char *key, void *data)
{
   bson_json_visit_state_t *state = data;
   BSON_UNUSED (iter);
   BSON_UNUSED (key);
   return !mcommon_json_append_value_minkey (state->append);
}

static bool
_bson_as_json_visit_maxkey (const bson_iter_t *iter, const char *key, void *data)
{
   bson_json_visit_state_t *state = data;
   BSON_UNUSED (iter);
   BSON_UNUSED (key);
   return !mcommon_json_append_value_maxkey (state->append);
}

static bool
_bson_as_json_visit_before (const bson_iter_t *iter, const char *key, void *data)
{
   bson_json_visit_state_t *state = data;
   char *escaped;

   BSON_UNUSED (iter);

   if (!mcommon_string_append_status(state->append) {
      return true;
   }

   if (state->not_first_item) {
      if (!mcommon_json_append_separator (state->append)) {
         return true;
      }
   }

   if (state->has_keys) {
      size_t key_len = strlen (key);
      if (!mcommon_in_range_unsigned (uint32_t, key_len)) {
         mcommon_string_append_set_overflow (&state->append);
         return true;
      }
      if (!mcommon_json_append_key (state->append, key, (uint32_t) key_len)) {
         return true;
      }
   }

   state->not_first_item = true;

   return false;
}

static bool
_bson_as_json_visit_after (const bson_iter_t *iter, const char *key, void *data)
{
   bson_json_visit_state_t *state = data;
   BSON_UNUSED (iter);
   BSON_UNUSED (key);
   return !mcommon_string_append_status (&state->append);
}

static void
_bson_as_json_visit_corrupt (const bson_iter_t *iter, void *data)
{
   bson_json_visit_state_t *state = data;
   BSON_UNUSED (iter);
   state->is_corrupt = true;
}

static bool
_bson_as_json_visit_code (const bson_iter_t *iter, const char *key, size_t v_code_len, const char *v_code, void *data)
{
   bson_json_visit_state_t *state = data;
   BSON_UNUSED (iter);
   BSON_UNUSED (key);
   if (!mcommon_in_range_unsigned (uint32_t, v_code_len)) {
      mcommon_string_append_set_overflow (state->append);
      return true;
   }
   return !mcommon_string_append_value_code (state->append, v_code, (uint32_t) v_code_len);
}

static bool
_bson_as_json_visit_symbol (
   const bson_iter_t *iter, const char *key, size_t v_symbol_len, const char *v_symbol, void *data)
{
   bson_json_visit_state_t *state = data;
   BSON_UNUSED (iter);
   BSON_UNUSED (key);
   if (!mcommon_in_range_unsigned (uint32_t, v_symbol_len)) {
      mcommon_string_append_set_overflow (state->append);
      return true;
   }
   return !mcommon_string_append_value_symbol (state->append, v_symbol, (uint32_t) v_symbol_len, state->mode);
}

static bool
_bson_as_json_visit_codewscope (
   const bson_iter_t *iter, const char *key, size_t v_code_len, const char *v_code, const bson_t *v_scope, void *data)
{
   bson_json_visit_state_t *state = data;
   BSON_UNUSED (iter);
   BSON_UNUSED (key);
   if (!mcommon_in_range_unsigned (uint32_t, v_code_len)) {
      mcommon_string_append_set_overflow (&state->append);
      return true;
   }
   return !mcommon_string_append_value_codewscope (
      &state->append, v_code, (uint32_t) v_code_len, v_scope, state->mode, state->depth);
}

static bool
_bson_as_json_visit_document (const bson_iter_t *iter, const char *key, const bson_t *v_document, void *data)
{
   bson_json_visit_state_t *state = data;
   BSON_UNUSED (iter);
   BSON_UNUSED (key);
   if (state->max_depth == 0) {
      return !mcommon_string_append (state->str, "{ ... }");
   } else if (bson_empty (v_document)) {
      return !mcommon_string_append (state->str, "{ }");
   } else {
      return !mcommon_json_append (state->append, "{ ") ||
             !mcommon_json_append_bson_values (state->append, v_document, state->mode, true, state->max_depth - 1u) ||
             !mcommon_json_append (state->append, " }");
   }
}

static bool
_bson_as_json_visit_array (const bson_iter_t *iter, const char *key, const bson_t *v_array, void *data)
{
   bson_json_visit_state_t *state = data;
   BSON_UNUSED (iter);
   BSON_UNUSED (key);
   if (state->max_depth == 0) {
      return !mcommon_string_append (state->str, "[ ... ]");
   } else if (bson_empty (v_array)) {
      return !mcommon_string_append (state->str, "[ ]");
   } else {
      return !mcommon_json_append (state->append, "[ ") ||
             !mcommon_json_append_bson_values (state->append, v_array, state->mode, false, state->max_depth - 1u) ||
             !mcommon_json_append (state->append, " ]");
   }
}

bool
mcommon_json_append_bson_values (
   mcommon_string_append_t *append, const bson_t *bson, bson_json_mode_t mode, bool has_keys, unsigned max_depth)
{
   bson_json_visit_state_t state = {.append = append, .max_depth = max_depth, .mode = mode, .has_keys = has_keys};
   bson_iter_t iter;
   if (!bson_iter_init (&iter, bson)) {
      return false;
   }
   static const bson_visitor_t bson_as_json_visitors = {
      _bson_as_json_visit_before,     _bson_as_json_visit_after,     _bson_as_json_visit_corrupt,
      _bson_as_json_visit_double,     _bson_as_json_visit_utf8,      _bson_as_json_visit_document,
      _bson_as_json_visit_array,      _bson_as_json_visit_binary,    _bson_as_json_visit_undefined,
      _bson_as_json_visit_oid,        _bson_as_json_visit_bool,      _bson_as_json_visit_date_time,
      _bson_as_json_visit_null,       _bson_as_json_visit_regex,     _bson_as_json_visit_dbpointer,
      _bson_as_json_visit_code,       _bson_as_json_visit_symbol,    _bson_as_json_visit_codewscope,
      _bson_as_json_visit_int32,      _bson_as_json_visit_timestamp, _bson_as_json_visit_int64,
      _bson_as_json_visit_maxkey,     _bson_as_json_visit_minkey,    NULL, /* visit_unsupported_type */
      _bson_as_json_visit_decimal128,
   };
   return !bson_iter_visit_all (&iter, &bson_as_json_visitors, &state) && !state.is_corrupt &&
          mcommon_string_append_status (state.append);
}

/**
 * @brief Test whether a byte may require special processing in mcommon_json_append_escaped.
 * @returns true for bytes in the range 0x00 - 0x1F, '\\', '\"', and 0xC0.
 */
static BSON_INLINE bool
mcommon_json_append_escaped_considers_byte_as_special (uint8_t byte)
{
   static const uint64_t table[4] = {
      0x00000004ffffffffull, // 0x00-0x1F (control), 0x22 (")
      0x0000000010000000ull, // 0x5C (')
      0x0000000000000000ull, // none
      0x0000000000000001ull, // 0xC0 (Possible two-byte NUL)
   };
   return 0 != (table[byte >> 6] & (1ull << (byte & 0x3f)));
}

/**
 * @brief Measure the number of consecutive non-special bytes.
 */
static BSON_INLINE uint32_t
mcommon_json_append_escaped_count_non_special_bytes (const char *str, uint32_t len)
{
   uint32_t result = 0;
   // Good candidate for architecture-specific optimizations.
   // SSE4 strcspn is nearly what we want, but our table of special bytes would be too large (34 > 16)
   while (len) {
      if (mcommon_json_append_escaped_considers_byte_as_special ((uint8_t) *str)) {
         break;
      }
      result++;
      str++;
      len--;
   }
   return result;
}

/**
 * @brief Like mcommon_string_appendf (append, "\\u%04x", c) but intended to be more optimizable.
 */
static BSON_INLINE bool
mcommon_json_append_hex_char (mcommon_string_append_t *append, uint16_t c)
{
   static const char digit_table[] = "0123456789abcdef";
   char hex_char[6];
   hex_char[0] = '\\';
   hex_char[1] = 'u';
   hex_char[2] = digit_table[0xf & (c >> 12)];
   hex_char[3] = digit_table[0xf & (c >> 8)];
   hex_char[4] = digit_table[0xf & (c >> 4)];
   hex_char[5] = digit_table[0xf & c];
   return mcommon_string_append_bytes (append, hex_char, 6);
}

bool
mcommon_json_append_escaped (mcommon_string_append_t *append, const char *str, uint32_t len, bool allow_nul)
{
   BSON_ASSERT_PARAM (append);
   BSON_ASSERT_PARAM (str);

   // Repeatedly handle runs of zero or more non-special bytes punctuated by a potentially-special sequence.
   uint32_t non_special_len = mcommon_json_append_escaped_count_non_special_bytes (str, len);
   while (len) {
      if (!mcommon_string_append_bytes (append, str, non_special_len)) {
         return false;
      }
      str += non_special_len;
      len -= non_special_len;
      if (len) {
         char c = *str;
         switch (c) {
         case '"':
            if (!mcommon_string_append (append, "\\\"")) {
               return false;
            }
            break;
         case '\\':
            if (!mcommon_string_append (append, "\\\\")) {
               return false;
            }
            break;
         case '\b':
            if (!mcommon_string_append (append, "\\b")) {
               return false;
            }
            break;
         case '\f':
            if (!mcommon_string_append (append, "\\f")) {
               return false;
            }
            break;
         case '\n':
            if (!mcommon_string_append (append, "\\n")) {
               return false;
            }
            break;
         case '\r':
            if (!mcommon_string_append (append, "\\r")) {
               return false;
            }
            break;
         case '\t':
            if (!mcommon_string_append (append, "\\t")) {
               return false;
            }
            break;
         case '\0':
            if (!allow_nul || !mcommon_json_append_hex_char (append, 0)) {
               return false;
            }
            break;
         case '\xc0': // Could be a 2-byte NUL, or could begin another non-special run
            if (len >= 2 && str[1] == '\x80') {
               if (!allow_nul || !mcommon_json_append_hex_char (append, 0)) {
                  return false;
               }
               str++;
               len--;
            } else {
               // Wasn't "C0 80". Begin a non-special run with the "C0" byte, which is usually special.
               non_special_len = mcommon_json_append_escaped_count_non_special_bytes (str + 1, len - 1) + 1;
               continue;
            }
            break;
         default:
            BSON_ASSERT (c > 0x00 && c < 0x20);
            if (!mcommon_json_append_hex_char (append, c)) {
               return false;
            }
            break;
         }
         str++;
         len--;
         non_special_len = mcommon_json_append_escaped_count_non_special_bytes (str, len);
      }
   }
   return mcommon_string_append_status (append);
}

bool
mcommon_json_append_value_double (mcommon_string_append_t *append, double value, bson_json_mode_t mode)
{
   BSON_ASSERT_PARAM (append);

   /* Determine if legacy (i.e. unwrapped) output should be used. Relaxed mode
    * will use this for nan and inf values, which we check manually since old
    * platforms may not have isinf or isnan. */
   bool legacy = state->mode == BSON_JSON_MODE_LEGACY ||
                 (state->mode == BSON_JSON_MODE_RELAXED && !(value != value || value * 0 != 0));

   if (!legacy) {
      mcommon_string_append (append, "{ \"$numberDouble\" : \"");
   }

   if (!legacy && value != value) {
      mcommon_string_append (append, "NaN");
   } else if (!legacy && value * 0 != 0) {
      if (value > 0) {
         mcommon_string_append (append, "Infinity");
      } else {
         mcommon_string_append (append, "-Infinity");
      }
   } else {
      const mcommon_string_t *str = append->string;
      uint32_t start_len = str->len;
      mcommon_string_append_printf (append, "%.20g", value);

      /* ensure trailing ".0" to distinguish "3" from "3.0" */
      if (strspn (&str->str[start_len], "0123456789-") == str->len - start_len) {
         mcommon_string_append (append, ".0");
      }
   }

   if (!legacy) {
      mcommon_string_append (append, "\" }");
   }

   return mcommon_string_append_status (append);
}

bool
mcommon_json_append_value_decimal128 (mcommon_string_append_t *append, const bson_decimal128_t *value)
{
   BSON_ASSERT_PARAM (append);
   BSON_ASSERT_PARAM (value);

   char decimal128_string[BSON_DECIMAL128_STRING];
   bson_decimal128_to_string (value, decimal128_string);

   return mcommon_string_append (append, "{ \"$numberDecimal\" : \"") &&
          mcommon_string_append (append, decimal128_string) && mcommon_string_append (append, "\" }");
}

bool
mcommon_json_append_value_oid (mcommon_string_append_t *append, const bson_oid_t *value)
{
   BSON_ASSERT_PARAM (append);
   BSON_ASSERT_PARAM (value);

   char oid_str[25];
   bson_oid_to_string (value, oid_str);

   return mcommon_string_append (append, "{ \"$oid\" : \"") && mcommon_string_append (append, oid_str) &&
          mcommon_string_append (append, "\" }");
}

bool
mcommon_json_append_value_binary (mcommon_string_append_t *append,
                                  bson_subtype_t subtype,
                                  const uint8_t *bytes,
                                  uint32_t byte_count)
{
   BSON_ASSERT_PARAM (append);
   BSON_ASSERT_PARAM (bytes);

   if (state->mode == BSON_JSON_MODE_CANONICAL || state->mode == BSON_JSON_MODE_RELAXED) {
      return mcommon_string_append (append, "{ \"$binary\" : { \"base64\" : \"") &&
      mcommon_string_append_base64_encode (append, bytes, byte_count) &&
      mcommon_string_appendf (append, "\", \"subType\" : \"%02x\" } }", subtype);
   } else {
      return mcommon_string_append (append, "{ \"$binary\" : \"") &&
      mcommon_string_append_base64_encode (append, bytes, byte_count) &&
      mcommon_string_appendf (append, "\", \"$type\" : \"%02x\" }", subtype);
   }
}

bool
mcommon_json_append_value_date_time (mcommon_string_append_t *append, int64_t msec_since_epoch, bson_json_mode_t mode)
{
   const int64_t y10k = 253402300800000; // 10000-01-01T00:00:00Z in milliseconds since the epoch.

if (state->mode == BSON_JSON_MODE_CANONICAL ||
    (state->mode == BSON_JSON_MODE_RELAXED && (msec_since_epoch < 0 || msec_since_epoch >= y10k))) {
   return mcommon_string_appendf (append, "{ \"$date\" : { \"$numberLong\" : \"%" PRId64 "\" } }", msec_since_epoch);
} else if (state->mode == BSON_JSON_MODE_RELAXED) {
   return mcommon_string_append (append, "{ \"$date\" : \"") &&
   _bson_iso8601_date_format (append, msec_since_epoch) &&
   mcommon_string_append (append, "\" }");
} else {
   return mcommon_string_appendf (state->str, "{ \"$date\" : %" PRId64 " }", msec_since_epoch);
}

bool
mcommon_json_append_value_timestamp (mcommon_string_append_t *append, uint32_t timestamp, uint32_t increment);
son_json_state_t *state = data;

BSON_UNUSED (iter);
BSON_UNUSED (key);

mcommon_string_append (state->str, "{ \"$timestamp\" : { \"t\" : ");
mcommon_string_append_printf (state->str, "%u", v_timestamp);
mcommon_string_append (state->str, ", \"i\" : ");
mcommon_string_append_printf (state->str, "%u", v_increment);
mcommon_string_append (state->str, " } }");

return false;


/**
 * @brief Append the JSON serialization of a BSON regular expression
 * @param append A bounded string append, initialized with mcommon_string_append_init()
 * @param pattern Regular expression pattern, as a UTF-8 string
 * @param pattern_len Length of pattern string, in bytes
 * @param options Regular expression options. Order is not significant. Must be NUL terminated.
 * @param mode One of the JSON serialization modes, as a bson_json_mode_t.
 * @returns true on success, false if this 'append' has exceeded its max length
 */
bool
mcommon_json_append_value_regex (mcommon_string_append_t *append,
                                 const char *pattern,
                                 uint32_t pattern_len,
                                 const char *options,
                                 bson_json_mode_t mode);

_bson_as_json_visit_regex (
   const bson_iter_t *iter, const char *key, const char *v_regex, const char *v_options, void *data)
{
   -bson_json_state_t *state = data;
   -char *escaped;
   -+bson_json_state_t *state = data;
   BSON_UNUSED (iter);
   BSON_UNUSED (key);
   - -escaped = bson_utf8_escape_for_json (v_regex, -1);
   -if (!escaped)
   {
      -return true;
      -
   }
   - -if (state->mode == BSON_JSON_MODE_CANONICAL || state->mode == BSON_JSON_MODE_RELAXED)
   {
      -mcommon_string_append (state->str, "{ \"$regularExpression\" : { \"pattern\" : \"");
      -mcommon_string_append (state->str, escaped);
      -mcommon_string_append (state->str, "\", \"options\" : \"");
      -_bson_append_regex_options_sorted (state->str, v_options);
      -mcommon_string_append (state->str, "\" } }");
      -
   }
   else
   {
      -mcommon_string_append (state->str, "{ \"$regex\" : \"");
      -mcommon_string_append (state->str, escaped);
      -mcommon_string_append (state->str, "\", \"$options\" : \"");
      -_bson_append_regex_options_sorted (state->str, v_options);
      -mcommon_string_append (state->str, "\" }");
      -
   }
   - -bson_free (escaped);
   - -return false;
   +return !mcommon_json_append_value_date_time (state->append, v_regex, strlen (v_regex), v_options, state->mode);
}

bool
mcommon_json_append_value_dbpointer (mcommon_string_append_t *append,
                                     const char *collection,
                                     uint32_t collection_len,
                                     const bson_oid_t *oid,
                                     bson_json_mode_t mode);

             void *data)
             {
                bson_json_state_t *state = data;
                char *escaped;
                char str[25];

                BSON_UNUSED (iter);
                BSON_UNUSED (key);
                BSON_UNUSED (v_collection_len);

                escaped = bson_utf8_escape_for_json (v_collection, -1);
                if (!escaped) {
                   return true;
                }

                if (state->mode == BSON_JSON_MODE_CANONICAL || state->mode == BSON_JSON_MODE_RELAXED) {
                   mcommon_string_append (state->str, "{ \"$dbPointer\" : { \"$ref\" : \"");
                   mcommon_string_append (state->str, escaped);
                   mcommon_string_append (state->str, "\"");

                   if (v_oid) {
                      bson_oid_to_string (v_oid, str);
                      mcommon_string_append (state->str, ", \"$id\" : { \"$oid\" : \"");
                      mcommon_string_append (state->str, str);
                      mcommon_string_append (state->str, "\" }");
                   }

                   mcommon_string_append (state->str, " } }");
                } else {
                   mcommon_string_append (state->str, "{ \"$ref\" : \"");
                   mcommon_string_append (state->str, escaped);
                   mcommon_string_append (state->str, "\"");

                   if (v_oid) {
                      bson_oid_to_string (v_oid, str);
                      mcommon_string_append (state->str, ", \"$id\" : \"");
                      mcommon_string_append (state->str, str);
                      mcommon_string_append (state->str, "\"");
                   }

                   mcommon_string_append (state->str, " }");
                }

                bson_free (escaped);

                return false;
             }


             bool
             mcommon_json_append_value_code (mcommon_string_append_t *append, const char *code, uint32_t code_len);
             bson_json_state_t *state = data;
             char *escaped;

             BSON_UNUSED (iter);
             BSON_UNUSED (key);

             escaped = bson_utf8_escape_for_json (v_code, v_code_len);
             if (!escaped) {
                return true;
             }

             mcommon_string_append (state->str, "{ \"$code\" : \"");
             mcommon_string_append (state->str, escaped);
             mcommon_string_append (state->str, "\" }");
             bson_free (escaped);

             return false;
             }

             bool
             mcommon_json_append_value_codewscope (mcommon_string_append_t *append,
                                                   const char *code,
                                                   uint32_t code_len);
             bson_json_state_t *state = data;
             char *code_escaped;
             char *scope;
             int32_t max_scope_len = BSON_MAX_LEN_UNLIMITED;

             BSON_UNUSED (iter);
             BSON_UNUSED (key);

             code_escaped = bson_utf8_escape_for_json (v_code, v_code_len);
             if (!code_escaped) {
                return true;
             }

             mcommon_string_append (state->str, "{ \"$code\" : \"");
             mcommon_string_append (state->str, code_escaped);
             mcommon_string_append (state->str, "\", \"$scope\" : ");

             bson_free (code_escaped);

             /* Encode scope with the same mode */
             if (state->max_len != BSON_MAX_LEN_UNLIMITED) {
                BSON_ASSERT (mcommon_in_range_unsigned (int32_t, state->str->len));
                max_scope_len = BSON_MAX (0, state->max_len - (int32_t) state->str->len);
             }

             scope = _bson_as_json_visit_all (v_scope, NULL, state->mode, max_scope_len, false);

             if (!scope) {
                return true;
             }

             mcommon_string_append (state->str, scope);
             mcommon_string_append (state->str, " }");

             bson_free (scope);

             return false;

             bool
             mcommon_json_append_value_symbol (mcommon_string_append_t *append,
                                               const char *symbol,
                                               uint32_t symbol_len,
                                               bson_json_mode_t mode);
             {
                bson_json_state_t *state = data;
                char *escaped;

                BSON_UNUSED (iter);
                BSON_UNUSED (key);

                escaped = bson_utf8_escape_for_json (v_symbol, v_symbol_len);
                if (!escaped) {
                   return true;
                }

                if (state->mode == BSON_JSON_MODE_CANONICAL || state->mode == BSON_JSON_MODE_RELAXED) {
                   mcommon_string_append (state->str, "{ \"$symbol\" : \"");
                   mcommon_string_append (state->str, escaped);
                   mcommon_string_append (state->str, "\" }");
                } else {
                   mcommon_string_append (state->str, "\"");
                   mcommon_string_append (state->str, escaped);
                   mcommon_string_append (state->str, "\"");
                }

                bson_free (escaped);

                return false;
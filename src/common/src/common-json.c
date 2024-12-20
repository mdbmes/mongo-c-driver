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
#include <bson/bson-utf8.h>

/*
 * Test whether a byte may require special processing in mcommon_json_append_escaped.
 * Returns true for bytes in the range 0x00 - 0x1F, '\\', '\"', and 0xC0.
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

/*
 * Measure the number of consecutive non-special bytes.
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

static BSON_INLINE bool
mcommon_json_append_hex_char (mcommon_string_append_t *append, uint16_t c)
{
   // Like mcommon_string_appendf (append, "\\u%04x", c) but intended to be more optimizable.
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


/*
 *--------------------------------------------------------------------------
 *
 * mcommon_json_append_escaped --
 *
 *       Escapes special characters in @str while appending to @append.
 *
 *       Both " and \ characters will be escaped, and any ASCII control characters.
 *
 *       If 'allow_nul' is true, NUL characters and are allowed and encoded as
 *       "\u0000". If 'allow_nul' is false, NUL characters will truncate the
 *       string and cause an unsuccessful return.
 *
 *       The two-byte sequence "C0 80" is also interpreted as an internal NUL,
 *       for historical reasons. This sequence is considered invalid according to RFC3629.
 *
 * Parameters:
 *       @str: A UTF-8 encoded string.
 *       @len: The length of @utf8 in bytes.
 *       @allow_nul: true to encode NUL or "C0 80" sequences as "\u0000", false to treat them as invalid
 *
 * Returns:
 *       true on success. false if the append length limit is exceeded, or we encounter invalid UTF-8 in 'str'.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

bool
mcommon_json_append_escaped (mcommon_string_append_t *append, const char *str, uint32_t len, bool allow_nul)
{
   BSON_ASSERT_PARAM (append);
   BSON_ASSERT_PARAM (str);

   // Repeatedly handle runs of non-special bytes punctuated by special bytes.
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

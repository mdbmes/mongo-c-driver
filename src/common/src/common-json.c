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
 *--------------------------------------------------------------------------
 *
 * _is_special_char --
 *
 *       Uses a bit mask to check if a character requires special formatting
 *       or not. Called from bson_utf8_escape_for_json.
 *
 * Parameters:
 *       @c: An unsigned char c.
 *
 * Returns:
 *       true if @c requires special formatting. otherwise false.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

static BSON_INLINE bool
_is_special_char (unsigned char c)
{
   /*
   C++ equivalent:
   std::bitset<256> charmap = [...]
   return charmap[c];
   */
   static const bson_unichar_t charmap[8] = {0xffffffff, // control characters
                                             0x00000004, // double quote "
                                             0x10000000, // backslash
                                             0x00000000,
                                             0xffffffff,
                                             0xffffffff,
                                             0xffffffff,
                                             0xffffffff}; // non-ASCII
   const int int_index = ((int) c) / ((int) sizeof (bson_unichar_t) * 8);
   const int bit_index = ((int) c) & ((int) sizeof (bson_unichar_t) * 8 - 1);
   return ((charmap[int_index] >> bit_index) & ((bson_unichar_t) 1)) != 0u;
}

/*
 *--------------------------------------------------------------------------
 *
 * _bson_utf8_handle_special_char --
 *
 *       Appends a special character in the correct format when converting
 *       from UTF-8 to JSON. This includes characters that should be escaped
 *       as well as ASCII control characters.
 *
 *       Normal ASCII characters and multi-byte UTF-8 sequences are handled
 *       in bson_utf8_escape_for_json, where this function is called from.
 *
 * Parameters:
 *       @c: A uint8_t ASCII codepoint.
 *       @append: A bounded string append operation
 *
 * Returns:
 *       true if the append is still successful,
 *       false if any truncation has occurred so far.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

static BSON_INLINE bool
_bson_utf8_handle_special_char (const uint8_t c, mcommon_string_append_t *append)
{
   BSON_ASSERT (c < 0x80u);

   switch (c) {
   case '"':
      return mcommon_string_append (append, "\\\"");
   case '\\':
      return mcommon_string_append (append, "\\\\");
   case '\b':
      return mcommon_string_append (append, "\\b");
   case '\f':
      return mcommon_string_append (append, "\\f");
   case '\n':
      return mcommon_string_append (append, "\\n");
   case '\r':
      return mcommon_string_append (append, "\\r");
   case '\t':
      return mcommon_string_append (append, "\\t");
   default: {
      // ASCII control character
      BSON_ASSERT (c < 0x20u);

      const char digits[] = "0123456789abcdef";
      char codepoint[6] = "\\u0000";

      codepoint[4] = digits[(c >> 4) & 0x0fu];
      codepoint[5] = digits[c & 0x0fu];
      return mcommon_string_append_bytes (append, codepoint, sizeof codepoint);
   }
   }
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
 *  cause and any non-ascii cAdditionally, if a NUL
 *       byte is found before @utf8_len bytes, it will be converted to the
 *       two byte UTF-8 sequence.
 *
 * Parameters:
 *       @utf8: A UTF-8 encoded string.
 *       @utf8_len: The length of @utf8 in bytes or -1 if NUL terminated.
 *
 * Returns:
 *       A newly allocated string that should be freed with bson_free().
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

bool
mcommon_json_append_escaped (mcommon_string_append_t *append, const char *str, uint32_t len, bool allow_nul);

char *
bson_utf8_escape_for_json (const char *utf8, /* IN */
                           ssize_t utf8_len) /* IN */
{
   bool length_provided = true;
   size_t utf8_ulen;

   BSON_ASSERT (utf8);

   if (utf8_len < 0) {
      length_provided = false;
      utf8_ulen = strlen (utf8);
   } else {
      utf8_ulen = (size_t) utf8_len;
   }

   if (utf8_ulen == 0) {
      return bson_strdup ("");
   }

   const char *const end = utf8 + utf8_ulen;

   mcommon_string_t *const str = _bson_string_alloc (utf8_ulen);

   size_t normal_chars_seen = 0u;

   do {
      const uint8_t current_byte = (uint8_t) utf8[normal_chars_seen];
      if (!_is_special_char (current_byte)) {
         // Normal character, no need to do anything besides iterate
         // Copy rest of the string if we reach the end
         normal_chars_seen++;
         utf8_ulen--;
         if (utf8_ulen == 0) {
            _bson_string_append_ex (str, utf8, normal_chars_seen);
            break;
         }

         continue;
      }

      // Reached a special character. Copy over all of normal characters
      // we have passed so far
      if (normal_chars_seen > 0) {
         _bson_string_append_ex (str, utf8, normal_chars_seen);
         utf8 += normal_chars_seen;
         normal_chars_seen = 0;
      }

      // Check if expected char length goes past end
      // bson_utf8_get_char will crash without this check
      {
         uint8_t mask;
         uint8_t length_of_char;

         mcommon_utf8_get_sequence (utf8, &length_of_char, &mask);
         if (utf8 > end - length_of_char) {
            goto invalid_utf8;
         }
      }

      // Check for null character
      // Null characters are only allowed if the length is provided
      if (utf8[0] == '\0' || (utf8[0] == '\xc0' && utf8[1] == '\x80')) {
         if (!length_provided) {
            goto invalid_utf8;
         }

         mcommon_string_append (str, "\\u0000");
         utf8_ulen -= *utf8 ? 2u : 1u;
         utf8 += *utf8 ? 2 : 1;
         continue;
      }

      // Multi-byte UTF-8 sequence
      if (current_byte > 0x7fu) {
         const char *utf8_old = utf8;
         size_t char_len;

         bson_unichar_t unichar = bson_utf8_get_char (utf8);

         if (!unichar) {
            goto invalid_utf8;
         }

         mcommon_string_append_unichar (str, unichar);
         utf8 = bson_utf8_next_char (utf8);

         char_len = (size_t) (utf8 - utf8_old);
         BSON_ASSERT (utf8_ulen >= char_len);
         utf8_ulen -= char_len;

         continue;
      }

      // Special ASCII characters (control chars and misc.)
      _bson_utf8_handle_special_char (current_byte, str);

      if (current_byte > 0) {
         utf8++;
      } else {
         goto invalid_utf8;
      }

      utf8_ulen--;
   } while (utf8_ulen > 0);

   return mcommon_string_free (str, false);

invalid_utf8:
   mcommon_string_free (str, true);
   return NULL;
}


/*
 *--------------------------------------------------------------------------
 *
 * bson_utf8_get_char --
 *
 *       Fetches the next UTF-8 character from the UTF-8 sequence.
 *
 * Parameters:
 *       @utf8: A string containing validated UTF-8.
 *
 * Returns:
 *       A 32-bit bson_unichar_t reprsenting the multi-byte sequence.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

bson_unichar_t
bson_utf8_get_char (const char *utf8) /* IN */
{
   bson_unichar_t c;
   uint8_t mask;
   uint8_t num;
   int i;

   BSON_ASSERT (utf8);

   mcommon_utf8_get_sequence (utf8, &num, &mask);
   c = (*utf8) & mask;

   for (i = 1; i < num; i++) {
      c = (c << 6) | (utf8[i] & 0x3F);
   }

   return c;
}


/*
 *--------------------------------------------------------------------------
 *
 * bson_utf8_next_char --
 *
 *       Returns an incremented pointer to the beginning of the next
 *       multi-byte sequence in @utf8.
 *
 * Parameters:
 *       @utf8: A string containing validated UTF-8.
 *
 * Returns:
 *       An incremented pointer in @utf8.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

const char *
bson_utf8_next_char (const char *utf8) /* IN */
{
   uint8_t mask;
   uint8_t num;

   BSON_ASSERT (utf8);

   mcommon_utf8_get_sequence (utf8, &num, &mask);

   return utf8 + num;
}


/*
 *--------------------------------------------------------------------------
 *
 * bson_utf8_from_unichar --
 *
 *       Converts the unichar to a sequence of utf8 bytes and stores those
 *       in @utf8. The number of bytes in the sequence are stored in @len.
 *
 * Parameters:
 *       @unichar: A bson_unichar_t.
 *       @utf8: A location for the multi-byte sequence.
 *       @len: A location for number of bytes stored in @utf8.
 *
 * Returns:
 *       None.
 *
 * Side effects:
 *       @utf8 is set.
 *       @len is set.
 *
 *--------------------------------------------------------------------------
 */

void
bson_utf8_from_unichar (bson_unichar_t unichar,                      /* IN */
                        char utf8[BSON_ENSURE_ARRAY_PARAM_SIZE (6)], /* OUT */
                        uint32_t *len)                               /* OUT */
{
   // Inlined implementation from common-utf8-private
   mcommon_utf8_from_unichar (unichar, utf8, len);
}

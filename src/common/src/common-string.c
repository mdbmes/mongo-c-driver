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

#include "common-string-private.h"
#include "common-bits-private.h"
#include "common-utf8-private.h"


mcommon_string_t *
mcommon_string_new (const char *str)
{
   size_t length = strlen (str);
   BSON_ASSERT (length < (size_t) UINT32_MAX);
   return mcommon_string_new_with_capacity (str, (uint32_t) length, 0);
}

mcommon_string_t *
mcommon_string_new_with_capacity (const char *str, uint32_t length, uint32_t min_capacity)
{
   BSON_ASSERT (length < UINT32_MAX && min_capacity < UINT32_MAX);
   uint32_t capacity = BSON_MAX (length, min_capacity);
   uint32_t alloc = capacity + 1u;
   char *buffer = bson_malloc (alloc);
   memcpy (buffer, str, length);
   buffer[length] = '\0';
   return mcommon_string_new_with_buffer (buffer, length, alloc);
}

mcommon_string_t *
mcommon_string_new_with_buffer (char *buffer, uint32_t length, uint32_t alloc)
{
   BSON_ASSERT_PARAM (buffer);
   BSON_ASSERT (length < UINT32_MAX && alloc >= length + 1u);
   BSON_ASSERT (buffer[length] == '\0');
   mcommon_string_t *string = bson_malloc0 (sizeof *string);
   string->str = buffer;
   string->len = length;
   string->alloc = alloc;
   return string;
}

void
mcommon_string_destroy (mcommon_string_t *string)
{
   if (string) {
      bson_free (mcommon_string_destroy_into_buffer (string));
   }
}

char *
mcommon_string_destroy_into_buffer (mcommon_string_t *string)
{
   if (string) {
      char *buffer = string->str;
      BSON_ASSERT (buffer[string->len] == '\0');
      bson_free (string);
      return buffer;
   } else {
      return NULL;
   }
}

void
mcommon_string_grow_to_capacity (mcommon_string_t *string, uint32_t capacity)
{
   BSON_ASSERT_PARAM (string);
   BSON_ASSERT (capacity < UINT32_MAX);
   uint32_t min_alloc_needed = capacity + 1u;
   if (string->alloc < min_alloc_needed) {
      uint32_t alloc = mcommon_next_power_of_two_u32 (min_alloc_needed);
      string->str = bson_realloc (string->str, alloc);
      string->alloc = alloc;
   }
}

// Handle cases omitted from the inlined mcommon_string_append_bytes()
bool
mcommon_string_append_bytes_internal (mcommon_string_append_t *append, const char *str, uint32_t len)
{
   mcommon_string_t *string = append->_string;
   uint32_t old_len = string->len;
   uint32_t max_len = append->_max_len;
   BSON_ASSERT (max_len < UINT32_MAX);

   uint32_t max_append_len = old_len < max_len ? max_len - old_len : 0;
   uint32_t truncated_append_len = len;
   if (len > max_append_len) {
      // Search for an actual append length, <= the maximum allowed, which preserves UTF-8 validity
      append->_max_len_exceeded = true;
      truncated_append_len = mcommon_utf8_truncate_len (str, max_append_len);
   }

   uint32_t new_len = old_len + truncated_append_len;
   BSON_ASSERT (new_len <= max_len);
   mcommon_string_grow_to_capacity (string, new_len);
   char *buffer = string->str;

   memcpy (buffer + old_len, str, truncated_append_len);
   buffer[new_len] = '\0';
   string->len = new_len;

   return mcommon_string_append_status (append);
}

bool
mcommon_string_append_unichar_internal (mcommon_string_append_t *append, bson_unichar_t unichar)
{
   mcommon_string_t *string = append->_string;
   uint32_t old_len = string->len;
   uint32_t max_len = append->_max_len;
   BSON_ASSERT (max_len < UINT32_MAX);

   const uint32_t max_utf8_sequence = 6;
   uint32_t max_append_len = old_len < max_len ? max_len - old_len : 0;

   // Usually we can write the UTF-8 sequence directly
   if (BSON_LIKELY (max_append_len >= max_utf8_sequence)) {
      uint32_t actual_sequence_len;
      mcommon_string_grow_to_capacity (string, old_len + max_utf8_sequence);
      mcommon_utf8_from_unichar (unichar, string->str + old_len, &actual_sequence_len);
      BSON_ASSERT (actual_sequence_len <= max_utf8_sequence);
      BSON_ASSERT (append->_max_len_exceeded == false);
      string->len = old_len + actual_sequence_len;
      return true;
   }

   // If we are near max_len, avoid growing the buffer beyond it.
   char temp_buffer[max_utf8_sequence];
   uint32_t actual_sequence_len;
   mcommon_utf8_from_unichar (unichar, temp_buffer, &actual_sequence_len);
   return mcommon_string_append_bytes_internal (append, temp_buffer, actual_sequence_len);
}

bool
mcommon_string_appendf (mcommon_string_append_t *append, const char *format, ...)
{
   va_list args;
   BSON_ASSERT_PARAM (format);
   va_start (args, format);
   bool ret = mcommon_string_vappendf (append, format, args);
   va_end (args);
   return ret;
}

bool
mcommon_string_vappendf (mcommon_string_append_t *append, const char *format, va_list args)
{
   // Fast path: already hit limit
   if (BSON_UNLIKELY (!mcommon_string_append_status (append))) {
      return false;
   }

   mcommon_string_t *string = append->_string;
   uint32_t old_len = string->len;
   uint32_t max_len = append->_max_len;
   BSON_ASSERT (max_len < UINT32_MAX);
   uint32_t max_append_len = old_len < max_len ? max_len - old_len : 0;

   // Initial minimum buffer length; increases on retry.
   uint32_t min_format_buffer_capacity = 16;

   while (true) {
      // Allocate room for a format buffer at the end of the string.
      // It will be at least this round's min_format_buffer_capacity, but if we happen to have extra space allocated we
      // do want that to be available to vsnprintf().

      min_format_buffer_capacity = BSON_MIN (min_format_buffer_capacity, max_append_len);
      mcommon_string_grow_to_capacity (string, old_len + min_format_buffer_len);
      uint32_t actual_format_buffer_len = BSON_MIN (string->alloc - old_len, max_append_len);
      char *format_buffer = string->str + old_len;
      BSON_ASSERT (actual_format_buffer_len >= min_format_buffer_len);
      BSON_ASSERT (actual_format_buffer_len < UINT32_MAX);
      uint32_t format_buffer_alloc = actual_format_buffer_len + 1u;

      va_list args_copy;
      va_copy (args_copy, args);
      int format_result = bson_vsnprintf (format_buffer, format_buffer_alloc, format, args_copy);
      va_end (args_copy);

      if (format_result > -1 && format_result <= actual_format_buffer_len) {
         // Successful result, no truncation.
         format_buffer[format_result] = '\0';
         string->len = old_len + format_result;
         BSON_ASSERT (string->len <= append->_max_len);
         BSON_ASSERT (append->_max_len_exceeded == false);
         return true;
      }

      if (actual_format_buffer_len == max_append_len) {
         // No more space to grow into, this must be the final result.

         if (format_result > -1) {
            // We have truncated output from vsnprintf. Clean it up by removing
            // any partial UTF-8 sequences that might be left on the end.
            uint32_t truncated_append_len =
               mcommon_utf8_truncate_len (format_buffer, BSON_MIN (actual_format_buffer_len, (uint32_t) format_result));
            BSON_ASSERT (truncated_append_len <= actual_format_buffer_len);
            format_buffer[truncated_append_len] = '\0';
            string->len = old_len + truncated_append_len;
            append->_max_len_exceeded = true;
            return false;
         }

         // Error from vsnprintf; This operation fails, but we do not set max_len_exceeded.
         return false;
      }

      // Choose a larger format_buffer_len and try again. Length will be clamped to max_append_len above.
      if (n > -1 && n < UINT32_MAX) {
         format_buffer_len = (uint32_t) n + 1u;
      } else if (format_buffer_len < UINT32_MAX / 2) {
         format_buffer_len *= 2;
      } else {
         format_buffer_len = UINT32_MAX;
      }
   }
}
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

#include "common-prelude.h"

#ifndef MONGO_C_DRIVER_COMMON_STRING_PRIVATE_H
#define MONGO_C_DRIVER_COMMON_STRING_PRIVATE_H

#include <bson/bson.h>


/* Until the deprecated bson_string_t is removed, this must have the same members in the same order, so we can safely
 * cast between the two types. Afterward, we are free to modify the memory layout as needed.
 *
 * In mcommon_string_t, 'str' is guaranteed to be NUL terminated and valid UTF-8. Unused portions of the buffer may be
 * uninitialized, and must not be compared or copied.
 *
 * Until bson_string_t is deprecated, it is implemented in terms of mcommon_string_t where possible. In this usage,
 * UTF-8 validity is not guaranteed.
 *
 * 'len' is measured in bytes, not including the NUL terminator.
 *
 * 'alloc' is the actual length of the bson_malloc() allocation in bytes, including the required space for NUL
 * termination.
 *
 * When we use 'capacity', it refers to the largest 'len' that the buffer could store. alloc == capacity + 1.
 */
typedef struct mcommon_string_t {
   char *str;
   uint32_t len;
   uint32_t alloc;
} mcommon_string_t;

/* Parameters and outcome for a bounded append operation on a mcommon_string_t. Individual type-specific append
 * functions can consume this struct to communicate bounds info. "max_len_exceeded" can be tested any time an
 * algorithmic exit is convenient; the actual appended content will be limited by max_len. Truncation is guaranteed not
 * to split a UTF-8 byte sequence.
 *
 * Members are here to support inline definitions; not intended for direct access.
 *
 * Multiple mcommon_string_append_t may simultaneously refer to the same 'string' but this usage is not recommended.
 *
 * 'max_len_exceeded' only includes operations undertaken on this specific mcommon_string_append_t. It will not be set
 * if the string was already overlong, or if a different mcommon_string_append_t experiences an overage.
 */
typedef struct mcommon_string_append_t {
   mcommon_string_t *_string;
   uint32_t _max_len;
   bool _max_len_exceeded;
} mcommon_string_append_t;

#define mcommon_string_new COMMON_NAME (string_new)
#define mcommon_string_new_with_capacity COMMON_NAME (string_new_with_capacity)
#define mcommon_string_new_with_buffer COMMON_NAME (string_new_with_buffer)
#define mcommon_string_destroy COMMON_NAME (string_destroy)
#define mcommon_string_destroy_into_buffer COMMON_NAME (string_destroy_into_buffer)
#define mcommon_string_grow_to_capacity COMMON_NAME (string_grow_to_capacity)
#define mcommon_string_append_bytes_internal COMMON_NAME (string_append_bytes_internal)
#define mcommon_string_append_unichar_internal COMMON_NAME (string_append_unichar_internal)
#define mcommon_string_append_base64_encode COMMON_NAME (string_append_base64_encode)
#define mcommon_string_appendf COMMON_NAME (string_appendf)
#define mcommon_string_vappendf COMMON_NAME (string_vappendf)

bool
mcommon_string_append_bytes_internal (mcommon_string_append_t *append, const char *str, uint32_t len);

bool
mcommon_string_append_unichar_internal (mcommon_string_append_t *append, bson_unichar_t unichar);

/**
 * @brief Allocate a new mcommon_string_t with a copy of the supplied initializer string.
 *
 * @param str NUL terminated string, must be valid UTF-8. Must be less than UINT32_MAX bytes long.
 * @returns A new mcommon_string_t that must be freed with mcommon_string_free() or mcommon_string_into_cstr() and
 * bson_free().
 */
mcommon_string_t *
mcommon_string_new (const char *str);

/**
 * @brief Allocate a new mcommon_string_t with a copy of the supplied initializer string and an explicit buffer
 * capacity.
 *
 * @param str Initializer string, must be valid UTF-8.
 * @param length Length of initializer string, in bytes.
 * @param min_capacity Minimum string capacity, in bytes, the buffer must be able to store without reallocating. Does
 * not include the NUL terminator. Must be less than UINT32_MAX.
 * @returns A new mcommon_string_t that must be freed with mcommon_string_free() or mcommon_string_into_cstr() and
 * bson_free(). It will hold 'str' in its entirety, even if the requested min_capacity was smaller.
 */
mcommon_string_t *
mcommon_string_new_with_capacity (const char *str, uint32_t length, uint32_t min_capacity);

/**
 * @brief Allocate a new mcommon_string_t, taking ownership of an existing buffer
 *
 * @param buffer Buffer to adopt, containing a valid UTF-8 string, suitable for bson_free() and bson_realloc()
 * @param length Length of the valid UTF-8 string data, in bytes, not including the required NUL terminator.
 * @param alloc Actual allocated size of the buffer, in bytes, including room for NUL termination.
 * @returns A new mcommon_string_t that must be freed with mcommon_string_free() or mcommon_string_into_buffer() and
 * bson_free().
 */
mcommon_string_t *
mcommon_string_new_with_buffer (char *buffer, uint32_t length, uint32_t alloc);

/**
 * @brief Deallocate a mcommon_string_t and its internal buffer
 * @param string String allocated with mcommon_string_new, or NULL.
 */
void
mcommon_string_destroy (mcommon_string_t *string);

/**
 * @brief Deallocate a mcommon_string_t and return its internal buffer
 * @param string String allocated with mcommon_string_new, or NULL.
 * @returns A freestanding NUL-terminated string in a buffer that must be freed with bson_free(), or NULL if 'string'
 * was NULL.
 */
char *
mcommon_string_destroy_into_buffer (mcommon_string_t *string);

/**
 * @brief Grow a mcommon_string_t buffer if necessary to ensure a minimum capacity
 *
 * @param string String allocated with mcommon_string_new
 * @param capacity Minimum string length, in bytes, the buffer must be able to store without reallocating. Does not
 * include the NUL terminator. Must be less than UINT32_MAX.
 *
 * If a reallocation is necessary, the actual allocation size will be chosen as the next highest power-of-two above the
 * minimum needed to store 'capacity' as well as the NUL terminator.
 */
void
mcommon_string_grow_to_capacity (mcommon_string_t *string, uint32_t capacity);

/**
 * @brief Begin appending to an mcommon_string_t, with an explicit length limit
 * @param new_append Pointer to an uninitialized mcommon_string_append_t
 * @param string String allocated with mcommon_string_new
 * @param max_len Maximum allowed length for the resulting string, in bytes. Must be less than UINT32_MAX.
 *
 * The mcommon_string_append_t does not need to be deallocated. It is invalidated if the underlying mcommon_string_t is
 * freed.
 *
 * If the string was already over maximum length, it will not be modified. All append operations are guaranteed not to
 * lengthen the string beyond max_len. Truncations are guaranteed to happen at UTF-8 code point boundaries.
 */
static BSON_INLINE void
mcommon_string_append_init_with_limit (mcommon_string_append_t *new_append, mcommon_string_t *string, uint32_t max_len)
{
   BSON_ASSERT_PARAM (new_append);
   BSON_ASSERT_PARAM (string);
   BSON_ASSERT (max_len < UINT32_MAX);
   new_append->_string = string;
   new_append->_max_len = max_len;
   new_append->_max_len_exceeded = false;
}

/**
 * @brief Begin appending to an mcommon_string_t
 * @param new_append Pointer to an uninitialized mcommon_string_append_t
 * @param string String allocated with mcommon_string_new
 *
 * The mcommon_string_append_t does not need to be deallocated. It is invalidated if the underlying mcommon_string_t is
 * freed.
 *
 * The maximum string length will be set to the largest representable by the data type, UINT32_MAX - 1.
 */
static BSON_INLINE void
mcommon_string_append_init (mcommon_string_append_t *new_append, mcommon_string_t *string)
{
   mcommon_string_append_init_with_limit (new_append, string, UINT32_MAX - 1u);
}

/**
 * @brief Check the status of an append operation
 * @param append Append operation, initialized with mcommon_string_append_init
 * @returns true if the append operation has no permanent error status. false if the max length has been exceeded.
 */
static BSON_INLINE bool
mcommon_string_append_status (const mcommon_string_append_t *append)
{
   return !append->_max_len_exceeded;
}

/**
 * @brief Signal an explicit overflow during string append
 * @param append Append operation, initialized with mcommon_string_append_init
 *
 * Future calls to mcommon_string_append_status() return false, exactly as if an overlong append was attempted and
 * failed. This should be used for cases when a logical overflow is occurring but it was detected early enough that no
 * actual append was attempted.
 */
static BSON_INLINE
mcommon_string_append_set_overflow (mcommon_string_append_t *append)
{
   append->_max_len_exceeded = true;
}

/**
 * @brief Append a valid UTF-8 string with known length to the mcommon_string_t
 * @param append Append operation, initialized with mcommon_string_append_init
 * @param str UTF-8 string to append a copy of
 * @param len Length of 'str', in bytes
 * @returns true if the append operation has no permanent error status. false if the max length has been exceeded.
 *
 * If the string must be truncated to fit in the limit set by mcommon_string_append_init, it will always be split
 * in-between UTF-8 code points.
 */
static BSON_INLINE bool
mcommon_string_append_bytes (mcommon_string_append_t *append, const char *str, uint32_t len)
{
   if (BSON_UNLIKELY (!mcommon_string_append_status (append))) {
      return false;
   }

   mcommon_string_t *string = append->_string;
   char *buffer = string->str;
   uint64_t alloc = (uint64_t) string->alloc;
   uint64_t old_len = (uint64_t) string->len;
   uint64_t max_len = (uint64_t) append->_max_len;
   uint64_t new_len = old_len + (uint64_t) len;
   uint64_t new_len_with_nul = new_len + 1;

   // Fast path: no truncation, no buffer growing
   if (BSON_LIKELY (new_len <= max_len && new_len_with_nul <= alloc)) {
      memcpy (buffer + old_len, str, len);
      buffer[new_len] = '\0';
      string->len = (uint32_t) new_len;
      return true;
   }

   // Other cases are not inlined
   return mcommon_string_append_bytes_internal (append, str, len);
}

/**
 * @brief Append a valid NUL-terminated UTF-8 string to the mcommon_string_t
 * @param append Append operation, initialized with mcommon_string_append_init
 * @param str NUL-terminated UTF-8 sequence to append a copy of
 * @returns true if the append operation has no permanent error status. false if the max length has been exceeded.
 *
 * If the string must be truncated to fit in the limit set by mcommon_string_append_init, it will always be split
 * in-between UTF-8 code points.
 */
static BSON_INLINE bool
mcommon_string_append (mcommon_string_append_t *append, const char *str)
{
   return mcommon_string_append_bytes (append, str, strlen (str));
}

/**
 * @brief Append base64 encoded bytes to an mcommon_string_t
 * @param append Append operation, initialized with mcommon_string_append_init
 * @param bytes Bytes to be encoded
 * @param len Number of bytes to encoded
 * @returns true if the append operation has no permanent error status. false if the max length has been exceeded.
 */
bool
mcommon_string_append_base64_encode (mcommon_string_append_t *append, const uint8_t *bytes, uint32_t len);

/**
 * @brief Append printf() formatted text to a mcommon_string_t
 * @param append Append operation, initialized with mcommon_string_append_init
 * @param format printf() format string
 * @param ... Format string arguments
 * @returns true if the append operation has no permanent error status, and this operation has succeeded. false if the
 * max length has been surpassed or this printf() experienced an unrecoverable error.
 *
 * Writes the printf() result directly into the mcommon_string_t buffer, growing it as needed.
 *
 * If the string must be truncated to fit in the limit set by mcommon_string_append_init, it will always be split
 * in-between UTF-8 code points.
 */
bool
mcommon_string_appendf (mcommon_string_append_t *append, const char *format, ...) BSON_GNUC_PRINTF (2, 3);

/**
 * @brief Variant of mcommon_string_appendf() that takes a va_list
 * @param append Append operation, initialized with mcommon_string_append_init
 * @param format printf() format string
 * @param args Format string arguments
 * @returns true if the append operation has no permanent error status, and this operation has succeeded. false if the
 * max length has been surpassed or this printf() experienced an unrecoverable error.
 *
 * Writes the printf() result directly into the mcommon_string_t buffer, growing it as needed.
 *
 * If the string must be truncated to fit in the limit set by mcommon_string_append_init, it will always be split
 * in-between UTF-8 code points.
 */
bool
mcommon_string_vappendf (mcommon_string_append_t *append, const char *format, va_list args) BSON_GNUC_PRINTF (2, 0);

/**
 * @brief Append one code point to a mcommon_string_t
 * @param append Append operation, initialized with mcommon_string_append_init
 * @param unichar Code point to append, as a bson_unichar_t
 * @returns true if the append operation has no permanent error status. false if the max length has been exceeded.
 *
 * Guaranteed not to truncate. The character will fully append or no change will be made.
 *
 * There is no separate method to append single bytes. It is not supported to build up UTF-8 sequences from single-byte
 * parts. Every append operation must produce a valid UTF-8 string. Single ASCII characters can be appended efficiently
 * using this function, or as part of a mcommon_string_appendf().
 */
static BSON_INLINE bool
mcommon_string_append_unichar (mcommon_string_append_t *append, bson_unichar_t unichar)
{
   if (BSON_UNLIKELY (!mcommon_string_append_status (append))) {
      return false;
   }

   mcommon_string_t *string = append->_string;
   char *buffer = string->str;
   uint64_t alloc = (uint64_t) string->alloc;
   uint64_t old_len = (uint64_t) string->len;
   uint64_t max_len = (uint64_t) append->_max_len;

   // Fast path: single-byte character, no truncation, no buffer growing
   if (BSON_LIKELY (unichar <= 0x7f)) {
      uint64_t new_len = old_len + 1;
      uint64_t new_len_with_nul = new_len + 1;
      if (BSON_LIKELY (new_len <= max_len && new_len_with_nul <= alloc)) {
         buffer[old_len] = (char) unichar;
         buffer[new_len] = '\0';
         return true;
      }
   }

   // Other cases are not inlined
   return mcommon_string_append_unichar_internal (append, unichar);
}


#endif /* MONGO_C_DRIVER_COMMON_STRING_PRIVATE_H */

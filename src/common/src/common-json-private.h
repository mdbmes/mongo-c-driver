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

#ifndef MONGO_C_DRIVER_COMMON_JSON_PRIVATE_H
#define MONGO_C_DRIVER_COMMON_JSON_PRIVATE_H

#include "common-string-private.h"

#define mcommon_json_append_escaped COMMON_NAME (json_append_escaped)

/**
 * @brief Append a UTF-8 string with all special characters escaped
 *
 * @param append A bounded string append, initialized with mcommon_string_append_init()
 * @param str UTF-8 string to escape and append
 * @param len Length of 'str' in bytes
 * @param allow_nul true if internal "00" bytes or "C0 80" sequences should be encoded as "\u0000", false to treat them as invalid data
 * @returns true on success, false if this 'append' has exceeded its max length or if we encountered invalid UTF-8 or
 * disallowed NUL bytes in 'str'
 *
 * The string may include internal NUL characters. It does not need to be NUL terminated.
 * The two-byte sequence "C0 80" is also interpreted as an internal NUL, for historical reasons. This sequence is considered invalid according to RFC3629.
 */
bool
mcommon_json_append_escaped (mcommon_string_append_t *append, const char *str, uint32_t len, bool allow_nul);

#endif /* MONGO_C_DRIVER_COMMON_JSON_PRIVATE_H */

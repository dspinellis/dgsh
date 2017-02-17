/*
 * Copyright 2013-2017 Diomidis Spinellis
 *
 * Debug macros
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */

#ifndef DEBUG_H
#define DEBUG_H

#include <stdarg.h>

/* The debug level can be set by users
 * with env var DGSH_DEBUG_LEVEL
 */
void dgsh_dprintf(int debug_level, const char *fmt, ...);

/* ## is a gcc extension that removes trailing comma if no args */
#define DPRINTF(debug_level, fmt, ...) dgsh_dprintf(debug_level, fmt, ##__VA_ARGS__)

#endif /* DEBUG_H */

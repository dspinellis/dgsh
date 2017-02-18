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

extern int dgsh_debug_level;

/* ## is a gcc extension that removes trailing comma if no args */
#define DPRINTF(debug_level, fmt, ...) ((debug_level) <= dgsh_debug_level ? fprintf(stderr, "%d: " fmt "\n", (int)getpid(), ##__VA_ARGS__) : 0)

#endif /* DEBUG_H */

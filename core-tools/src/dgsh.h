/*
 * Copyright 2016-2017 Diomidis Spinellis and Marios Fragkoulis
 *
 * Dgsh public API
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

#ifndef DGSH_H
#define DGSH_H

#define DGSH_HANDLE_ERROR 0x100

int
dgsh_negotiate(int flags, const char *tool_name, int *n_input_fds,
		int *n_output_fds, int **input_fds, int **output_fds);

#endif

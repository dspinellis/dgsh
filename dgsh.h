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

/**
 * Each tool in the dgsh graph calls dgsh_negotiate() to take part in
 * peer-to-peer negotiation. A message block (MB) is circulated among tools
 * and is filled with tools' I/O requirements. When all requirements are in
 * place, an algorithm runs that tries to find a solution that satisfies
 * all requirements. If a solution is found, pipes are allocated and
 * set up according to the solution. The appropriate file descriptors
 * are provided to each tool and the negotiation phase ends.
 * The function's return value is zero for success and a non-zero for failure.
 */
int
dgsh_negotiate(const char *tool_name, /* Input variable: the program's name */
                    int *n_input_fds, /* Input/Output variable:
				       * number of input file descriptors
				       * required. The number may be changed
				       * by the API and will reflect the size
				       * of the input file descriptor array.
				       * If NULL is provided, then 0 or 1
				       * is implied and no file descriptor
				       * array is returned. The input file
				       * descriptor to return (in case of 1)
				       * substitutes stdin.
				       */
                    int *n_output_fds,/* Input/Output variable:
				       * number of output file descriptors
				       * provided. The semantics for n_input_fds
				       * apply here respectively.
				       */
                    int **input_fds,  /* Output variable:
				       * input file descriptor array
				       * The caller has the responsbility
				       * to free the array.
				       */
                    int **output_fds);/* Output variable:
				       * output file descriptor array
				       * The caller has the responsbility
				       * to free the array.
				       */

#endif

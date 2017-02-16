/*
 * Copyright 2017 Diomidis Spinellis
 *
 * Enumerate an arbitrary number of output channels.
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

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <err.h>
#include <unistd.h>

#include "dgsh.h"

int
main(int argc, char *argv[])
{
	int n_input_fds = 0, n_output_fds;
	int *output_fds;
	int i;

	switch (argc) {
	case 1:
		n_output_fds = -1;
		break;
	case 2:
		n_output_fds = atoi(argv[1]);
		break;
	default:
		errx(1, "usage: %s [n]", argv[0]);
	}


	dgsh_negotiate(DGSH_HANDLE_ERROR, argv[0], &n_input_fds,
				&n_output_fds, NULL, &output_fds);

	for (i = 0; i < n_output_fds; i++) {
		char buff[10];

		snprintf(buff, sizeof(buff), "%d\n", i);
		write(output_fds[i], buff, strlen(buff));
		close(output_fds[i]);
	}

	return 0;
}

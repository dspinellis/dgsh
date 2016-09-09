/*
 * Copyright 2016 Diomidis Spinellis
 *
 * Wrap any command to participate in the sgsh negotiation
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

/*
 * Examples:
 * sgsh-wrap yes | fsck
 * tar cf - / | sgsh-wrap dd of=/dev/st0
 * ls | sgsh-wrap sort -k5n | more
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include "sgsh-negotiate.h"

static const char *program_name;
static const char *guest_program_name;

static void
usage(void)
{
	fprintf(stderr, "Usage: %s [-d | -m] program [arguments ...]\n"
			"-d"		"Requires no input; d for deaf\n"
			"-m"		"Provides no output; m for mute\n",
		program_name);
	exit(1);
}


int
main(int argc, char *argv[])
{
	int arg = 1;
	int *ninputs = NULL, *noutputs = NULL;

	if (argc == 1)
		usage();

	program_name = argv[0];
	if (argv[1][0] == '-') {
		if (!strcmp(argv[1], "-d")) {
			ninputs = (int *)malloc(sizeof(int));
			*ninputs = 0;
			arg++;
		} else if (!strcmp(argv[1], "-m")) {
			noutputs = (int *)malloc(sizeof(int));
			*noutputs = 0;
			arg++;
		} else
			usage();
	}
	guest_program_name = argv[arg];

	if (sgsh_negotiate(guest_program_name, ninputs, noutputs, NULL, NULL) != 0)
		exit(1);

	int i, j = 0;
	char *exec_argv[argc];
	for (i = arg; i < argc; i++, j++)
		exec_argv[j] = argv[i];
	exec_argv[j] = NULL;

	if (exec_argv[0][0] == '/')
		execv(guest_program_name, exec_argv);
	else
		execvp(guest_program_name, exec_argv);

	if (ninputs)
		free(ninputs);
	if (noutputs)
		free(noutputs);

	return 0;
}

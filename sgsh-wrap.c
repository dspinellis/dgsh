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
static char *guest_program_name = NULL;

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
	int pos = 1;
	int *ninputs = NULL, *noutputs = NULL;

	fprintf(stderr, "argc: %d\n", argc);
	int k = 0;
	for (k = 0; k < argc; k++)
		fprintf(stderr, "argv[%d]: %s\n", k, argv[k]);

	program_name = argv[0];

	if (argv[1][0] == '-') {
		if (argv[1][1] == 'd') {
			ninputs = (int *)malloc(sizeof(int));
			*ninputs = 0;
			pos++;
			// XXX argv[1] may carry also the guest program's name
			if (argv[1][2] == ' ')
				guest_program_name = &argv[1][3];
		} else if (argv[1][1] == 'm') {
			noutputs = (int *)malloc(sizeof(int));
			*noutputs = 0;
			pos++;
			if (argv[1][2] == ' ')
				guest_program_name = &argv[1][3];
		} else
			usage();
	}

	if (!guest_program_name) {
		guest_program_name = argv[pos];
		pos++;
	}
	fprintf(stderr, "guest_program_name: %s\n", guest_program_name);

	int exec_argv_len = argc - 1;
	char *exec_argv[exec_argv_len];
	int i, j;

	exec_argv[0] = guest_program_name;

	int compare_chars = strlen(argv[0]) - strlen("sgsh-wrap");
	fprintf(stderr, "argv[0]: %s, argv[2]: %s, compare_chars: %d\n",
			argv[0], argv[2], compare_chars);
	/* Wrapper script to executable?
	 * Then skip argv position that contains the wrapper script
	 */
	int cmp;
	if (compare_chars > 0 &&
			!(cmp = strncmp(argv[2], argv[0], compare_chars)))
		pos++;
	fprintf(stderr, "cmp: %d, pos: %d\n", cmp, pos);

	for (i = pos, j = 1; i < argc; i++, j++)
		exec_argv[j] = argv[i];
	exec_argv[j] = NULL;

	for (k = 0; k < argc -1; k++)
		fprintf(stderr, "exec_argv[%d]: %s\n", k, exec_argv[k]);

	fprintf(stderr, "argc: %d\n", argc);
	char negotiation_title[100];
	if (argc >= 5)	// [4] does not exist, [3] is NULL
		snprintf(negotiation_title, 100, "%s %s %s",
				guest_program_name, exec_argv[1], exec_argv[2]);
	else if (argc == 4) // [3] does not exist, [2] is NULL
		snprintf(negotiation_title, 100, "%s %s",
				guest_program_name, exec_argv[1]);
	else
		snprintf(negotiation_title, 100, "%s", guest_program_name);

	if (sgsh_negotiate(negotiation_title, ninputs, noutputs, NULL, NULL) != 0)
		exit(1);
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

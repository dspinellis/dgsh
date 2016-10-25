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

#include "sgsh.h"
#include "sgsh-negotiate.h"

static const char *program_name;
static char *guest_program_name = NULL;

static void
usage(void)
{
	frpintf(stderr, "Usage: %s [-d | -m] program [arguments ...]\n"
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
	int *input_fds = NULL;

	DPRINTF("argc: %d\n", argc);
	int k = 0;
	for (k = 0; k < argc; k++)
		DPRINTF("argv[%d]: %s\n", k, argv[k]);

	program_name = argv[0];

	/* Parse any arguments to sgsh-wrap
	 * Tried getopt, but it swallows spaces in this case
	 */
	if (argv[1][0] == '-') {
		if (argv[1][1] == 'd') {
			ninputs = (int *)malloc(sizeof(int));
			*ninputs = 0;
			pos++;
			// argv[1] may carry also the guest program's name
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
	DPRINTF("guest_program_name: %s\n", guest_program_name);

	int exec_argv_len = argc - 1;
	char *exec_argv[exec_argv_len];
	int i, j;

	exec_argv[0] = guest_program_name;

	/* Arguments might contain the sgsh-wrap script to executable
	 * Skip the argv item that contains the wrapper script
	 */
	int cmp, compare_chars = strlen(argv[0]) - strlen("sgsh-wrap");
	DPRINTF("argv[0]: %s, argv[2]: %s, compare_chars: %d\n",
			argv[0], argv[2], compare_chars);
	if (compare_chars > 0 &&
			!(cmp = strncmp(argv[2], argv[0], compare_chars)))
		pos++;
	DPRINTF("cmp: %d, pos: %d\n", cmp, pos);

	// Pass argv arguments to exec_argv for exec() call.
	for (i = pos, j = 1; i < argc; i++, j++)
		exec_argv[j] = argv[i];
	exec_argv[j] = NULL;

	// Mark special argument "<|" that means input from /proc/self/fd/x
	for (k = 0; k < argc - 2; k++) {	// exec_argv[argc - 1] = NULL
		DPRINTF("exec_argv[%d]: %s\n", k, exec_argv[k]);
		if (!strcmp(exec_argv[k], "<|") || strstr(exec_argv[k], "<|")) {
			if (!ninputs) {
				ninputs = (int *)malloc(sizeof(int));
				*ninputs = 1;
			}
			(*ninputs)++;
			DPRINTF("ninputs: %d\n", *ninputs);
		}
	}

	/* Build command title to be used in negotiation
	 * Include the first two arguments
	 */
	DPRINTF("argc: %d\n", argc);
	char negotiation_title[100];
	if (argc >= 5)	// [4] does not exist, [3] is NULL
		snprintf(negotiation_title, 100, "%s %s %s",
				guest_program_name, exec_argv[1], exec_argv[2]);
	else if (argc == 4) // [3] does not exist, [2] is NULL
		snprintf(negotiation_title, 100, "%s %s",
				guest_program_name, exec_argv[1]);
	else
		snprintf(negotiation_title, 100, "%s", guest_program_name);

	// Participate in negotiation
	if (sgsh_negotiate(negotiation_title, ninputs, noutputs, &input_fds,
				NULL) != 0)
		exit(1);

	int n = 1;
	char fds[argc - 2][50];		// /proc/self/fd/x or arg=/proc/self/fd/x
	memset(fds, 0, sizeof(fds));

	if (ninputs)
		DPRINTF("%s returned %d input fds\n",
				negotiation_title, *ninputs);
	/* Substitute special argument "<|" with /proc/self/fd/x received
	 * from negotiation
	 */
	for (k = 0; k < argc - 2; k++) {	// exec_argv[argc - 1] = NULL
		char *m = NULL;
		DPRINTF("exec_argv[%d]: %s\n", k, exec_argv[k]);
		if (!strcmp(exec_argv[k], "<|") ||
				(m = strstr(exec_argv[k], "<|"))) {
			if (m) {	// substring match
				char argv_start[strlen(exec_argv[k])];
				char argv_end[strlen(exec_argv[k])];
				char proc_fd[20];
				sprintf(proc_fd, "/proc/self/fd/%d",
						input_fds[n++]);
				strncpy(argv_start, exec_argv[k], m - exec_argv[k]);
				DPRINTF("argv_start: %s\n", argv_start);
				// pointer math: skip "<|" and copy
				strcpy(argv_end, m + 2);
				DPRINTF("argv_end: %s\n", argv_end);
				sprintf(fds[k], "%s%s%s",
						argv_start, proc_fd, argv_end);
				DPRINTF("new_argv: %s\n", fds[k]);
			} else	// full match, just substitute
				sprintf(fds[k], "/proc/self/fd/%d", input_fds[n++]);
			exec_argv[k] = fds[k];
		}
		DPRINTF("After sub exec_argv[%d]: %s\n", k, exec_argv[k]);
	}

	// Execute command
	if (exec_argv[0][0] == '/')
		execv(guest_program_name, exec_argv);
	else
		execvp(guest_program_name, exec_argv);

	if (ninputs)
		free(ninputs);
	if (noutputs)
		free(noutputs);

	if (input_fds)
		free(input_fds);

	return 0;
}

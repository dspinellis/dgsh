/*
 * Copyright 2016 Diomidis Spinellis
 *
 * Wrap any command to participate in the dgsh negotiation
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
 * dgsh-wrap yes | fsck
 * tar cf - / | dgsh-wrap dd of=/dev/st0
 * ls | dgsh-wrap sort -k5n | more
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <err.h>

#include "dgsh.h"
#include "debug.h"		/* DPRINTF() */

#define PROC_FD_PATH_LEN 20

static const char *program_name;
static char *guest_program_name = NULL;

static void
usage(void)
{
	fprintf(stderr, "Usage: %s [-d | -m] program [arguments ...]\n"
			"-d"		"Requires no input; d for deaf\n"
			"-m"		"Provides no output; m for mute\n"
			"-s"		"Include stdin to the channel assignments\n",
		program_name);
	exit(1);
}

/*
 * Remove from the PATH environment variable an entry with the specified string
 */
static void
remove_from_path(const char *string)
{
	char *start, *end, *path, *strptr;

	path = getenv("PATH");
	if (!path)
		return;
	path = strdup(path);
	if (!path)
		err(1, "Error allocating path copy");
	strptr = strstr(path, string);
	if (!strptr)
		return;
	/* Find start of this path element */
	for (start = strptr; start != path && *start != ':'; start--)
		;
	/* Find end of this path element */
	for (end = strptr; *end && *end != ':'; end++)
		;
	/*
	 * At this point:
	 * start can point to : or path,
	 * end can point to : or \0.
	 * Work through all cases.
	 */
	if (*end == '\0')
		*start = '\0';
	else if (*start == ':')
		memmove(start, end, strlen(end));
	else /* first element, followed by another */
		memmove(start, end + 1, strlen(end + 1));

	if (setenv("PATH", path, 1) != 0)
		err(1, "Setting path");

	free(path);
}

int
main(int argc, char *argv[])
{
	int pos = 1;
	int ninputs = -1, noutputs = -1;
	int *input_fds = NULL;
	int feed_stdin = 0, special_args = 0;

	/* Preclude recursive wrapping */
	DPRINTF("PATH before: [%s]", getenv("PATH"));
	remove_from_path("libexec/dgsh");
	DPRINTF("PATH after: [%s]", getenv("PATH"));

	DPRINTF("argc: %d", argc);
	int k = 0;
	for (k = 0; k < argc; k++)
		DPRINTF("argv[%d]: %s", k, argv[k]);

	program_name = argv[0];

	if (argc < 2)
		usage();

	/* Parse any arguments to dgsh-wrap
	 * Tried getopt, but it swallows spaces in this case
	 */
	if (argv[1][0] == '-') {
		char *m = ++argv[1];
		DPRINTF("m: %s", m);
		while (m) {
			if (m[0] == 'd') {
				ninputs = 0;
				pos++;
				// argv[1] may carry also the guest program's name
				if (strlen(m) > 2 && m[2] != '-')
					guest_program_name = &m[2];
			} else if (m[0] == 'm') {
				noutputs = 0;
				pos++;
				if (strlen(m) > 2 && m[2] != '-')
					guest_program_name = &m[2];
			} else if (m[0] == 's') {
				feed_stdin = 1;
				pos++;
				if (strlen(m) > 2 && m[2] != '-')
					guest_program_name = &m[2];
			} else
				usage();

			m = strchr(m, '-');
		}
	}

	if (!guest_program_name) {
		guest_program_name = argv[pos];
		pos++;
	}
	DPRINTF("guest_program_name: %s", guest_program_name);

	int exec_argv_len = argc - 1;
	int i, j;
	char **exec_argv = (char **)malloc((exec_argv_len + 1) *
			sizeof(char *));

	if (exec_argv == NULL)
		err(1, "Error allocating exec_argv memory");
	exec_argv[0] = guest_program_name;

	/* Arguments might contain the dgsh-wrap script to executable
	 * Skip the argv item that contains the wrapper script
	 */
	int cmp = 0, compare_chars = strlen(argv[0]) - strlen("dgsh-wrap");
	DPRINTF("argv[0]: %s, argv[2]: %s, compare_chars: %d",
			argv[0], argv[pos], compare_chars);
	if (compare_chars > 0 &&
			!(cmp = strncmp(argv[pos], argv[0], compare_chars)))
		pos++;
	DPRINTF("cmp: %d, pos: %d", cmp, pos);

	// Pass argv arguments to exec_argv for exec() call.
	for (i = pos, j = 1; i < argc; i++, j++)
		exec_argv[j] = argv[i];
	exec_argv[j] = NULL;

	// Mark special argument "<|" that means input from /proc/self/fd/x
	for (k = 0; exec_argv[k] != NULL; k++) {	// exec_argv[argc - 1] = NULL
		DPRINTF("exec_argv[%d]: %s", k, exec_argv[k]);
		char *m = NULL;
		if (!strcmp(exec_argv[k], "<|") ||
				(m = strstr(exec_argv[k], "<|"))) {
			special_args = 1;
			if (!m)
				ninputs++;
			while (m) {
				ninputs++;
				m += 2;
				m = strstr(m, "<|");
			}
			DPRINTF("ninputs: %d", ninputs);
		}
	}
	/* originally ninputs = -1, so +1 to 0 and
	 * +1 if stdin should be included in channel assignments
	 */
	if (special_args)
		ninputs += 1 + feed_stdin;

	/* Build command title to be used in negotiation
	 * Include the first two arguments
	 */
	DPRINTF("argc: %d", argc);
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
	dgsh_negotiate(DGSH_HANDLE_ERROR, negotiation_title,
					ninputs == -1 ? NULL : &ninputs,
					noutputs == -1 ? NULL : &noutputs,
					&input_fds, NULL);
	int n = feed_stdin ? 1 : 0;

	char **fds = calloc(argc - 2, sizeof(char *));		// /proc/self/fd/x or arg=/proc/self/fd/x

	if (ninputs != -1)
		DPRINTF("%s returned %d input fds", negotiation_title, ninputs);
	/* Substitute special argument "<|" with /proc/self/fd/x received
	 * from negotiation
	 */
	for (k = 0; exec_argv[k] != NULL; k++) {	// exec_argv[argc - 1] = NULL
		char *m = NULL;
		DPRINTF("exec_argv[%d] to sub: %s", k, exec_argv[k]);
		if (!strcmp(exec_argv[k], "<|") ||
			(m = strstr(exec_argv[k], "<|"))) {

			size_t size = sizeof(char) *
				(strlen(exec_argv[k]) + PROC_FD_PATH_LEN * ninputs);
			DPRINTF("fds[k] size: %d", (int)size);
			fds[k] = (char *)malloc(size);
			if (fds[k] == NULL)
				err(1, "Unable to allocate %zu bytes for fds", size);
			memset(fds[k], 0, size);

			if (!m)	// full match, just substitute
				sprintf(fds[k], "/proc/self/fd/%d",
						input_fds[n++]);

			char *argv_end = NULL;
			while (m) {	// substring match
				DPRINTF("Matched: %s", m);
				char *new_argv = calloc(size, 1);
				char *argv_start = calloc(size, 1);
				char *proc_fd = calloc(PROC_FD_PATH_LEN, 1);
				if (new_argv == NULL ||
						argv_start == NULL ||
						proc_fd == NULL)
					err(1, "Error allocating argv memory");

				sprintf(proc_fd, "/proc/self/fd/%d",
						input_fds[n++]);
				DPRINTF("proc_fd: %s", proc_fd);
				if (!argv_end)
					strncpy(argv_start, exec_argv[k],
							m - exec_argv[k]);
				else
					strncpy(argv_start, argv_end,
							m - argv_end);
				DPRINTF("argv_start: %s", argv_start);
				argv_end = m + 2;
				DPRINTF("argv_end: %s", argv_end);
				if (strlen(fds[k]) > 0) {
					strcpy(new_argv, fds[k]);
					sprintf(fds[k], "%s%s%s", new_argv,
							argv_start, proc_fd);
				} else
					sprintf(fds[k], "%s%s", argv_start,
							proc_fd);
				m = strstr(argv_end, "<|");
				if (!m) {
					strcpy(new_argv, fds[k]);
					sprintf(fds[k], "%s%s",
						new_argv, argv_end);
				}
				DPRINTF("fds[k]: %s", fds[k]);
			}
			exec_argv[k] = fds[k];
		}
		DPRINTF("After sub exec_argv[%d]: %s", k, exec_argv[k]);
	}

	// Execute command
	if (exec_argv[0][0] == '/')
		execv(guest_program_name, exec_argv);
	else
		execvp(guest_program_name, exec_argv);

	err(1, "Unable to execute %s", guest_program_name);
	return 1;
}

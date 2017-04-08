/*
 * Copyright 2016, 2017 Diomidis Spinellis, Marios Fragkoulis
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
#include "dgsh-debug.h"		/* DPRINTF(4, ) */

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
	int ninputs = -1, noutputs = -1;
	int *input_fds = NULL;
	int feed_stdin = 0, special_args = 0;
	int k = 0, i = 0, j = 0, r = 0;
	int ch;
	int internal_argc = 0;
	char **internal_argv;
	int dgsh_wrap_args;
	char *m, *dup_arg;
	int skip_pos_dup_arg = -1;
	char *debug_level;

	debug_level = getenv("DGSH_DEBUG_LEVEL");
	if (debug_level)
		dgsh_debug_level = atoi(debug_level);

	/* Preclude recursive wrapping */
	DPRINTF(4, "PATH before: [%s]", getenv("PATH"));
	remove_from_path("libexec/dgsh");
	DPRINTF(4, "PATH after: [%s]", getenv("PATH"));

	DPRINTF(4, "argc: %d", argc);
	for (k = 0; k < argc; k++)
		DPRINTF(4, "argv[%d]: %s, len: %d", k, argv[k], strlen(argv[k]));

	program_name = argv[0];

	if (argc < 2)
		usage();

	/* If args to dgsh-wrap are there, they start with argv[1] */
	j = 1;
	while (argv[j][0] == '-' && (strchr(argv[j], ' ') != NULL ||
				strchr(argv[j], '\t') != NULL)) {
		int count_space = 0;
		int p;

		/* Figure out allocation for space for internal_argv */
		for (i = 0; i < strlen(argv[j]); i++)
			if (argv[j][i] == ' ' || argv[j][i] == '\t')
				count_space++;
		DPRINTF(4, "In %s count_space: %d", argv[j], count_space);
		internal_argc = argc + count_space;
		internal_argv = malloc(sizeof(char *) * internal_argc);

		/* Copy the same slots from the beginning of argv.
		 * getopt works with program name as first arg.
		 */
		for (r = 0; r < j; r++)
			internal_argv[r] = strdup(argv[r]);

		/* Continue to fill in internal_argv by
		 * copying the tokens found between non-handled spaces.
		 */
		p = r;
		m = strtok(argv[j], " \t");
		while (m != NULL) {
			internal_argv[p] = strdup(m);
			DPRINTF(4, "tokenised internal_argv[%d]: %s",
					p, internal_argv[p]);
			m = strtok(NULL, " \t");
			p++;
		}

		/* Copy the remaining slots of argv into internal_argv
		 * until the end of argv
		 */
		for (r = j + 1; r < argc; r++, p++)
			internal_argv[p] = strdup(argv[r]);
		j++;
	}

	/* If no args to dgsh-wrap: copy argv to internal_argv
	 * for uniform processing of the provided command.
	 * Else: separate args to dgsh-wrap from the guest command.
	 */
	if (j == 1) {
		dgsh_wrap_args = 1;	// exclude argv[0] = dgsh-wrap
		internal_argc = argc;
		internal_argv = malloc(sizeof(char *) * internal_argc);
		for (r = 0; r < argc; r++)
			internal_argv[r] = strdup(argv[r]);
	} else {
		j = 1;
		while (internal_argv[j][0] == '-')
			j++;
		dgsh_wrap_args = j;
	}
	DPRINTF(4, "dgsh_wrap_args: %d", dgsh_wrap_args);

	// For debugging.
	DPRINTF(4, "internal_argc: %d", internal_argc);
	for (k = 0; k < internal_argc; k++)
		DPRINTF(4, "internal_argv[%d]: %s", k, internal_argv[k]);

	/* dgsh_wrap_args: handle only args to dgsh-wrap */
        while ((ch = getopt(dgsh_wrap_args, internal_argv, "dms")) != -1) {
		switch (ch) {
		case 'd':
			ninputs = 0;
			break;
		case 'm':
			noutputs = 0;
			break;
		case 's':
			feed_stdin = 1;
			break;
		case '?':
		default:
			usage();
		}
	}
	DPRINTF(3, "After getopt: ninputs: %d, noutputs: %d, feed_stdin: %d",
			ninputs, noutputs, feed_stdin);

	/* Record guest program name (without path) */
	m = strchr(internal_argv[j], '/');
	guest_program_name = internal_argv[j];
	while (m != NULL) {
		m++;
		guest_program_name = m;
		m = strchr(m, '/');
	}
	DPRINTF(4, "guest_program_name: %s", guest_program_name);

	/* Arguments might contain the dgsh-wrap script to executable
	 * Skip the argv item that contains the wrapper script
	 */
	if (internal_argc > j + 1) {
		m = strchr(internal_argv[j + 1], '/');
		dup_arg = internal_argv[j + 1];
		while (m != NULL) {
			m++;
			dup_arg = m;
			m = strchr(m, '/');
		}
		DPRINTF(4, "dup_arg: %s", dup_arg);
		if (!strcmp(guest_program_name, dup_arg))
			skip_pos_dup_arg = j + 1;
	}
	DPRINTF(4, "skip_pos_dup_arg: %d", skip_pos_dup_arg);

	// Pass internal_argv arguments to exec_argv for exec() call.
	int exec_argc = internal_argc + 1 - dgsh_wrap_args -
						(skip_pos_dup_arg > 0);
	char **exec_argv = (char **)malloc(sizeof(char *) * exec_argc);

	if (exec_argv == NULL)
		err(1, "Error allocating exec_argv memory");

	j = 0;
	i = dgsh_wrap_args;
	while (i < internal_argc) {
		if (skip_pos_dup_arg == i) {
			i++;
			continue;
		}
		exec_argv[j] = internal_argv[i];
		i++;
		j++;
	}
	exec_argv[j] = NULL;

	/* Mark special argument "<|" that means input from /proc/self/fd/x
	 * exec_argv[argc - 1] == NULL
	 */
	for (k = 0; exec_argv[k] != NULL; k++) {
		DPRINTF(4, "exec_argv[%d]: %s", k, exec_argv[k]);
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
			DPRINTF(4, "ninputs: %d", ninputs);
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
	char negotiation_title[100];
	if (exec_argc > 3)	// [3] is NULL
		snprintf(negotiation_title, 100, "%s %s %s",
				guest_program_name, exec_argv[1], exec_argv[2]);
	else if (exec_argc == 3) // [2] is NULL
		snprintf(negotiation_title, 100, "%s %s",
				guest_program_name, exec_argv[1]);
	else
		snprintf(negotiation_title, 100, "%s", guest_program_name);

	/* Participate in negotiation */
	dgsh_negotiate(DGSH_HANDLE_ERROR, negotiation_title,
					ninputs == -1 ? NULL : &ninputs,
					noutputs == -1 ? NULL : &noutputs,
					&input_fds, NULL);
	int n = feed_stdin ? 1 : 0;

	/* /proc/self/fd/x or arg=/proc/self/fd/x */
	char **fds = calloc(argc, sizeof(char *));

	if (ninputs != -1)
		DPRINTF(3, "%s returned %d input fds", negotiation_title, ninputs);

	/* Substitute special argument "<|" with /proc/self/fd/x received
	 * from negotiation.
	 * exec_argv[argc - 1] == NULL
	 */
	for (k = 0; exec_argv[k] != NULL; k++) {
		char *m = NULL;
		DPRINTF(3, "exec_argv[%d] to sub: %s", k, exec_argv[k]);
		if (!strcmp(exec_argv[k], "<|") ||
			(m = strstr(exec_argv[k], "<|"))) {

			size_t size = sizeof(char) *
				(strlen(exec_argv[k]) + PROC_FD_PATH_LEN * ninputs);
			DPRINTF(4, "fds[k] size: %d", (int)size);
			fds[k] = (char *)malloc(size);
			if (fds[k] == NULL)
				err(1, "Unable to allocate %zu bytes for fds",
						size);
			memset(fds[k], 0, size);

			if (!m)	// full match, just substitute
				sprintf(fds[k], "/proc/self/fd/%d",
						input_fds[n++]);

			char *argv_end = NULL;
			while (m) {	// substring match
				DPRINTF(4, "Matched: %s", m);
				char *new_argv = calloc(size, 1);
				char *argv_start = calloc(size, 1);
				char *proc_fd = calloc(PROC_FD_PATH_LEN, 1);
				if (new_argv == NULL ||
						argv_start == NULL ||
						proc_fd == NULL)
					err(1, "Error allocating argv memory");

				sprintf(proc_fd, "/proc/self/fd/%d",
						input_fds[n++]);
				DPRINTF(4, "proc_fd: %s", proc_fd);
				if (!argv_end)
					strncpy(argv_start, exec_argv[k],
							m - exec_argv[k]);
				else
					strncpy(argv_start, argv_end,
							m - argv_end);
				DPRINTF(4, "argv_start: %s", argv_start);
				argv_end = m + 2;
				DPRINTF(4, "argv_end: %s", argv_end);
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
				DPRINTF(4, "fds[k]: %s", fds[k]);
			}
			exec_argv[k] = fds[k];
		}
		DPRINTF(4, "After sub exec_argv[%d]: %s", k, exec_argv[k]);
	}

	// Execute command
	if (exec_argv[0][0] == '/')
		execv(exec_argv[0], exec_argv);
	else
		execvp(exec_argv[0], exec_argv);

	err(1, "Unable to execute %s", exec_argv[0]);
	return 1;
}

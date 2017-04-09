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

#include <assert.h>
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
	fprintf(stderr, "Usage:\t%s [-d | -m | -s | -I] program [program-arguments ...]\n"
			"\t%s -i [-d | -m | -s] [program-arguments ...]\n"
			"-d\t"		"Requires no input (deaf)\n"
			"-I\t"		"Process flags and program as a #! interpreter\n"
			"-i\t"		"Process flags as a #! interpreter\n"
			"\t"		"(-I or -i must be the first flag)\n"
			"-m\t"		"Provides no output; (mute)\n"
			"-s\t"		"Include stdin to the channel assignments\n",
		program_name, program_name);
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

static void
dump_args(int argc, char *argv[])
{
	int i;

	for (i = 0; i <= argc; i++)
		DPRINTF(4, "argv[%d]: [%s]", i, argv[i]);
}

static void *
xmalloc(size_t size)
{
	void *r = malloc(size);
	if (r == NULL)
		err(1, "out of memory");
	return r;
}

static void *
xrealloc(void *ptr, size_t size)
{
	void *r = realloc(ptr, size);
	if (r == NULL)
		err(1, "out of memory");
	return r;
}

/*
 * Process the arguments of a #! invocation to make them equivalent
 * to a command-line one.
 * This entails tokenizing argv[1], which contains all the #! line
 * after the name of the interpreter and
 * if argv[1] starts with -i removing path from  argv[2]
 * if argv[1] starts with -I removing argv[2]
 *
 * Example 1 (-I: supply program to execute on #! line):
 * Input arguments:
 * argv[0]: /usr/local/libexec/dgsh/dgsh-wrap
 * argv[1]: -I -d /bin/uname
 * argv[2]: /usr/local/libexec/dgsh/uname
 * argv[3]: -s
 * Output arguments:
 * argv[0]: /usr/local/libexec/dgsh/dgsh-wrap
 * argv[1]: -I
 * argv[2]: -d
 * argv[3]: /bin/uname
 * argv[5]: -s
 *
 * Example 2 (-i: derive program to execute from script's name):
 * Input arguments:
 * argv[0]: /usr/local/libexec/dgsh/dgsh-wrap
 * argv[1]: -i -d /bin/uname
 * argv[2]: -s
 * Output arguments:
 * argv[0]: /usr/local/libexec/dgsh/dgsh-wrap
 * argv[1]: -i
 * argv[2]: -d
 * argv[3]: uname
 * argv[5]: -s
 */
static void
interpreter_process(int *argcp, char ***argvp)
{
	int argc = *argcp;
	char **argv = *argvp;

	assert(argc >= 2);
	if (argc >= 3)
		/*
		 * Process argv[2], which is the name of the script
		 * supplied by the kernel to the interpreter, i.e.
		 * the name of the program we are being executed as.
		 */
		if (argv[1][1] == 'I') {
			/* #!dgsh-wrap -I -d program-name: remove argv[2] */
			memmove(argv + 2, argv + 3, (argc - 3) * sizeof(char *));
			argc -= 1;
			argv[argc] = NULL;
		} else if (argv[1][1] == 'i') {
			/* #!dgsh-wrap -i: Remove absolute path from argv[2] */
			char *p = strrchr(argv[2], '/');
			if (p)
				memmove(argv[2], p + 1, strlen(p) + 1);
		}
	DPRINTF(4, "Arguments after processing argv[2]");
	dump_args(argc, argv);

	/* Tokenize argv[1] into nargv */
	argv = xmalloc((argc + 1) * sizeof(char *));
	memcpy(argv, *argvp, (argc + 1) * sizeof(char *));
	int i = 1;
	const char *delim = " \t";
	char *p = strtok(argv[1], delim);
	assert(p != NULL);	/* One arg (-i) is guaranteed to exist */
	for (;;) {
		argv[i] = p;
		p = strtok(NULL, delim);
		if (p == NULL)
			break;
		/* Make room for new string */
		argc += 1;
		i++;
		argv = xrealloc(argv, (argc + 1) * sizeof(char *));
		memmove(argv + i + 1, argv + i, (argc - 1) * sizeof(char *));
	}
	*argvp = argv;
	*argcp = argc;
	DPRINTF(4, "Arguments after interpreter_process");
	dump_args(*argcp, *argvp);
}

int
main(int argc, char *argv[])
{
	int ninputs = -1, noutputs = -1;
	int *input_fds = NULL;
	int feed_stdin = 0, special_args = 0;
	int ch, i;
	int dgsh_wrap_args;
	char *p;
	char *debug_level;

	debug_level = getenv("DGSH_DEBUG_LEVEL");
	if (debug_level)
		dgsh_debug_level = atoi(debug_level);

	/* Preclude recursive wrapping */
	DPRINTF(4, "PATH before: [%s]", getenv("PATH"));
	remove_from_path("libexec/dgsh");
	DPRINTF(4, "PATH after: [%s]", getenv("PATH"));

	DPRINTF(4, "Initial arguments");
	dump_args(argc, argv);

	/* Check for #! interpreter argument processing */
	if (argc >= 2 && argv[1][0] == '-' && tolower(argv[1][1]) == 'i')
		interpreter_process(&argc, &argv);

	program_name = argv[0];

	/* dgsh_wrap_args: handle only args to dgsh-wrap */
        while ((ch = getopt(argc, argv, "dIims")) != -1) {
		switch (ch) {
		case 'd':
			ninputs = 0;
			break;
		case 'I':
		case 'i':
			/* Complain -I or -i is not the first flag */
			if (ninputs == 0 || noutputs == 0 || feed_stdin)
				usage();
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

	if (optind >= argc)
		usage();

	/* Obtain guest program name (without path) */
	p = strrchr(argv[optind], '/');
	if (p == NULL)
		guest_program_name = argv[optind];
	else
		guest_program_name = p + 1;
	DPRINTF(4, "guest_program_name: %s", guest_program_name);

	/* Handle special argument "<|", which means input from /proc/self/fd/x
	 */
	for (i = optind + 1; exec_argv[k] != NULL; k++) {
		if (strcmp(argv[i], "<|") == 0) {
			special_args = 1;
			ninputs++;
		}
	}
	/* originally ninputs = -1, so +1 to 0 and
	 * +1 if stdin should be included in channel assignments
	 */
	if (special_args)
		ninputs += 1 + feed_stdin;

	/* Participate in negotiation */
	dgsh_negotiate(DGSH_HANDLE_ERROR, guest_program_name,
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
#endif
}

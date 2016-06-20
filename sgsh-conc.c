/*
 * Copyright 2016 Diomidis Spinellis
 *
 * A passive component that aids the sgsh negotiation by passing
 * message blocks among participating processes.
 * When the negotiation is finished and the processes get connected by
 * pipes, it exits.
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

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <err.h>
#include <errno.h>
#include <limits.h>
#include <string.h>
#include <unistd.h>

#include "sgsh-negotiate.h"

static const char *program_name;

static void
usage(void)
{
	fprintf(stderr, "Usage: %s -i|-o nprog\n"
		"-i"		"\tInput concentrator: multiple inputs to single output\n"
		"-o"		"\tOutput concentrator: single input to multiple outputs\n",
		program_name);
	exit(1);
}

static struct portinfo {
	int lowest_pid;		// Lowest pid seen on this port
	int seen_times;		// Number of times the pid was seen
	bool run_ready;		// True when the associated process can run
} *pi;

/*
 * True when we're concentrating inputs, i.e. gathering 0, 3, 4, ... to 1
 * Otherwise we scatter 0 to 1, 3, 4 ...
 */
STATIC bool multiple_inputs;
STATIC int nfd;

// Return the next fd where a block should be passed
STATIC int
next_fd(int fd)
{
	if (multiple_inputs)
		switch (fd) {
		case 0:
			return 1;
		case 1:
			return nfd;
		case 3:
			return 0;
		default:
			return fd - 1;
		}
	else
		switch (fd) {
		case 0:
			return 1;
		case 1:
			return 3;
		case 4:
			return 0;
		default:
			return fd + 1;
		}
}

int
main(int argc, char *argv[])
{
	int ch;

	program_name = argv[0];

	while ((ch = getopt(argc, argv, "io")) != -1) {
		switch (ch) {
		case 'i':
			multiple_inputs = true;
			break;
		case 'o':
			multiple_inputs = false;
			break;
		case '?':
		default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;

	if (argc != 1)
		usage();

	nfd = atoi(argv[0]);
	pi = (struct portinfo *)calloc(nfd + 1, sizeof(struct portinfo));

	// Pass around message blocks

	// Scatter or gather file descriptors

	return 0;
}

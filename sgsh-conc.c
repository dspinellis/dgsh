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

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <err.h>
#include <errno.h>
#include <limits.h>
#include <string.h>
#include <unistd.h>
#include <sys/select.h>

#include "sgsh-negotiate.h"
#include "sgsh-internal-api.h"

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
	struct sgsh_negotiation *to_write; // Block pending a write
} *pi;

/*
 * True when we're concentrating inputs, i.e. gathering 0, 3, 4, ... to 1
 * Otherwise we scatter 0 to 1, 3, 4 ...
 */
STATIC bool multiple_inputs;

/*
 * Total number of file descriptors on which the process performs I/O
 * (including stderr).
 */
STATIC int nfd;

/**
 * Return the next fd where a read block should be passed
 */
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
		default:
			if (fd == nfd)
				return 0;
			else
				return fd + 1;
		}
}

#define max(a, b) ((a) > (b) ? (a) : (b))

void
pass_blocks(void)
{
	fd_set readfds, writefds;
	int nfds = 0;
	int i;

	for (;;) {
		// Create select(2) masks
		FD_ZERO(&readfds);
		FD_ZERO(&writefds);
		for (i = 0; i < nfd + 1; i++) {
			if (i == STDERR_FILENO)
				continue;
			if (!pi[i].run_ready &&
					pi[next_fd(i)].to_write == NULL) {
				FD_SET(i, &readfds);
				nfds = max(i + 1, nfds);
			}
			if (pi[i].to_write) {
				FD_SET(i, &writefds);
				nfds = max(i + 1, nfds);
			}
		}

	again:
		if (select(nfds, &readfds, &writefds, NULL, NULL) < 0) {
			if (errno == EINTR)
				goto again;
			/* All other cases are internal errors. */
			perror("select");
			exit(1);
		}

		// Read/write what we can
		for (i = 0; i < nfd + 1; i++) {
			if (FD_ISSET(i, &writefds)) {
				assert(pi[i].to_write);
				chosen_mb = pi[i].to_write;
				write_message_block(i); // XXX check return
				free_mb(pi[i].to_write);
				pi[i].to_write = NULL;
			}
			if (FD_ISSET(i, &readfds)) {
				struct sgsh_negotiation *rb;
				int next = next_fd(i);

				assert(!pi[i].run_ready);
				assert(pi[next].to_write == NULL);
				// read_message_block(i, &pi[next].to_write); // XXX check return
				rb = pi[next].to_write;

				// Count # times we see a block on a port
				if (rb->initiator_pid < pi[i].lowest_pid) {
					pi[i].lowest_pid = rb->initiator_pid;
					pi[i].seen_times = 1;
				} else if (rb->initiator_pid == pi[i].lowest_pid)
					pi[i].seen_times++;

				if (pi[i].seen_times == 2)
					pi[i].run_ready = true;
			}
		}

		// See if all processes are run-ready
		nfds = 0;
		for (i = 0; i < nfd + 1; i++)
			if (pi[i].run_ready)
				nfds++;
		if (nfds == nfd)
			return;
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

	pass_blocks();
	// Scatter or gather file descriptors XXX

	return 0;
}

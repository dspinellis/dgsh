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
#include "sgsh.h"		/* DPRINTF */

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

/*
 * Information for each I/O file descriptor on which
 * the concentrator operates.
 */
static struct portinfo {
	pid_t pid;		// The id of the process talking to this port
	pid_t lowest_pid;	// Lowest pid seen on this port
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
 * (including stderr).  The last fd used in nfd - 1.
 */
STATIC int nfd;


/**
 * Return the next fd where a read block should be passed
 * Return whether we should record the origin of the block or
 * restore it
 */
#define FREE_FILENO (STDERR_FILENO + 1)
STATIC int
next_fd(int fd, bool *ro)
{
	if (multiple_inputs)
		switch (fd) {
		case STDIN_FILENO:
			return STDOUT_FILENO;
		case STDOUT_FILENO:
			return nfd - 1;
		case FREE_FILENO:
			*ro = true;
			return STDIN_FILENO;
		default:
			*ro = true;
			return fd - 1;
		}
	else
		switch (fd) {
		case STDIN_FILENO:
			return STDOUT_FILENO;
		case STDOUT_FILENO:
			*ro = true;
			return FREE_FILENO;
		default:
			if (fd == nfd - 1)
				return STDIN_FILENO;
			else {
				*ro = true;
				return fd + 1;
			}
		}
}

#define max(a, b) ((a) > (b) ? (a) : (b))


/*
 * Pass around the message blocks so that they reach all processes
 * connected through the concentrator.
 */
STATIC struct sgsh_negotiation *
pass_message_blocks(void)
{
	fd_set readfds, writefds;
	int nfds = 0;
	int i;
	struct sgsh_negotiation *solution = NULL;
	int oi = -1;		/* scatter/gather block's origin index */
	int ofd = -1;		/* scatter/gather block's origin fd direction */
	bool ro = false;	/* Whether the read block's origin should
				 * be restored
				 */

	for (;;) {
		// Create select(2) masks
		FD_ZERO(&readfds);
		FD_ZERO(&writefds);
		for (i = 0; i < nfd; i++) {
			if (i == STDERR_FILENO)
				continue;
			if (!pi[i].run_ready &&
					pi[next_fd(i, &ro)].to_write == NULL) {
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
			err(1, "select");
		}

		// Read/write what we can
		for (i = 0; i < nfd; i++) {
			if (FD_ISSET(i, &writefds)) {
				assert(pi[i].to_write);
				chosen_mb = pi[i].to_write;
				write_message_block(i); // XXX check return

				/* The process whose id is set does not pass
				 * the block. Prepare exit.
				 */
				if (pi[i].to_write->state == PS_RUN &&
						pi[i].to_write->preceding_process_pid == pi[i].pid)
					pi[i].run_ready = true;
				else {
					free_mb(pi[i].to_write);
					pi[i].to_write = NULL;
				}
			}
			if (FD_ISSET(i, &readfds)) {
				struct sgsh_negotiation *rb;
				bool nnt = false;	// Node is non terminal
				ro = false;
				int next = next_fd(i, &ro);

				assert(!pi[i].run_ready);
				assert(pi[next].to_write == NULL);
				read_message_block(i, &pi[next].to_write); // XXX check return
				rb = pi[next].to_write;

				if (oi == -1)
					if ((multiple_inputs && i == 1) ||
							(!multiple_inputs &&
							 i == 0)) {
					oi = rb->origin_index;
					ofd = rb->origin_fd_direction;
				}

				pi[i].pid = get_origin_pid(rb, &nnt);

				/* If needed, re-set origin.
				 * Don't move this block before get_origin_pid()
				 */
				if (ro) {
					pi[next].to_write->origin_index = oi;
					pi[next].to_write->origin_fd_direction = ofd;
				}

				/* Count # times we see a block on a port
				 * after a solution has been found
				 */
				if (rb->state == PS_NEGOTIATION)
					continue;
				if (pi[i].lowest_pid == 0 ||
						rb->initiator_pid < pi[i].lowest_pid) {
					pi[i].lowest_pid = rb->initiator_pid;
					pi[i].seen_times = 1;
				} else if (rb->initiator_pid == pi[i].lowest_pid)
					pi[i].seen_times++;

				DPRINTF("%s(): pi[%d].pid: %d, initiator pid: %d, pi[%d].lowest_pid %d, pi[%d].seen_times: %d", __func__, i, (int)pi[i].pid, (int)rb->initiator_pid, i, (int)pi[i].lowest_pid, i, pi[i].seen_times);
				if (pi[i].seen_times == 1 + nnt) {
					solution = pi[next].to_write;
					pi[i].run_ready = true;
					DPRINTF("%s(): pi[%d] is run ready", __func__, i);
				}
			}
		}

		// See if all processes are run-ready
		nfds = 0;
		for (i = 0; i < nfd; i++) {
			if (pi[i].run_ready)
				nfds++;
			DPRINTF("%s(): pi[%d].pid: %d run ready?: %d, nfds: %d",
				__func__, i, pi[i].pid, (int)pi[i].run_ready, nfds);
		}
		if (nfds == nfd - 1) {
			assert(solution != NULL);
			return solution;
		}
	}
}



/*
 * Scatter the fds read from the input process to multiple outputs.
 */
STATIC void
scatter_input_fds(struct sgsh_negotiation *mb)
{
	int n_to_read = get_provided_fds_n(mb, pi[STDIN_FILENO].pid);
	DPRINTF("%s(): fds to read: %d", __func__, n_to_read);
	int *read_fds = (int *)malloc(n_to_read * sizeof(int));
	int i, j, write_index;

	for (i = 0; i < n_to_read; i++)
		read_fds[i] = read_fd(STDIN_FILENO);

	write_index = 0;
	bool ro = false;	/* Ignore */
	for (i = STDOUT_FILENO; i < nfd; i = next_fd(i, &ro)) {
		int n_to_write = get_expected_fds_n(mb, pi[i].pid);
		DPRINTF("%s(): fds to write for p[%d].pid %d: %d",
				__func__, i, pi[i].pid, n_to_write);
		for (j = write_index; j < write_index + n_to_write; j++) {
			write_fd(i, read_fds[j]);
			DPRINTF("%s(): Write fd: %d to output channel: %d",
					__func__, read_fds[j], i);
		}
		write_index += n_to_write;
	}
	assert(write_index == n_to_read);
}

/*
 * Gather the fds read from input processes to a single output.
 */
STATIC void
gather_input_fds(struct sgsh_negotiation *mb)
{
	int n_to_write = get_expected_fds_n(mb, pi[STDOUT_FILENO].pid);
	DPRINTF("%s(): fds to write: %d", __func__, n_to_write);
	int *read_fds = (int *)malloc(n_to_write * sizeof(int));
	int i, j, read_index;

	read_index = 0;
	//bool ro = false;	/* Ignore */
	//for (i = nfd - 1; i != STDOUT_FILENO; i = next_fd(i, &ro)) {
	for (i = 0; i < nfd; i == 0 ? i = 3 : i++) {
		int n_to_read = get_provided_fds_n(mb, pi[i].pid);
		DPRINTF("%s(): fds to read for p[%d].pid %d: %d",
				__func__, i, pi[i].pid, n_to_read);
		for (j = read_index; j < read_index + n_to_read; j++) {
			read_fds[j] = read_fd(i);
			DPRINTF("%s(): Read fd: %d from input channel: %d",
					__func__, read_fds[j], i);
		}
		read_index += n_to_read;
	}
	assert(read_index == n_to_write);

	for (i = 0; i < n_to_write; i++)
		write_fd(STDOUT_FILENO, read_fds[i]);

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

	/* +1 for stdin when scatter/stdout when gather
	 * +1 for stderr which is not used
	 */
	nfd = atoi(argv[0]) + 2;
	pi = (struct portinfo *)calloc(nfd, sizeof(struct portinfo));

	chosen_mb = pass_message_blocks();
	if (multiple_inputs)
		gather_input_fds(chosen_mb);
	else
		scatter_input_fds(chosen_mb);
	free_mb(chosen_mb);
	return 0;
}

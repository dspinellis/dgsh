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
#include <unistd.h>		/* getpid() */
#include <sys/select.h>

#include "sgsh-negotiate.h"
#include "sgsh-internal-api.h"
#include "sgsh.h"		/* DPRINTF */

static const char *program_name;
static pid_t pid;

static void
usage(void)
{
	fprintf(stderr, "Usage: %s -i|-o nprog [-r]\n"
		"-i"		"\tInput concentrator: multiple inputs to single output\n"
		"-o"		"\tOutput concentrator: single input to multiple outputs\n"
		"-r"		"\tPass the block to origin (except for stdin, stdout); used with input concentrator\n",
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
	bool seen;		// True when the pid was seen
	bool written;		// True when we wrote to pid
	bool run_ready;		// True when the associated process can run
	struct sgsh_negotiation *to_write; // Block pending a write
} *pi;

/*
 * True when we're concentrating inputs, i.e. gathering 0, 3, 4, ... to 1
 * Otherwise we scatter 0 to 1, 3, 4 ...
 */
STATIC bool multiple_inputs;

/* True when input concentrator is the gather endpoint
 * of a scatter-first-gather-then block.
 * In this case using the default route of the input
 * concentrator results in complex cycles and ruins
 * the algorithms that decide when negotiation should
 * end both for concentrators and participating processes.
 * The favored scheme in this case is:
 * stdin -> stdout
 * stdout -> stdin
 * fd -> fd
 */
STATIC bool pass_origin;

/*
 * Total number of file descriptors on which the process performs I/O
 * (including stderr).  The last fd used in nfd - 1.
 */
STATIC int nfd;

#define FREE_FILENO (STDERR_FILENO + 1)

/**
 * Return the next fd where a read block should be passed
 * Return whether we should restore the origin of the block
 */
STATIC int
next_fd(int fd, bool *ro)
{
	if (multiple_inputs)
		if (pass_origin)
			switch (fd) {
			case STDIN_FILENO:
				return STDOUT_FILENO;
			case STDOUT_FILENO:
				return STDIN_FILENO;
			default:
				*ro = true;
				return fd;
			}
		else
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

/* Search for conc with pid in message block mb
 * and return a pointer to the structure or
 * NULL if not found.
 */
STATIC struct sgsh_conc *
find_conc(struct sgsh_negotiation *mb, pid_t pid)
{
	int i;
	struct sgsh_conc *ca = mb->conc_array;
	for (i = 0; i < mb->n_concs; i++)
		if (ca[i].pid == pid)
			return &ca[i];
	return NULL;
}

/**
 * Return whether the process at port i
 * is ready to run.
 * Check whether this is the process whose
 * pid is set and prepare for exit.
 */
STATIC bool
is_ready(int i, struct sgsh_negotiation *mb)
{
	bool ignore = false;
	/* The process whose id is set does not pass
	 * the block. Prepare exit.
	 */
	DPRINTF("%s: pi[%d].pid: %d, mb->preceding_process_pid: %d\n",
			__func__, i, pi[i].pid, mb->preceding_process_pid);
	if (mb->state != PS_RUN)
		return false;
	if (pi[i].pid == mb->preceding_process_pid &&
			!find_conc(mb, pi[i].pid)) {
		pi[i].seen = true;	/* Fake that */
		/* If the solution is also in our territory
		 * fake writing to it.
		 */
		if (!pi[next_fd(i, &ignore)].run_ready) {
			assert(pi[next_fd(i, &ignore)].seen);
			pi[next_fd(i, &ignore)].written = true;
			pi[next_fd(i, &ignore)].run_ready = true;
		}
		return true;
	} else if (pi[i].seen && pi[i].written)
		return true;
	return false;
}

/**
 * Retrieve from message block the current concentrator's input and
 * output channels.
 * This is required in cases where a concentrator is directly
 * connected to another concentrator.
 */
STATIC int
set_io(struct sgsh_negotiation *mb, struct sgsh_conc *c)
{
	bool ignore = false;
	int i, n_to_read, n_to_write;

	assert(c->pid == pid);
	if (c->input_fds > 0 && c->output_fds > 0)
		return 0;

	c->input_fds = 0;
	c->output_fds = 0;

	if (multiple_inputs) {		// gather
		for (i = STDIN_FILENO; i < nfd; i == STDIN_FILENO ? i = FREE_FILENO : i++) {
			n_to_read = get_provided_fds_n(mb, pi[i].pid);
			DPRINTF("%s(): fds to read for p[%d].pid %d: %d",
				__func__, i, pi[i].pid, n_to_read);
			if (n_to_read == -1) {
				c->input_fds = -1;
				break;
			} else
				c->input_fds += n_to_read;
		}

		c->output_fds = get_expected_fds_n(mb, pi[STDOUT_FILENO].pid);
		/* If we know how many fds to read in the gather end, then
		 * we have to write equal number of fds.
		 * We have to make this estimation in cases of conc-to-conc
		 * communication.
		 */
		if (c->input_fds >= 0 && (c->output_fds == -1 ||
					c->output_fds != c->input_fds))
			c->output_fds = c->input_fds;
		DPRINTF("%s(): fds to write: %d", __func__, c->output_fds);
	} else {		// scatter
		for (i = STDOUT_FILENO; i != STDIN_FILENO; i = next_fd(i, &ignore)) {
			n_to_write = get_expected_fds_n(mb, pi[i].pid);
			DPRINTF("%s(): fds to write for p[%d].pid %d: %d",
				__func__, i, pi[i].pid, n_to_write);
			if (n_to_write == -1) {
				c->output_fds = -1;
				break;
			} else
				c->output_fds += n_to_write;
		}

		c->input_fds = get_provided_fds_n(mb, pi[STDIN_FILENO].pid);
		/* If we know how many fds to write in the scatter end, then
		 * we have to read equal number of fds.
		 * We have to make this estimation in cases of conc-to-conc
		 * communication.
		 */
		if (c->output_fds >= 0 && (c->input_fds == -1 ||
					c->input_fds != c->output_fds))
			c->input_fds = c->output_fds;
		DPRINTF("%s(): fds to read: %d", __func__, c->input_fds);
	}
	return 0;
}

/**
 * Register current concentrator to message block's
 * concentrator array
 */
STATIC int
set_io_channels(struct sgsh_negotiation *mb)
{
	bool exists = false;
	struct sgsh_conc *c;
	if (!mb->conc_array) {
		mb->conc_array = (struct sgsh_conc *)malloc(sizeof(struct sgsh_conc));
		mb->n_concs = 1;
	} else {
		if (!(c = find_conc(mb, pid))) {
			mb->n_concs++;
			mb->conc_array = (struct sgsh_conc *)realloc(mb->conc_array,
					sizeof(struct sgsh_conc) * mb->n_concs);
		} else
			exists = true;
	}
	if (!exists) {
		int i = mb->n_concs - 1;
		c = &mb->conc_array[i];
		c->pid = pid;
		c->input_fds = -1;
		c->output_fds = -1;
		DPRINTF("%s(): Added conc with pid: %d, now n_concs: %d",
				__func__, c->pid, mb->n_concs);
	}
	return set_io(mb, c);
}


STATIC void
print_state(int i, int var, int pcase)
{
	switch (pcase) {
		case 1:
			DPRINTF("%s(): pi[%d].pid: %d", 
					__func__, i, (int)pi[i].pid);
			DPRINTF("  initiator pid: %d",
					var);
			DPRINTF("  pi[%d].lowest_pid %d, pi[%d].seen: %d",
					i, (int)pi[i].lowest_pid, i, pi[i].seen);
			DPRINTF("  write: %d", pi[i].written);
		case 2:
			DPRINTF("%s(): pi[%d].pid: %d",
					__func__, i, pi[i].pid);
			DPRINTF("  run ready?: %d, seen times: %d",
					(int)pi[i].run_ready, pi[i].seen);
			DPRINTF("  written: %d, nfds: %d",
					pi[i].written, var);
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
	int ofd = -1;		/* ... origin fd direction */
	int oinit = -1;		/* initiator pid of origin */
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
					pi[next_fd(i, &ro)].to_write == NULL &&
					!pi[i].seen) {
				FD_SET(i, &readfds);
				nfds = max(i + 1, nfds);
			}
			if (pi[i].to_write && !pi[i].written) {
				FD_SET(i, &writefds);
				nfds = max(i + 1, nfds);
				pi[i].to_write->is_origin_conc = true;
				pi[i].to_write->conc_pid = pid;
				DPRINTF("Origin: conc with pid %d", pid);
				DPRINTF("**fd i: %d set for writing", i);
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
				if (chosen_mb->state == PS_NEGOTIATION)
					chosen_mb->preceding_process_pid = pid;
				write_message_block(i); // XXX check return

				if (pi[i].to_write->state == PS_RUN)
					pi[i].written = true;

				// Write side exit
				if (is_ready(i, pi[i].to_write)) {
					solution = pi[i].to_write;
					pi[i].run_ready = true;
					DPRINTF("**%s(): pi[%d] is run ready",
							__func__, i);
				}
				pi[i].to_write = NULL;
			}
			if (FD_ISSET(i, &readfds)) {
				struct sgsh_negotiation *rb;
				ro = false;
				int next = next_fd(i, &ro);

				assert(!pi[i].run_ready);
				assert(pi[next].to_write == NULL);
				read_message_block(i, &pi[next].to_write); // XXX check return
				rb = pi[next].to_write;
				DPRINTF("%s(): next write via fd %d to pid %d",
						__func__, next, pi[next].pid);

				if (oi == -1 || rb->initiator_pid < oinit) {
					if ((multiple_inputs && i == 1) ||
							(!multiple_inputs &&
							 i == 0)) {
						oi = rb->origin_index;
						ofd = rb->origin_fd_direction;
						oinit = rb->initiator_pid;
						DPRINTF("**Store origin: %d, fd: %s, initiator: %d",
							oi, ofd ? "stdout" : "stdin", oinit);
					} else {
						DPRINTF("**Reset origin from %d",
								oinit);
						oi = -1;
						ofd = -1;
						oinit = -1;
					}
				}

				/* If conc talks to conc, set conc's pid
				 * Required in order to allocate fds correctly
				 * in the end
				 */
				if (rb->is_origin_conc)
					pi[i].pid = rb->conc_pid;
				else
					pi[i].pid = get_origin_pid(rb);

				/* If needed, re-set origin.
				 * Don't move this block before get_origin_pid()
				 */
				if (ro) {
					DPRINTF("**Restore origin: %d, fd: %s",
							oi, ofd ? "stdout" : "stdin");
					pi[next].to_write->origin_index = oi;
					pi[next].to_write->origin_fd_direction = ofd;
				}

				if (pi[i].lowest_pid == 0 ||
						rb->initiator_pid < pi[i].lowest_pid)
					pi[i].lowest_pid = rb->initiator_pid;

				/* Count # times we see a block on a port
				 * after a solution has been found
				 */
				if (rb->state == PS_NEGOTIATION)
					continue;
				/* Set a conc's required/provided IO in mb */
				set_io_channels(pi[next].to_write);
				if (rb->initiator_pid == pi[i].lowest_pid)
					pi[i].seen = true;

				print_state(i, (int)rb->initiator_pid, 1);
				if (pi[i].seen && pi[i].written) {
					solution = pi[next].to_write;
					pi[i].run_ready = true;
					DPRINTF("**%s(): pi[%d] is run ready",
							__func__, i);
				}
			}
		}

		// See if all processes are run-ready
		nfds = 0;
		for (i = 0; i < nfd; i++) {
			if (pi[i].run_ready)
				nfds++;
			print_state(i, nfds, 2);
		}
		if (nfds == nfd - 1) {
			assert(solution != NULL);
			DPRINTF("%s(): conc leaves negotiation", __func__);
			return solution;
		} else if (nfds == nfd - 2 &&
				solution->preceding_process_pid == pid) {
			for (i = 0; i < nfd; i++) {
				if (!pi[i].run_ready && pi[i].seen) {
					DPRINTF("conc is the preceding process");
					DPRINTF("No write to pi[%d].pid %d",
							i, pi[i].pid);
					assert(solution != NULL);
					DPRINTF("%s(): conc leaves negotiation", __func__);
					return solution;
				}
			}
		} else if (chosen_mb != NULL) {
			free_mb(chosen_mb);
			chosen_mb = NULL;
		}
	}
}



/*
 * Scatter the fds read from the input process to multiple outputs.
 */
STATIC void
scatter_input_fds(struct sgsh_negotiation *mb)
{
	struct sgsh_conc *this_conc = find_conc(mb, pid);
	if (!this_conc) {
		printf("%s(): Concentrator with pid %d not registered",
				__func__, pid);
		exit(1);	// XXX
	}
	int n_to_read = this_conc->input_fds;
	int *read_fds = (int *)malloc(n_to_read * sizeof(int));
	int i, j, write_index = 0;
	bool ignore = false;
	DPRINTF("%s(): fds to read: %d", __func__, n_to_read);

	for (i = 0; i < n_to_read; i++)
		read_fds[i] = read_fd(STDIN_FILENO);

	for (i = STDOUT_FILENO; i != STDIN_FILENO; i = next_fd(i, &ignore)) {
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
	struct sgsh_conc *this_conc = find_conc(mb, pid);
	if (!this_conc) {
		printf("%s(): Concentrator with pid %d not registered",
				__func__, pid);
		exit(1);	// XXX
	}
	int n_to_write = this_conc->output_fds;
	int *read_fds = (int *)malloc(n_to_write * sizeof(int));
	int i, j, read_index;
	DPRINTF("%s(): fds to write: %d", __func__, n_to_write);

	read_index = 0;
	for (i = STDIN_FILENO; i < nfd; i == STDIN_FILENO ? i = FREE_FILENO : i++) {
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

#ifndef UNIT_TESTING

int
main(int argc, char *argv[])
{
	int ch;

	program_name = argv[0];
	pid = getpid();
	pass_origin = false;

	while ((ch = getopt(argc, argv, "ior")) != -1) {
		switch (ch) {
		case 'i':
			multiple_inputs = true;
			break;
		case 'o':
			multiple_inputs = false;
			break;
		case 'r':
			if (multiple_inputs)
				pass_origin = true;
			else
				usage();
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

	chosen_mb = NULL;
	chosen_mb = pass_message_blocks();
	if (multiple_inputs)
		gather_input_fds(chosen_mb);
	else
		scatter_input_fds(chosen_mb);
	free_mb(chosen_mb);
	free(pi);
	return 0;
}

#endif

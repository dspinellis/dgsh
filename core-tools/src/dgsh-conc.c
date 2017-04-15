/*
 * Copyright 2016 Diomidis Spinellis
 *
 * A passive component that aids the dgsh negotiation by passing
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
#include <unistd.h>		/* getpid(), alarm() */
#include <sys/select.h>
#include <signal.h>		/* sig_atomic_t */

#include "negotiate.h"		/* read/write_message_block(),
				   set_negotiation_complete() */
#include "dgsh-debug.h"		/* DPRINTF */

/* Alarm mechanism and on_exit handling */
extern volatile sig_atomic_t negotiation_completed;

#ifdef TIME
#include <time.h>
static struct timespec tstart={0,0}, tend={0,0};
#endif

static const char *program_name;
static pid_t pid;

static void
usage(void)
{
	fprintf(stderr, "Usage: %s -i|-o [-n] nprog\n"
		"-i"		"\tInput concentrator: multiple inputs to single output\n"
		"-o"		"\tOutput concentrator: single input to multiple outputs\n"
		"-n"		"\tDo not consider standard input (used with -o)\n",
		program_name);
	exit(1);
}

/*
 * Information for each I/O file descriptor on which
 * the concentrator operates.
 */
static struct portinfo {
	pid_t pid;		// The id of the process talking to this port
	bool seen;		// True when the pid was seen
	bool written;		// True when we wrote to pid
	bool run_ready;		// True when the associated process can run
	struct dgsh_negotiation *to_write; // Block pending a write
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
//STATIC bool pass_origin;
STATIC bool noinput;

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
			if (!noinput)
				return STDOUT_FILENO;
		case STDOUT_FILENO:
			if (!noinput)
				*ro = true;
			if (nfd > 2)	// if ==2, treat in default case
				return FREE_FILENO;
		default:
			if (fd == nfd - 1)
				if (!noinput)
					return STDIN_FILENO;
				else
					return STDOUT_FILENO;
			else {
				if (!noinput)
					*ro = true;
				return fd + 1;
			}
		}
}

/**
 * Return whether the process at port i
 * is ready to run.
 * Check whether this is the process whose
 * pid is set and prepare for exit.
 */
STATIC bool
is_ready(int i, struct dgsh_negotiation *mb)
{
	bool ready = false;
	if (pi[i].seen && pi[i].written)
		ready = true;
	DPRINTF(4, "pi[%d].pid: %d %s?: %d\n",
			i, pi[i].pid, __func__, ready);
	return ready;
}

/**
 * Register current concentrator to message block's
 * concentrator array
 */
STATIC int
set_io_channels(struct dgsh_negotiation *mb)
{
	if (find_conc(mb, pid))
		return 0;

	struct dgsh_conc c;
	c.pid = pid;
	c.input_fds = -1;
	c.output_fds = -1;
	c.n_proc_pids = (nfd > 2 ? nfd - 2 : 1);
	c.multiple_inputs = multiple_inputs;
	c.proc_pids = (int *)malloc(sizeof(int) * c.n_proc_pids);
	int j = 0, i;

	DPRINTF(4, "%s: n_proc_pids: %d", __func__, c.n_proc_pids);
	if (multiple_inputs) {
		c.endpoint_pid = pi[STDOUT_FILENO].pid;
		if (c.endpoint_pid == 0)
			return 1;
		for (i = STDIN_FILENO; i < nfd; i == STDIN_FILENO ? i = FREE_FILENO : i++)
			if (pi[i].pid == 0) {
				free(c.proc_pids);
				return 1;
			} else
				c.proc_pids[j++] = pi[i].pid;
	} else {
		bool ignore;
		c.endpoint_pid = pi[STDIN_FILENO].pid;
		if (c.endpoint_pid == 0)
			return 1;
		for (i = STDOUT_FILENO; i != STDIN_FILENO; i = next_fd(i, &ignore))
			if (pi[i].pid == 0) {
				free(c.proc_pids);
				return 1;
			} else
				c.proc_pids[j++] = pi[i].pid;
	}

	if (!mb->conc_array) {
		mb->conc_array = (struct dgsh_conc *)malloc(sizeof(struct dgsh_conc));
		mb->n_concs = 1;
	} else {
		mb->n_concs++;
		mb->conc_array = (struct dgsh_conc *)realloc(mb->conc_array,
					sizeof(struct dgsh_conc) * mb->n_concs);
	}
	memcpy(&mb->conc_array[mb->n_concs - 1], &c, sizeof(struct dgsh_conc));

	DPRINTF(4, "%s(): Added conc with pid: %d, now n_concs: %d",
			__func__, mb->conc_array[mb->n_concs - 1].pid, mb->n_concs);

	return 0;
}

STATIC void
print_state(int i, int var, int pcase)
{
	switch (pcase) {
		case 1:
			DPRINTF(4, "%s(): pi[%d].pid: %d",
					__func__, i, (int)pi[i].pid);
			DPRINTF(4, "  initiator pid: %d",
					var);
			DPRINTF(4, "  pi[%d].seen: %d",
					i, pi[i].seen);
			DPRINTF(4, "  write: %d", pi[i].written);
		case 2:
			DPRINTF(4, "%s(): pi[%d].pid: %d",
					__func__, i, pi[i].pid);
			DPRINTF(4, "  run ready?: %d, seen times: %d",
					(int)pi[i].run_ready, pi[i].seen);
			DPRINTF(4, "  written: %d, nfds: %d",
					pi[i].written, var);
	}
}

#define max(a, b) ((a) > (b) ? (a) : (b))

/*
 * Pass around the message blocks so that they reach all processes
 * connected through the concentrator.
 */
STATIC int
pass_message_blocks(void)
{
	fd_set readfds, writefds;
	int nfds = 0;
	int i;
	int oi = -1;		/* scatter/gather block's origin index */
	int ofd = -1;		/* ... origin fd direction */
	bool ro = false;	/* Whether the read block's origin should
				 * be restored
				 */
	bool iswrite = false;

	if (noinput) {
#ifdef TIME
		clock_gettime(CLOCK_MONOTONIC, &tstart);
#endif
		construct_message_block("dgsh-conc", pid);
		chosen_mb->origin_fd_direction = STDOUT_FILENO;
		chosen_mb->is_origin_conc = true;
		chosen_mb->conc_pid = pid;
		pi[STDOUT_FILENO].to_write = chosen_mb;
	}

	for (;;) {
		// Create select(2) masks
		FD_ZERO(&readfds);
		FD_ZERO(&writefds);
		for (i = 0; i < nfd; i++) {
			if (i == STDERR_FILENO)
				continue;
			if (!pi[i].seen) {
				FD_SET(i, &readfds);
				nfds = max(i + 1, nfds);
			}
			if (pi[i].to_write && !pi[i].written) {
				FD_SET(i, &writefds);
				nfds = max(i + 1, nfds);
				pi[i].to_write->is_origin_conc = true;
				pi[i].to_write->conc_pid = pid;
				DPRINTF(4, "Origin: conc with pid %d", pid);
				DPRINTF(4, "**fd i: %d set for writing", i);
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
				iswrite = true;
				assert(pi[i].to_write);
				chosen_mb = pi[i].to_write;
				write_message_block(i); // XXX check return

				if (pi[i].to_write->state == PS_RUN ||
					(pi[i].to_write->state == PS_DRAW_EXIT &&
						pi[i].to_write->is_draw_exit_confirmed) ||
					(pi[i].to_write->state == PS_ERROR &&
						pi[i].to_write->is_error_confirmed))
					pi[i].written = true;

				// Write side exit
				if (is_ready(i, pi[i].to_write)) {
					pi[i].run_ready = true;
					DPRINTF(4, "**%s(): pi[%d] is run ready",
							__func__, i);
				}
				pi[i].to_write = NULL;
			}
			if (FD_ISSET(i, &readfds)) {
				struct dgsh_negotiation *rb;
				ro = false;
				int next = next_fd(i, &ro);

				assert(!pi[i].run_ready);
				assert(pi[next].to_write == NULL);
				read_message_block(i, &pi[next].to_write); // XXX check return
				rb = pi[next].to_write;

				DPRINTF(4, "%s(): next write via fd %d to pid %d",
						__func__, next, pi[next].pid);

				if (oi == -1) {
					if ((multiple_inputs && i == 1) ||
							(!multiple_inputs &&
							 i == 0)) {
						oi = rb->origin_index;
						ofd = rb->origin_fd_direction;
						DPRINTF(4, "**Store origin: %d, fd: %s",
							oi, ofd ? "stdout" : "stdin");
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
					DPRINTF(4, "**Restore origin: %d, fd: %s",
							oi, ofd ? "stdout" : "stdin");
					pi[next].to_write->origin_index = oi;
					pi[next].to_write->origin_fd_direction = ofd;
				} else if (noinput) {
					pi[next].to_write->origin_index = -1;
					pi[next].to_write->origin_fd_direction = STDOUT_FILENO;
				}

				/* Set a conc's required/provided IO in mb */
				if (!noinput)
					set_io_channels(pi[next].to_write);

				if (rb->state == PS_NEGOTIATION &&
						noinput) {
					int j, seen = 0;
					pi[i].seen = true;
					for (j = 1; j < nfd; j++)
						if (pi[j].seen)
							seen++;
					if ((nfd > 2 && seen == nfd - 2) ||
							seen == nfd - 1) {
						chosen_mb = rb;
						DPRINTF(1, "%s(): Gathered I/O requirements.", __func__);
						int state = solve_dgsh_graph();
						if (state == OP_ERROR) {
							pi[next].to_write->state = PS_ERROR;
							pi[next].to_write->is_error_confirmed = true;
						} else if (state == OP_DRAW_EXIT) {
							pi[next].to_write->state = PS_DRAW_EXIT;
							pi[next].to_write->is_draw_exit_confirmed = true;
						} else {
							DPRINTF(1, "%s(): Computed solution", __func__);
							pi[next].to_write->state = PS_RUN;
						}
						for (j = 1; j < nfd; j++)
							pi[j].seen = false;
						// Don't free
						chosen_mb = NULL;
					}
				} else if (rb->state == PS_RUN ||
						(rb->state == PS_DRAW_EXIT &&
						rb->is_draw_exit_confirmed) ||
						(rb->state == PS_ERROR &&
						rb->is_error_confirmed))
					pi[i].seen = true;
				else if (rb->state == PS_ERROR)
					rb->is_error_confirmed = true;
				else if (rb->state == PS_DRAW_EXIT)
					rb->is_draw_exit_confirmed = true;

				print_state(i, (int)rb->initiator_pid, 1);
				if (pi[i].seen && pi[i].written) {
					chosen_mb = pi[next].to_write;
					pi[i].run_ready = true;
					DPRINTF(4, "**%s(): pi[%d] is run ready",
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
		if ((nfd > 2 && (nfds == nfd - 1 ||
					(noinput && nfds == nfd - 2))) ||
		    (nfds == nfd || (noinput && nfds == nfd - 1))) {
			assert(chosen_mb != NULL);
			DPRINTF(4, "%s(): conc leaves negotiation", __func__);
			return chosen_mb->state;
		} else if (chosen_mb != NULL &&	iswrite) { // Free if we have written
			DPRINTF(4, "chosen_mb: %lx, i: %d, next: %d, pi[next].to_write: %lx\n",
				(long)chosen_mb, i, next_fd(i, &ro), (long)pi[next_fd(i, &ro)].to_write);
			free_mb(chosen_mb);
			chosen_mb = NULL;
			iswrite = false;
		}
	}
}



/*
 * Scatter the fds read from the input process to multiple outputs.
 */
STATIC void
scatter_input_fds(struct dgsh_negotiation *mb)
{
	struct dgsh_conc *this_conc = find_conc(mb, pid);
	if (!this_conc) {
		printf("%s(): Concentrator with pid %d not registered",
				__func__, pid);
		exit(1);	// XXX
	}
	int n_to_read = this_conc->input_fds;
	int *read_fds = (int *)malloc(n_to_read * sizeof(int));
	int i, j, write_index = 0;
	bool ignore = false;
	DPRINTF(4, "%s(): fds to read: %d", __func__, n_to_read);

	for (i = 0; i < n_to_read; i++)
		read_fds[i] = read_fd(STDIN_FILENO);

	for (i = STDOUT_FILENO; i != STDIN_FILENO; i = next_fd(i, &ignore)) {
		int n_to_write = get_expected_fds_n(mb, pi[i].pid);
		DPRINTF(4, "%s(): fds to write for p[%d].pid %d: %d",
				__func__, i, pi[i].pid, n_to_write);
		for (j = write_index; j < write_index + n_to_write; j++) {
			write_fd(i, read_fds[j]);
			DPRINTF(4, "%s(): Write fd: %d to output channel: %d",
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
gather_input_fds(struct dgsh_negotiation *mb)
{
	struct dgsh_conc *this_conc = find_conc(mb, pid);
	if (!this_conc) {
		printf("%s(): Concentrator with pid %d not registered",
				__func__, pid);
		exit(1);	// XXX
	}
	int n_to_write = this_conc->output_fds;
	int *read_fds = (int *)malloc(n_to_write * sizeof(int));
	int i, j, read_index;
	DPRINTF(4, "%s(): fds to write: %d", __func__, n_to_write);

	read_index = 0;
	for (i = STDIN_FILENO; i < nfd; i == STDIN_FILENO ? i = FREE_FILENO : i++) {
		int n_to_read = get_provided_fds_n(mb, pi[i].pid);
		DPRINTF(4, "%s(): fds to read for p[%d].pid %d: %d",
				__func__, i, pi[i].pid, n_to_read);
		for (j = read_index; j < read_index + n_to_read; j++) {
			read_fds[j] = read_fd(i);
			DPRINTF(4, "%s(): Read fd: %d from input channel: %d",
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
	int exit;
	char *debug_level = NULL;

	program_name = argv[0];
	pid = getpid();
	noinput = false;

	while ((ch = getopt(argc, argv, "ion")) != -1) {
		switch (ch) {
		case 'i':
			multiple_inputs = true;
			break;
		case 'o':
			multiple_inputs = false;
			break;
		case 'n':	// special output conc that takes no input
			if (!multiple_inputs)
				noinput = true;
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

	debug_level = getenv("DGSH_DEBUG_LEVEL");
	if (debug_level != NULL)
		dgsh_debug_level = atoi(debug_level);

	signal(SIGALRM, dgsh_alarm_handler);
	alarm(5);

	/* +1 for stdin when scatter/stdout when gather
	 * +1 for stderr which is not used
	 */
	if (atoi(argv[0]) == 1)
		nfd = 2;
	else
		nfd = atoi(argv[0]) + 2;
	pi = (struct portinfo *)calloc(nfd, sizeof(struct portinfo));

	chosen_mb = NULL;
	exit = pass_message_blocks();
	if (exit == PS_RUN) {
		if (noinput)
			DPRINTF(1, "%s(): Communicated the solution", __func__);
		if (multiple_inputs)
			gather_input_fds(chosen_mb);
		else if (!noinput)	// Output noinput conc has no job here
			scatter_input_fds(chosen_mb);
		exit = PS_COMPLETE;
	}
	free_mb(chosen_mb);
	free(pi);
	DPRINTF(3, "conc with pid %d terminates %s",
		pid, exit == PS_COMPLETE ? "normally" : "with error");
#ifdef DEBUG
	fflush(stderr);
#endif
#ifdef TIME
	if (noinput) {
		clock_gettime(CLOCK_MONOTONIC, &tend);
		fprintf(stderr, "The dgsh negotiation procedure took about %.5f seconds\n",
			((double)tend.tv_sec + 1.0e-9*tend.tv_nsec) -
			((double)tstart.tv_sec + 1.0e-9*tstart.tv_nsec));
		fflush(stderr);

	}
#endif
	set_negotiation_complete();
	alarm(0);			// Cancel alarm
	signal(SIGALRM, SIG_IGN);	// Do not handle the signal
	return exit;
}

#endif

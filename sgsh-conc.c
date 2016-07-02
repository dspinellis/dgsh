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
 */
#define FREE_FILENO (STDERR_FILENO + 1)
STATIC int
next_fd(int fd)
{
	if (multiple_inputs)
		switch (fd) {
		case STDIN_FILENO:
			return STDOUT_FILENO;
		case STDOUT_FILENO:
			return nfd - 1;
		case FREE_FILENO:
			return STDIN_FILENO;
		default:
			return fd - 1;
		}
	else
		switch (fd) {
		case STDIN_FILENO:
			return STDOUT_FILENO;
		case STDOUT_FILENO:
			return FREE_FILENO;
		default:
			if (fd == nfd - 1)
				return STDIN_FILENO;
			else
				return fd + 1;
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

	for (;;) {
		// Create select(2) masks
		FD_ZERO(&readfds);
		FD_ZERO(&writefds);
		for (i = 0; i < nfd; i++) {
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
			err(1, "select");
		}

		// Read/write what we can
		for (i = 0; i < nfd; i++) {
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
				read_message_block(i, &pi[next].to_write); // XXX check return
				rb = pi[next].to_write;

				pi[i].pid = get_origin_pid(rb);
				// Count # times we see a block on a port
				if (rb->initiator_pid < pi[i].lowest_pid) {
					pi[i].lowest_pid = rb->initiator_pid;
					pi[i].seen_times = 1;
				} else if (rb->initiator_pid == pi[i].lowest_pid)
					pi[i].seen_times++;

				if (pi[i].seen_times == 2) {
					solution = pi[next].to_write;
					pi[i].run_ready = true;
				}
			}
		}

		// See if all processes are run-ready
		nfds = 0;
		for (i = 0; i < nfd; i++)
			if (pi[i].run_ready)
				nfds++;
		if (nfds == nfd - 1) {
			assert(solution != NULL);
			return solution;
		}
	}
}

/*
 * Write the file descriptor fd_to_write to
 * the socket file descriptor output_socket.
 */
STATIC void
write_fd(int output_socket, int fd_to_write)
{
	struct msghdr    msg;
	struct cmsghdr  *cmsg;
	unsigned char    buf[CMSG_SPACE(sizeof(int))];

	memset(&msg, 0, sizeof(msg));
	msg.msg_control = buf;
	msg.msg_controllen = CMSG_LEN(sizeof(int));

	cmsg = CMSG_FIRSTHDR(&msg);
	cmsg->cmsg_len = CMSG_LEN(sizeof(int));
	cmsg->cmsg_level = SOL_SOCKET;
	cmsg->cmsg_type = SCM_RIGHTS;
	*(int *)CMSG_DATA(cmsg) = fd_to_write;

	if (sendmsg(output_socket, &msg, 0) == -1)
		err(1, "sendmsg on fd %d", output_socket);
}

/*
 * Read a file descriptor from socket input_socket and return it.
 */
STATIC int
read_fd(int input_socket)
{
	struct msghdr    msg;
	struct cmsghdr  *cmsg;
	unsigned char    buf[CMSG_SPACE(sizeof(int))];

	memset(&msg, 0, sizeof(msg));
	msg.msg_control = buf;
	msg.msg_controllen = sizeof(buf);

	if (recvmsg(input_socket, &msg, 0) == -1)
		err(1, "recvmsg on fd %d", input_socket);
	if ((msg.msg_flags & MSG_TRUNC) || (msg.msg_flags & MSG_CTRUNC))
		errx(1, "control message truncated on fd %d", input_socket);
	for (cmsg = CMSG_FIRSTHDR(&msg); cmsg != NULL;
	    cmsg = CMSG_NXTHDR(&msg, cmsg)) {
		if (cmsg->cmsg_len == CMSG_LEN(sizeof(int)) &&
		    cmsg->cmsg_level == SOL_SOCKET &&
		    cmsg->cmsg_type == SCM_RIGHTS)
			return *(int *)CMSG_DATA(cmsg);
	}
	errx(1, "unable to read file descriptor from fd %d", input_socket);
}

/*
 * Scatter the fds read from the input process to multiple outputs.
 */
STATIC void
scatter_input_fds(void)
{
	int n_to_read = get_provided_fds_n(pi[STDIN_FILENO].pid);
	int *read_fds = (int *)malloc(n_to_read * sizeof(int));
	int i, j, write_index;

	for (i = 0; i < n_to_read; i++)
		read_fds[i] = read_fd(STDIN_FILENO);

	write_index = 0;
	for (i = STDOUT_FILENO; i < nfd; i = next_fd(i)) {
		int n_to_write = get_expected_fds_n(pi[i].pid);
		for (j = write_index; j < write_index + n_to_write; j++)
			write_fd(i, read_fds[j]);
		write_index += n_to_write;
	}
	assert(write_index == n_to_read);
}

/*
 * Gather the fds read from input processes to a single output.
 */
STATIC void
gather_input_fds(void)
{
	int n_to_write = get_expected_fds_n(pi[STDOUT_FILENO].pid);
	int *read_fds = (int *)malloc(n_to_write * sizeof(int));
	int i, j, read_index;

	read_index = 0;
	for (i = nfd - 1; i != STDOUT_FILENO; i = next_fd(i)) {
		int n_to_read = get_provided_fds_n(pi[i].pid);
		for (j = read_index; j < read_index + n_to_read; j++)
			read_fds[j] = read_fd(i);
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

	nfd = atoi(argv[0]) + 1;
	pi = (struct portinfo *)calloc(nfd, sizeof(struct portinfo));

	chosen_mb = pass_message_blocks();
	if (multiple_inputs)
		gather_input_fds();
	else
		scatter_input_fds();

	return 0;
}

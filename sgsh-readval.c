/*
 * Copyright 2013 Diomidis Spinellis
 *
 * Communicate with the data store specified as a Unix-domain socket.
 * By default the command will read a value.
 * Calling it with the -q flag will send the data store a termination
 * command.
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

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <sys/un.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <err.h>
#include <errno.h>
#include <limits.h>
#include <string.h>
#include <unistd.h>

#include "sgsh.h"

static bool retry_connection = true;

static const char *program_name;

static void
usage(void)
{
	fprintf(stderr, "Usage: %s [-c|l] [-n] [-q] -s path\n"
		"-c"		"\tRead the current value from the store\n"
		"-l"		"\tRead the last (before EOF) value from the store (default)\n"
		"-n"		"\tDo not retry failed connection to write store\n"
		"-q"		"\tAsk the write-end to quit\n"
		"-s path"	"\tSpecify the socket to connect to\n",
		program_name);
	exit(1);
}

/* Write a command to the specified socket, and return the socket */
static int
write_command(const char *name, char cmd)
{
	int s;
	socklen_t len;
	struct sockaddr_un remote;

	if ((s = socket(AF_UNIX, SOCK_STREAM, 0)) == -1)
		err(1, "socket");

	DPRINTF("Connecting to %s", name);

	remote.sun_family = AF_UNIX;
	if (strlen(name) >= sizeof(remote.sun_path) - 1)
		errx(6, "Socket name [%s] must be shorter than %d characters",
			name, (int)sizeof(remote.sun_path));
	strcpy(remote.sun_path, name);
	len = strlen(remote.sun_path) + 1 + sizeof(remote.sun_family);
again:
	if (connect(s, (struct sockaddr *)&remote, len) == -1) {
		if (retry_connection && errno == ENOENT) {
			DPRINTF("Retrying connection setup");
			sleep(1);
			goto again;
		}
		err(2, "connect %s", name);
	}
	DPRINTF("Connected");

	if (write(s, &cmd, 1) == -1)
		err(3, "write");
	DPRINTF("Wrote command");
	return s;
}

int
main(int argc, char *argv[])
{
	int s, ch, n;
	char buff[PIPE_BUF];
	int content_length;
	char cbuff[CONTENT_LENGTH_DIGITS + 2];
	struct iovec iov[2];
	int quit = 0;
	char cmd = 0;
	const char *socket_path = NULL;

	program_name = argv[0];

	/* Default if nothing else is specified */
	if (argc == 3)
		cmd = 'L';

	while ((ch = getopt(argc, argv, "clnqs:")) != -1) {
		switch (ch) {
		case 'c':	/* Read current value */
			cmd = 'C';
			break;
		case 'l':	/* Read last value */
			cmd = 'L';
			break;
		case 'n':
			retry_connection = false;
			break;
		case 'q':
			quit = 1;
			break;
		case 's':
			socket_path = optarg;
			break;
		case '?':
		default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;

	if (argc != 0 || socket_path == NULL)
		usage();

	switch (cmd) {
	case 0:		/* No I/O specified */
		break;
	case 'C':	/* Read current value */
	case 'L':	/* Read last value */
		s = write_command(socket_path, cmd);

		/* Read content length and some data */
		iov[0].iov_base = cbuff;
		iov[0].iov_len = CONTENT_LENGTH_DIGITS;
		iov[1].iov_base = buff;
		iov[1].iov_len = sizeof(buff);
		if ((n = readv(s, iov, 2)) == -1)
			err(5, "readv");
		cbuff[CONTENT_LENGTH_DIGITS] = 0;
		if (sscanf(cbuff, "%u", &content_length) != 1) {
			fprintf(stderr, "Unable to read content length from string [%s]\n", cbuff);
			exit(1);
		}
		DPRINTF("Content length is %u", content_length);
		n -= CONTENT_LENGTH_DIGITS;
		if (write(STDOUT_FILENO, buff, n) == -1)
			err(4, "write");
		content_length -= n;

		/* Read rest of data */
		while (content_length > 0) {
			if ((n = read(s, buff, sizeof(buff))) == -1)
				err(5, "read");
			DPRINTF("Read %d bytes", n);
			if (write(STDOUT_FILENO, buff, n) == -1)
				err(4, "write");
			content_length -= n;
		}
		break;
	}

	if (quit)
		(void)write_command(socket_path, 'Q');

	return 0;
}

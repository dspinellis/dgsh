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
#include <stdio.h>
#include <stdlib.h>
#include <err.h>
#include <errno.h>
#include <limits.h>
#include <string.h>
#include <unistd.h>

#define CONTENT_LENGTH_DIGITS 10

#ifdef DEBUG
/* ## is a gcc extension that removes trailing comma if no args */
#define DPRINTF(fmt, ...) fprintf(stderr, fmt "\n", ##__VA_ARGS__)
#else
#define DPRINTF(fmt, ...)
#endif


static void
usage(const char *name)
{
	fprintf(stderr, "Usage: %s [-q] socket_name\n", name);
	exit(1);
}

int
main(int argc, char *argv[])
{
	int s, len, ch, n;
	struct sockaddr_un remote;
	char cmd = 'R';
	char buff[PIPE_BUF];
	int content_length;
	char cbuff[CONTENT_LENGTH_DIGITS + 2];
	struct iovec iov[2];

	while ((ch = getopt(argc, argv, "q")) != -1) {
		switch (ch) {
		case 'q':
			cmd = 'Q';
			break;
		case '?':
		default:
			usage(argv[0]);
		}
	}
	argc -= optind;
	argv += optind;

	if ((s = socket(AF_UNIX, SOCK_STREAM, 0)) == -1)
		err(1, "socket");

	DPRINTF("Connecting to %s", argv[0]);

	remote.sun_family = AF_UNIX;
	strcpy(remote.sun_path, argv[0]);
	len = strlen(remote.sun_path) + sizeof(remote.sun_family);
	if (connect(s, (struct sockaddr *)&remote, len) == -1)
		err(2, "connect");

	DPRINTF("Connected");

	if (write(s, &cmd, 1) == -1)
		err(3, "write");
	DPRINTF("Wrote command");

	if (cmd == 'R') {
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
	}
	while (content_length > 0) {
		if ((n = read(s, buff, sizeof(buff))) == -1)
			err(5, "read");
		DPRINTF("Read %d bytes", n);
		if (write(STDOUT_FILENO, buff, n) == -1)
			err(4, "write");
		content_length -= n;
	}
	return 0;
}

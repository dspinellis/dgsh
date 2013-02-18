/*
 * Copyright 2013 Diomidis Spinellis
 *
 * Write the latest single value read from the standard input to the specified
 * fifo every time someone tries to read a value from the fifo.
 * Thus this process acts in effect as a data store: it reads a series of
 * values (think of them as assignements) and provides a way to read the
 * store's current value (from the fifo).
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
#include <sys/stat.h>
#include <sys/select.h>
#include <assert.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <unistd.h>

int
main(int argc, char *argv[])
{
	int fd, nr, nw;
	char buff[PIPE_BUF];
	int reached_eof;

	if (argc != 2) {
		fprintf(stderr, "Usage: %s file\n", argv[0]);
		return 1;
	}

	fprintf(stderr, "Calling open\n");
	if ((fd = open(argv[1], O_WRONLY)) == -1)
		err(2, "Error opening %s", argv[1]);
	fprintf(stderr, "Open returns\n");

	nr = 0;
	reached_eof = 0;
	for (;;) {
		fd_set source_fds;
		fd_set sink_fds;

		FD_ZERO(&source_fds);
		FD_ZERO(&sink_fds);

		if (!reached_eof)
			FD_SET(STDIN_FILENO, &source_fds);
		if (nr)
			FD_SET(fd, &sink_fds);

		fprintf(stderr, "Calling select\n");
		if (select(fd + 1, &source_fds, &sink_fds, NULL, NULL) < 0)
			err(3, "select");
		fprintf(stderr, "Select returns\n");

		if (FD_ISSET(STDIN_FILENO, &source_fds)) {
			fprintf(stderr, "Input available\n");
			switch (nr = read(STDIN_FILENO, buff, sizeof(buff))) {
			case -1:
				err(4, "Error reading from standard input");
				break;
			case 0:	/* EOF */
				reached_eof = 1;
				break;
			default:
				break;
			}
		}

		if (FD_ISSET(fd, &sink_fds)) {
			fprintf(stderr, "Can write\n");
			switch (nw = write(fd, buff, nr)) {
			default:
				if (nr != nw)
					fprintf(stderr, "Short write (tried %d, wrote %d) writing to %s", nr, nw, argv[1]);
					exit(1);
				break;
			case -1:
				err(5, "Error writing to %s", argv[1]);
				break;
			case 0:
				err(5, "Zero bytes written");
				break;
			}
		}
	}
}

/*
 * Copyright 2013 Diomidis Spinellis
 *
 * Read a single value written on the specified fifo by sgsh-writeval,
 * and display it on standard output.
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
#include <unistd.h>
#include <unistd.h>

int
main(int argc, char *argv[])
{
	int fd, nr, nw, nwritten;
	char buff[PIPE_BUF];

	if (argc != 2) {
		fprintf(stderr, "Usage: %s file\n", argv[0]);
		return 1;
	}

	if ((fd = open(argv[1], O_RDONLY)) == -1)
		err(2, "Error opening %s", argv[1]);
	if ((nr = read(fd, buff, sizeof(buff))) < 0)
		err(3, "Error reading from %s", argv[1]);

	nwritten = 0;
	for (;;) {
		nw = write(STDOUT_FILENO, buff + nwritten, nr - nwritten);
		if (nw == nr)
			return 0;
		else if (nw == -1)
			err(3, "Error writing to standard output", argv[1]);
		else
			nwritten += nw;
	}
}

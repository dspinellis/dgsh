/*
 * Copyright 2013 Diomidis Spinellis
 *
 * Prepend lines read with timestamp, number of lines, number of bytes.
 * Used for providing sgsh monitoring ports.
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
#include <sys/time.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <err.h>
#include <errno.h>
#include <limits.h>
#include <string.h>
#include <unistd.h>

#include "sgsh.h"

static const char *program_name;

static void
usage(void)
{
	fprintf(stderr, "Usage: %s\n", program_name);
	exit(1);
}

int
main(int argc, char *argv[])
{
	int c;
	unsigned long nlines, nbytes;
	struct timeval t;
	bool write_header = true;

	program_name = argv[0];

	/* Default if nothing else is specified */
	if (argc != 1)
		usage();

	nbytes = nlines = 0;

	while ((c = getchar()) != EOF) {
		if (write_header) {
			gettimeofday(&t, NULL);
			printf("%lld.%06d %lu %lu ",
				(long long)t.tv_sec,
				(int)t.tv_usec,
				nlines,
				nbytes);
			write_header = false;
		}
		putchar(c);
		nbytes++;
		if (c == '\n') {
			nlines++;
			write_header = true;
		}
	}

	return 0;
}

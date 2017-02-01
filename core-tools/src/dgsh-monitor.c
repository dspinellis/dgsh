/*
 * Copyright 2013 Diomidis Spinellis
 *
 * Prepend lines read with timestamp, number of lines, number of bytes.
 * Used for providing dgsh monitoring ports.
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

#include "dgsh.h"

static const char *program_name;

static void
usage(void)
{
	fprintf(stderr, "Usage: %s\n", program_name);
	exit(1);
}

/* Print c with JSON escaping */
static void
escape(int c)
{
	switch (c) {
	case '\\': fputs("\\\\", stdout); break;
	case '"': fputs("\\\"", stdout); break;
	case '/': fputs("\\/", stdout); break;
	case '\b': fputs("\\b", stdout); break;
	case '\f': fputs("\\f", stdout); break;
	case '\n': fputs("\\n", stdout); break;
	case '\r': fputs("\\r", stdout); break;
	case '\t': fputs("\\t", stdout); break;
	default:
		if (c < 0x1f)
			printf("\\u%04x", c);
		else
			putchar(c);
	}
}

int
main(int argc, char *argv[])
{
	int c;
	unsigned long nlines, nbytes;
	struct timeval start, t;
	bool write_header = true;
	bool wrote_header = false;

	program_name = argv[0];

	/* Default if nothing else is specified */
	if (argc != 1)
		usage();

	nbytes = nlines = 0;
	gettimeofday(&start, NULL);

	while ((c = getchar()) != EOF) {
		if (write_header) {
			gettimeofday(&t, NULL);
			if (wrote_header)
				printf("\" }\n");
			printf("{ "
				// Absolute time (s)
				"\"atime\": %lld.%06d, "
				// Relative time (s, from program start)
				"\"rtime\": %.06lf, "
				"\"nlines\": %lu, "
				"\"nbytes\": %lu, "
				"\"data\": \"",
				(long long)t.tv_sec,
				(int)t.tv_usec,
				(t.tv_sec - start.tv_sec) +
				(t.tv_usec - start.tv_usec) / 1e6,
				nlines,
				nbytes);
			wrote_header = true;
			write_header = false;
		}
		escape(c);
		nbytes++;
		if (c == '\n') {
			nlines++;
			write_header = true;
		}
	}

	if (wrote_header)
		printf("\" }\n");

	return 0;
}

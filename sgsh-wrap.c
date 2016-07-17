/*
 * Copyright 2016 Diomidis Spinellis
 *
 * Wrap any command to participate in the sgsh negotiation
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

/*
 * Examples:
 * sgsh-wrap yes | fsck
 * tar cf - / | sgsh-wrap dd of=/dev/st0
 * ls | sgsh-wrap sort -k5n | more
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "sgsh-negotiate.h"

static const char *program_name;

static void
usage(void)
{
	fprintf(stderr, "Usage: %s program [arguments ...]\n",
		program_name);
	exit(1);
}


int
main(int argc, char *argv[])
{
	program_name = argv[0];

	if (argc != 1)
		usage();

	if (sgsh_negotiate(program_name, NULL, NULL, NULL, NULL) != 0)
		exit(1);
	if (argv[0][0] == '/')
		execv(argv[0], argv);
	else
		execvp(argv[0], argv);
}

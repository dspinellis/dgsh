#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include "dgsh.h"

int
main(int argc, char *argv[])
{
	int n_input_fds = 0;
	int n_output_fds = 1;

	if (dgsh_negotiate("secho", &n_input_fds, &n_output_fds, NULL,
				NULL) != 0)
		exit(1);

	assert(n_input_fds == 0);
	assert(n_output_fds == 1);

	++argv;
	while (*argv) {
		(void)printf("%s", *argv);
		if (*++argv)
			putchar(' ');
	}
	putchar('\n');

	return 0;
}

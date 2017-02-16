#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include "dgsh.h"

int
main(int argc, char *argv[])
{
	int n_input_fds = 0;
	int n_output_fds = 1;

	dgsh_negotiate(DGSH_HANDLE_ERROR, "secho", &n_input_fds, &n_output_fds, NULL,
				NULL);

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

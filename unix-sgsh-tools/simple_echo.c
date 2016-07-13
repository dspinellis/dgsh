#include <stdio.h>		/* printf(), putchar() */
#include <assert.h>		/* assert */
#include <unistd.h>		/* isatty() */
#include <stdlib.h>		/* exit() */

#include "sgsh-negotiate.h"

int
main(int argc, char *argv[])
{
	int ninputfds, noutputfds;
	int *inputfds, *outputfds;

	if (sgsh_negotiate("secho", 0, 1, &inputfds, &ninputfds,
			&outputfds, &noutputfds) != 0)
		exit(1);

	assert(ninputfds == 0);
	assert(noutputfds == 1);

	++argv;
	while (*argv) {
		(void)printf("%s", *argv);
		if (*++argv)
			putchar(' ');
	putchar('\n');

	return 0;
}

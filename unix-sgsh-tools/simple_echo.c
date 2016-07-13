#include <stdio.h>		/* fdopen(), fprintf(), fputc() */
#include <assert.h>		/* assert */
#include <sys/stat.h>		/* struct stat */
#include <error.h>		/* error() */
#include <errno.h>		/* errno */
#include <unistd.h>		/* isatty() */
#include <stdlib.h>		/* exit() */
#include "sgsh-negotiate.h"

int
main(int argc, char *argv[])
{
	int ninputfds = -1, ninputfds_expected = 1;
	int noutputfds = -1, noutputfds_expected = 0;
	int *inputfds;
	int *outputfds;
	int status = -1;
	struct stat stats;
	int re = fstat(fileno(stdout), &stats);
	FILE *ostream;
	if (re < 0)
		error(1, errno, "fstat failed\n");

	if (isatty(fileno(stdin)))
		ninputfds_expected = 0;

	/* If standard output not connected to terminal and
	 * connected to either a socket or a FIFO pipe
	 * then its output channel is part of the sgsh graph
	 */
	if (!isatty(fileno(stdout)) &&
			(S_ISFIFO(stats.st_mode) || S_ISSOCK(stats.st_mode)))
		noutputfds_expected = 1;

	if ((status = sgsh_negotiate("secho", ninputfds_expected, noutputfds_expected, &inputfds,
					&ninputfds, &outputfds, &noutputfds)))
	{
		printf("sgsh negotiation failed with status code %d.\n", status);
		exit(1);
	}
	assert(noutputfds == noutputfds_expected);

	if (noutputfds > 0)
		ostream = fdopen(outputfds[0], "w");
	else
		ostream = stdout;

	++argv;		/* skip program name */
	while (*argv) {
		(void)fprintf(ostream, "%s", *argv);
		if (*++argv)
			fputc(' ', ostream);
	}
	fputc('\n', ostream);
	return 0;
}

## Installation of unix tools adapted for sgsh

While under the sgsh root directory, type the following sequence of commands:

```bash
make negotiate        # compile the sgsh negotiation library
cd unix-sgsh-tools
make get-submodules   # download repos of Unix tools if not already cloned 
                      # with the superproject through --recursive
make configure        # configure tools
make make             # compile
make install          # install in ./bin
make -s test          # run tests
```

The current collection of Unix tools include *coreutils, diffutils, gawk, grep, parallel, sed, and tar*.
Of those currently *comm*, *join*, *paste*, and *sort* have been adapted for use with sgsh.

## Adaptation workflow

Head to the tool's repo, e.g., unix-sgsh-tools/coreutils and locate its source code, e.g., src/comm.c

The workflow includes the following steps:

- adapt the tool for use with sgsh
  - invoke the sgsh negotiation API similarly to the code that follows
  - use the provided file descriptors in place of the tool's original input and output interface. You can take a look at the already adapted tools source.

```C
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
```

- contribute tests
  - *cd simple_shell*
  - express a graph of sgsh adapted processes with interconnected inputs and outputs, such as the example that follows. The example has the secho and paste tools connect the former's output to the latter's input. Because they are both sgsh-compatible they use a socketpipe to carry out the sgsh negotiation phase. You can find this file as *secho_paste.sgsh*.
  - run the test with *python simple_shell.py secho_paste.sgsh secho_paste.out* to produce and store the output in file *secho_paste.out*.

```bash
1 /home/mfg/dds/sgsh/unix-sgsh-tools/bin/secho hello
2 /home/mfg/dds/sgsh/unix-sgsh-tools/bin/paste - world

%
socketpipe 1 2
```

- add tests to Makefile's *make test* recipe

```Makefile
PSEUDOSHELLDIR=../simple-shell

test:
    cd $PSEUDOSHELLDIR && \
    python simple-shell.py secho_paste.sgsh secho_paste.out && \
    diff secho_paste.out secho_paste.success && \
    echo "Test secho | paste successful." || \
    echo "Test secho | paste failed."
```

- run tests
```bash
make -s test
```

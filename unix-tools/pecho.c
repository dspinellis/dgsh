#include <stdio.h>		/* printf() */
#include <stdlib.h>		/* exit() */
#include <unistd.h>		/* getpagesize() */
#include "dgsh.h"

int
main(int argc, char *argv[])
{
	int ps = getpagesize();
	char buf[ps];
	int n = 1;
	int ninputs = -1;
	dgsh_negotiate(DGSH_HANDLE_ERROR, "dgsh-pecho", &ninputs,
			NULL, NULL, NULL);

	if (ninputs == 1) {
		n = read(STDIN_FILENO, buf, ps);
		while (n) {
			printf("%s", buf);
			n = read(STDIN_FILENO, buf, ps);
			if (n < 0)
				exit(1);
		}
	}

	++argv;
	while (*argv) {
		(void)printf("%s", *argv);
		if (*++argv)
			putchar(' ');
	}

	putchar('\n');

	return 0;
}

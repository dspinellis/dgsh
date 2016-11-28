#include <stdio.h>		/* printf() */
#include <stdlib.h>		/* exit() */
#include "dgsh-negotiate.h"

int
main(int argc, char *argv[])
{
	if (dgsh_negotiate("secho", NULL, NULL, NULL, NULL) != 0)
		exit(1);

	++argv;
	while (*argv) {
		(void)printf("%s", *argv);
		if (*++argv)
			putchar(' ');
	}
	putchar('\n');

	return 0;
}

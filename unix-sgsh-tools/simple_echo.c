#include <stdio.h>
	
int
main(int argc, char *argv[])
{
	++argv;		/* skip program name */
	while (*argv) {
		(void)printf("%s", *argv);
		if (*++argv)
			putchar(' ');
	}
	putchar('\n');
	return 0;
}

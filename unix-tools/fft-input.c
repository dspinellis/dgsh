#include <assert.h>	// assert()
#include <stdio.h>	// printf
#include <complex.h>	// double complex
#include <unistd.h>	// read(), write()
#include <stdlib.h>	// free()
#include <err.h>	// errx()
#include <errno.h>	// errno

#include "dgsh.h"
#include "debug.h"

int main(int argc, char **argv)
{
	char *input_file;
	FILE *f;
	int ninput = 4, nlines = 0, i;
	int ninputfds = 0, noutputfds;
	int *inputfds = NULL, *outputfds = NULL;
	size_t len = sizeof(long double), wsize;
	char line[len + 1];
	long double *input = (long double *)malloc(sizeof(long double) * ninput);

	if (argc == 1) {
		noutputfds = 8;
		goto negotiate;
	}

	input_file = argv[1];
	f = fopen(input_file, "r");
	if (!f)
		errx(2, "Open file %s failed", input_file);
	DPRINTF(3, "Opened input file: %s", input_file);

	while (fgets(line, len, f)) {
		assert(len == sizeof(input[nlines - 1]));
		nlines++;
		if (nlines == ninput) {
			ninput *= 2;
			input = (long double *)realloc(input,
					sizeof(long double) * ninput);
			if (!input)
				errx(2, "Realloc for input numbers failed");
		}
		input[nlines - 1] = atof(line);

		DPRINTF(3, "Retrieved input %.10Lf\n", input[nlines - 1]);
	}
	noutputfds = nlines;

negotiate:

	dgsh_negotiate(DGSH_HANDLE_ERROR, "fft-input", &ninputfds, &noutputfds,
					&inputfds, &outputfds);
	DPRINTF(3, "Read %d inputs, received %d fds", nlines, noutputfds);
	assert(ninputfds == 0);
	assert(noutputfds == nlines);

	for (i = 0; i < noutputfds; i++) {
		DPRINTF(3, "Write input %.10Lf to fd %d", input[i], outputfds[i]);
		wsize = write(outputfds[i], &input[i],
				sizeof(long double));
		if (wsize == -1) {
			DPRINTF(3, "ERROR: write failed: errno: %d", errno);
			return 1;
		}
	}

	fclose(f);
	free(input);
	return 0;
}

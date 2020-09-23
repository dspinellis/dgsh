#include <assert.h>	// assert()
#include <math.h>	// M_PI, pow()
#include <complex.h>	// I, cexp(), cpow()
#include <stdio.h>	// DPRINTF
#include <stdlib.h>	// atoi()
#include <err.h>	// errx()
#include <unistd.h>	// read(), write()
#include <string.h>	// memcpy()

#include "dgsh.h"
#include "dgsh-debug.h"

#if !defined(HAVE_CPOW)
#include "../../unix-tools/cpow.c"
#endif

void
read_number(int fd, long double *x, long double complex *xc)
{
	char buf[sizeof(long double complex) + 5];	// \n\0
	char real[sizeof(long double)];
	char imag[sizeof(long double)];
	int rd_size;

	// Read input: 2 float values
	rd_size = read(fd, buf, sizeof(buf));
	if (rd_size == -1)
		err(1, "write failed");
	DPRINTF(4, "Read %zu characters, long double size: %zu, long double complex size: %zu",
			rd_size, sizeof(long double), sizeof(long double complex));
	if (rd_size == sizeof(long double)) {
		memcpy(x, buf, sizeof(*x));
		DPRINTF(4, "Read input x: %.10Lf", *x);
	} else {
		sscanf(buf, "%s %s", real, imag);
		*xc = atof(real) + atof(imag)*I;
		DPRINTF(4, "##xc: %.10f + %.10fi (read %zu characters)\n",
				creal(*xc), cimag(*xc), rd_size);
	}
}

void
write_number(int fd, long double complex y)
{
	char buf[sizeof(long double complex)];
	int wr_size;

	// Write output: 2 float values
	memset(buf, 0, sizeof(buf));
	snprintf(buf, sizeof(buf), "%.10f %.10fi", creal(y), cimag(y));
	DPRINTF(4, "##buf(y): %s, len: %d", buf, strlen(buf));
	wr_size = write(fd, buf, sizeof(buf));
	if (wr_size == -1)
		err(1, "write failed");
	DPRINTF(4, "##y: %.10f + %.10fi (wrote %zu characters)\n",
			creal(y), cimag(y), wr_size);
}

int
main(int argc, char** argv)
{
	int noutputfds = 2;
	int *outputfds = NULL;
	int ninputfds = 2;
	int *inputfds = NULL;
	long double x1 = -1.0, x2 = -1.0;
	long double complex xc1, xc2;
	long double complex y1, y2, w, wmn;
	size_t wr_size;
	char negotiation_title[10];
	int s;	// stage (stage=1,2,3)
	int m;	// 2^stage
	int n;	// nth root of unity

	assert(argc == 3);
	s = atoi(argv[1]);
	m = pow(2, s);
	n = atoi(argv[2]);

	snprintf(negotiation_title, sizeof(negotiation_title),
			"%s %s %s", argv[0], argv[1], argv[2]);
	dgsh_negotiate(DGSH_HANDLE_ERROR, negotiation_title, &ninputfds,
			&noutputfds, &inputfds, &outputfds);
	assert(ninputfds == 2);
	assert(noutputfds == 2);

	read_number(inputfds[0], &x1, &xc1);
	read_number(inputfds[1], &x2, &xc2);

	// Calculate
	w = 2 * M_PI * I / m;
	wmn = cpow(cexp(w), n);
	DPRINTF(4, "w: %.10f + %.10fi", creal(w), cimag(w));
	DPRINTF(4, "m: %d, n: %d, wmn: %.10f + %.10fi\n",
			m, n, creal(wmn), cimag(wmn));
	if (x1 == -1.0 && x2 == -1.0) {
		y1 = xc1 + wmn * xc2;
		y2 = xc1 - wmn * xc2;
	} else {
		y1 = x1 + wmn * x2;
		y2 = x1 - wmn * x2;
	}

	write_number(outputfds[0], y1);
	write_number(outputfds[1], y2);

	return 0;
}

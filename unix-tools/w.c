#include <assert.h>	// assert()
#include <math.h>	// M_PI, pow()
#include <complex.h>	// I, cexp(), cpow()
#include <stdio.h>	// DPRINTF
#include <stdlib.h>	// atoi()
#include <err.h>	// errx()
#include <unistd.h>	// read(), write()
#include <errno.h>	//errno
#include <string.h>	// memcpy()

#include "dgsh.h"
#include "debug.h"

int main(int argc, char** argv)
{
	int noutputfds = 2;
	int *outputfds = NULL;
	int ninputfds = 2;
	int *inputfds = NULL;
	int status;
	long double x1 = -1.0, x2 = -1.0;
	long double complex xc1, xc2;
	long double complex y1, y2, w, wmn;
	size_t wr_size;
	char buf[sizeof(long double complex) + 5];	// \n\0
	char real[sizeof(long double)];
	char imag[sizeof(long double)];
	char negotiation_title[10];
	int s;	// stage (stage=1,2,3)
	int m;	// 2^stage
	int n;	// nth root of unity
	memset(buf, 0, sizeof(buf));

	assert(argc == 3);
	s = atoi(argv[1]);
	m = pow(2, s);
	n = atoi(argv[2]);

	sprintf(negotiation_title, "%s %s %s", argv[0], argv[1], argv[2]);
	if ((status = dgsh_negotiate(negotiation_title, &ninputfds, &noutputfds,
					&inputfds, &outputfds)) != 0)
		errx(2, "dgsh negotiation failed with status code %d", status);
	assert(ninputfds == 2);
	assert(noutputfds == 2);

	// Read input: 2 float values
	wr_size = read(inputfds[0], buf, sizeof(buf));
	if (wr_size == -1) {
		DPRINTF("ERROR: write failed: errno: %d", errno);
		return 1;
	}
	//DPRINTF("Read %zu characters, long double size: %zu, long double complex size: %zu",
	//		wr_size, sizeof(long double), sizeof(long double complex));
	if (wr_size == sizeof(long double)) {
		memcpy(&x1, buf, sizeof(x1));
		DPRINTF("Read input x1: %.10Lf", x1);
	} else {
		//DPRINTF("##buf(xc1): %s", buf);
		sscanf(buf, "%s %s\n", real, imag);
		xc1 = atof(real) + atof(imag)*I;
		DPRINTF("##xc1: %.10f + %.10fi (read %zu characters)\n",
				creal(xc1), cimag(xc1), wr_size);
	}

	memset(buf, 0, sizeof(buf));
	wr_size = read(inputfds[1], buf, sizeof(buf));
	if (wr_size == -1) {
		DPRINTF("ERROR: write failed: errno: %d", errno);
		return 1;
	}
	//DPRINTF("Read %zu characters, long double size: %zu, long double complex size: %zu",
	//		wr_size, sizeof(long double), sizeof(long double complex));
	if (wr_size == sizeof(long double)) {
		memcpy(&x2, buf, sizeof(x2));
		DPRINTF("Read input x2: %.10Lf", x2);
	} else {
		//DPRINTF("##buf(xc2): %s", buf);
		sscanf(buf, "%s %s\n", real, imag);
		xc2 = atof(real) + atof(imag)*I;
		DPRINTF("##xc2: %.10f + %.10fi (read %zu characters)\n",
				creal(xc2), cimag(xc2), wr_size);
	}

	// Calculate
	w = 2 * M_PI * I / m;
	wmn = cpow(cexp(w), n);
	DPRINTF("w: %.10f + %.10fi", creal(w), cimag(w));
	DPRINTF("##m: %d, n: %d, wmn: %.10f + %.10fi\n",
			m, n, creal(wmn), cimag(wmn));
	if (x1 == -1.0 && x2 == -1.0) {
		y1 = xc1 + wmn * xc2;
		y2 = xc1 - wmn * xc2;
	} else {
		y1 = x1 + wmn * x2;
		y2 = x1 - wmn * x2;
	}

	// Write output: 2 float values
	memset(buf, 0, sizeof(buf));
	sprintf(buf, "%.10f %.10fi\n", creal(y1), cimag(y1));
	//DPRINTF("##buf(y1): %s", buf);
	wr_size = write(outputfds[0], buf, sizeof(buf));
	if (wr_size == -1) {
		DPRINTF("ERROR: write failed: errno: %d", errno);
		return 1;
	}
	DPRINTF("##y1: %.10f + %.10fi (wrote %zu characters)\n",
			creal(y1), cimag(y1), wr_size);

	memset(buf, 0, sizeof(buf));
	sprintf(buf, "%.10f %.10fi\n", creal(y2), cimag(y2));
	//DPRINTF("##buf(y2): %s", buf);
	wr_size = write(outputfds[1], buf, sizeof(buf));
	if (wr_size == -1) {
		DPRINTF("ERROR: write failed: errno: %d", errno);
		return 1;
	}
	DPRINTF("##y2: %.10f + %.10fi (wrote %zu characters)\n",
			creal(y2), cimag(y2), wr_size);

	return 0;
}

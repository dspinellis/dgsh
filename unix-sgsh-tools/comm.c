/*
 * Copyright (c) 1989, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Case Larsen.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef lint
static const char copyright[] =
"@(#) Copyright (c) 1989, 1993, 1994\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif

#if 0
#ifndef lint
static char sccsid[] = "From: @(#)comm.c	8.4 (Berkeley) 5/4/95";
#endif
#endif

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <err.h>
#include <limits.h>
#include <locale.h>
#include <stdint.h>
#define _WITH_GETLINE
#include <stdio.h>
#include <stdlib.h>   // + setenv()
#include <string.h>
#include <unistd.h>
#include <wchar.h>
#include <wctype.h>

/* SGSH */
#include <assert.h>
/* Socket pairs. */
#include <sys/socket.h>
#include <sys/un.h>
/* FCNTL system call and flags. */
#include <fcntl.h>
/* Retry socket connection. */
#define RETRY_LIMIT 10
/* Debugging. */
#include "../sgsh.h"
/* Negotiate API. */
#include "../sgsh-negotiate.h"
/* Macros. */
#define PAGE_SIZE getpagesize()
#define BUF_SIZE PAGE_SIZE

static int iflag;
static const char *tabs[] = { "", "\t", "\t\t" };

static FILE	*file(const char *, int *);
static wchar_t	*convert(const char *);
static void	show(FILE *, const char *, const char *, char **, size_t *);
static void	usage(void);

/* sgsh. */
static int	create_bind_listen_socket(const char *name);
static void	non_block(int fd);

int
main(int argc, char *argv[])
{
	int comp, read1, read2;
	int ch, flag1, flag2, flag3;
	FILE *fp[2];
	const char *col1, *col2, *col3;
	size_t line1len, line2len;
	char *line1, *line2;
	char *frmline1 = (char *)malloc(sizeof(char) * BUF_SIZE);
	char *frmline2 = (char *)malloc(sizeof(char) * BUF_SIZE);
	char *frmline3 = (char *)malloc(sizeof(char) * BUF_SIZE);
	ssize_t n1, n2;
	wchar_t *tline1, *tline2;
	const char **p;

	/* number of input streams [0,2] */
	int nistreams = 0;
	int ninputfds, noutputfds;
	int *inputfds, *outputfds;
	(void) setlocale(LC_ALL, "");

	putenv("SGSH_IN=1");
	putenv("SGSH_OUT=1");
        
	flag1 = flag2 = flag3 = 1;

	while ((ch = getopt(argc, argv, "123i")) != -1)
		switch(ch) {
		case '1':
			flag1 = 0;
			break;
		case '2':
			flag2 = 0;
			break;
		case '3':
			flag3 = 0;
			break;
		case 'i':
			iflag = 1;
			break;
		case '?':
		default:
			usage();
		}
	argc -= optind;
	argv += optind;

	if (argc != 2)
		usage();

	for (int i = 0; i < 2; i++)
		fp[i] = file(argv[i], &nistreams);

	sgsh_negotiate("comm", nistreams, 3, &inputfds, &ninputfds, 
						&outputfds, &noutputfds);

	/* fd -> FILE* to use the existing interface for input streams */
	for (int i = 0; i < nistreams; i++) {
		assert(nistreams == ninputfds);
		if (fp[i] == stdin)
			fp[i] = fdopen(inputfds[i], "r");
	}

	/* for each column printed, add another tab offset */
	p = tabs;
	col1 = col2 = col3 = NULL;
	if (flag1)
		col1 = *p++;
	if (flag2)
		col2 = *p++;
	if (flag3)
		col3 = *p;

	line1len = line2len = 0;
	line1 = line2 = NULL;
	n1 = n2 = -1;

	for (read1 = read2 = 1;;) {
		/* read next line, check for EOF */
		if (read1) {
			n1 = getline(&line1, &line1len, fp[0]);
			if (n1 < 0 && ferror(fp[0]))
				err(1, "%s", argv[0]);
			if (n1 > 0 && line1[n1 - 1] == '\n')
				line1[n1 - 1] = '\0';

		}
		if (read2) {
			n2 = getline(&line2, &line2len, fp[1]);
			if (n2 < 0 && ferror(fp[1]))
				err(1, "%s", argv[1]);
			if (n2 > 0 && line2[n2 - 1] == '\n')
				line2[n2 - 1] = '\0';
		}

		/* if one file done, display the rest of the other file */
		if (n1 < 0) {
			if (n2 >= 0 && col2 != NULL)
				show(fp[1], argv[1], col2, &line2, &line2len);
			break;
		}
		if (n2 < 0) {
			if (n1 >= 0 && col1 != NULL)
				show(fp[0], argv[0], col1, &line1, &line1len);
			break;
		}

		tline2 = NULL;
		if ((tline1 = convert(line1)) != NULL)
			tline2 = convert(line2);
		if (tline1 == NULL || tline2 == NULL)
			comp = strcmp(line1, line2);
		else
			comp = wcscoll(tline1, tline2);
		if (tline1 != NULL)
			free(tline1);
		if (tline2 != NULL)
			free(tline2);

		/* lines are the same */
		if (!comp) {
			read1 = read2 = 1;
			if (col3 != NULL) {
				(void)sprintf(frmline3, "%s%s\n", col3, line1);
				write(outputfds[2], frmline3, BUF_SIZE);
			}
			continue;
		}

		/* lines are different */
		if (comp < 0) {
			read1 = 1;
			read2 = 0;
			if (col1 != NULL) {
				(void)sprintf(frmline1, "%s%s\n", col1, line1);
				write(outputfds[0], frmline1, BUF_SIZE);
			}
		} else {
			read1 = 0;
			read2 = 1;
			if (col2 != NULL) {
				(void)sprintf(frmline2, "%s%s\n", col2, line2);
				write(outputfds[1], frmline2, BUF_SIZE);
			}
		}
	}
	exit(0);
}

static wchar_t *
convert(const char *str)
{
	size_t n;
	wchar_t *buf, *p;

	if ((n = mbstowcs(NULL, str, 0)) == (size_t)-1)
		return (NULL);
	if (SIZE_MAX / sizeof(*buf) < n + 1)
		errx(1, "conversion buffer length overflow");
	if ((buf = malloc((n + 1) * sizeof(*buf))) == NULL)
		err(1, "malloc");
	if (mbstowcs(buf, str, n + 1) != n)
		errx(1, "internal mbstowcs() error");

	if (iflag) {
		for (p = buf; *p != L'\0'; p++)
			*p = towlower(*p);
	}

	return (buf);
}

static void
show(FILE *fp, const char *fn, const char *offset, char **bufp, size_t *buflenp)
{
	ssize_t n;

	do {
		(void)printf("%s%s\n", offset, *bufp);
		if ((n = getline(bufp, buflenp, fp)) < 0)
			break;
		if (n > 0 && (*bufp)[n - 1] == '\n')
			(*bufp)[n - 1] = '\0';
	} while (1);
	if (ferror(fp))
		err(1, "%s", fn);
}

static FILE *
file(const char *name, int *nistreams)
{
	FILE *fp;

	if (!strcmp(name, "-")) {
		(*nistreams)++;
		return (stdin);
	}
	if ((fp = fopen(name, "r")) == NULL) {
		err(1, "%s", name);
	}
	return (fp);
}

static void
usage(void)
{
	(void)fprintf(stderr, "usage: comm [-123i] file1 file2\n");
	exit(1);
}

/* Create, bind, and listen to a specified socket; finally return the socket */
static int
create_bind_listen_socket(const char *name)
{
        int s;
        socklen_t len;
        struct sockaddr_un local;

        if ((s = socket(AF_UNIX, SOCK_STREAM, 0)) == -1)
                err(1, "socket");

        DPRINTF("Binding to %s", name);

        local.sun_family = AF_UNIX;
        if (strlen(name) >= sizeof(local.sun_path) - 1)
                errx(6, "Socket name [%s] must be shorter than %d characters",
                        name, (int)sizeof(local.sun_path));
        strcpy(local.sun_path, name);
        len = strlen(local.sun_path) + 1 + sizeof(local.sun_family);
        if (bind(s, (struct sockaddr *)&local, len) == -1)
                err(3, "Error binding socket to Unix domain address %s", name);

        if (listen(s, 5) == -1)
                err(4, "listen");

        non_block(s);
	sleep(10);
/*
again:
        if (connect(s, (struct sockaddr *)&remote, len) == -1) {
                if (retry_connection &&
                    (errno == ENOENT || errno == ECONNREFUSED) &&
                    counter++ < RETRY_LIMIT) {
                        DPRINTF("Retrying connection setup");
                        sleep(1);
                        goto again;
                }
                err(2, "connect %s", name);
        }
//        DPRINTF("Connected");

        if (write(s, &cmd, 1) == -1)
                err(3, "write");
//        DPRINTF("Wrote command");
*/
        return s;
}

/*
 * Set the specified file descriptor to operate in non-blocking
 * mode.
 * It seems that even if select returns for a specified file
 * descriptor, performing I/O to it may block depending on the
 * amount of data specified.
 * See See http://pubs.opengroup.org/onlinepubs/009695399/functions/write.html#tag_03_866
 */
static void
non_block(int fd)
{
        int flags = fcntl(fd, F_GETFL, 0);
        if (flags < 0)
                err(2, "Error getting flags for socket");
        if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0)
                err(2, "Error setting socket to non-blocking mode");
}


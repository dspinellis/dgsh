/*-
 *
 * Provide HTTP access to the dgsh key-value store.
 *
 * Based on micro_httpd - really small HTTP server heavily modified by
 * Diomidis Spinellis to use IP sockets, instead of depending on inetd,
 * and to serve dgsh key-value store data, instead of files.
 *
 * micro_httpd:
 * Copyright (c) 1999,2005 by Jef Poskanzer <jef@mail.acme.com>.
 * All rights reserved.
 *
 * Dgsh modifications:
 * Copyright 2013 Diomidis Spinellis
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <err.h>
#include <time.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "kvstore.h"

#define SERVER_NAME "dgsh-httpval"
#define SERVER_URL "http://www.spinellis.gr/sw/dgsh"

#define PROTOCOL "HTTP/1.0"
#define RFC1123FMT "%a, %d %b %Y %H:%M:%S GMT"

/* Forwards. */
static void send_error(FILE *out, int status, char *title, char *extra_header,
    char *text);
static void send_headers(FILE *out, int status, char *title, char *extra_header,
    const char *mime_type, off_t length, time_t mod);
static char * get_mime_type(char *name);
static void strdecode(char *to, char *from);
static int hexit(char c);
static void http_serve(FILE *in, FILE *out, const char *mime_type);

#define c_isxdigit(x) isxdigit((unsigned char)(x))

static const char *program_name;

static void
usage(void)
{
	fprintf(stderr, "Usage: %s [-a] [-b query:cmd] [-p port]\n"
		"-a\t"		"\tAllow non-localhost access\n"
		"-b query:cmd"	"\tSpecify a command for a given HTTP query\n"
		"-m MIME-type"	"\tSpecify the store Content-type header value\n"
		"-n"		"\tNon-blocking read from stores\n"
		"-p port"	"\tSpecify the port to listen to\n",
		program_name);
	exit(1);
}

static struct query {
	const char *query;
	const char *cmd;
	int narg;
	struct query *next;
} *query_list;

/* Command to read from stores: blocking read current record */
static char read_cmd = 'C';

int
main(int argc, char *argv[])
{
	int sockfd, newsockfd;
	struct sockaddr_in cli_addr, serv_addr;
	int ch, port = 0;
	bool localhost_access = true;
	const char *mime_type = "text/plain";
	int so_reuseaddr = 1;
	struct linger so_linger;

	program_name = argv[0];

	while ((ch = getopt(argc, argv, "ab:m:np:")) != -1) {
		char *p;
		struct query *q;

		switch (ch) {
		case 'a':
			localhost_access = false;
			break;
		case 'b':
			if ((p = strchr(optarg, ':')) == NULL)
				usage();
			/* Save query */
			*p = 0;
			q = malloc(sizeof(struct query));
			q->query = strdup(optarg);
			q->cmd = strdup(p + 1);
			/* Count number of query arguments */
			q->narg = 0;
			for (p = optarg; *p; p++)
				if (*p == '%') {
					if (p[1] == '%') {
						p++;
						continue;
					}
					q->narg++;
				}

			if (q->narg > 10) {
				fprintf(stderr, "%s: More than ten query arguments specified.\n", program_name);
				exit(1);
			}

			/* Insert query into the linked list */
			q->next = query_list;
			query_list = q;
			break;
		case 'm':
			mime_type = optarg;
			break;
		case 'n':
			/* Non-blocking read */
			read_cmd = 'c';
			break;
		case 'p':
			port = atoi(optarg);
			break;
		case '?':
		default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;

	if (argc != 0)
		usage();

	if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
		err(2, "socket");

	if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &so_reuseaddr,
			sizeof (so_reuseaddr)) < 0)
		err(2, "setsockopt SO_REUSEADDR");

	so_linger.l_onoff = 1;
	so_linger.l_linger = 1;
	if (setsockopt(sockfd, SOL_SOCKET, SO_LINGER, &so_linger,
			sizeof (so_linger)) < 0)
		err(2, "setsockopt SO_LINGER");

	memset((char *)&serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	serv_addr.sin_port = htons(port);

	if (bind(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
		err(2, "bind");

	if (port == 0) {
		socklen_t len = sizeof(serv_addr);

		serv_addr.sin_port = 0;
		if (getsockname(sockfd, (struct sockaddr *)&serv_addr, &len) < 0)
			err(2, "getsockname");
		printf("%d\n", ntohs(serv_addr.sin_port));
		fflush(stdout);
	}

	listen(sockfd, 5);

	for (;;) {
		socklen_t cli_len = sizeof(cli_addr);
		FILE *in, *out;

		if ((newsockfd = accept(sockfd, (struct sockaddr *)&cli_addr, &cli_len)) < 0)
			err(2, "accept");

		if (localhost_access && strcmp(inet_ntoa(cli_addr.sin_addr), "127.0.0.1")) {
			close(newsockfd);
			continue;
		}

		if ((in = fdopen(newsockfd, "r")) == NULL)
			err(2, "fdopen for input");
		setvbuf(in, NULL, _IOLBF, 4096);

		/* Must dup(2) so that fclose will work correctly */
		if ((out = fdopen(dup(newsockfd), "w")) == NULL)
			err(2, "fdopen for output");

		http_serve(in, out, mime_type);

		(void)fclose(in);
		(void)fclose(out);
	}
}

/* Serve a single HTTP request */
static void
http_serve(FILE *in, FILE *out, const char *mime_type)
{
	char line[10000], method[10000], path[10000], protocol[10000];
	char *file;
	size_t len;
	struct stat sb;
	struct query *q;

	if (fgets(line, sizeof(line), in) == (char *) 0) {
		send_error(out, 400, "Bad Request", (char *) 0,
		    "No request found.");
		return;
	}
	if (sscanf(line, "%[^ ] %[^ ] %[^ ]", method, path, protocol) != 3) {
		send_error(out, 400, "Bad Request", (char *) 0,
		    "Can't parse request.");
		return;
	}
	while (fgets(line, sizeof(line), in) != (char *) 0) {
		if (strcmp(line, "\n") == 0 || strcmp(line, "\r\n") == 0)
			break;
	}
	if (strcasecmp(method, "get") != 0) {
		send_error(out, 501, "Not Implemented", (char *) 0,
		    "That method is not implemented.");
		return;
	}
	if (path[0] != '/') {
		send_error(out, 400, "Bad Request", (char *) 0, "Bad filename.");
		return;
	}
	file = &(path[1]);
	strdecode(file, file);

	if (strcmp(file, ".server?quit") == 0) {
		send_error(out, 200, "OK", (char *) 0,
		    "Quitting.");
		exit(0);
	}

	len = strlen(file);

	/* Guard against attempts to move outside our directory */
	if (file[0] == '/' || strcmp(file, "..") == 0
	    || strncmp(file, "../", 3) == 0
	    || strstr(file, "/../") != (char *) 0
	    || strcmp(&(file[len - 3]), "/..") == 0) {
		send_error(out, 400, "Bad Request", (char *) 0,
		    "Illegal filename.");
		return;
	}

	/* User-specified query name space */
	for (q = query_list; q; q = q->next) {
		int v0, v1, v2, v3, v4, v5, v6, v7, v8, v9;
		char cmd[11000];

		*cmd = 0;
		if (q->narg == 0 && strcmp(file, q->query) == 0)
			strncpy(cmd, q->cmd, sizeof(cmd));
		else if (q->narg && sscanf(file, q->query, &v0, &v1, &v2, &v3, &v4, &v5, &v6, &v7, &v8, &v9) == q->narg)
			snprintf(cmd, sizeof(cmd), q->cmd, v0, v1, v2, v3, v4, v5, v6, v7, v8, v9);
		if (*cmd) {
			int ich;
			FILE *fp;

			fp = popen(cmd, "r");
			if (fp == NULL) {
				send_error(out, 502, "Bad Gateway", NULL, "Error in executing command.");
				return;
			}
			send_headers(out, 200, "Ok", NULL, mime_type, -1,
				time(NULL));
			while ((ich = getc(fp)) != EOF)
				putc(ich, out);
			fclose(fp);
			return;
		}
	}

	/* File system name space */
	if (stat(file, &sb) < 0) {
		send_error(out, 404, "Not Found", NULL, "File not found.");
		return;
	}
	if (S_ISSOCK(sb.st_mode)) {
		/* Value store */
		send_headers(out, 200, "Ok", NULL, mime_type,
		    -1, (time_t)-1);
		(void)fflush(out);
		dgsh_send_command(file, read_cmd, true, false, fileno(out));
	} else if (S_ISREG(sb.st_mode)) {
		/* Regular file */
		int ich;
		FILE *fp;

		fp = fopen(file, "r");
		if (fp == NULL) {
			send_error(out, 403, "Forbidden", NULL, "File is protected.");
			return;
		}
		send_headers(out, 200, "Ok", NULL, get_mime_type(file),
			sb.st_size, sb.st_mtime);
		while ((ich = getc(fp)) != EOF)
			putc(ich, out);
		fclose(fp);
	} else {
		send_error(out, 403, "Forbidden", (char *) 0,
		    "File is not a regular file or a Unix domain socket.");
	}
}

static void
send_error(FILE *out, int status, char *title, char *extra_header, char *text)
{
	send_headers(out, status, title, extra_header, "text/html", -1, -1);
	(void)fprintf(out,
	    "<html><head><title>%d %s</title></head>\n<body><h4>%d %s</h4>\n",
	    status, title, status, title);
	(void)fprintf(out, "%s\n", text);
	(void)fprintf(out,
	    "<hr />\n<address><a href=\"%s\">%s</a></address>\n</body></html>\n",
	    SERVER_URL, SERVER_NAME);
	(void)fflush(out);
}

static void
send_headers(FILE *out, int status, char *title, char *extra_header,
    const char *mime_type, off_t length, time_t mod)
{
	time_t now;
	char timebuf[100];

	(void)fprintf(out, "%s %d %s\015\012", PROTOCOL, status, title);
	(void)fprintf(out, "Server: %s\015\012", SERVER_NAME);
	now = time((time_t *) 0);
	(void)strftime(timebuf, sizeof(timebuf), RFC1123FMT, gmtime(&now));
	(void)fprintf(out, "Date: %s\015\012", timebuf);
	if (extra_header != (char *) 0)
		(void)fprintf(out, "%s\015\012", extra_header);
	if (mime_type != (char *) 0)
		(void)fprintf(out, "Content-Type: %s\015\012", mime_type);
	if (length >= 0)
#if __STDC_VERSION__ >= 199901L
		(void)fprintf(out, "Content-Length: %jd\015\012",
		    (intmax_t) length);
#else
		(void)fprintf(out, "Content-Length: %lld\015\012",
		    (long long) length);
#endif
	if (mod != (time_t) - 1) {
		(void)strftime(timebuf, sizeof(timebuf), RFC1123FMT,
		    gmtime(&mod));
		(void)fprintf(out, "Last-Modified: %s\015\012", timebuf);
	}
	(void)fprintf(out, "Connection: close\015\012");
	(void)fprintf(out, "\015\012");
}

static char *
get_mime_type(char *name)
{
	char *dot;

	dot = strrchr(name, '.');
	if (dot == NULL)
		return "text/plain";
	if (strcmp(dot, ".json") == 0)
		return "application/json";
	if (strcmp(dot, ".html") == 0)
		return "text/html";
	if (strcmp(dot, ".js") == 0)
		return "text/javascript";
	if (strcmp(dot, ".png") == 0)
		return "image/png";
	if (strcmp(dot, ".css") == 0)
		return "text/css";
	return "text/plain; charset=iso-8859-1";
}

static void
strdecode(char *to, char *from)
{
	for (; *from != '\0'; ++to, ++from) {
		if (from[0] == '%' && c_isxdigit(from[1]) && c_isxdigit(from[2])) {
			*to = hexit(from[1]) * 16 + hexit(from[2]);
			from += 2;
		} else
			*to = *from;
	}
	*to = '\0';
}

static int
hexit(char c)
{
	if (c >= '0' && c <= '9')
		return c - '0';
	if (c >= 'a' && c <= 'f')
		return c - 'a' + 10;
	if (c >= 'A' && c <= 'F')
		return c - 'A' + 10;
	/* Shouldn't happen, we're guarded by isxdigit() */
	return 0;
}

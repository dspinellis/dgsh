/*-
 * micro_httpd - really small HTTP server
 *
 * Copyright (c) 1999,2005 by Jef Poskanzer <jef@mail.acme.com>.
 * All rights reserved.
 *
 * Modified by Diomidis Spinellis to use IP sockets instead of depending
 * on inetd.
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
#include <dirent.h>
#include <ctype.h>
#include <time.h>
#include <netinet/in.h>

#define SERVER_NAME "micro_httpd"
#define SERVER_URL "http://www.acme.com/software/micro_httpd/"
#define PROTOCOL "HTTP/1.0"
#define RFC1123FMT "%a, %d %b %Y %H:%M:%S GMT"

/* Forwards. */
static void send_error(FILE *out, int status, char *title, char *extra_header,
    char *text);
static void send_headers(FILE *out, int status, char *title, char *extra_header,
    char *mime_type, off_t length, time_t mod);
static void strdecode(char *to, char *from);
static int hexit(char c);
static void strencode(char *to, size_t tosize, const char *from);
static void http_serve(FILE *in, FILE *out);

#define SERV_TCP_PORT	8187

int
main(int argc, char *argv[])
{
	int sockfd, newsockfd;
	struct sockaddr_in cli_addr, serv_addr;
	char buff[1024];

	if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		perror("socket");
		exit(1);
	}
	memset((char *)&serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	serv_addr.sin_port = htons(SERV_TCP_PORT);

	if (bind(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
		perror("bind");
		exit(1);
	}
	listen(sockfd, 5);

	for (;;) {
		int cli_len = sizeof(cli_addr);
		FILE *in, *out;

		if ((newsockfd = accept(sockfd, (struct sockaddr *)&cli_addr, &cli_len)) < 0) {
			perror("accept");
			exit(1);
		}

		if ((in = fdopen(newsockfd, "r")) == NULL) {
			perror("fdopen for input");
			exit(1);
		}
		setvbuf(in, NULL, _IOLBF, 4096);

		/* Must dup(2) so that fclose will work correctly */
		if ((out = fdopen(dup(newsockfd), "w")) == NULL) {
			perror("fdopen for output");
			exit(1);
		}

		http_serve(in, out);

		(void)fclose(in);
		(void)fclose(out);
	}
}

/* Serve a single HTTP request */
static void
http_serve(FILE *in, FILE *out)
{
	char line[10000], method[10000], path[10000], protocol[10000],
	    idx[20000], location[20000], command[20000];
	char *file;
	size_t len;
	int ich;
	struct stat sb;
	FILE *fp;
	struct dirent **dl;
	int i, n;

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
	if (file[0] == '\0')
		file = "./";
	len = strlen(file);
	if (file[0] == '/' || strcmp(file, "..") == 0
	    || strncmp(file, "../", 3) == 0
	    || strstr(file, "/../") != (char *) 0
	    || strcmp(&(file[len - 3]), "/..") == 0) {
		send_error(out, 400, "Bad Request", (char *) 0,
		    "Illegal filename.");
		return;
	}
	if (stat(file, &sb) < 0) {
		send_error(out, 404, "Not Found", (char *) 0, "File not found.");
		return;
	}
	if (S_ISDIR(sb.st_mode)) {
		send_error(out, 403, "Forbidden", (char *) 0,
		    "File is a directory.");
		return;
	} else {
	      do_file:
		fp = fopen(file, "r");
		if (fp == (FILE *) 0) {
			send_error(out, 403, "Forbidden", (char *) 0,
			    "File is protected.");
			return;
		}
		send_headers(out, 200, "Ok", (char *) 0, "text/plain; charset=iso-8859-1",
		    sb.st_size, sb.st_mtime);
		while ((ich = getc(fp)) != EOF)
			fputc(ich, out);
	}

	(void)fflush(out);
}

static void
send_error(FILE *out, int status, char *title, char *extra_header, char *text)
{
	send_headers(out, status, title, extra_header, "text/html", -1, -1);
	(void)fprintf (out,
	    "<html><head><title>%d %s</title></head>\n<body bgcolor=\"#cc9999\"><h4>%d %s</h4>\n",
	    status, title, status, title);
	(void)fprintf(out, "%s\n", text);
	(void)fprintf(out,
	    "<hr>\n<address><a href=\"%s\">%s</a></address>\n</body></html>\n",
	    SERVER_URL, SERVER_NAME);
	(void)fflush(out);
}

static void
send_headers(FILE *out, int status, char *title, char *extra_header, char *mime_type,
    off_t length, time_t mod)
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
		(void)fprintf(out, "Content-Length: %lld\015\012",
		    (int64_t) length);
	if (mod != (time_t) - 1) {
		(void)strftime(timebuf, sizeof(timebuf), RFC1123FMT,
		    gmtime(&mod));
		(void)fprintf(out, "Last-Modified: %s\015\012", timebuf);
	}
	(void)fprintf(out, "Connection: close\015\012");
	(void)fprintf(out, "\015\012");
}

static void
strdecode(char *to, char *from)
{
	for (; *from != '\0'; ++to, ++from) {
		if (from[0] == '%' && isxdigit(from[1]) && isxdigit(from[2])) {
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

static void
strencode(char *to, size_t tosize, const char *from)
{
	int tolen;

	for (tolen = 0; *from != '\0' && tolen + 4 < tosize; ++from) {
		if (isalnum(*from) || strchr("/_.-~", *from) != (char *) 0) {
			*to = *from;
			++to;
			++tolen;
		} else {
			(void)sprintf(to, "%%%02x", (int) *from & 0xff);
			to += 3;
			tolen += 3;
		}
	}
	*to = '\0';

}

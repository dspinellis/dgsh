/*
 * Copyright 2013 Diomidis Spinellis
 *
 * Write the latest single value read from the standard input to the specified
 * unix domain socket every time a process tries to read a value from it.
 * Thus this process acts in effect as a data store: it reads a series of
 * values (think of them as assignements) and provides a way to read the
 * store's current value (from the socket).
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <sys/un.h>
#include <assert.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <unistd.h>

#define MAX(a, b) ((a) > (b) ? (a) : (b))

#define CONTENT_LENGTH_DIGITS 10
#define CONTENT_LENGTH_FORMAT "%010u"

#ifdef DEBUG
/* ## is a gcc extension that removes trailing comma if no args */
#define DPRINTF(fmt, ...) fprintf(stderr, fmt "\n", ##__VA_ARGS__)

/* Small buffer size to catch errors with data spanning buffers */
#define BUFFER_SIZE 5
#else
#define DPRINTF(fmt, ...)

/* PIPE_BUF is a reasonable size heuristic. */
#define BUFFER_SIZE PIPE_BUF
#endif

/* True once we reach the end of file on standard input */
static int reached_eof;

/* True if a complete record (ending in rs) is available */
static int have_record;

/* Record separator (normally terminator) */
static char rs = '\n';

/* Queue (doubly linked list) of buffers used for storing the last read record */
struct buffer {
	struct buffer *next;
	struct buffer *prev;
	int count;		/* Actual number of bytes stored */
	char data[BUFFER_SIZE];
};

static struct buffer *head, *tail;

/* The oldest buffer whose contents are still being written to a socket. */
static struct buffer *oldest_buffer_being_written;

/* A pointer to a character stored in a buffer */
struct dpointer {
	struct buffer *b;	/* The buffer */
	int pos;		/* The position within the buffer */
};

/* The last complete record read */
static struct dpointer last_record_begin, last_record_end;

/* The clients we're talking to */
struct client {
	int fd;
	struct dpointer write_begin;	/* Start of data for next write */
	struct dpointer write_end;	/* End of data to write */
	enum {
		s_inactive,		/* Free (unused or closed) */
		s_read_command,		/* Waiting for a command (Q or R) to be read */
		s_send_current,		/* Waiting for the current value to be written */
		s_send_last,		/* Waiting for the last (before EOF) value to be written */
		s_sending_response,	/* A response is being written */
		s_wait_close,		/* Wait for the client to close the connection */
	} state;
};

#define MAX_CLIENTS 64
static struct client clients[MAX_CLIENTS];

static const char *program_name;
static const char *socket_path;

static const char *socket_path;

/* Return the oldest of the two buffers (the one that comes first in the list) */
static struct buffer *
oldest_buffer(struct buffer *a, struct buffer *b)
{
	struct buffer *bp;

	if (a == NULL)
		return b;
	else if (b == NULL)
		return a;
	for (bp = head; bp ; bp = bp->next)
		if (bp == a)
			return a;
		else if (bp == b)
			return b;
	assert(0);
}

/*
 * Update oldest_buffer_being_written according to the
 * buffers used by all clients sending a response.
 */
static void
update_oldest_buffer(void)
{
	int i;

	oldest_buffer_being_written = NULL;
	for (i = 0; i < MAX_CLIENTS; i++)
		if (clients[i].state == s_sending_response)
			oldest_buffer_being_written =
				oldest_buffer(oldest_buffer_being_written, clients[i].write_begin.b);
	DPRINTF("Oldest buffer beeing written is %p", oldest_buffer_being_written);
}

/* Free buffers preceding last_record_begin */
static void
free_unused_buffers(void)
{
	struct buffer *b, *bnext;

	for (b = head; b; b = bnext) {
		if (b == last_record_begin.b || b == oldest_buffer_being_written) {
			head = b;
			b->prev = NULL;
			DPRINTF("After freeing buffer(s) head=%p tail=%p", head, tail);
			return;
		}
		bnext = b->next;
		DPRINTF("Freeing buffer %p prev=%p next=%p", b, b->prev, b->next);
		free(b);
	}
	/* Should have encountered last_record_begin.b along the way. */
	assert(0);
}

/* Return the last position of the record terminator in n bytes starting at s */
const char *
find_rs(const char *s, size_t n)
{
	const char *cp;

	for (cp = s + n; n > 0; n--)
		if (*--cp == rs)
			return cp;
	return NULL;
}

/*
 * Find the last record separator, in the area ending
 * at the specified buffer and (excluded) position.
 * Return the position of that separator as a dpointer;
 * return with b set to 0 if no separator was found.
 */
static struct dpointer
find_last_rs(struct buffer *b, int end_pos)
{
	const char *p;
	struct buffer *bp;
	struct dpointer result;

	for (bp = b; bp; bp = bp->prev)
		if ((p = find_rs(bp->data, bp == b ? end_pos : bp->count))) {
			result.b = bp;
			result.pos = p - bp->data;
			return result;
		}
	result.b = 0;
	return result;
}

/* Return the content length of the client's buffer */
static unsigned int
content_length(struct client *c)
{
	struct buffer *bp;
	unsigned int length;

	if (c->write_begin.b == c->write_end.b)
		length = c->write_end.pos - c->write_begin.pos;
	else {
		length = c->write_begin.b->count - c->write_begin.pos;
		for (bp = c->write_begin.b->next; bp && bp != c->write_end.b; bp = bp->next)
			length += bp->count;
		length +=  c->write_end.pos;
	}
	DPRINTF("content_length returns %u", length);
	return length;
}

/*
 * Update the pointers to the last record read.
 * Set have_record if a record is available.
 */
static void
update_last_record(void)
{
	struct dpointer dp_end, dp_begin;

	dp_end = find_last_rs(tail, tail->count);
	if (dp_end.b == NULL) {
		DPRINTF("update_last_record: no rs found");
		return;		/* Not found */
	}
	dp_end.pos++;		/* Point past the end */
	if (memcmp(&dp_end, &last_record_end, sizeof(dp_end)) == 0) {
		DPRINTF("update_last_record: same as before");
		return;		/* Same as before */
	}

	/* Scan for the begin rs one character before the end */
	if (dp_end.pos == 1)
		dp_begin = find_last_rs(dp_end.b->prev, dp_end.b->prev ? dp_end.b->prev->count : 0);
	else
		dp_begin = find_last_rs(dp_end.b, dp_end.pos - 1);

	if (dp_begin.b == NULL) {
		/* No other found. This is the first record. */
		dp_begin.b = head;
		dp_begin.pos = 0;
	} else {
		/* Advance by one character past the found rs */
		dp_begin.pos++;
		if (dp_begin.pos == dp_begin.b->count) {
			dp_begin.b = dp_begin.b->next;
			dp_begin.pos = 0;
		}
	}

	last_record_begin = dp_begin;
	last_record_end = dp_end;
	have_record = 1;
	DPRINTF("update_last_record: begin b=%p pos=%d", last_record_begin.b, last_record_begin.pos);
	DPRINTF("update_last_record: end b=%p pos=%d", last_record_end.b, last_record_end.pos);
	free_unused_buffers();
}

/*
 * Read a one character command from the specifid client and act on it
 * The following commands are supported:
 * R: Read value (the client wants to read our current store value)
 * Q: Quit (Terminate the operation of this data store)
 */

static void
read_command(struct client *c)
{
	char cmd;
	int n;

	switch (n = read(c->fd, &cmd, 1)) {
	case -1: 		/* Error */
		switch (errno) {
		case EAGAIN:
			DPRINTF("EAGAIN on client socket read");
			break;
		default:
			err(3, "Read from socket");
		}
		break;
	case 0:			/* EOF */
		close(c->fd);
		c->state = s_inactive;
		DPRINTF("Done with client %p", c);
		update_oldest_buffer();
		break;
	default:		/* Have data. Insert buffer at the end of the queue. */
		DPRINTF("Read command %c from client %p", cmd, c);
		switch (cmd) {
		case 'L':
			c->state = s_send_last;
			break;
		case 'Q':
			(void)unlink(socket_path);
			exit(0);
		case 'C':
			c->state = s_send_current;
			break;
		default:
			fprintf(stderr, "Unknown command [%c]\n", cmd);
			exit(1);
		}
	}
}

/*
 * Write a single record to the specified client
 * Update the write_begin pointer
 * Close the connection and clean the client if done
 * If write_length is true, precede the record with
 * CONTENT_LENGTH_DIGITS digits representing the record's
 * length.
 */
static void
write_record(struct client *c, int write_length)
{
	int n;
	int towrite;
	struct iovec iov[2], *iovptr;
	char length[CONTENT_LENGTH_DIGITS + 2];

	DPRINTF("Write %srecord for client %p", write_length ? "first " : "", c);
	if (c->write_begin.b == c->write_end.b) {
		towrite = c->write_end.pos - c->write_begin.pos;
		DPRINTF("Single buffer %p: writing %d bytes. write_end.pos=%d write_begin.pos=%d",
			c->write_begin.b, towrite, c->write_end.pos, c->write_begin.pos);
	} else {
		towrite = c->write_begin.b->count - c->write_begin.pos;
		DPRINTF("Multiple buffers %p %p: writing %d bytes. write_begin.b->count=%d write_begin.pos=%d",
			c->write_begin.b, c->write_end.b,
			towrite, c->write_begin.b->count, c->write_begin.pos);
	}

	iov[1].iov_base = c->write_begin.b->data + c->write_begin.pos;
	iov[1].iov_len = towrite;

	if (write_length) {
		snprintf(length, sizeof(length), CONTENT_LENGTH_FORMAT, content_length(c));
		iov[0].iov_base = length;
		iov[0].iov_len = CONTENT_LENGTH_DIGITS;
		iovptr = iov;
	} else
		iovptr = iov + 1;

	if ((n = writev(c->fd, iovptr, write_length ? 2 : 1)) == -1)
		switch (errno) {
		case EAGAIN:
			DPRINTF("EAGAIN on client socket write");
			return;
		default:
			err(3, "Write to socket");
		}

	if (write_length) {
		if (n < CONTENT_LENGTH_DIGITS) {
			fprintf(stderr, "Short content length record write: %d\n", n);
			exit(1);
		}
		n -= CONTENT_LENGTH_DIGITS;
	}

	c->write_begin.pos += n;
	DPRINTF("Wrote %u data bytes. Current buffer position=%d", n, c->write_begin.pos);
	/*
	 * More data to write from this buffer?
	 * Yes, if there is still more data in the buffer
	 * and either the end is in another buffer, or we haven't
	 * reached it.
	 */
	if (c->write_begin.pos < c->write_begin.b->count &&
	    (c->write_begin.b != c->write_end.b || c->write_begin.pos < c->write_end.pos)) {
		DPRINTF("Continuing with same buffer");
		return;
	}

	/* More buffers to write from? */
	if (c->write_begin.b != c->write_end.b) {
		c->write_begin.b = c->write_begin.b->next;
		c->write_begin.pos = 0;
		DPRINTF("Moving to next buffer %p with size %u", c->write_begin.b, c->write_begin.b->count);
		return;
	}

	/* Done with this client */
	DPRINTF("No more data to write for client %p", c);
	c->state = s_wait_close;
}

/* Read data from STDIN into a new buffer */
static void
buffer_read(void)
{
	struct buffer *b;

	if ((b = malloc(sizeof(struct buffer))) == NULL)
		err(1, "Unable to allocate read buffer");

	DPRINTF("Calling read on stdin for buffer %p", b);
	switch (b->count = read(STDIN_FILENO, b->data, sizeof(b->data))) {
	case -1: 		/* Error */
		switch (errno) {
		case EAGAIN:
			DPRINTF("EAGAIN on standard input");
			free(b);
			break;
		default:
			err(3, "Read from standard input");
		}
		break;
	case 0:			/* EOF */
		reached_eof = 1;
		if (have_record) {
			free(b);
			break;
		}
		if (head == NULL) {
			/* Setup an empty record */
			b->count = 0;
			b->prev = b->next = NULL;
			head = tail = b;
			last_record_begin.b = last_record_end.b = b;
			last_record_begin.pos = last_record_end.pos = 0;
		} else {
			free(b);
			/* Setup all input as a record */
			last_record_begin.b = head;
			last_record_end.b = tail;
			last_record_begin.pos = 0;
			last_record_end.pos = tail->count;
		}
		have_record = 1;
		break;
	default:		/* Have data. Insert buffer at the end of the queue. */
		b->prev = tail;
		b->next = NULL;
		if (tail)
			tail->next = b;
		tail = b;
		if (!head)
			head = b;
		DPRINTF("Read %d bytes into %p prev=%p next=%p head=%p tail=%p",
			b->count, b, b->prev, b->next, head, tail);
		update_last_record();
		break;
	}
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

/* Return an initialized free client entry, or exit with an error. */
static struct client *
get_free_client(void)
{
	int i;

	for (i = 0; i < MAX_CLIENTS; i++)
		if (clients[i].state == s_inactive)
			return &clients[i];
	fprintf(stderr, "%s: Maximum number of clients exceeded for socket %s\n",
		program_name, socket_path);
	exit(1);
}

static void
usage(const char *name)
{
	fprintf(stderr, "Usage: %s socket_name\n", name);
	exit(1);
}

int
main(int argc, char *argv[])
{
	int max_fd, sock;
	socklen_t len;
	struct sockaddr_un local, remote;

	if (argc != 2)
		usage(argv[0]);

	program_name = argv[0];
	socket_path = argv[1];
	(void)unlink(socket_path);

	if ((sock = socket(AF_UNIX, SOCK_STREAM, 0)) == -1)
		err(2, "Error creating socket");

	local.sun_family = AF_UNIX;
	strncpy(local.sun_path, socket_path, sizeof(local.sun_path));
	len = strlen(local.sun_path) + sizeof(local.sun_family);
	if (bind(sock, (struct sockaddr *)&local, len) == -1)
		err(3, "Error binding socket to Unix domain address %s", argv[1]);

	if (listen(sock, 5) == -1)
		err(4, "listen");

	non_block(sock);

	reached_eof = 0;
	for (;;) {
		fd_set source_fds;
		fd_set sink_fds;
		int i;

		/* Set the fds that interest us */
		FD_ZERO(&source_fds);
		FD_ZERO(&sink_fds);

		max_fd = -1;
		/* Read from standard input */
		if (!reached_eof) {
			FD_SET(STDIN_FILENO, &source_fds);
			max_fd = STDIN_FILENO;
		}

		/* Accept incoming connection */
		FD_SET(sock, &source_fds);
		max_fd = MAX(sock, max_fd);

		/* I/O with a client */
		for (i = 0; i < MAX_CLIENTS; i++)
			switch (clients[i].state) {
			case s_inactive:		/* Free (unused or closed) */
				break;
			case s_wait_close:		/* Wait for the client to close the connection */
			case s_read_command:		/* Waiting for a command (Q or R) to be read */
				FD_SET(clients[i].fd, &source_fds);
				max_fd = MAX(clients[i].fd, max_fd);
				break;
			case s_send_last:		/* Waiting for the last (before EOF) value to be written */
				if (reached_eof) {
					FD_SET(clients[i].fd, &sink_fds);
					max_fd = MAX(clients[i].fd, max_fd);
				}
				break;
			case s_send_current:		/* Waiting for a response to be written */
				if (have_record) {
					FD_SET(clients[i].fd, &sink_fds);
					max_fd = MAX(clients[i].fd, max_fd);
				}
				break;
			case s_sending_response:	/* A response is being sent */
				FD_SET(clients[i].fd, &sink_fds);
				max_fd = MAX(clients[i].fd, max_fd);
				break;
			}

		DPRINTF("Calling select");
		if (select(max_fd + 1, &source_fds, &sink_fds, NULL, NULL) < 0)
			err(3, "select");
		DPRINTF("Select returns");

		if (FD_ISSET(STDIN_FILENO, &source_fds))
			buffer_read();

		for (i = 0; i < MAX_CLIENTS; i++)
			switch (clients[i].state) {
			case s_inactive:		/* Free (unused or closed) */
				break;
			case s_read_command:		/* Waiting for a command (Q or R) to be read */
			case s_wait_close:		/* Wait for the client to close the connection */
				if (FD_ISSET(clients[i].fd, &source_fds))
					read_command(&clients[i]);
				break;
			case s_send_last:		/* Waiting for the last (before EOF) value to be written */
				/* FALLTHROUGH */
			case s_send_current:		/* Waiting for a response to be written */
				if (FD_ISSET(clients[i].fd, &sink_fds)) {
					assert(have_record);
					/* Start writing the most fresh last record */
					clients[i].write_begin = last_record_begin;
					clients[i].write_end = last_record_end;
					clients[i].state = s_sending_response;
					oldest_buffer_being_written =
						oldest_buffer(oldest_buffer_being_written, clients[i].write_begin.b);
					write_record(&clients[i], 1);
				}
				break;
			case s_sending_response:		/* A response is being written */
				if (FD_ISSET(clients[i].fd, &sink_fds)) {
					assert(have_record);
					write_record(&clients[i], 0);
				}
				break;
			}

		if (FD_ISSET(sock, &source_fds)) {
			int rsock;
			struct client *c;

			len = sizeof(remote);
			rsock = accept(sock, (struct sockaddr *)&remote, &len);
			if (rsock == -1 && errno != EAGAIN)
				err(5, "accept");

			c = get_free_client();
			non_block(rsock);
			c->fd = rsock;
			c->state = s_read_command;
		}
	}
}

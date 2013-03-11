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
#include <sys/time.h>
#include <sys/uio.h>
#include <sys/un.h>
#include <assert.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <unistd.h>

#include "sgsh.h"

#ifdef DEBUG
/* Small buffer size to catch errors with data spanning buffers */
#define BUFFER_SIZE 5
#else
/* PIPE_BUF is a reasonable size heuristic. */
#define BUFFER_SIZE PIPE_BUF
#endif

/* User options start here */
/* Record terminator */
static char rt = '\n';

/* Record length; 0 if we use a record terminator */
static int rl = 0;

/* True if the begin and end are specified using a time window */
static bool time_window;

/*
 * Specified response record.
 * This is specified using reverse iterators (counted from the end of the stream).
 * The _rbegin is inclusive, _rend is exclusive
 * Examples:
 * To get the last record use the range rbegin = 0 rend = 1
 * To get 5 records starting 10 records away from the end
 * use the range rbegin = 10 rend = 15
 */
static union {
	struct timeval t;	/* Used if time_window is true */
	int r;			/* Used if time_window is false */
	double d;		/* Used when parsing */
} record_rbegin, record_rend;

/* User options end here */

/* True once we reach the end of file on standard input */
static bool reached_eof;

/* True if a complete record (ending in rt) is available */
static bool have_record;

/* Queue (doubly linked list) of buffers used for storing the last read record */
struct buffer {
	struct buffer *next;
	struct buffer *prev;
	int size;				/* Actual number of bytes stored */
	struct timeval timestamp;		/* Time the buffer was read */
	long long record_count;			/* Total number of complete records read (including this buffer)
						   (0-based ordinal of first record not in buffer) */
	long long byte_count;			/* Total number of bytes read (including this buffer) */
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
static struct dpointer current_record_begin, current_record_end;

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

/*
 * Increment dp by one byte.
 * If no more bytes are available return false
 * leaving pos to point one byte past the last available one
 */
static bool
dpointer_increment(struct dpointer *dp)
{
	DPRINTF("%p pos=%d", dp->b, dp->pos);
	dp->pos++;
	if (dp->pos == dp->b->size) {
		if (!dp->b->next)
			return false;
		dp->b = dp->b->next;
		dp->pos = 0;
	}
	DPRINTF("return %p pos=%d", dp->b, dp->pos);
	return true;
}

/* Decrement dp by one byte. Return false if no more bytes are available */
static bool
dpointer_decrement(struct dpointer *dp)
{
	DPRINTF("%p pos=%d", dp->b, dp->pos);
	dp->pos--;
	if (dp->pos == -1) {
		if (!dp->b->prev) {
			dp->pos++;
			return false;
		}
		dp->b = dp->b->prev;
		dp->pos = dp->b->size - 1;
	}
	DPRINTF("return %p pos=%d", dp->b, dp->pos);
	return true;
}

/*
 * Add to dp the specified number of bytes.
 * Return true of OK.
 * If not enough bytes are available return false
 * and set dp to point to the last byte in the buffer.
 */
static bool
dpointer_add(struct dpointer *dp, int n)
{
	DPRINTF("%p pos=%d n=%d", dp->b, dp->pos, n);
	while (n > 0) {
		int add = MIN(dp->b->size - dp->pos, n);
		n -= add;
		dp->pos += add;
		if (dp->pos == dp->b->size) {
			if (!dp->b->next)
				return false;
			dp->b = dp->b->next;
			dp->pos = 0;
		}
	}
	DPRINTF("return %p pos=%d", dp->b, dp->pos);
	return true;
}

/*
 * Subtract from dp the specified number of bytes.
 * Return true of OK.
 * If not enough bytes are available return false
 * and set dp to point beyond the first available byte.
 */
static bool
dpointer_subtract(struct dpointer *dp, int n)
{
	DPRINTF("%p pos=%d n=%d", dp->b, dp->pos, n);
	while (n > 0) {
		int subtract = MIN((dp->pos + 1) - 0, n);
		n -= subtract;
		dp->pos -= subtract;
		if (dp->pos == -1) {
			if (!dp->b->prev)
				return false;
			dp->b = dp->b->prev;
			dp->pos = dp->b->size - 1;
		}
	}
	DPRINTF("return %p pos=%d", dp->b, dp->pos);
	return true;
}

/*
 * Move back dp the specified number of rt-terminated records.
 * Postcondition: dp will point at the beginning of
 * a record.
 * Return true if OK, false if not enough records are available
 * Example: to move back over one complete record, the function will
 * encounter two rts, and return with pos set immediately after the
 * second one.
 */
static bool
dpointer_move_back(struct dpointer *dp, int n)
{
	DPRINTF("%p pos=%d (size=%d, prev=%p) n=%d",
		dp->b, dp->pos, dp->b->size, dp->b->prev, n);
	for (;;) {
		if (dpointer_decrement(dp)) {
			if (dp->b->data[dp->pos] == rt && --n == -1) {
				dpointer_increment(dp);
				DPRINTF("return %p pos=%d", dp->b, dp->pos);
				return true;
			}
		} else {
			if (--n == -1) {
				DPRINTF("(at begin) returns: %p pos=%d", dp->b, dp->pos);
				return true;
			} else
				return false;	/* Not enough records available */
		}
	}
}

/*
 * Move forward dp the specified number of rt-terminated records.
 * Postcondition: dp will point past the end of a record.
 * Return true if OK, false if not enough records are available
 */
static bool
dpointer_move_forward(struct dpointer *dp, int n)
{
	DPRINTF("%p pos=%d (size=%d, next=%p) n=%d",
		dp->b, dp->pos, dp->b->size, dp->b->next, n);
	/* Cover the case where we are already at the beginning of the record */
	if (!dpointer_decrement(dp)) {
		DPRINTF("return %p pos=%d (at head)", dp->b, dp->pos);
		return true;
	}
	for (;;) {
		if (dp->b->data[dp->pos] == rt && --n == -1) {
			dpointer_increment(dp);
			DPRINTF("return %p pos=%d", dp->b, dp->pos);
			return true;
		}
		if (!dpointer_increment(dp))
				return false;	/* Not enough records available */
	}
}

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

/* Free buffers preceding in position the used buffer */
static void
free_unused_buffers_by_position(struct buffer *used)
{
	struct buffer *b, *bnext;

	for (b = head; b; b = bnext) {
		if (b == used || b == oldest_buffer_being_written) {
			head = b;
			b->prev = NULL;
			DPRINTF("After freeing buffer(s) head=%p tail=%p", head, tail);
			return;
		}
		bnext = b->next;
		DPRINTF("Freeing buffer %p prev=%p next=%p", b, b->prev, b->next);
		free(b);
	}
	/* Should have encountered used along the way. */
	assert(0);
}

/* Free buffers preceding in time (older than) the used buffer */
static void
free_unused_buffers_by_time(struct timeval *used)
{
	struct buffer *b;

	DPRINTF("Free buffers older than %lld.%06d",
		(long long)used->tv_sec, (int)used->tv_usec);

	/* Find first useful record */
	for (b = head; b; b = b->next)
		if (timercmp(&b->timestamp, used, >=) || b == oldest_buffer_being_written)
			break;
	assert(b);	/* Should have encountered used along the way. */

	DPRINTF("First used buffer is %p", b);
	/* Must now leave another record in case a record extends backward */
	if (rl) {
		int n = rl;

		do {
			b = b->prev;
		} while (b && (n -= b->size) > 0);
	} else {
		do {
			b = b->prev;
		} while (b && !memchr(b->data, rt, b->size));
	}
	DPRINTF("After extending back %p", b);
	if (b)
		free_unused_buffers_by_position(b);
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
		length = c->write_begin.b->size - c->write_begin.pos;
		for (bp = c->write_begin.b->next; bp && bp != c->write_end.b; bp = bp->next)
			length += bp->size;
		length +=  c->write_end.pos;
	}
	DPRINTF("return %u", length);
	return length;
}

/*
 * Update the pointers to the current response record based on the defined
 * record terminator.
 */
static void
update_current_record_by_rt_number(void)
{
	bool ret;

	/* Point to the end of read data */
	current_record_end.b = tail;
	current_record_end.pos = tail->size;

	/* Remove data that forms an incomplete record */
	ret = dpointer_move_back(&current_record_end, 0);
	assert(ret);

	/* Go back to the end of the specified record */
	ret = dpointer_move_back(&current_record_end, record_rbegin.r);
	assert(ret);

	/* Go further back to the begin of the specified record */
	current_record_begin = current_record_end;
	ret = dpointer_move_back(&current_record_begin, record_rend.r - record_rbegin.r);
	assert(ret);
}

/*
 * Update the pointers to the current response record based on the defined
 * record length.
 */
static void
update_current_record_by_rl_number(void)
{
	bool ret;

	/* Point to the end of read data */
	current_record_end.b = tail;
	current_record_end.pos = tail->size;

	/* Remove data that forms an incomplete record */
	ret = dpointer_subtract(&current_record_end, tail->byte_count % rl);
	assert(ret);

	/* Go back to the end of the specified record */
	ret = dpointer_subtract(&current_record_end, record_rbegin.r * rl);
	assert(ret);

	/* Go further back to the begin of the specified record */
	current_record_begin = current_record_end;
	ret = dpointer_subtract(&current_record_begin, (record_rend.r - record_rbegin.r) * rl);
	assert(ret);
}

/*
 * Update the pointers to the current response record to include the first
 * record terminated record beginning in or after the begin buffer and
 * the last record beginning in the end buffer.
 * Set have_record to true if the corresponding data range exists
 */
static void
update_current_record_by_rt_time(struct buffer *begin, struct buffer *end)
{
	/* Point to the begin of the data window */
	current_record_begin.b = begin;
	current_record_begin.pos = 0;

	/* Go to the begin of a record starting at or after the buffer */
	if (!dpointer_move_forward(&current_record_begin, 0))
		return;

	/* Point to the end of the data window */
	current_record_end.b = end;
	current_record_end.pos = end->size;
	dpointer_decrement(&current_record_end);

	/* Adjust data that forms an incomplete record */
	if (!dpointer_move_forward(&current_record_end, 0)) {
		current_record_end.b = end;
		current_record_end.pos = end->size;
		if (!dpointer_move_back(&current_record_end, 0))
			return;
		if (memcmp(&current_record_begin, &current_record_end, sizeof(struct dpointer)) == 0)
			return;
	}

	have_record = true;
}

/*
 * Update the pointers to the current response record to include the first
 * fixed length record beginning in or after the begin buffer and
 * the last record beginning in the end buffer.
 * Set have_record to true if the corresponding data range exists
 */
static void
update_current_record_by_rl_time(struct buffer *begin, struct buffer *end)
{
	int mod;

	DPRINTF("Adjusting begin");
	current_record_begin.b = begin;
	current_record_begin.pos = 0;
	if (begin->prev && (mod = begin->prev->byte_count % rl) != 0)
		/*
		 * Example: rl == 10, prev->byte_count == 53
		 * mod = 3, dpointer_add(..., 7)
		 */
		if (!dpointer_add(&current_record_begin, rl - mod))
			return;		/* Next record not there */

	DPRINTF("Adjusting end");
	current_record_end.b = end;
	current_record_end.pos = end->size;
	if ((mod = end->byte_count % rl) != 0) {
		/*
		 * Example: rl == 10, end->byte_count == 82
		 * mod = 2, dpointer_add(..., 8)
		 * Decrement and increment to convert between an iterator
		 * pointing beyond the range, and valid positions that dpointer_add
		 * can handle correctly.
		 */
		if (!dpointer_decrement(&current_record_end) ||
		    !dpointer_add(&current_record_end, rl - mod)) {
			DPRINTF("incomplete last record");
			/* Try going back */
			current_record_end.b = end;
			current_record_end.pos = end->size;
			if (!dpointer_subtract(&current_record_end, mod))
				return;
		} else
			(void)dpointer_increment(&current_record_end);
	}

	if (memcmp(&current_record_begin, &current_record_end, sizeof(struct dpointer)) == 0)
		return;
	have_record = true;
}

#ifdef DEBUG
/* Dump the buffer list using relative timestamps */
static void
dump_buffer_times(void)
{
	struct buffer *bp;
	struct timeval now, t;

	gettimeofday(&now, NULL);

	DPRINTF("update_current_record: now=%lld.%06d rend=%lld.%06d rbegin=%lld.%06d",
		(long long)now.tv_sec, (int)now.tv_usec,
		(long long)record_rend.t.tv_sec, (int)record_rend.t.tv_usec,
		(long long)record_rbegin.t.tv_sec, (int)record_rbegin.t.tv_usec);
	for (bp = head; bp != NULL; bp = bp->next) {
		timersub(&now, &bp->timestamp, &t);

		DPRINTF("\t%p size=%3d byte_count=%5lld Tr=%3lld.%06d Ta=%3lld.%06d [%.*s]",
			bp, bp->size, bp->byte_count,
			(long long)t.tv_sec, (int)t.tv_usec,
			(long long)bp->timestamp.tv_sec, (int)bp->timestamp.tv_usec,
			bp->size, bp->data);
	}
}

static void
timestamp(const char *msg)
{
	struct timeval now;
	gettimeofday(&now, NULL);
	DPRINTF("%lld.%06d %s", (long long)now.tv_sec, (int)now.tv_usec, msg);
}

#define TIMESTAMP(x) timestamp(x)
#define DUMP_BUFFER_TIMES() dump_buffer_times()

#else
#define DUMP_BUFFER_TIMES()
#define TIMESTAMP(x)
#endif

/*
 * Update the pointers to the current response record.
 * Set have_record if a record is available.
 */
static void
update_current_record(void)
{
	assert(head && tail);

	if (time_window) {
		struct timeval now, tbegin, tend;	/* In absolute time units */
		struct buffer *bbegin, *bend, *begin_candidate = NULL;

		DUMP_BUFFER_TIMES();
		have_record = false;		/* Records in the window come and go */

		/* Convert to absolute time */
		gettimeofday(&now, NULL);
		timersub(&now, &record_rend.t, &tbegin);

		DPRINTF("tail->timestamp=%lld.%06d tbegin=%lld.%06d",
			(long long)tail->timestamp.tv_sec, (int)tail->timestamp.tv_usec,
			(long long)tbegin.tv_sec, (int)tbegin.tv_usec);

		if (timercmp(&tail->timestamp, &tbegin, <)) {
			free_unused_buffers_by_position(tail);
			return;		/* No records fresh enough */
		}

		timersub(&now, &record_rbegin.t, &tend);

		DPRINTF("head->timestamp=%lld.%06d tend=%lld.%06d",
			(long long)head->timestamp.tv_sec, (int)head->timestamp.tv_usec,
			(long long)tend.tv_sec, (int)tend.tv_usec);

		if (timercmp(&head->timestamp, &tend, >))
			return;		/* No records old enough */

		/* Find the record range */
		DPRINTF("Looking for record range");
		for (bend = tail; timercmp(&bend->timestamp, &tend, >); bend = bend->prev)
			;
		DPRINTF("bend=%p %lld.%06d", bend, (long long)bend->timestamp.tv_sec, (int)bend->timestamp.tv_usec);

		for (bbegin = bend; bbegin && timercmp(&bbegin->timestamp, &tbegin, >); bbegin = bbegin->prev)
			begin_candidate = bbegin;

		if (!begin_candidate) {
			free_unused_buffers_by_time(&tbegin);
			return;		/* No records within the window */
		}
		bbegin = begin_candidate;
		DPRINTF("bbegin=%p %lld.%06d", bbegin, (long long)bbegin->timestamp.tv_sec, (int)bbegin->timestamp.tv_usec);

		if (rl)
			update_current_record_by_rl_time(bbegin, bend);
		else
			update_current_record_by_rt_time(bbegin, bend);

		free_unused_buffers_by_time(&tbegin);
	} else {
		DPRINTF("tail->record_count=%lld record_rend.r=%d",
			tail->record_count, record_rend.r);
		if (tail->record_count - record_rend.r < 0)
			/* Not enough records */
			return;

		if (rl)
			update_current_record_by_rl_number();
		else
			update_current_record_by_rt_number();
		have_record = true;
		free_unused_buffers_by_position(current_record_begin.b);
	}

	DPRINTF("have_record=%d", have_record);
	DPRINTF("begin b=%p pos=%d", current_record_begin.b, current_record_begin.pos);
	DPRINTF("end b=%p pos=%d", current_record_end.b, current_record_end.pos);
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
			if (time_window)
				update_current_record();	/* Refresh have_record */
			break;
		default:
			errx(5, "Unknown command [%c]", cmd);
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
		towrite = c->write_begin.b->size - c->write_begin.pos;
		DPRINTF("Multiple buffers %p %p: writing %d bytes. write_begin.b->size=%d write_begin.pos=%d",
			c->write_begin.b, c->write_end.b,
			towrite, c->write_begin.b->size, c->write_begin.pos);
	}

	iov[1].iov_base = c->write_begin.b->data + c->write_begin.pos;
	iov[1].iov_len = towrite;
	DPRINTF("Writing [%.*s]", (int)iov[1].iov_len, (char *)iov[1].iov_base);

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
		if (n < CONTENT_LENGTH_DIGITS)
			errx(5, "Short content length record write: %d", n);
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
	if (c->write_begin.pos < c->write_begin.b->size &&
	    (c->write_begin.b != c->write_end.b || c->write_begin.pos < c->write_end.pos)) {
		DPRINTF("Continuing with same buffer");
		return;
	}

	/* More buffers to write from? */
	if (c->write_begin.b != c->write_end.b) {
		c->write_begin.b = c->write_begin.b->next;
		c->write_begin.pos = 0;
		DPRINTF("Moving to next buffer %p with size %u", c->write_begin.b, c->write_begin.b->size);
		return;
	}

	/* Done with this client */
	DPRINTF("No more data to write for client %p", c);
	c->state = s_wait_close;
}

/* Set the buffer's counters */
void
set_buffer_counters(struct buffer *b)
{
	if (time_window)
		gettimeofday(&b->timestamp, NULL);

	if (rl == 0) {
		/* Count records using RS */
		int i;

		b->record_count = b->prev ? b->prev->record_count : 0;
		for (i = 0; i < b->size; i++)
			if (b->data[i] == rt)
				b->record_count++;
	} else {
		/* Count records using RL */

		b->byte_count = b->prev ? b->prev->byte_count : 0;
		b->byte_count += b->size;
		b->record_count = b->byte_count / rl;
	}
}

/* Read data from STDIN into a new buffer */
static void
buffer_read(void)
{
	struct buffer *b;
	struct timeval now, abs_rend_time;

	if ((b = malloc(sizeof(struct buffer))) == NULL)
		err(1, "Unable to allocate read buffer");

	DPRINTF("Calling read on stdin for buffer %p", b);
	switch (b->size = read(STDIN_FILENO, b->data, sizeof(b->data))) {
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
		reached_eof = true;
		if (time_window) {
			/* Make abs_rend_time the latest absolute time that interests us */
			gettimeofday(&now, NULL);
			timeradd(&now, &record_rend.t, &abs_rend_time);
		}
		if (have_record) {
			free(b);
		} else if (!time_window || !tail ||
		    timercmp(&tail->timestamp, &abs_rend_time, >)) {
			/* Setup an empty record, if there will never be a record to send */
			b->size = 0;
			b->prev = b->next = NULL;
			head = tail = b;
			current_record_begin.b = current_record_end.b = b;
			current_record_begin.pos = current_record_end.pos = 0;
			have_record = true;
		}
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
			b->size, b, b->prev, b->next, head, tail);
		set_buffer_counters(b);
		update_current_record();
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
	errx(5, "Maximum number of clients exceeded for socket %s", socket_path);
}

static void
usage(void)
{
	fprintf(stderr, "Usage: %s [-l len|-t char] [-b n] [-e n] [-u s|m|h|d|r] -s path\n"
		"-b n"		"\tStore records beginning in a window n away from the end (default 1)\n"
		"-e n"		"\tStore records ending in a window n away from the end (default 0)\n"
		"-l len"	"\tProcess fixed-width len-sized records\n"
		"-s path"	"\tSpecify the socket to connect to\n"
		"-t char"	"\tProcess char-terminated records (newline default)\n"
		"-u unit"	"\tSpecify the unit of window boundaries\n"
		""		"\ts: seconds\n"
		""		"\tm: minutes\n"
		""		"\th: hours\n"
		""		"\td: days\n"
		""		"\tr: records (default)\n",
		program_name);
	exit(1);
}

/*
 * Parse a number >= 0
 * Exit with an error if an error occurs
 */
static double
parse_double(const char *s)
{
	char *endptr;
	double d;

	errno = 0;
	d = strtod(s, &endptr);
	if (endptr - s != strlen(s) || *s == 0)
		errx(6, "Error in parsing [%s] as a number", s);
	if (errno != 0)
		err(6, "[%s]", s);
	if (d < 0)
		errx(6, "Argument [%s] cannot be negative", s);
	return d;
}

/* Return the passed double as a timeval */
struct timeval
double_to_timeval(double d)
{
	struct timeval t;

	t.tv_sec = (time_t)d;
	t.tv_usec = (int)((d - t.tv_sec) * 1e6);
	return t;
}

/* Parse the program's arguments */
static void
parse_arguments(int argc, char *argv[])
{
	int ch;
	char unit = 'r';

	program_name = argv[0];
	/* By default return the last record read */
	record_rbegin.d = 0;
	record_rend.d = 1;

	while ((ch = getopt(argc, argv, "b:e:l:s:t:u:")) != -1) {
		switch (ch) {
		case 'b':	/* Begin record, measured from the end (0) */
			record_rend.d = parse_double(optarg);
			break;
		case 'e':	/* End record, measured from the end (0) */
			record_rbegin.d = parse_double(optarg);
			break;
		case 'l':	/* Fixed record length */
			rl = atoi(optarg);
			if (rl <= 0)
				usage();
			break;
		case 's':
			socket_path = optarg;
			break;
		case 't':	/* Record terminator */
			/* We allow \0 as rt */
			if (strlen(optarg) > 1)
				usage();
			rt = *optarg;
			break;
		case 'u':	/* Measurement unit */
			if (strlen(optarg) != 1 || strchr("smhdr", *optarg) == NULL)
				usage();
			unit = *optarg;
			break;
		case '?':
		default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;

	if (argc != 0 || socket_path == NULL)
		usage();

	switch (unit) {
	case 'r':
		if (record_rbegin.d != (int)record_rbegin.d ||
		    record_rend.d != (int)record_rend.d)
		    	errx(6, "Record numbers must be integers");
		record_rbegin.r = (int)record_rbegin.d;
		record_rend.r = (int)record_rend.d;
		time_window = false;
		break;
	case 'd':
		record_rbegin.d *= 24;
		record_rend.d *= 24;
		/* FALLTHROUGH */
	case 'h':
		record_rbegin.d *= 60;
		record_rend.d *= 60;
		/* FALLTHROUGH */
	case 'm':
		record_rbegin.d *= 60;
		record_rend.d *= 60;
		/* FALLTHROUGH */
	case 's':
		record_rbegin.t = double_to_timeval(record_rbegin.d);
		record_rend.t = double_to_timeval(record_rend.d);
		if (!timercmp(&record_rbegin.t, &record_rend.t, <))
			errx(6, "Begin time must be older than end time");
		time_window = true;
		break;
	}
}

/*
 * Handle the events associated with the following elements
 * The passed socket
 * Standard input
 * Communicating clients
 * Elapsed time values
 * This is called in an endless loop to do the following things:
 *   Setup select(2) arguments
 *   Call select(2)
 *   Process events that can be processed
 */
static void
handle_events(int sock)
{
	fd_set source_fds;
	fd_set sink_fds;
	struct timeval wait_time, *waitptr;
	bool set_waitptr;
	int i, max_fd, nfds;
	socklen_t len;
	struct sockaddr_un remote;

	/* Set the fds that interest us */
	FD_ZERO(&source_fds);
	FD_ZERO(&sink_fds);
	waitptr = NULL;

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
	set_waitptr = false;
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
			} else if (time_window)
				set_waitptr = true;
			break;
		case s_sending_response:	/* A response is being sent */
			FD_SET(clients[i].fd, &sink_fds);
			max_fd = MAX(clients[i].fd, max_fd);
			break;
		}

	if (set_waitptr) {
		/*
		 * Find the oldest buffer that hasn't yet entered the time
		 * window and arrange for select(2) to wait for it to enter.
		 */
		struct buffer *bp, *candidate_buffer = NULL;
		struct timeval now, abs_rbegin_time;

		gettimeofday(&now, NULL);
		timersub(&now, &record_rbegin.t, &abs_rbegin_time);
		DPRINTF("have to wait for a buffer to enter window %lld.%06d",
			(long long)abs_rbegin_time.tv_sec, (int)abs_rbegin_time.tv_usec);
		/*
		 * rbegin = 10
		 * 13            19     20    21  23
		 * abs_rbegin    ...    ... tail  now
		 */
		for (bp = tail; bp && timercmp(&bp->timestamp, &abs_rbegin_time, >); bp = bp->prev)
			candidate_buffer = bp;
		if (candidate_buffer) {
			/* There is a buffer worth waiting for */
			waitptr = &wait_time;
			timersub(&candidate_buffer->timestamp, &abs_rbegin_time, waitptr);
			DPRINTF("waiting %lld.%06d for %p %lld.%06d to enter the window",
				(long long)wait_time.tv_sec, (int)wait_time.tv_usec,
				candidate_buffer,
				(long long)candidate_buffer->timestamp.tv_sec,
				(int)candidate_buffer->timestamp.tv_usec);
		} else
			DPRINTF("No candidate buffer found");
	}

	TIMESTAMP("Calling select");
	if ((nfds = select(max_fd + 1, &source_fds, &sink_fds, NULL, waitptr)) < 0)
		err(3, "select");
	TIMESTAMP("Select returns");

	if (FD_ISSET(STDIN_FILENO, &source_fds))
		buffer_read();

	if (waitptr && nfds == 0)
		/* Expired timer; records may have entered the window */
		update_current_record();

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
				clients[i].write_begin = current_record_begin;
				clients[i].write_end = current_record_end;
				clients[i].state = s_sending_response;
				oldest_buffer_being_written =
					oldest_buffer(oldest_buffer_being_written, clients[i].write_begin.b);
				write_record(&clients[i], 1);
			}
			break;
		case s_sending_response:	/* A response is being written */
			if (FD_ISSET(clients[i].fd, &sink_fds))
				write_record(&clients[i], 0);
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

int
main(int argc, char *argv[])
{
	int sock;
	socklen_t len;
	struct sockaddr_un local;

	parse_arguments(argc, argv);

	if (strlen(socket_path) >= sizeof(local.sun_path) - 1)
		errx(6, "Socket name [%s] must be shorter than %lu characters",
			socket_path, (long unsigned)sizeof(local.sun_path));

	(void)unlink(socket_path);

	if ((sock = socket(AF_UNIX, SOCK_STREAM, 0)) == -1)
		err(2, "Error creating socket");

	local.sun_family = AF_UNIX;
	strcpy(local.sun_path, socket_path);
	len = strlen(local.sun_path) + 1 + sizeof(local.sun_family);
	if (bind(sock, (struct sockaddr *)&local, len) == -1)
		err(3, "Error binding socket to Unix domain address %s", argv[1]);

	if (listen(sock, 5) == -1)
		err(4, "listen");

	non_block(sock);

	reached_eof = false;
	for (;;)
		handle_events(sock);
}

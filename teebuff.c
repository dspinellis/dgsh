/*
 * Copyright 2013 Diomidis Spinellis
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
#include <assert.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define MIN(a, b) ((a) < (b) ? (a) : (b))

/* #define DEBUG */

/*
 * Data that can't be written is stored in a sequential pool of buffers
 */

static int buffer_size = 1024 * 1024;

static char **buffers;

static off_t source_pos;

/* A buffer that can be used for reading */
struct buffer {
	void *p;	/* Memory pointer */
	size_t size;	/* Buffer size */
};

/* Information regarding the files we write to */
struct sink_info {
	char *name;		/* Output file name */
	int fd;			/* Output file descriptor */
	off_t sink_pos;		/* Position up to which written */
	int active;		/* True if this sink is still active */
};

/*
 * Allocate memory for the specified pool
 */
static void
memory_allocate(int pool)
{
	static int pool_size;
	static int allocated_pool_end;
	int i;

	if (pool < allocated_pool_end)
		return;

	/* Resize bank, if needed. One iteration should suffice. */
	while (pool >= pool_size) {
		if (pool_size == 0)
			pool_size = 1;
		else
			pool_size *= 2;
		if ((buffers = realloc(buffers, pool_size * sizeof(char *))) == NULL)
			err(1, "Unable to reallocate buffer pool bank");
	}

	/* Allocate buffer memory [allocated_pool_end, pool]. */
	for (i = allocated_pool_end; i <= pool; i++) {
		if ((buffers[i] = malloc(buffer_size)) == NULL)
			err(1, "Unable to allocate %d bytes for buffer %d", buffer_size, i);
		#ifdef DEBUG
		fprintf(stderr, "Allocated buffer %d to %p\n", i, buffers[i]);
		#endif
	}
	allocated_pool_end = pool + 1;
}

/*
 * Ensure that pool buffers from [0,pos) are free.
 */
static void
memory_free(off_t pos)
{
	static int pool_begin = 0;
	int pool_end = pos / buffer_size;
	int i;

	for (i = pool_begin; i < pool_end; i++) {
		free(buffers[i]);
		#ifdef DEBUG
		buffers[i] = NULL;
		fprintf(stderr, "Freed buffer %d (pos = %ld, begin=%d end=%d)\n",
			i, (long)pos, pool_begin, pool_end);
		#endif
	}
	pool_begin = pool_end;
}

/*
 * Return the buffer to write to for reading from a file from
 * position onward, ensuring that sufficient memory is allocated.
 */
static struct buffer
source_buffer(off_t pos)
{
	struct buffer b;
	int pool = pos / buffer_size;
	size_t pool_offset = pos % buffer_size;

	memory_allocate(pool);
	b.p = buffers[pool] + pool_offset;
	b.size = buffer_size - pool_offset;
	#ifdef DEBUG
	fprintf(stderr, "Source buffer(%ld) returns pool %d(%p) o=%ld l=%ld a=%p\n",
		(long)pos, pool, buffers[pool], (long)pool_offset, (long)b.size, b.p);
	#endif
	return b;
}

/*
 * Return a buffer to read from for writing to a file from a position onward
 */
static struct buffer
sink_buffer(off_t pos)
{
	struct buffer b;
	int pool = pos / buffer_size;
	size_t pool_offset = pos % buffer_size;
	size_t source_bytes = source_pos - pos;

	b.p = buffers[pool] + pool_offset;
	b.size = MIN(buffer_size - pool_offset, source_bytes);
	#ifdef DEBUG
	fprintf(stderr, "Sink buffer(%ld) returns pool %d(%p) o=%ld l=%ld a=%p\n",
		(long)pos, pool, buffers[pool], (long)pool_offset, (long)b.size, b.p);
	#endif
	return b;
}

/*
 * Read from the source into the memory buffer
 * Return the number of bytes read, or 0 on end of file.
 */
static size_t
source_read(fd_set *source_fds)
{
	int n;
	struct buffer b;

	b = source_buffer(source_pos);
	if ((n = read(STDIN_FILENO, b.p, b.size)) == -1)
		err(3, "Read from standard input");
	source_pos += n;
	#ifdef DEBUG
	fprintf(stderr, "Read %d out of %d bytes\n", n, b.size);
	#endif
	return n;
}


/*
 * Write out from the memory buffer to the sinks where write will not block.
 * Free memory no more needed even by the write pointer farthest behind.
 * Return the number of bytes written.
 */
static size_t
sink_write(fd_set *sink_fds, struct sink_info *files, int nfiles)
{
	struct sink_info *si;
	off_t min_pos = source_pos;
	size_t written = 0;

	for (si = files; si < files + nfiles; si++) {
		if (FD_ISSET(si->fd, sink_fds)) {
			int n;
			struct buffer b;

			b = sink_buffer(si->sink_pos);
			n = write(si->fd, b.p, b.size);
			if (n < 0)
				switch (errno) {
				/* EPIPE is acceptable, for the sink's reader can terminate early. */
				case EPIPE:
					si->active = 0;
					#ifdef DEBUG
					fprintf(stderr, "EPIPE for %s\n", si->name);
					#endif
					break;
				default:
					err(2, "Error writing to %s", si->name);
				}
			else {
				si->sink_pos += n;
				written += n;
			}
			#ifdef DEBUG
			fprintf(stderr, "Wrote %d out of %d bytes for file %s\n",
				n, b.size, si->name);
			#endif
		}
		if (si->active)
			min_pos = MIN(min_pos, si->sink_pos);
	}
	memory_free(min_pos);
	return written;
}

static void
usage(const char *name)
{
	fprintf(stderr, "Usage %s [-b buffer_size] [-s] [-l]\n");
	exit(1);
}

int
main(int argc, char *argv[])
{
	int max_fd = 0;
	int i;
	struct sink_info *files;
	int reached_eof = 0;
	int ch;


	while ((ch = getopt(argc, argv, "b:sl")) != -1) {
		switch (ch) {
		case 'b':
			buffer_size = atoi(optarg);
			break;
		case '?':
		default:
			usage(argv[0]);
		}
	}
	argc -= optind;
	argv += optind;

	if ((files = (struct sink_info *)malloc(argc * sizeof(struct sink_info))) == NULL)
		err(1, NULL);

	/* Open files */
	for (i = 0; i < argc; i++) {
		if ((files[i].fd = open(argv[i], O_WRONLY | O_CREAT | O_TRUNC, DEFFILEMODE)) < 0)
			err(2, "Error opening %s", argv[i]);
		max_fd = MAX(files[i].fd, max_fd);
		files[i].name = argv[i];
		files[i].active = 1;
	}

	/* We will handle SIGPIPE explicitly when calling write(2). */
	signal(SIGPIPE, SIG_IGN);

	/* Copy source to sink without allowing any single file to block us. */
	for (;;) {
		struct sink_info *si;
		fd_set source_fds;
		fd_set sink_fds;
		int active_fds;

		/* Set the fd's we're interested to read/write. */
		FD_ZERO(&source_fds);
		if (!reached_eof)
			FD_SET(STDIN_FILENO, &source_fds);

		active_fds = 0;
		FD_ZERO(&sink_fds);
		for (si = files; si < files + argc; si++)
			if (si->sink_pos < source_pos && si->active) {
				FD_SET(si->fd, &sink_fds);
				active_fds++;
			}


		/* If no read possible, and no writes pending, terminate. */
		if (reached_eof && active_fds == 0)
			return 0;

		/* Block until we can read or write. */
		if (select(max_fd + 1, &source_fds, &sink_fds, NULL, NULL) < 0)
			err(3, "select");

		/* Write to all file descriptors that accept writes. */
		if (sink_write(&sink_fds, files, argc) > 0)
			/*
			 * If we wrote something, we made progress on the
			 * downstream end.  Loop without reading to avoid
			 * allocating excessive buffer memory.
			 */
			continue;

		/* Read, if possible. */
		if (FD_ISSET(STDIN_FILENO, &source_fds))
			if (source_read(&source_fds) == 0)
				reached_eof = 1;
	}
	return 0;
}

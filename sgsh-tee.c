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
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "sgsh.h"

/*
 * Data that can't be written is stored in a sequential pool of buffers,
 * each buffer_size long.
 * As more data is read the buffer pool with the pointers (buffers)
 * is continuously increased; there is no round-robin mechanism.
 * However, as data is written out to all sinks, the actual buffers are
 * freed, thus keeping memory consumption reasonable.
 */

static int buffer_size = 1024 * 1024;

static char **buffers;

/* The position up to which we have read data */
static off_t source_pos_read;

/* A buffer that can be used for reading */
struct buffer {
	void *p;	/* Memory pointer */
	size_t size;	/* Buffer size */
};

/* Maximum amount of memory to allocate. (Set through -S) */
static unsigned long max_mem = 256 * 1024 * 1204;

/* Scatter the output across the files, rather than copying it. */
static bool opt_scatter = false;

/*
 * Split scattered data on blocks of specified size; otherwise on line boundaries
 * Currently there is no support for this option; a -l option should be added.
 */

static bool block_len = 0;

/* Allocated bufffer information */
static int buffers_allocated, buffers_freed, max_buffers_allocated;

/* Set to true when we reach EOF on input */
static bool reached_eof = false;

/* Record terminator */
static char rt = '\n';

/* Linked list of files we write to */
struct sink_info {
	struct sink_info *next;	/* Next list element */
	char *name;		/* Output file name */
	int fd;			/* Output file descriptor */
	off_t pos_written;	/* Position up to which written */
	off_t pos_to_write;	/* Position up to which to write */
	bool active;		/* True if this sink is still active */
};

/* Linked list of files we read from */
struct source_info {
	struct source_info *next;	/* Next list element */
	char *name;			/* Input file name */
	int fd;				/* Input file descriptor */
};

/*
 * States for the copying engine.
 * Two disjunct sets:
 * input side buffering (ib) and output side buffering (ob)
 * Input side buffering will always read input if it is available,
 * presenting an infinite output buffer to the upstream process.
 * The output-side buffering will read input only if at least one
 * active output buffer is empty.
 * The setting in effect is determined by the program's -i flag.
 *
 * States read_ib and read_ob have select return:
 * - if data is available for reading,
 * - if the process can write out data already read,
 * - not if the process can write to other fds
 *
 * States drain_ib and write_ob have select return
 * if the process can write to any fd.
 * Waiting on all output buffers (not only those with data)
 * is needed to avoid starvation of processes with no pending output.
 * If a program can accept data this process will then transition to
 * read_* to read more data.
 *
 * State drain_ob has select return only if the process can write out
 * data already read.
 */
enum state {
	read_ib,		/* Must read input; write if data available */
	read_ob,		/* As above, but don't transition to write */
	drain_ib,		/* Don't read input; write if possible */
	drain_ob,		/* Empty data buffers by writing */
	write_ob,		/* Write data, before reading */
};


/*
 * Allocate memory for the specified pool
 * Return false if we're out of memory by reaching the user-specified limit
 * or a system's hard limit.
 */
static bool
memory_allocate(int pool)
{
	static int pool_size;
	static int allocated_pool_end;
	int i, orig_pool_size;
	char **orig_buffers;

	if (pool < allocated_pool_end)
		return true;

	DPRINTF("Buffers allocated: %d Freed: %d", buffers_allocated, buffers_freed);
	/* Check soft memory limit through allocated plus requested memory. */
	if (((buffers_allocated - buffers_freed) + (allocated_pool_end - pool + 1)) * buffer_size > max_mem)
		return false;

	/* Keep original values to undo on failure. */
	orig_pool_size = pool_size;
	orig_buffers = buffers;
	/* Resize bank, if needed. One iteration should suffice. */
	while (pool >= pool_size) {
		if (pool_size == 0)
			pool_size = 1;
		else
			pool_size *= 2;
		if ((buffers = realloc(buffers, pool_size * sizeof(char *))) == NULL) {
			DPRINTF("Unable to reallocate buffer pool bank");
			pool_size = orig_pool_size;
			buffers = orig_buffers;
			return false;
		}
	}

	/* Allocate buffer memory [allocated_pool_end, pool]. */
	for (i = allocated_pool_end; i <= pool; i++) {
		if ((buffers[i] = malloc(buffer_size)) == NULL) {
			DPRINTF("Unable to allocate %d bytes for buffer %d", buffer_size, i);
			max_buffers_allocated = MAX(buffers_allocated - buffers_freed, max_buffers_allocated);
			allocated_pool_end = i;
			return false;
		}
		DPRINTF("Allocated buffer %d to %p", i, buffers[i]);
		buffers_allocated++;
		max_buffers_allocated = MAX(buffers_allocated - buffers_freed, max_buffers_allocated);
	}
	allocated_pool_end = pool + 1;
	return true;
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

	DPRINTF("memory_free: pos = %ld, begin=%d end=%d",
		(long)pos, pool_begin, pool_end);
	for (i = pool_begin; i < pool_end; i++) {
		free(buffers[i]);
		buffers_freed++;
		#ifdef DEBUG
		buffers[i] = NULL;
		#endif
		DPRINTF("Freed buffer %d (pos = %ld, begin=%d end=%d)",
			i, (long)pos, pool_begin, pool_end);
	}
	pool_begin = pool_end;
}

/*
 * Set the buffer to write to for reading from a file from
 * position onward, ensuring that sufficient memory is allocated.
 * Return false if no memory is available.
 */
static bool
source_buffer(off_t pos, /* OUT */ struct buffer *b)
{
	int pool = pos / buffer_size;
	size_t pool_offset = pos % buffer_size;

	if (!memory_allocate(pool))
		return false;
	b->p = buffers[pool] + pool_offset;
	b->size = buffer_size - pool_offset;
	DPRINTF("Source buffer(%ld) returns pool %d(%p) o=%ld l=%ld a=%p",
		(long)pos, pool, buffers[pool], (long)pool_offset, (long)b->size, b->p);
	return true;
}

/*
 * Return a buffer to read from for writing to a file from a position onward
 * When processing lines, b.size can be 0
 */
static struct buffer
sink_buffer(struct sink_info *ofp)
{
	struct buffer b;
	int pool = ofp->pos_written / buffer_size;
	size_t pool_offset = ofp->pos_written % buffer_size;
	size_t source_bytes = ofp->pos_to_write - ofp->pos_written;

	b.p = buffers[pool] + pool_offset;
	b.size = MIN(buffer_size - pool_offset, source_bytes);
	DPRINTF("Sink buffer(%ld-%ld) returns pool %d(%p) o=%ld l=%ld a=%p",
		(long)ofp->pos_written, (long)ofp->pos_to_write, pool, buffers[pool], (long)pool_offset, (long)b.size, b.p);
	return b;
}

/*
 * Return a pointer to read from for writing to a file from a position onward
 */
static char *
sink_pointer(off_t pos_written)
{
	int pool = pos_written / buffer_size;
	size_t pool_offset = pos_written % buffer_size;

	return buffers[pool] + pool_offset;
}

/*
 * Return the size of a buffer region that can be read for the specified endpoints
 */
static size_t
sink_buffer_length(off_t start, off_t end)
{
	size_t pool_offset = start % buffer_size;
	size_t source_bytes = end - start;

	DPRINTF("sink_buffer_length(%ld, %ld) = %ld",
		(long)start, (long)end,  (long)MIN(buffer_size - pool_offset, source_bytes));
	return MIN(buffer_size - pool_offset, source_bytes);
}


/* The result of the following read operation. */
enum read_result {
	read_ok,	/* Normal read */
	read_oom,	/* Out of buffer memory */
	read_again,	/* EAGAIN */
	read_eof,	/* EOF (0 bytes read) */
};

/*
 * Read from the source into the memory buffer
 * Return the number of bytes read, or -1 on end of file.
 */
static enum read_result
source_read(struct source_info *ifp)
{
	int n;
	struct buffer b;

	if (!source_buffer(source_pos_read, &b)) {
		DPRINTF("Memory full");
		/* Provide some time for the output to drain. */
		return read_oom;
	}
	if ((n = read(ifp->fd, b.p, b.size)) == -1)
		switch (errno) {
		case EAGAIN:
			DPRINTF("EAGAIN on %s", ifp->name);
			return read_again;
		default:
			err(3, "Read from %s", ifp->name);
		}
	source_pos_read += n;
	DPRINTF("Read %d out of %d bytes from %s", n, b.size, ifp->name);
	/* Return -1 on EOF */
	return n ? read_ok : read_eof;
}

/*
 * Allocate available read data to empty sinks that can be written to,
 * by adjusting their pos_written and pos_to_write pointers.
 */
static void
allocate_data_to_sinks(fd_set *sink_fds, struct sink_info *files)
{
	struct sink_info *ofp;
	int available_sinks = 0;
	off_t pos_assigned = 0;
	size_t available_data, data_per_sink;
	size_t data_to_assign = 0;
	bool use_reliable = false;

	/* Easy case: distribute to all files. */
	if (!opt_scatter) {
		for (ofp = files; ofp; ofp = ofp->next)
			ofp->pos_to_write = source_pos_read;
		return;
	}

	/*
	 * Difficult case: fair scattering across available sinks
	 */

	/* Determine amount of fresh data to write and number of available sinks. */
	for (ofp = files; ofp; ofp = ofp->next) {
		pos_assigned = MAX(pos_assigned, ofp->pos_to_write);
		if (ofp->pos_written == ofp->pos_to_write && FD_ISSET(ofp->fd, sink_fds))
			available_sinks++;
	}

	/*
	 * Ensure we operate in a continuous memory region by clamping
	 * the length of the available data to terminate at the end of
	 * the buffer.
	 */
	available_data = sink_buffer_length(pos_assigned, source_pos_read);

	if (available_sinks == 0)
		return;

	/* Assign data to sinks. */
	data_per_sink = available_data / available_sinks;
	for (ofp = files; ofp; ofp = ofp->next) {
		/* Move to next file if this has data to write, or isn't ready. */
		if (ofp->pos_written != ofp->pos_to_write || !FD_ISSET(ofp->fd, sink_fds))
			continue;

		DPRINTF("pos_assigned=%ld source_pos_read=%ld available_data=%ld available_sinks=%d data_per_sink=%ld",
			(long)pos_assigned, (long)source_pos_read, (long)available_data, available_sinks, (long)data_per_sink);
		/* First file also gets the remainder bytes. */
		if (data_to_assign == 0)
			data_to_assign = sink_buffer_length(pos_assigned,
				pos_assigned + data_per_sink + available_data % available_sinks);
		else
			data_to_assign = data_per_sink;
		/*
		 * Assign data_to_assign to *ofp (pos_written, pos_to_write),
		 * and advance pos_assigned.
		 */
		ofp->pos_written = pos_assigned;		/* Initially nothing has been written. */
		if (block_len == 0) {			/* Write whole lines */
			if (available_data > buffer_size / 2 && !use_reliable) {
				/*
				 * Efficient algorithm:
				 * Assume that multiple lines appear in data_per_sink.
				 * Go to a calculated boundary and scan backward to find
				 * a new line.
				 */
				off_t data_end = pos_assigned + data_to_assign - 1;

				for (;;) {
					if (*sink_pointer(data_end) == rt) {
						pos_assigned = data_end + 1;
						break;
					}
					data_end--;
					if (data_end + 1 == pos_assigned) {
						/*
						 * If no newline was found with backward scanning
						 * degenerate to the efficient algorithm. This will
						 * scan further forward, and can defer writing the
						 * last chunk, until more data is read.
						 */
						use_reliable = true;
						goto reliable;
					}
				}
			} else {
				/*
				 * Reliable algorithm:
				 * Scan forward for new lines until at least
				 * data_per_sink are covered, or we reach the end of available data.
				 * Keep a record of the last encountered newline.
				 * This is used to backtrack when we scan past the end of the
				 * available data.
				 */
				off_t data_end, last_nl;

			reliable:
				last_nl = -1;
				data_end = pos_assigned;
				for (;;) {
					if (data_end >= source_pos_read) {
						if (last_nl != -1) {
							pos_assigned = last_nl + 1;
							break;
						} else {
							/* No newline found in buffer; defer writing. */
							ofp->pos_to_write = pos_assigned;
							DPRINTF("scatter to file[%s] no newline from %ld to %ld",
								ofp->name, (long)pos_assigned, (long)data_end);
							return;
						}
					}

					if (*sink_pointer(data_end) == rt) {
						last_nl = data_end;
						if (data_end - pos_assigned > data_per_sink) {
							pos_assigned = data_end + 1;
							break;
						}
					}
					data_end++;
				}
			}
		} else
			pos_assigned += data_to_assign;
		ofp->pos_to_write = pos_assigned;
		DPRINTF("scatter to file[%s] pos_written=%ld pos_to_write=%ld",
			ofp->name, (long)ofp->pos_written, (long)ofp->pos_to_write);
	}
}


/*
 * Write out from the memory buffer to the sinks where write will not block.
 * Free memory no more needed even by the write pointer farthest behind.
 * Return the number of bytes written.
 */
static size_t
sink_write(fd_set *sink_fds, struct sink_info *files)
{
	struct sink_info *ofp;
	off_t min_pos = source_pos_read;
	size_t written = 0;

	allocate_data_to_sinks(sink_fds, files);
	for (ofp = files; ofp; ofp = ofp->next) {
		if (ofp->active && FD_ISSET(ofp->fd, sink_fds)) {
			int n;
			struct buffer b;

			b = sink_buffer(ofp);
			if (b.size == 0)
				/* Can happen when a line spans a buffer */
				n = 0;
			else
				n = write(ofp->fd, b.p, b.size);
			if (n < 0)
				switch (errno) {
				/* EPIPE is acceptable, for the sink's reader can terminate early. */
				case EPIPE:
					ofp->active = false;
					(void)close(ofp->fd);
					DPRINTF("EPIPE for %s", ofp->name);
					break;
				case EAGAIN:
					DPRINTF("EAGAIN for %s", ofp->name);
					n = 0;
					break;
				default:
					err(2, "Error writing to %s", ofp->name);
				}
			else {
				ofp->pos_written += n;
				written += n;
			}
			DPRINTF("Wrote %d out of %d bytes for file %s pos_written=%lu",
				n, b.size, ofp->name, ofp->pos_written);
		}
		if (ofp->active)
			min_pos = MIN(min_pos, ofp->pos_written);
	}
	memory_free(min_pos);
	DPRINTF("Wrote %d total bytes", written);
	return written;
}

static void
usage(const char *name)
{
	fprintf(stderr, "Usage %s [-b size] [-i file] [-IMs] [-o file] [-m size] [-t char]\n"
		"-b size"	"\tSpecify the size of the buffer to use (used for stress testing)\n"
		"-I"		"\tInput-side buffering\n"
		"-i file"	"\tGather input from specified file\n"
		"-m size[k|M|G]""\tSpecify the maximum buffer memory size\n"
		"-M"		"\tProvide memory use statistics on termination\n"
		"-o file"	"\tScatter output to specified file\n"
		"-s"		"\tScatter the input across the files, rather than copying it to all\n"
		"-t char"	"\tProcess char-terminated records (newline default)\n",
		name);
	exit(1);
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
non_block(int fd, const char *name)
{
	int flags = fcntl(fd, F_GETFL, 0);
	if (flags < 0)
		err(2, "Error getting flags for %s", name);
	if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0)
		err(2, "Error setting %s to non-blocking mode", name);
}

/*
 * Show the arguments passed to select(2)
 * in human-readable form
 */
static void
show_select_args(const char *msg, struct source_info *ifp, fd_set *source_fds, fd_set *sink_fds, struct sink_info *files)
{
	#ifdef DEBUG
	struct sink_info *ofp;

	fprintf(stderr, "%s: ", msg);
	if (FD_ISSET(ifp->fd, source_fds))
			fprintf(stderr, "%s ", ifp->name);
	for (ofp = files; ofp; ofp = ofp->next)
		if (FD_ISSET(ofp->fd, sink_fds))
			fprintf(stderr, "%s ", ofp->name);
	fputc('\n', stderr);
	#endif
}

static void
show_state(enum state state)
{
	#ifdef DEBUG
	char *s;

	switch (state) {
	case read_ib:
		s = "read_ib";
		break;
	case read_ob:
		s = "read_ob";
		break;
	case drain_ib:
		s = "drain_ib";
		break;
	case drain_ob:
		s = "drain_ob";
		break;
	case write_ob:
		s = "write_ob";
		break;
	}
	fprintf(stderr, "State: %s\n", s);
	#endif
}

/* Parse the specified option as a size with a suffix and return its value. */
static unsigned long
parse_size(const char *progname, const char *opt)
{
	char size;
	unsigned long n;

	size = 'b';
	if (sscanf(opt, "%lu%c", &n, &size) < 1)
		usage(progname);
	switch (size) {
	case 'B' : case 'b':
		return n;
	case 'K' : case 'k':
		return n * 1024;
	case 'M' : case 'm':
		return n * 1024 * 1024;
	case 'G' : case 'g':
		return n * 1024 * 1024 * 1024;
	default:
		usage(progname);
	}
	/* NOTREACHED */
	return 0;
}

int
main(int argc, char *argv[])
{
	int max_fd = 0;
	struct sink_info *ofiles = NULL, *ofp;
	struct source_info *ifiles = NULL, *ifp, *end = NULL;
	int ch;
	const char *progname = argv[0];
	enum state state = read_ob;
	bool opt_memory_stats = false;

	while ((ch = getopt(argc, argv, "b:S:sIi:Mm:o:t:")) != -1) {
		switch (ch) {
		case 'b':
			buffer_size = (int)parse_size(progname, optarg);
			break;
		case 'I':
			state = read_ib;
			break;
		case 'i':	/* Specify input file */
			if ((ifp = (struct source_info *)malloc(sizeof(struct source_info))) == NULL)
				err(1, NULL);
			if ((ifp->fd = open(optarg, O_RDONLY)) < 0)
				err(2, "Error opening %s", optarg);
			max_fd = MAX(ifp->fd, max_fd);
			ifp->name = strdup(optarg);
			non_block(ifp->fd, ifp->name);

			/* At file at the end of the linked list */
			ifp->next = NULL;
			if (end)
				end->next = ifp;
			else
				ifiles = ifp;
			end = ifp;
			break;
		case 'm':
			max_mem = parse_size(progname, optarg);
			break;
		case 'M':	/* Provide memory use statistics on termination */
			opt_memory_stats = true;
			break;
		case 'o':	/* Specify output file */
			if ((ofp = (struct sink_info *)malloc(sizeof(struct sink_info))) == NULL)
				err(1, NULL);
			if ((ofp->fd = open(optarg, O_WRONLY | O_CREAT | O_TRUNC, DEFFILEMODE)) < 0)
				err(2, "Error opening %s", optarg);
			max_fd = MAX(ofp->fd, max_fd);
			ofp->name = strdup(optarg);
			ofp->active = true;
			ofp->pos_written = ofp->pos_to_write = 0;
			non_block(ofp->fd, ofp->name);
			ofp->next = ofiles;
			ofiles = ofp;
			break;
		case 's':
			opt_scatter = true;
			break;
		case 't':	/* Record terminator */
			/* We allow \0 as rt */
			if (strlen(optarg) > 1)
				usage(progname);
			rt = *optarg;
			break;
		case '?':
		default:
			usage(progname);
		}
	}
	argc -= optind;
	argv += optind;

	if (argc)
		usage(progname);

	if (buffer_size > max_mem)
		errx(1, "Buffer size %d is larger than the program's maximum memory limit %lu", buffer_size, max_mem);

	if (ofiles == NULL) {
		/* Output to stdout */
		if ((ofp = (struct sink_info *)malloc(sizeof(struct sink_info))) == NULL)
			err(1, NULL);
		ofp->fd = STDOUT_FILENO;
		max_fd = MAX(ofp->fd, max_fd);
		ofp->name = "standard output";
		ofp->active = true;
		ofp->pos_written = ofp->pos_to_write = 0;
		non_block(ofp->fd, ofp->name);
		ofp->next = ofiles;
		ofiles = ofp;
	}

	if (ifiles == NULL) {
		/* Input from stdin */
		if ((ifp = (struct source_info *)malloc(sizeof(struct source_info))) == NULL)
			err(1, NULL);
		ifp->fd = STDIN_FILENO;
		max_fd = MAX(ifp->fd, max_fd);
		ifp->name = "standard input";
		non_block(ifp->fd, ifp->name);
		ifp->next = ifiles;
		ifiles = ifp;
	}


	/* We will handle SIGPIPE explicitly when calling write(2). */
	signal(SIGPIPE, SIG_IGN);

	ifp = ifiles;

	/* Copy source to sink without allowing any single file to block us. */
	for (;;) {
		fd_set source_fds;
		fd_set sink_fds;

		show_state(state);
		/* Set the fd's we're interested to read/write; close unneeded ones. */
		FD_ZERO(&source_fds);
		FD_ZERO(&sink_fds);

		if (!reached_eof && (state == read_ib || state == read_ob))
			FD_SET(ifp->fd, &source_fds);

		for (ofp = ofiles; ofp; ofp = ofp->next)
			if (ofp->active) {
				switch (state) {
				case read_ib:
				case read_ob:
				case drain_ob:
					if (ofp->pos_written < ofp->pos_to_write)
						FD_SET(ofp->fd, &sink_fds);
					break;
				case drain_ib:
				case write_ob:
					FD_SET(ofp->fd, &sink_fds);
					break;
				}
			}


		/* Block until we can read or write. */
		show_select_args("Entering select", ifp, &source_fds, &sink_fds, ofiles);
		if (select(max_fd + 1, &source_fds, &sink_fds, NULL, NULL) < 0)
			err(3, "select");
		show_select_args("Select returned", ifp, &source_fds, &sink_fds, ofiles);

		/* Write to all file descriptors that accept writes. */
		if (sink_write(&sink_fds, ofiles) > 0) {
			/*
			 * If we wrote something, we made progress on the
			 * downstream end.  Loop without reading to avoid
			 * allocating excessive buffer memory.
			 */
			if (state == drain_ob)
				state = write_ob;
			continue;
		}

		if (reached_eof) {
			int active_fds = 0;

			for (ofp = ofiles; ofp; ofp = ofp->next)
				if (ofp->active) {
					if (ofp->pos_written < ofp->pos_to_write)
						active_fds++;
					else {
						DPRINTF("Retiring file %s pos_written=pos_to_write=%ld source_pos_read=%ld",
							ofp->name, (long)ofp->pos_written, (long)source_pos_read);
						/* No more data to write; close fd to avoid deadlocks downstream. */
						if (close(ofp->fd) == -1)
							err(2, "Error closing %s", ofp->name);
						ofp->active = false;
					}
				}
			if (active_fds == 0) {
				/* If no read possible, and no writes pending, terminate. */
				if (opt_memory_stats)
					fprintf(stderr, "Buffers allocated: %d Freed: %d Maximum allocated: %d\n",
						buffers_allocated, buffers_freed, max_buffers_allocated);
				return 0;
			}
		}

		/*
		 * Note that we never reach this point after a successful write.
		 * See the continue statement above.
		 */
		switch (state) {
		case read_ib:
			/* Read, if possible. */
			if (FD_ISSET(ifp->fd, &source_fds))
				switch (source_read(ifp)) {
				case read_eof:
					ifp = ifp->next;
					if (ifp == NULL) {
						reached_eof = true;
						state = drain_ib;
					}
					break;
				case read_oom:	/* Cannot fullfill promise to never block source, so bail out. */
					errx(1, "Out of memory with input-side buffering specified");
					break;
				case read_ok:
				case read_again:
					break;
				}
			break;
		case read_ob:
			/* Read, if possible. */
			if (FD_ISSET(ifp->fd, &source_fds))
				switch (source_read(ifp)) {
				case read_eof:
					ifp = ifp->next;
					if (ifp == NULL) {
						reached_eof = true;
						state = drain_ib;
					}
					break;
				case read_again:
					break;
				case read_oom:	/* Allow buffers to empty. */
					state = drain_ob;
					break;
				case read_ok:
					state = write_ob;
					break;
				}
			break;
		case drain_ib:
			break;
		case drain_ob:
			if (reached_eof)
				state = write_ob;
			else
				state = read_ob;
			break;
		case write_ob:
			if (!reached_eof)
				state = read_ob;
			break;
		}
	}
}

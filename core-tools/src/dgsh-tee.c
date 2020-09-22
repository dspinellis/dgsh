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

#ifdef __linux__
#define _XOPEN_SOURCE 500	// pread pwrite
#endif

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

#include "dgsh.h"
#include "dgsh-debug.h"
#include "minmax.h"

#if defined(DEBUG_DATA)
#define DATA_DUMP 1
#else
#define DATA_DUMP 0
#endif

/*
 * Data that can't be written is stored in a sequential pool of buffers,
 * each buffer_size long.
 * As more data is read the buffer pool with the pointers (buffers)
 * is continuously increased; there is no round-robin mechanism.
 * However, as data is written out to all sinks, the actual buffers are
 * freed, thus keeping memory consumption reasonable.
 */

static int buffer_size = 1024 * 1024;

/*
 * A buffer in the memory pool.
 */
struct pool_buffer {
	void *p;		/* Memory allocated for it (b_memory) */
	enum {
		s_none,		/* Stored nowhere */
		s_memory,	/* Stored in memory */
		s_memory_backed,/* Stored in memory and backed to temporary file */
		s_file		/* Stored in temporary file */
	} s; 			/* Where it is stored */
};

/*
 * A pool of buffers
 */
struct buffer_pool {
	struct pool_buffer *buffers;	/* A dynamically adjusted vector of buffers */
	int pool_size;			/* Size of allocated pool_buffers vectors */
	int allocated_pool_end;		/* The first buffer in the above pool that has not been allocated */

	/* Allocated bufffer information */
	int buffers_allocated, buffers_freed, max_buffers_allocated;

	/* Paging information */
	int buffers_paged_out, buffers_paged_in, pages_freed;

	int page_out_ptr;		/* Pointer to first buffer to page out */
	int page_file_fd;		/* File descriptor of temporary file used for paging buffer pool */
	int free_pool_begin;		/* Start of freed area */
};


/* Construct a new buffer pool object */
static struct buffer_pool *
new_buffer_pool(void)
{
	struct buffer_pool *bp;

	if ((bp = (struct buffer_pool *)malloc(sizeof(struct buffer_pool))) == NULL)
		err(1, NULL);
	bp->buffers = NULL;
	bp->pool_size = 0;
	bp->page_out_ptr = 0;
	bp->page_file_fd = -1;
	bp->free_pool_begin = 0;

	bp->allocated_pool_end = 0;

	bp->buffers_allocated = bp->buffers_freed = bp->max_buffers_allocated =
	bp->buffers_paged_out = bp->buffers_paged_in = bp->pages_freed = 0;

	return bp;
}

/*
 * A buffer that is used for I/O.
 * It points to a part of a pool buffer.
 */
struct io_buffer {
	void *p;	/* Memory pointer */
	size_t size;	/* Buffer size */
};

/* Maximum amount of memory to allocate. (Set through -S) */
static unsigned long max_mem = 256 * 1024 * 1204;

/* Scatter the output across the files, rather than copying it. */
static bool opt_scatter = false;

/*
 * When set, permute the inputs to the specified outputs
 * Ordinals and number of the destination outputs
 */
static int *permute_dest = NULL;
static int permute_n = 0;

/* Use a temporary file for overflowing buffered data */
static bool use_tmp_file = false;

/* User-specified temporary directory */
static char *opt_tmp_dir = NULL;

/*
 * Split scattered data on blocks of specified size; otherwise on line boundaries
 * Currently there is no support for this option; a -l option should be added.
 */

static bool block_len = 0;

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
	struct source_info *ifp;/* Input file we read from */
	bool chain_last;	/* True if last element in a group; Writing  (copy or scatter)
				   should not continue to next element */
};

/* Construct a new sink_info object */
static struct sink_info *
new_sink_info(const char *name)
{
	struct sink_info *ofp;

	if ((ofp = (struct sink_info *)malloc(sizeof(struct sink_info))) == NULL)
		err(1, NULL);
	ofp->name = name ? strdup(name) : NULL;
	ofp->active = true;
	ofp->pos_written = ofp->pos_to_write = 0;
	ofp->next = NULL;
	return ofp;
}

/* Linked list of files we read from */
struct source_info {
	struct source_info *next;	/* Next list element */
	char *name;			/* Input file name */
	int fd;				/* Input file descriptor */
	struct buffer_pool *bp;		/* Buffers where pending input is stored */
	off_t source_pos_read;		/* The position up to which all sinks have read data */
	bool reached_eof;		/* True if we reached EOF for this source */
	off_t read_min_pos;		/* Minimum position read by all sinks */
	bool active;			/* True if this is a source that should be currently
					   read (rather than chained later on) */
	bool is_read;			/* True if an active sink reads it */
	bool chain_last;		/* True if reading should stop at this element rather
					   than continue to the next element */
};

/* Return the name of a source or sink */
#define fp_name(fp) ((fp)->name ? (fp)->name : fd_name((fp)->fd))
static char *
fd_name(int fd)
{
	static char buff[40];

	sprintf(buff, "fd(%d)", fd);
	return buff;
}



/* Construct a new source_info object */
static struct source_info *
new_source_info(const char *name)
{
	struct source_info *ifp;

	if ((ifp = (struct source_info *)malloc(sizeof(struct source_info))) == NULL)
		err(1, NULL);
	ifp->name = name ? strdup(name) : NULL;
	ifp->bp = new_buffer_pool();
	ifp->source_pos_read = 0;
	ifp->reached_eof = false;
	ifp->next = NULL;
	return ifp;
}

/*
 * States for the copying engine.
 * Two disjunct sets:
 * input side buffering (ib) and output side buffering (ob)
 * Input side buffering will always read input if it is available,
 * presenting an infinite output buffer to the upstream process.
 * The output-side buffering will read input only if at least one
 * active output buffer is empty.
 * The setting in effect is determined by the program's -I flag.
 *
 * States read_ib and read_ob have select return:
 * - if data is available for reading,
 * - if the process can write out data already read,
 * - not if the process can write to other fds
 *
 * States drain_ib and write_ob have select return
 * if the process can write to any fd.
 * Waiting on all output buffers (not only those with data)
 * is needed to avoid starvation of downstream processes
 * when no output is available.
 * If a program can accept data this process will then transition to
 * read_* to read more data.
 *
 * State drain_ob has select return only if the process can write out
 * data already read.
 *
 * See also the diagram tee-state.dot
 */
enum state {
	read_ib,		/* Must read input; write if data available */
	read_ob,		/* As above, but don't transition to write */
	drain_ib,		/* Don't read input; write if possible */
	drain_ob,		/* Empty data buffers by writing */
	write_ob,		/* Write data, before reading */
};

/*
 * Return the total number of bytes required for storing all buffers
 * up to the specified memory pool
 */
static unsigned long
memory_pool_size(struct buffer_pool *bp, int pool)
{
	return ((bp->buffers_allocated - bp->buffers_freed) + (pool - bp->allocated_pool_end + 1)) * buffer_size;
}

/* Write half of the allocated buffer pool to the temporary file */
static void
page_out(struct buffer_pool *bp)
{
	if (bp->page_file_fd == -1) {
		char *template;

		/*
		 * Create a temporary file that will be deleted on exit.
		 * The location follows tempnam rules (argument, TMPDIR,
		 * P_tmpdir, /tmp), while the creation through mkstemp
		 * avoids race conditions.
		 */
		if ((template = tempnam(opt_tmp_dir, "sg-")) == NULL)
			err(1, "Unable to obtain temporary file name");
		if ((template = realloc(template, strlen(template) + 7)) == NULL)
			err(1, "Error obtaining temporary file name space");
		strcat(template, "XXXXXX");
		if ((bp->page_file_fd = mkstemp(template)) == -1)
			err(1, "Unable to create temporary file %s", template);
	}

	/*
	 * Page-out memory buffers from the pool, round-robin fashion,
	 * starting from the oldest buffers.
	 * This is good enough for the simple common case where one output fd is blocked.
	 */
	while (memory_pool_size(bp, bp->allocated_pool_end - 1) > max_mem / 2) {
		switch (bp->buffers[bp->page_out_ptr].s) {
		case s_memory:
			if (pwrite(bp->page_file_fd, bp->buffers[bp->page_out_ptr].p, buffer_size, (off_t)bp->page_out_ptr * buffer_size) != buffer_size)
				err(1, "Write to temporary file failed");
			/* FALLTHROUGH */
		case s_memory_backed:
			DPRINTF(4, "Page out buffer %d %p", bp->page_out_ptr, bp->buffers[bp->page_out_ptr].p);
			bp->buffers[bp->page_out_ptr].s = s_file;
			free(bp->buffers[bp->page_out_ptr].p);
			bp->buffers_freed++;
			bp->buffers_paged_out++;
			DPRINTF(4, "Paged out buffer %d %p", bp->page_out_ptr, bp->buffers[bp->page_out_ptr].p);
			break;
		case s_file:
		case s_none:
			break;
		default:
			assert(false);
		}
		if (++bp->page_out_ptr == bp->allocated_pool_end)
			bp->page_out_ptr = 0;
	}
}

/*
 * Allocate memory for the specified pool member.
 * Return false if no such memory is available.
 */
static bool
allocate_pool_buffer(struct buffer_pool *bp, int pool)
{
	struct pool_buffer *b = &bp->buffers[pool];

	if ((b->p = malloc(buffer_size)) == NULL) {
		DPRINTF(4, "Unable to allocate %d bytes for buffer %ld", buffer_size, b - bp->buffers);
		bp->max_buffers_allocated = MAX(bp->buffers_allocated - bp->buffers_freed, bp->max_buffers_allocated);
		return false;
	}
	b->s = s_memory;
	DPRINTF(4, "Allocated buffer %ld to %p", b - bp->buffers, b->p);
	bp->buffers_allocated++;
	bp->max_buffers_allocated = MAX(bp->buffers_allocated - bp->buffers_freed, bp->max_buffers_allocated);
	return true;
}


/*
 * Ensure that the specified pool buffer is in memory
 */
static void
page_in(struct buffer_pool *bp, int pool)
{
	struct pool_buffer *b = &bp->buffers[pool];

	switch (b->s) {
	case s_memory_backed:
	case s_memory:
		break;
	case s_file:
		/* Good time to ensure that there will be page-in memory available */
		if (memory_pool_size(bp, bp->allocated_pool_end - 1) > max_mem)
			page_out(bp);
		if (!allocate_pool_buffer(bp, pool))
			err(1, "Out of memory paging-in buffer");
		if (pread(bp->page_file_fd, b->p, buffer_size, (off_t)pool * buffer_size) != buffer_size)
			err(1, "Read from temporary file failed");
		bp->buffers_paged_in++;
		b->s = s_memory_backed;
		DPRINTF(4, "Page in buffer %d", pool);
		break;
	case s_none:
	default:
		DPRINTF(4, "Buffer %d has invalid storage %d", pool, b->s);
		assert(false);
		break;
	}

}

/*
 * Allocate memory for the specified pool
 * If we're out of memory by reaching the user-specified limit
 * or a system's hard limit return false.
 * If sufficient memory is available return true.
 * buffers[pool] will then point to a buffer_size block of available memory.
 */
static bool
memory_allocate(struct buffer_pool *bp, int pool)
{
	int i, orig_pool_size;
	struct pool_buffer *orig_buffers;

	if (pool < bp->allocated_pool_end)
		return true;

	DPRINTF(4, "Buffers allocated: %d Freed: %d", bp->buffers_allocated, bp->buffers_freed);
	/* Check soft memory limit through allocated plus requested memory. */
	if (memory_pool_size(bp, pool) > max_mem) {
		if (use_tmp_file)
			page_out(bp);
		else
			return false;
	}

	/* Keep original values to undo on failure. */
	orig_pool_size = bp->pool_size;
	orig_buffers = bp->buffers;
	/* Resize bank, if needed. One iteration should suffice. */
	while (pool >= bp->pool_size) {
		if (bp->pool_size == 0)
			bp->pool_size = 1;
		else
			bp->pool_size *= 2;
		if ((bp->buffers = realloc(bp->buffers, bp->pool_size * sizeof(struct pool_buffer))) == NULL) {
			DPRINTF(4, "Unable to reallocate buffer pool bank");
			bp->pool_size = orig_pool_size;
			bp->buffers = orig_buffers;
			return false;
		}
	}

	/* Allocate buffer memory [allocated_pool_end, pool]. */
	for (i = bp->allocated_pool_end; i <= pool; i++)
		if (!allocate_pool_buffer(bp, i)) {
			bp->allocated_pool_end = i;
			return false;
		}
	bp->allocated_pool_end = pool + 1;
	return true;
}

/*
 * Free a file-backed buffer at the specified pool location
 * by punching a hole to the file. This is a best effort
 * operation, as it is only supported on Linux.
 */
static void
buffer_file_free(struct buffer_pool *bp, int pool)
{
#ifdef FALLOC_FL_PUNCH_HOLE
	static bool warned = false;

	if (fallocate(bp->page_file_fd, FALLOC_FL_PUNCH_HOLE, pool * buffer_size, buffer_size) < 0 &&
	    !warned) {
		warn("Failed to free temporary buffer space");
		warned = true;
	}
#endif
	bp->pages_freed++;
}

/*
 * Ensure that pool buffers from [0,pos) are free.
 */
static void
memory_free(struct buffer_pool *bp, off_t pos)
{
	int pool_end = pos / buffer_size;
	int i;

	DPRINTF(4, "memory_free: pool=%p pos = %ld, begin=%d end=%d",
		bp, (long)pos, bp->free_pool_begin, pool_end);
	for (i = bp->free_pool_begin; i < pool_end; i++) {
		switch (bp->buffers[i].s) {
		case s_memory:
			free(bp->buffers[i].p);
			bp->buffers_freed++;
			break;
		case s_file:
			buffer_file_free(bp, i);
			break;
		case s_memory_backed:
			buffer_file_free(bp, i);
			free(bp->buffers[i].p);
			bp->buffers_freed++;
			break;
		case s_none:
			break;
		default:
			assert(false);
			break;
		}
		bp->buffers[i].s = s_none;
		DPRINTF(4, "Freed buffer %d %p (pos = %ld, begin=%d end=%d)",
			i, bp->buffers[i].p, (long)pos, bp->free_pool_begin, pool_end);
		#ifdef DEBUG
		bp->buffers[i].p = NULL;
		#endif
	}
	bp->free_pool_begin = pool_end;
}

/*
 * Set the buffer to write to for reading from a file from
 * position onward, ensuring that sufficient memory is allocated.
 * Return false if no memory is available.
 */
static bool
source_buffer(struct source_info *ifp, /* OUT */ struct io_buffer *b)
{
	int pool = ifp->source_pos_read / buffer_size;
	size_t pool_offset = ifp->source_pos_read % buffer_size;

	if (!memory_allocate(ifp->bp, pool))
		return false;
	if (ifp->bp->buffers[pool].s != s_memory)
		DPRINTF(4, "ifp->bp->buffers[pool].s = 0x%x, pool=%d\n", ifp->bp->buffers[pool].s, pool);
	assert(ifp->bp->buffers[pool].s == s_memory);
	b->p = ifp->bp->buffers[pool].p + pool_offset;
	b->size = buffer_size - pool_offset;
	DPRINTF(4, "Source buffer(%ld) returns pool %d(%p) o=%ld l=%ld a=%p",
		(long)ifp->source_pos_read, pool, ifp->bp->buffers[pool].p, (long)pool_offset, (long)b->size, b->p);
	return true;
}

/*
 * Return a buffer to read from for writing to a file from a position onward
 * When processing lines, b.size can be 0
 */
static struct io_buffer
sink_buffer(struct sink_info *ofp)
{
	struct io_buffer b;
	int pool = ofp->pos_written / buffer_size;
	size_t pool_offset = ofp->pos_written % buffer_size;
	size_t source_bytes = ofp->pos_to_write - ofp->pos_written;

	b.size = MIN(buffer_size - pool_offset, source_bytes);
	if (b.size == 0)
		b.p = NULL;
	else {
		if (ofp->ifp->bp->page_file_fd != -1)
			page_in(ofp->ifp->bp, pool);
		b.p = ofp->ifp->bp->buffers[pool].p + pool_offset;
	}
	DPRINTF(4, "Sink buffer(%ld-%ld) returns pool %d(%p) o=%ld l=%ld a=%p for input fd: %s",
		(long)ofp->pos_written, (long)ofp->pos_to_write, pool, b.size ? ofp->ifp->bp->buffers[pool].p : NULL, (long)pool_offset, (long)b.size, b.p, fp_name(ofp->ifp));
	return b;
}

/*
 * Return a pointer to read from for writing to a file from a position onward
 */
static char *
sink_pointer(struct buffer_pool *bp, off_t pos_written)
{
	int pool = pos_written / buffer_size;
	size_t pool_offset = pos_written % buffer_size;

	if (bp->page_file_fd != -1)
		page_in(bp, pool);
	return bp->buffers[pool].p + pool_offset;
}

/*
 * Return the size of a buffer region that can be read for the specified endpoints
 */
static size_t
sink_buffer_length(off_t start, off_t end)
{
	size_t pool_offset = start % buffer_size;
	size_t source_bytes = end - start;

	DPRINTF(4, "sink_buffer_length(%ld, %ld) = %ld",
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
	struct io_buffer b;

	if (!source_buffer(ifp, &b)) {
		DPRINTF(4, "Memory full");
		/* Provide some time for the output to drain. */
		return read_oom;
	}
	if ((n = read(ifp->fd, b.p, b.size)) == -1)
		switch (errno) {
		case EAGAIN:
			DPRINTF(4, "EAGAIN on %s", fp_name(ifp));
			return read_again;
		default:
			err(3, "Read from %s", fp_name(ifp));
		}
	ifp->source_pos_read += n;
	DPRINTF(4, "Read %d out of %zu bytes from %s data=[%.*s]", n, b.size, fp_name(ifp),
		(int)n * DATA_DUMP, (char *)b.p);
	/* Return -1 on EOF */
	return n ? read_ok : read_eof;
}

/*
 * Allocate available read data to empty sinks that can be written to,
 * by adjusting their ifp, pos_written, and pos_to_write pointers.
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
		for (ofp = files; ofp; ofp = ofp->next) {
			/* Advance to next input file, if required */
			if (ofp->pos_written == ofp->ifp->source_pos_read &&
			    ofp->ifp->reached_eof &&
			    !ofp->ifp->chain_last) {
				DPRINTF(4, "%s(): advance to input file %s\n",
						__func__, fp_name(ofp->ifp));
				ofp->ifp = ofp->ifp->next;
				ofp->ifp->active = true;
				ofp->pos_written = 0;
			}
			ofp->pos_to_write = ofp->ifp->source_pos_read;
		}
		return;
	}

	/*
	 * Difficult case: fair scattering across available sinks
	 * Thankfully here we only have a single input file
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
	available_data = sink_buffer_length(pos_assigned, files->ifp->source_pos_read);

	if (available_sinks == 0)
		return;

	/* Assign data to sinks. */
	data_per_sink = available_data / available_sinks;
	for (ofp = files; ofp; ofp = ofp->next) {
		/* Move to next file if this has data to write, or isn't ready. */
		if (ofp->pos_written != ofp->pos_to_write || !FD_ISSET(ofp->fd, sink_fds))
			continue;

		DPRINTF(4, "pos_assigned=%ld source_pos_read=%ld available_data=%ld available_sinks=%d data_per_sink=%ld",
			(long)pos_assigned, (long)ofp->ifp->source_pos_read, (long)available_data, available_sinks, (long)data_per_sink);
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
					if (data_end <= pos_assigned) {
						/*
						 * If no newline was found with backward scanning
						 * degenerate to the efficient algorithm. This will
						 * scan further forward, and can defer writing the
						 * last chunk, until more data is read.
						 */
						use_reliable = true;
						goto reliable;
					}
					if (*sink_pointer(ofp->ifp->bp, data_end) == rt) {
						pos_assigned = data_end + 1;
						break;
					}
					data_end--;
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
					if (data_end >= ofp->ifp->source_pos_read) {
						if (last_nl != -1) {
							pos_assigned = last_nl + 1;
							break;
						} else {
							/* No newline found in buffer; defer writing. */
							ofp->pos_to_write = pos_assigned;
							DPRINTF(4, "scatter to file[%s] no newline from %ld to %ld",
								fp_name(ofp), (long)pos_assigned, (long)data_end);
							return;
						}
					}

					if (*sink_pointer(ofp->ifp->bp, data_end) == rt) {
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
		DPRINTF(4, "scatter to file[%s] pos_written=%ld pos_to_write=%ld data=[%.*s]",
			fp_name(ofp), (long)ofp->pos_written, (long)ofp->pos_to_write,
			(int)(ofp->pos_to_write - ofp->pos_written) * DATA_DUMP, sink_pointer(ofp->ifp->bp, ofp->pos_written));
	}
}


/*
 * Write out from the memory buffer to the sinks where write will not block.
 * Free memory no more needed even by the write pointer farthest behind.
 * Return the number of bytes written.
 */
static size_t
sink_write(struct source_info *ifiles, fd_set *sink_fds, struct sink_info *ofiles)
{
	struct sink_info *ofp;
	struct source_info *ifp;
	size_t written = 0;

	for (ifp = ifiles; ifp; ifp = ifp->next) {
		ifp->read_min_pos = ifp->source_pos_read;
		ifp->is_read = false;
	}

	allocate_data_to_sinks(sink_fds, ofiles);
	for (ofp = ofiles; ofp; ofp = ofp->next) {
		DPRINTF(4, "\n%s(): try write to file %s", __func__, fp_name(ofp));
		if (ofp->active && FD_ISSET(ofp->fd, sink_fds)) {
			int n;
			struct io_buffer b;

			b = sink_buffer(ofp);
			DPRINTF(4, "\n%s(): sink buffer returned %d bytes to write",
					__func__, (int)b.size);
			if (b.size == 0)
				/* Can happen when a line spans a buffer */
				n = 0;
			else {
				n = write(ofp->fd, b.p, b.size);
				if (n < 0)
					switch (errno) {
					/* EPIPE is acceptable, for the sink's reader can terminate early. */
					case EPIPE:
						ofp->active = false;
						(void)close(ofp->fd);
						DPRINTF(4, "EPIPE for %s", fp_name(ofp));
						break;
					case EAGAIN:
						DPRINTF(4, "EAGAIN for %s", fp_name(ofp));
						n = 0;
						break;
					default:
						err(2, "Error writing to %s", fp_name(ofp));
					}
				else {
					ofp->pos_written += n;
					written += n;
				}
			}
			DPRINTF(4, "Wrote %d out of %zu bytes for file %s pos_written=%lu data=[%.*s]",
				n, b.size, fp_name(ofp), (unsigned long)ofp->pos_written, (int)n * DATA_DUMP, (char *)b.p);
		}
		if (ofp->active) {
			ofp->ifp->read_min_pos = MIN(ofp->ifp->read_min_pos, ofp->pos_written);
			ofp->ifp->is_read = true;
		}
	}

	/* Free buffers all sinks have read */
	for (ifp = ifiles; ifp; ifp = ifp->next) {
		memory_free(ifp->bp, ifp->read_min_pos);
		/*
		 * We are reading this source, so don't even think freeing
		 * sources after this.
		 */
		if (ifp->is_read)
			break;
	}

	DPRINTF(4, "Wrote %zu total bytes", written);
	return written;
}

static void
usage(const char *name)
{
	fprintf(stderr, "Usage %s [-b size] [-i file] [-IMs] [-o file] [-m size] [-t char]\n"
		"-a"		"\tOpen output file(s) for appending\n"
		"-b size"	"\tSpecify the size of the buffer to use (used for stress testing)\n"
		"-f"		"\tOverflow buffered data into a temporary file\n"
		"-I"		"\tInput-side buffering\n"
		"-i file"	"\tGather input from specified file\n"
		"-m size[k|M|G]""\tSpecify the maximum buffer memory size\n"
		"-M"		"\tProvide memory use statistics on termination\n"
		"-o file"	"\tScatter output to specified file\n"
		"-p d1[,d2...]"	"\tPermute inputs to specified outputs\n"
		"-s"		"\tScatter the input across the files, rather than copying it to all\n"
		"-T dir"	"\tSpecify directory for storing temporary file\n"
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
 * Show the arguments passed to select(2) in human-readable form
 * If check is true, abort the program if no bit is on
 */
static void
show_select_args(const char *msg, fd_set *source_fds, struct source_info *ifiles, fd_set *sink_fds, struct sink_info *ofiles, bool check)
{
	#ifdef DEBUG
	struct sink_info *ofp;
	struct source_info *ifp;
	int nbits = 0;

	fprintf(stderr, "%s: ", msg);
	for (ifp = ifiles; ifp; ifp = ifp->next)
		if (FD_ISSET(ifp->fd, source_fds)) {
			fprintf(stderr, "%s ", fp_name(ifp));
			nbits++;
		}
	for (ofp = ofiles; ofp; ofp = ofp->next)
		if (FD_ISSET(ofp->fd, sink_fds)) {
			fprintf(stderr, "%s ", fp_name(ofp));
			nbits++;
		}
	fputc('\n', stderr);
	if (check && nbits == 0)
		abort();
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
		fprintf(stderr, "Unknown size suffix: %c\n", size);
		usage(progname);
	}
	/* NOTREACHED */
	return 0;
}

/*
 * Parse and validate a comma-separated list of integers setting the
 * variables permute_dest and permute_n.
 */
static void
parse_permute(char *s)
{
	char *p;
	char *copy = strdup(s);
	int i;

	if (copy == NULL)
		errx(1, "Out of memory for destination string");
	DPRINTF(4, "In parse_permute [%s]", s);
	for (p = strtok(copy, ","); p != NULL; p = strtok(NULL, ","))
		permute_n++;
	free(copy);
	if ((permute_dest = (int *)malloc(sizeof(int) * permute_n)) == NULL)
		errx(1, "Out of memory for permutation destination");
	for (p = strtok(s, ","), i= 0; p != NULL; p = strtok(NULL, ","), i++) {
		permute_dest[i] = atoi(p) - 1;
		if (permute_dest[i] < 0 || permute_dest[i] >= permute_n)
			errx(1, "Illegal permutation destination [%s]", s);
	}
	for (i = 0; i < permute_n; i++)
		DPRINTF(4, "%d = %d", i, permute_dest[i]);
	DPRINTF(4, "permute_n=%d", permute_n);
}

/*
 * Return the input file corresponding to the specified
 * permuted output file number.
 */
static struct source_info *
output_source(struct source_info *ifiles, int output_n)
{
	int i, input_n = -1;
	struct source_info *ifp;

	/* Find input file number */
	for (i = 0; i < permute_n; i++)
		if (permute_dest[i] == output_n) {
			input_n = i;
			break;
		}
	if (input_n == -1)
		errx(1, "Unspecified output %d", output_n + 1);

	/* Find input file pointer */
	for (ifp = ifiles, i = 0; ifp; ifp = ifp->next, i++)
		if (i == input_n)
			return ifp;
	assert(0);
	return NULL;
}

static void
memory_stats(struct source_info *ifiles)
{
	struct source_info *ifp;

	for (ifp = ifiles; ifp; ifp = ifp->next) {
		fprintf(stderr, "Input file: %s\n", fp_name(ifp));
		fprintf(stderr, "Buffers allocated: %d Freed: %d Maximum allocated: %d\n",
			ifp->bp->buffers_allocated, ifp->bp->buffers_freed, ifp->bp->max_buffers_allocated);
		fprintf(stderr, "Page out: %d In: %d Pages freed: %d\n",
			ifp->bp->buffers_paged_out, ifp->bp->buffers_paged_in, ifp->bp->pages_freed);
	}
}

/*
 * Return true if an element with ordinal number n,
 * is the first element of a group in a series of groups
 * of group_size each.
 * Example: elements 0 and 3 in groups of size 3.
 */
static bool
first_in_group(int group_size, int n)
{
	return n % group_size == 0;
}

/*
 * Return true if an element with ordinal number n,
 * is the last element of a group in a series of groups
 * of group_size each.
 * Example: elements 2 and 5 in groups of size 3.
 */
static bool
last_in_group(int group_size, int n)
{
	return (n + 1) % group_size == 0;
}

struct list {
	struct list *next;
};

/*
 * Transpose the elements of th specified linked list given
 * a notional new row length.  As an example, a list of 12 elements
 * with a row size of 3, would be transposed as follows.
 * 0 -> 4 -> 8 ->
 * 1 -> 5 -> 9 ->
 * 2 -> 6 -> 10 ->
 * 3 -> 7 -> 11
 * This function can handle arbitrary lists, as long
 * as the list's first element is the pointer to the
 * next one.
 */
static void
list_transpose(struct list *lst, int row_length)
{
	int i, count = 0;
	struct list **vector, *p;

	/* Create a vector of pointers to list elements */
	for (p = lst; p; p = p->next)
		count++;
	vector = (struct list **)malloc(count * sizeof(struct list *));
	if (vector == NULL)
		err(1, NULL);
	for (p = lst, i = 0; p; p = p->next, i++)
		vector[i] = p;
	/* Transpose notional rows into columns */
	for (i = 0; i < count - row_length; i++)
		vector[i]->next = vector[i + row_length];
	for (i = count - row_length; i < count - 1; i++)
		vector[i]->next = vector[(i + 1) % row_length];
	free(vector);
}



/*
 * Chain input and output files into groups
 * by setting the chain_last of all I/O files
 * and the input file (ifp) field of all output
 * files.
 *
 * These are the possible cases and the corresponding
 * group chains.
 *
 * Read from many, output to one (cat)
 * A->B->C	>	a
 * All input files are chained together
 *
 * Read from one, output to many (tee)
 * A		>	b->c->d
 * All output files are chained together
 *
 * Read from many output to permuted many (perm)
 * A		>	d
 * B		>	c
 * C		>	b
 * D		>	a
 * No files are chained
 *
 * Read from few, output to more (multipipe tee)
 * A		>	a->d->g
 * B		>	b->e->h
 * C		>	c->f->i
 * Output files are chained into groups
 *
 * Read from many, output to few (multipipe cat)
 * A->D->G	>	a
 * B->E->H	>	b
 * C->F->I	>	c
 * Input files are chained into groups
 *
 * Note that cat, tee, and perm are special cases of the multipipe ones
 * are are implemented as such.
 */
static void
chain_io_files(struct source_info *ifiles, struct sink_info *ofiles, bool permute)
{
	int nin = 0, nout = 0;
	int group_size, n, i;
	struct source_info *ifp;
	struct sink_info *ofp;

	for (ifp = ifiles; ifp; ifp = ifp->next)
		nin++;
	for (ofp = ofiles; ofp; ofp = ofp->next)
		nout++;

	if (nin >= nout) {
		/*
		 * Read from many output to few.
		 * First input element in group is active.
		 * Chain all but the last element of each input chain.
		 * None of the outputs are chained.
		 */
		if (nin % nout)
			errx(1, "The number of inputs %d is not an exact multiple of the number of outputs %d", nin, nout);
		group_size = nin / nout;
		list_transpose((struct list *)ifiles, group_size);
		for (ifp = ifiles, n = 0; ifp; ifp = ifp->next, n++) {
			ifp->active = first_in_group(group_size, n);
			ifp->chain_last = last_in_group(group_size, n);
		}
		for (ofp = ofiles, ifp = ifiles, n = 0; ofp; ofp = ofp->next, n++) {
			ofp->chain_last = true;
			ofp->ifp = permute ? output_source(ifiles, n) : ifp;
			for (i = 0; i <  group_size; i++)
				ifp = ifp->next;
		}
	} else {
		/*
		 * Read from few output to many.
		 * All inputs are active, none is chained.
		 * Chain all but the last element in the output chain.
		 */
		if (nout % nin)
			errx(1, "The number of outputs %d is not an exact multiple of the number of inputs %d", nin, nout);
		group_size = nout / nin;
		assert(!permute);
		list_transpose((struct list *)ofiles, group_size);
		for (ifp = ifiles, n = 0; ifp; ifp = ifp->next, n++) {
			ifp->active = true;
			ifp->chain_last = true;
		}
		for (ofp = ofiles, ifp = ifiles, n = 0; ofp; ofp = ofp->next, n++) {
			ofp->ifp = ifp;
			if (last_in_group(group_size, n)) {
				ifp = ifp->next;
				ofp->chain_last = true;
			} else
				ofp->chain_last = false;
		}
	}
	assert(ifp == NULL);

	DPRINTF(3, "Input files");
	for (ifp = ifiles; ifp; ifp = ifp->next)
		DPRINTF(3, "%p: chain_last=%d (%s)", ifp, ifp->chain_last, ifp->name);
	DPRINTF(3, "Output files");
	for (ofp = ofiles; ofp; ofp = ofp->next)
		DPRINTF(3, "%p: chain_last=%d ifp=%p (%s)", ofp, ofp->chain_last, ofp->ifp, ofp->name);
}

int
main(int argc, char *argv[])
{
	int max_fd = 0;
	struct sink_info *ofiles = NULL, *ofp;
	struct sink_info **oend = &ofiles;
	struct source_info *ifiles = NULL, *ifp;
	struct source_info **iend = &ifiles;
	struct source_info *front_ifp;	/* To keep output sequential, never output past this one */
	int ch;
	const char *progname = argv[0];
	enum state state = read_ob;
	bool opt_memory_stats = false;
	bool opt_append = false;

	while ((ch = getopt(argc, argv, "ab:fIi:Mm:o:p:S:sTt:")) != -1) {
		switch (ch) {
		case 'a':
			opt_append = true;
			break;
		case 'b':
			buffer_size = (int)parse_size(progname, optarg);
			break;
		case 'f':
			use_tmp_file = true;
			break;
		case 'I':
			state = read_ib;
			break;
		case 'i':	/* Specify input file */
			ifp = new_source_info(optarg);

			if ((ifp->fd = open(optarg, O_RDONLY)) < 0)
				err(2, "Error opening %s", optarg);
			max_fd = MAX(ifp->fd, max_fd);
			non_block(ifp->fd, fp_name(ifp));
			/* Add file at the end of the linked list */
			*iend = ifp;
			iend = &ifp->next;
			break;
		case 'm':
			max_mem = parse_size(progname, optarg);
			break;
		case 'M':	/* Provide memory use statistics on termination */
			opt_memory_stats = true;
			break;
		case 'o':	/* Specify output file */
			ofp = new_sink_info(optarg);
			if ((ofp->fd = open(optarg,
					(opt_append ? O_APPEND : 0) |
					O_WRONLY | O_CREAT | O_TRUNC, 0666)) < 0)
				err(2, "Error opening %s", optarg);
			max_fd = MAX(ofp->fd, max_fd);
			non_block(ofp->fd, fp_name(ofp));
			/* Add file at the end of the linked list */
			*oend = ofp;
			oend = &ofp->next;
			break;
		case 'p':
			parse_permute(optarg);
			break;
		case 's':
			opt_scatter = true;
			break;
		case 'T':
			opt_tmp_dir = optarg;
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

	/* dgsh */
	int j = 0;
	int noutputfds;
	int *outputfds;
	int ninputfds;
	int *inputfds;
	char *name;

	if (permute_n) {
		ninputfds = noutputfds = permute_n;
		name = "perm";
	} else {
		char *in, *out;

		/* No stdin or stdout, if these have been specified via args */
		ninputfds = ifiles ? 0 : -1;
		noutputfds = ofiles ? 0 : -1;

		/* Heuristic to determine name */
		in = getenv("DGSH_IN");
		if (in && *in == '0')
			in = NULL;
		out = getenv("DGSH_OUT");
		if (out && *out == '0')
			out = NULL;
		if (in && !out)
			name = "cat";
		else if (!in && out)
			name = "tee";
		else
			name = "dgsh-tee";
	}



	DPRINTF(3, "Calling negotiate in=%d out=%d", ninputfds, noutputfds);
	dgsh_negotiate(DGSH_HANDLE_ERROR, name, &ninputfds, &noutputfds, &inputfds, &outputfds);
	DPRINTF(3, "nin=%d nout=%d", ninputfds, noutputfds);
	assert(noutputfds >= 0);
	assert(ninputfds >= 0);

	if (permute_n && permute_n != ninputfds)
		errx(1, "The number of inputs %d is not equal to the specified permuted outputs %d", ninputfds, permute_n);
	if (permute_n && permute_n != noutputfds)
		errx(1, "The number of outputs %d is not equal to the specified permuted outputs %d", noutputfds, permute_n);

	for (j = 0; j < noutputfds; j++) {
		DPRINTF(3, "New ofp assigned fd %d", outputfds[j]);
		if (j == 0) {
			ofp = new_sink_info("standard output");
			ofp->fd = STDOUT_FILENO;
		} else {
			ofp = new_sink_info(NULL);
			ofp->fd = outputfds[j];
		}
		max_fd = MAX(ofp->fd, max_fd);
		non_block(ofp->fd, fp_name(ofp));
		/* Add file at the end of the linked list */
		*oend = ofp;
		oend = &ofp->next;
	}

	for (j = 0; j < ninputfds; j++) {
		DPRINTF(3, "New ifp assigned fd %d", inputfds[j]);
		if (j == 0) {
			ifp = new_source_info("standard input");
			ifp->fd = STDIN_FILENO;
		} else {
			ifp = new_source_info(NULL);
			ifp->fd = inputfds[j];
		}
		max_fd = MAX(ifp->fd, max_fd);
		non_block(ifp->fd, fp_name(ifp));
		/* Add file at the end of the linked list */
		*iend = ifp;
		iend = &ifp->next;
	}

	if (buffer_size > max_mem)
		errx(1, "Buffer size %d is larger than the program's maximum memory limit %lu", buffer_size, max_mem);

	if (opt_scatter && ifiles && ifiles->next)
		errx(1, "Scattering not supported with more than one input file");

	if (opt_scatter && permute_n)
		errx(1, "Scattering and permutation cannot be used together");

	if (ofiles == NULL) {
		/* Output to stdout */
		ofp = new_sink_info("standard output");
		ofp->fd = STDOUT_FILENO;
		max_fd = MAX(ofp->fd, max_fd);
		non_block(ofp->fd, fp_name(ofp));
		ofp->next = ofiles;
		ofiles = ofp;
	}

	if (ifiles == NULL) {
		/* Input from stdin */
		ifp = new_source_info("standard input");
		ifp->fd = STDIN_FILENO;
		max_fd = MAX(ifp->fd, max_fd);
		non_block(ifp->fd, fp_name(ifp));
		ifp->next = ifiles;
		ifiles = ifp;
	}

	/* We will handle SIGPIPE explicitly when calling write(2). */
	signal(SIGPIPE, SIG_IGN);

	front_ifp = ifiles;
	chain_io_files(ifiles, ofiles, permute_n != 0);

	/* Copy source to sink without allowing any single file to block us. */
	for (;;) {
		fd_set source_fds;
		fd_set sink_fds;

		show_state(state);
		/* Set the fd's we're interested to read/write; close unneeded ones. */
		FD_ZERO(&source_fds);
		FD_ZERO(&sink_fds);

		if (!reached_eof)
			switch (state) {
			case read_ib:
				for (ifp = front_ifp; ifp; ifp = ifp->next)
					if (!ifp->reached_eof)
						FD_SET(ifp->fd, &source_fds);
				break;
			case read_ob:
				for (ifp = front_ifp; ifp; ifp = ifp->next)
					if (ifp->active && !ifp->reached_eof)
						FD_SET(ifp->fd, &source_fds);
				break;
			default:
				break;
			}

		for (ofp = ofiles; ofp; ofp = ofp->next)
			if (ofp->active) {
				switch (state) {
				case read_ib:
				case read_ob:
				case drain_ob:
					DPRINTF(4, "Check active file[%s] pos_written=%ld pos_to_write=%ld",
						fp_name(ofp), (long)ofp->pos_written, (long)ofp->pos_to_write);
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
		show_select_args("Entering select", &source_fds, ifiles, &sink_fds, ofiles, true);
		if (select(max_fd + 1, &source_fds, &sink_fds, NULL, NULL) < 0)
			err(3, "select");
		show_select_args("Select returned", &source_fds, ifiles, &sink_fds, ofiles, false);

		/* Write to all file descriptors that accept writes. */
		if (sink_write(ifiles, &sink_fds, ofiles) > 0) {
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
						DPRINTF(3, "Retiring file %s pos_written=pos_to_write=%ld source_pos_read=%ld",
							fp_name(ofp), (long)ofp->pos_written, (long)ofp->ifp->source_pos_read);
						/* No more data to write; close fd to avoid deadlocks downstream. */
						if (close(ofp->fd) == -1)
							err(2, "Error closing %s", fp_name(ofp));
						ofp->active = false;
					}
				}
			if (active_fds == 0) {
				/* If no read possible, and no writes pending, terminate. */
				if (opt_memory_stats)
					memory_stats(ifiles);
				return 0;
			}
		}

		/*
		 * Note that we never reach this point after a successful write.
		 * See the continue statement above.
		 */
		switch (state) {
		case read_ib:
			/* Read, if possible; set global reached_eof if all have reached it */
			reached_eof = true;
			for (ifp = front_ifp; ifp; ifp = ifp->next) {
				if (FD_ISSET(ifp->fd, &source_fds))
					switch (source_read(ifp)) {
					case read_eof:
						ifp->reached_eof = true;
						break;
					case read_oom:	/* Cannot fullfill promise to never block source, so bail out. */
						errx(1, "Out of memory with input-side buffering specified");
						break;
					case read_ok:
					case read_again:
						break;
					}
				if (!ifp->reached_eof)
					reached_eof = false;
			}
			if (reached_eof)
				state = drain_ib;
			break;
		case read_ob:
			/* Read, from possible sources; set global reached_eof if all have reached it */
			reached_eof = true;
			for (ifp = front_ifp; ifp; ifp = ifp->next) {
				if (!ifp->active)
					continue;
				if (FD_ISSET(ifp->fd, &source_fds))
					switch (source_read(ifp)) {
					case read_eof:
						ifp->reached_eof = true;
						ifp->active = false;
						if (!ifp->chain_last)
							ifp->next->active = true;
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
				if (!ifp->reached_eof)
					reached_eof = false;
			}
			if (reached_eof)
				state = drain_ib;
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

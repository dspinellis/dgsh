#include <stdlib.h> /* getenv() */
#include <err.h> /* err() */
#include <unistd.h> /* getpid(), getpagesize(), 
			STDIN_FILENO, STDOUT_FILENO, STDERR_FILENO */
#include "sgsh-negotiate.h" /* sgsh_negotiate(), sgsh_run() */


#define OP_ERROR 1
#define OP_SUCCESS 0
#define PROT_STATE_ZERO 0
#define PROT_STATE_NEGOTIATION 1

struct sgsh_node {
        int id;
        char name[100];
        int requires_channels;
        int provides_channels;
	struct sgsh_node *prev;
	struct sgsh_node *next;
};

struct sgsh_negotiation {
        struct sgsh_node *node_list;
        int n_nodes;
	pid_t initiator_pid;
        int state_flag;
        int serial_no;
	size_t total_size; /* Compact allocation that includes sgsh_node
				memory allocation. */
};

static struct sgsh_negotiation *selected_message_block;


static int compete_memory_block(struct sgsh_negotiation *fresh_mb) {
	if (selected_memory_block) {
		pid_t selected_mb_pid = selected_memory_block->initiator_pid;
		pid_t fresh_mb_pid = fresh_mb->initiator_pid;
		if (selected_mb_pid > fresh_mb_pid) {
			free(selected_memory_block); /* Hail compact 
							allocation. */
			selected_memory_block = fresh_mb;
		} else if (selected_mb_pid < fresh_mb_pid)
			don't write
	} else
		selected_memory_block = fresh_mb;
}

static int adjust_memory_block_size(int fresh_mb_size) {
	active_mb_size = selected_message_block->total_size;
	if (active_mb_size > fresh_mb_size) {
		err(1, "Memory block seems to have shrunk or read operation \
			was incomplete.\n");
		return OP_ERROR;
	} else if (active_mb_size < fresh_mb_size) {
		void *p = realloc(selected_message_block, fresh_mb_size);
		if (!p) {
			err(1, "Memory block reallocation failed.\n");
			return OP_ERROR;
		} else 
			selected_message_block = (struct sgsh_negotiation *)p;
	}
}

static int call_read_pass_fail(int fd, char *buf, int buf_size, 
				int *fd_side, /* Input or output fd? */
				int *bytes_read, 
				int *error_code) {
	*error_code = 0;
	*fd_side = 0;
	if ((*bytes_read = read(fd, buf, buf_size)) == -1)
		*error_code = -errno;
	if ((*error_code == 0) || (*error_code != -EAGAIN)) {
		fd_side = 1;
		return 1;
	}
	return 0;
}

/* Read in circulated message block from either direction,
 * that is, input or output side. This capability
 * relies on an extension to a standard shell implementation,
 * e.g., bash, that allows reading and writing to both sides
 * for the negotiation phase. 
 * Set I/O to non-blocking in order to be able to retry on both
 * sides.
 * Returns the fd to write to the message block if need be transmitted.
 */
/* TODO:Error handling */
static int try_read_message_block(char *buf, int buf_size, 
					struct sgsh_negotiation *fresh_mb) {
	int bytes_read, error_code = -EAGAIN, stdin_side = 0, stdout_size = 0;
	while (error_code == -EAGAIN) {
		if ((call_read_pass_fail(STDIN_FILENO, buf, buf_size, 
				&stdin_side, &bytes_read, &error_code)) ||
		(call_read_pass_fail(STDOUT_FILENO, buf, buf_size, 
				&stdout_size, &bytes_read, &error_code)))
			break;
	}
	if (bytes_read == -1) {  /* Read failed. */
	 	err(1, "Reading from ");
		(stdin_side) ? err(1, "stdin ") : err(1, "stdout ");
		err(1, "file descriptor failed with error code %d.\n", 
						error_code);
		return error_code;
	} else {  /* Read succeeded. */
		if (adjust_memory_block_size(bytes_read) == OP_ERROR)
			return OP_ERROR;
		fresh_mb = (struct sgsh_negotiation *)malloc(bytes_read);
		memcpy(fresh_mb, buf, bytes_read);
		assert(bytes_read == fresh_mb->total_size);
		(stdin_side) ? return STDOUT_FILENO : return STDIN_FILENO;
	}
}

static int construct_message_block() {
	int memory_allocation_size = sizeof(struct sgsh_negotiation);
	selected_message_block = (struct sgsh_negotiation *)malloc(
				memory_allocation_size);
	if (!selected_message_block) {
		err(1, "Memory allocation of memory block failed.");
		return OP_ERROR;
	}
	selected_message_block->n_nodes = 0;
	selected_message_block->selected_pid = getpid();
	selected_message_block->state_flag = PROT_STATE_ZERO;
	selected_message_block->serial_no = 0;
	selected_message_block->total_size = memory_allocation_size;
	return OP_SUCCESS;
}


static int get_env_var(const char *env_var,int *value) {
	char *string_value = getenv(env_var);
	if (!string_value) {
		err(1, "Getting environment variable \
		%s failed.\n", env_var);
		return OP_ERROR;
	}
	*value = atoi(string_value);
	return OP_SUCCESS;
}

static int get_environment_vars(int *sgsh_in, int *sgsh_out) {
	int result;
	result = get_env_var("SGSH_IN", sgsh_in);
	if (!result) return OP_ERROR;
	result = get_env_var("SGSH_OUT", sgsh_out);
	if (!result) return OP_ERROR;
	return OP_SUCCESS;
}

/* Each tool in the sgsh graph calls sgsh_negotiate() to take part in
 * peer-to-peer negotiation. A message block is circulated among tools
 * and is filled with tools' I/O requirements. When all requirements are in 
 * place, an algorithm runs that tries to find a solution that satisfies
 * all requirements. If a solution is found, pipes are allocated and
 * set up according to the solution. The appropriate file descriptors
 * are provided to each tool and the negotiation phase ends.
 * The function's return value signifies success or failure of the
 * negotiation phase.
 */
int sgsh_negotiate(const char *tool_name, /* Input. */
                    int unique_id,        /* Identifier. (Distinguish multiple 
					appearances of same tool.) */
                    int channels_required, /* How many input channels can take. */
                    int channels_provided, /* How many output channels can 
						provide. */
                                     /* Output: to fill. */
                    int *input_fds,  /* Input file descriptors. */
                    int n_input_fds, /* Number of input file descriptors. */
                    int *output_fds, /* Output file descriptors. */
                    int n_output_fds) { /* Number of output file descriptors. */
	int sgsh_in, sgsh_out;
	int write_direction_fd;
	int buf_size = getpagesize();
	char buf[buf_size];
	struct sgsh_negotiation *fresh_mb;
	if (get_environment_vars(&sgsh_in, &sgsh_out) == OP_ERROR)
		err(1, "Failed to extract SGSH_IN, SGSH_OUT \ 
			environment variables.";)
        if ((sgsh_out) && (!sgsh_in)) {      /* Negotiation starter. */
                construct_message_block();
                write_direction_fd = STDOUT_FILENO;
        } else {
		selected_message_block = NULL;
		write_direction_fd = try_read_message_block(buf, buf_size, 
								fresh_mb);
	}


	compete_memory_block(fresh_mb);
	return 
}

/* If negotiation is successful, tools configure input and output 
 * according to the provided file descriptors and then they call
 * sgsh_run() to signal that they have set their input/output and
 * are ready for execution (or that they failed) by setting PROT_STATE_RUN
 * or PROT_STATE_ERROR. An algorithm
 * verifies that all tools completed this stage too successfully 
 * and the function returns success or failure.
 */
int sgsh_run(int unique_id) {

}

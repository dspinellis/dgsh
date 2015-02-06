#include <stdlib.h> /* getenv(), errno */
#include <err.h> /* err() */
#include <unistd.h> /* getpid(), getpagesize(), 
			STDIN_FILENO, STDOUT_FILENO, STDERR_FILENO */
#include <string.h> /* memcpy() */
#include <assert.h> /* assert() */
#include <errno.h> /* EAGAIN */
#include <stdio.h> /* fprintf() in DPRINTF() */
#include "sgsh-negotiate.h" /* sgsh_negotiate(), sgsh_run() */
#include "sgsh.h" /* DPRINTF() */

#define OP_SUCCESS 0
#define OP_ERROR 1
#define OP_QUIT 2

/* TODO: message block size vs page size check before write. */

/* Each tool that participates in an sgsh graph is modelled as follows. */
struct sgsh_node {
        int unique_id;
        char name[100];
        int requires_channels;
        int provides_channels;
};

/* The messge block structure that provides the vehicle for negotiation. */
struct sgsh_negotiation {
        struct sgsh_node *node_list;
        int n_nodes;
	pid_t initiator_pid;
        int state_flag;
        int serial_no;
	size_t total_size; /* Compact allocation that includes sgsh_node
				memory allocation. */
};

static struct sgsh_negotiation *chosen_mb; /* Our king message block. */

/* Add node to message block. Copy the node using offset-based
 * calculation from the start of the array of nodes.
 */
static void add_node(struct sgsh_node *node_to_fit) { 
	int n_nodes = chosen_mb->n_nodes;
	memcpy(&chosen_mb->node_list[n_nodes], node_to_fit, 
		sizeof(struct sgsh_node));
	chosen_mb->n_nodes++;
}

/* Reallocate message block to fit new node coming in. */
static int realloc_mb_new_node() {
	int new_size = chosen_mb->total_size + sizeof(struct sgsh_node);
	void *p = realloc(chosen_mb, new_size);
	if (!p) {
		err(1, "Message block reallocation for adding a new node \
			failed.\n");
		return OP_ERROR;
	} else {
		chosen_mb = (struct sgsh_negotiation *)p;
		chosen_mb->total_size = new_size;
		chosen_mb->node_list = (struct sgsh_node *)&chosen_mb[1]; /* 
						* Node instances go after
						* the message block instance. */
	}
	return OP_SUCCESS;
}

/* Add a node to message block, if it does not exist. */
static void try_add_sgsh_node(struct sgsh_node *node) {
	struct sgsh_node *node_list = chosen_mb->node_list;
	int n_nodes = chosen_mb->n_nodes;
	int i;
	for (i = 0; i < n_nodes; i++) 
		if (node_list[i].unique_id == node->unique_id) break;
	if (i == n_nodes) { /* Node not in graph yet. */
		realloc_mb_new_node();
		add_node(node);
	}
}

/* A constructor-like function for struct sgsh_node. */
static void fill_sgsh_node(struct sgsh_node *node, const char *tool_name,
				int unique_id, int requires_channels,
				int provides_channels) {
	node->unique_id = unique_id;
	memcpy(node->name, tool_name, strlen(tool_name) + 1);
	node->requires_channels = requires_channels;
	node->provides_channels = provides_channels;
}

/* Check if the arrived message block preexists our chosen one
 * and substitute the chosen if so.
 * If the arrived message block is younger discard it and don't
 * forward it.
 * Implcitly, if the arrived is the chosen, do nothing.
 */
static void compete_message_block(struct sgsh_negotiation *fresh_mb, 
			struct sgsh_node *self_node, int *should_transmit_mb) {
        if (fresh_mb->initiator_pid < chosen_mb->initiator_pid) {
		free(chosen_mb); /* Heil compact allocation. */
		chosen_mb = fresh_mb;
                try_add_sgsh_node(self_node);
        } else if (fresh_mb->initiator_pid > chosen_mb->initiator_pid) {
		free(fresh_mb);
                should_transmit_mb = 0;
	}
}

/* Allocate memory for message_block and copy from buffer. */
static void alloc_copy_mb(struct sgsh_negotiation *mb, char *buf, 
							int bytes_read) {
	mb = (struct sgsh_negotiation *)malloc(bytes_read);
	memcpy(mb, buf, bytes_read);
	assert(bytes_read == mb->total_size);
}

/* The actual call to read in the message block.
 * If the call does not succeed or does not signal retry we have
 * to quit operation.
 */
static int call_read_pass_fail(int fd, char *buf, int buf_size, 
				int *fd_side, /* Mark (input or output) fd. */
				int *bytes_read, 
				int *error_code) {
	*error_code = 0;
	*fd_side = 0;
	if ((*bytes_read = read(fd, buf, buf_size)) == -1)
		*error_code = -errno;
	if ((*error_code == 0) || (*error_code != -EAGAIN)) {
		*fd_side = 1; /* Mark the side where input is coming from. */
		return OP_QUIT;
	}
	return OP_SUCCESS;
}

/* Read in circulated message block from either direction,
 * that is, input or output side. This capability
 * relies on an extension to a standard shell implementation,
 * e.g., bash, that allows reading and writing to both sides
 * for the negotiation phase. 
 * Set I/O to non-blocking in order to be able to retry on both
 * sides.
 * Returns the fd to write the message block if need be transmitted.
 */
static int try_read_message_block(char *buf, int buf_size, 
					struct sgsh_negotiation *fresh_mb,
					int *write_direction_fd) {
	int bytes_read, error_code = -EAGAIN, stdin_side = 0, stdout_size = 0;
	while (error_code == -EAGAIN) {
		if ((call_read_pass_fail(STDIN_FILENO, buf, buf_size, 
			&stdin_side, &bytes_read, &error_code) == OP_QUIT) ||
		    (call_read_pass_fail(STDOUT_FILENO, buf, buf_size, 
			&stdout_size, &bytes_read, &error_code) == OP_QUIT))
			break;
	}
	if (bytes_read == -1) {  /* Read failed. */
	 	err(1, "Reading from ");
		(stdin_side) ? err(1, "stdin ") : err(1, "stdout ");
		err(1, "file descriptor failed with error code %d.\n", 
						error_code);
		return error_code;
	} else {  /* Read succeeded. */
		alloc_copy_mb(fresh_mb, buf, bytes_read);
		if (stdin_side) *write_direction_fd = STDOUT_FILENO;
		else *write_direction_fd = STDIN_FILENO;
	}
	return OP_SUCCESS;
}

/* Construct a message block to use as a vehicle for the negotiation phase. */
static int construct_message_block(pid_t self_pid) {
	int memory_allocation_size = sizeof(struct sgsh_negotiation);
	chosen_mb = (struct sgsh_negotiation *)malloc(
				memory_allocation_size);
	if (!chosen_mb) {
		err(1, "Memory allocation of message block failed.");
		return OP_ERROR;
	}
	chosen_mb->node_list = NULL;
	chosen_mb->n_nodes = 0;
	chosen_mb->initiator_pid = self_pid;
	chosen_mb->state_flag = PROT_STATE_NEGOTIATION;
	chosen_mb->serial_no = 0;
	chosen_mb->total_size = memory_allocation_size;
	return OP_SUCCESS;
}

/* Get environment variable env_var. */
static int get_env_var(const char *env_var,int *value) {
	char *string_value = getenv(env_var);
	if (!string_value) {
		err(1, "Getting environment variable %s failed.\n", env_var);
		return OP_ERROR;
	}
	*value = atoi(string_value);
	return OP_SUCCESS;
}

/* Get environment variables SGSH_IN, SGSH_OUT set up by
 * the shell (through execvpe()).
 */
static int get_environment_vars(int *sgsh_in, int *sgsh_out) {
	if (get_env_var("SGSH_IN", sgsh_in) == OP_ERROR) return OP_ERROR;
	if (get_env_var("SGSH_OUT", sgsh_out) == OP_ERROR) return OP_ERROR;
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
                    int *n_input_fds, /* Number of input file descriptors. */
                    int *output_fds, /* Output file descriptors. */
                    int *n_output_fds) { /* Number of output file 
						descriptors. */
	DPRINTF("Entered sgsh_negotiation().\n");
	int sgsh_in, sgsh_out;
	int write_direction_fd;
	int negotiation_round = 0;
	int done_negotiating = 0;
	int should_transmit_mb = 1;
	pid_t self_pid = getpid();
	int buf_size = getpagesize();
	char buf[buf_size];
	struct sgsh_negotiation *fresh_mb = NULL;
	struct sgsh_node self_node;
	DPRINTF("Try to get environment variables.");
	if (get_environment_vars(&sgsh_in, &sgsh_out) == OP_ERROR) {
		err(1, "Failed to extract SGSH_IN, SGSH_OUT \
			environment variables.");
		return PROT_STATE_ERROR;
	}
        if ((sgsh_out) && (!sgsh_in)) {      /* Negotiation starter. */
                if (construct_message_block(self_pid) == OP_ERROR) 
			return PROT_STATE_ERROR;
                write_direction_fd = STDOUT_FILENO;
        } else {
		chosen_mb = NULL;
		if (try_read_message_block(buf, buf_size, fresh_mb, 
				&write_direction_fd) == OP_ERROR)
			return PROT_STATE_ERROR;
		chosen_mb = fresh_mb;
	}
	fill_sgsh_node(&self_node, tool_name, unique_id, channels_required,
						channels_provided);
	try_add_sgsh_node(&self_node);
	
	while (chosen_mb->state_flag == PROT_STATE_NEGOTIATION) {
		if (self_pid == chosen_mb->initiator_pid) { /* Round end. */
			negotiation_round++;
			if (negotiation_round == 3) {
				/* Placeholder: run algorithm. */
				chosen_mb->state_flag = 
						PROT_STATE_NEGOTIATION_END;
				done_negotiating = 1;
			}
		}
		if (should_transmit_mb)
			write(write_direction_fd, buf, buf_size);
		if (done_negotiating) break; /* Spread the word, now leave. */
		if (try_read_message_block(buf, buf_size, fresh_mb, 
					&write_direction_fd) == OP_ERROR)
			return PROT_STATE_ERROR;
		compete_message_block(fresh_mb, &self_node, 
						&should_transmit_mb);
	}
	return chosen_mb->state_flag;
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
	return PROT_STATE_RUN;
}

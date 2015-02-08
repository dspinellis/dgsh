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

/* Identifies the node and node's fd that sent the message block. */
struct dispatcher_node {
	int index;
	int fd_direction;
};

/* Each tool that participates in an sgsh graph is modelled as follows. */
struct sgsh_node {
        int index; /* Position in node array. */
        char name[100];
        int requires_channels; /* Input channels it can take. */
        int provides_channels; /* Output channels it can provide. */
	int sgsh_in;   /* Takes input from other tool(s) in sgsh graph. */
	int sgsh_out;  /* Provides output to other tool(s) in sgsh graph. */
};

/* The messge block structure that provides the vehicle for negotiation. */
struct sgsh_negotiation {
        struct sgsh_node *node_array;
        int n_nodes;
	pid_t initiator_pid;
        int state_flag;
        int serial_no;
	struct dispatcher_node origin;
	size_t total_size; /* Compact allocation that includes sgsh_node
				memory allocation. */
};

static struct sgsh_negotiation *chosen_mb; /* Our king message block. */
static struct sgsh_node self_node;
static struct dispatcher_node self_dispatcher;


/* Reallocate message block to fit new node coming in. */
static int realloc_mb_new_node() {
	int new_size = chosen_mb->total_size + sizeof(struct sgsh_node);
	int old_size = chosen_mb->total_size;
	void *p = realloc(chosen_mb, new_size);
	if (!p) {
		err(1, "Message block reallocation for adding a new node \
			failed.\n");
		return OP_ERROR;
	} else {
		chosen_mb = (struct sgsh_negotiation *)p;
		chosen_mb->total_size = new_size;
		chosen_mb->node_array = (struct sgsh_node *)&chosen_mb[1]; /* 
						* Node instances go after
						* the message block instance. */
		DPRINTF("Reallocated memory (%d -> %d) ", old_size, new_size);
		DPRINTF("to message block to fit new node.\n");
	}
	return OP_SUCCESS;
}

/* Copy the dispatcher static object that identifies the node
 * in the message block node array and shows the write point of
 * the send operation. This is a deep copy for simplicity. */
static void set_dispatcher() {
	chosen_mb->origin.index = self_dispatcher.index;
	chosen_mb->origin.fd_direction = self_dispatcher.fd_direction;
}

/* Add node to message block. Copy the node using offset-based
 * calculation from the start of the array of nodes.
 */
static void add_sgsh_node() {
	int n_nodes = chosen_mb->n_nodes;
	realloc_mb_new_node();
	memcpy(&chosen_mb->node_array[n_nodes], &self_node, 
					sizeof(struct sgsh_node));
	self_dispatcher.index = self_node.index = n_nodes;
	DPRINTF("Added node %s indexed in position %d in sgsh graph.\n", 
					self_node.name, self_node.index);
	chosen_mb->n_nodes++;
	DPRINTF("Sgsh graph now has %d nodes.\n", chosen_mb->n_nodes);
}

/* A constructor-like function for struct sgsh_node. */
static void fill_sgsh_node(const char *tool_name, int requires_channels, 
						int provides_channels) {
	self_node.index = -1;
	memcpy(self_node.name, tool_name, strlen(tool_name) + 1);
	self_node.requires_channels = requires_channels;
	self_node.provides_channels = provides_channels;
	DPRINTF("Sgsh node for tool %s created.\n", tool_name);
}

/* Check if the arrived message block preexists our chosen one
 * and substitute the chosen if so.
 * If the arrived message block is younger discard it and don't
 * forward it.
 * Implcitly, if the arrived is the chosen, do nothing.
 */
static void compete_message_block(struct sgsh_negotiation *fresh_mb, 
						int *should_transmit_mb) {
        if (fresh_mb->initiator_pid < chosen_mb->initiator_pid) {
		free(chosen_mb); /* Heil compact allocation. */
		chosen_mb = fresh_mb;
                add_sgsh_node();
        } else if (fresh_mb->initiator_pid > chosen_mb->initiator_pid) {
		free(fresh_mb);
                should_transmit_mb = 0;
	}
}

/* Point next write operation to the correct file descriptor: stdin or stdout.*/
static void point_io_direction(int current_direction) {
	if ((current_direction == STDIN_FILENO) && (self_node.sgsh_out))
			self_dispatcher.fd_direction = STDOUT_FILENO;
	else if ((current_direction == STDOUT_FILENO) && (self_node.sgsh_in))
			self_dispatcher.fd_direction = STDIN_FILENO;
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
	DPRINTF("Try read from %s.\n", (fd) ? "stdout" : "stdin");
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
					struct sgsh_negotiation *fresh_mb) {
	int bytes_read, error_code = -EAGAIN, stdin_side = 0, stdout_size = 0;
	while (error_code == -EAGAIN) { /* Try read from stdin, then stdout. */
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
		point_io_direction(stdin_side);
		DPRINTF("Read succeeded: %d bytes read from %s.\n", bytes_read,
			(self_dispatcher.fd_direction) ? "stdout" : "stdin");
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
	chosen_mb->node_array = NULL;
	chosen_mb->n_nodes = 0;
	chosen_mb->initiator_pid = self_pid;
	chosen_mb->state_flag = PROT_STATE_NEGOTIATION;
	chosen_mb->serial_no = 0;
	chosen_mb->total_size = memory_allocation_size;
	DPRINTF("Message block created by pid %d.\n", (int)self_pid);
	return OP_SUCCESS;
}

/* Get environment variable env_var. */
static int get_env_var(const char *env_var,int *value) {
	DPRINTF("Call getenv().\n");
	char *string_value = getenv(env_var);
	if (!string_value) {
		err(1, "Getting environment variable %s failed.\n", env_var);
		return OP_ERROR;
	} else
		DPRINTF("getenv() returned string value %s.\n", string_value);
	*value = atoi(string_value);
	DPRINTF("Integer form of value is %d.\n", *value);
	return OP_SUCCESS;
}

/* Get environment variables SGSH_IN, SGSH_OUT set up by
 * the shell (through execvpe()).
 */
static int get_environment_vars() {
	DPRINTF("Try to get environment variable SGSH_IN.\n");
	if (get_env_var("SGSH_IN", &self_node.sgsh_in) == OP_ERROR) 
		return OP_ERROR;
	DPRINTF("Try to get environment variable SGSH_OUT.\n");
	if (get_env_var("SGSH_OUT", &self_node.sgsh_out) == OP_ERROR) 
		return OP_ERROR;
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
                    int channels_required, /* How many input channels can take. */
                    int channels_provided, /* How many output channels can 
						provide. */
                                     /* Output: to fill. */
                    int *input_fds,  /* Input file descriptors. */
                    int *n_input_fds, /* Number of input file descriptors. */
                    int *output_fds, /* Output file descriptors. */
                    int *n_output_fds) { /* Number of output file 
						descriptors. */
	int negotiation_round = 0;
	int done_negotiating = 0;
	int should_transmit_mb = 1;
	pid_t self_pid = getpid();
	int buf_size = getpagesize();
	char buf[buf_size];
	struct sgsh_negotiation *fresh_mb = NULL;
	
	memset(buf, 0, buf_size);
	DPRINTF("Tool %s with pid %d entered sgsh_negotiation.\n", tool_name,
							(int)self_pid);
	DPRINTF("Try to get environment variables.");
	if (get_environment_vars() == OP_ERROR) {
		err(1, "Failed to extract SGSH_IN, SGSH_OUT \
			environment variables.");
		return PROT_STATE_ERROR;
	}
        if ((self_node.sgsh_out) && (!self_node.sgsh_in)) { /* Negotiation starter. */
                if (construct_message_block(self_pid) == OP_ERROR) 
			return PROT_STATE_ERROR;
                self_dispatcher.fd_direction = STDOUT_FILENO;
        } else {
		chosen_mb = NULL;
		if (try_read_message_block(buf, buf_size, fresh_mb) == OP_ERROR)
			return PROT_STATE_ERROR;
		chosen_mb = fresh_mb;
	}
	fill_sgsh_node(tool_name, channels_required, channels_provided);
	add_sgsh_node();
	
	while (chosen_mb->state_flag == PROT_STATE_NEGOTIATION) {
		if (self_pid == chosen_mb->initiator_pid) { /* Round end. */
			negotiation_round++;
			if (negotiation_round == 3) {
				/* Placeholder: run algorithm. */
				chosen_mb->state_flag = 
						PROT_STATE_NEGOTIATION_END;
				done_negotiating = 1;
				DPRINTF("Negotiation protocol state change: end of negotiation phase.\n");
			}
		}
		if (should_transmit_mb) {
			assert(self_node.index >= 0); /* Node is added. */
			set_dispatcher();
			memcpy(buf, chosen_mb, chosen_mb->total_size);
			write(self_dispatcher.fd_direction, buf, 
						chosen_mb->total_size);
			DPRINTF("Ship message block to next node in graph from file descriptor: %s.\n", (self_dispatcher.fd_direction) ? "stdout" : "stdin");
		}
		if (done_negotiating) break; /* Spread the word, now leave. */
		if (try_read_message_block(buf, buf_size, fresh_mb) == OP_ERROR)
			return PROT_STATE_ERROR;
		compete_message_block(fresh_mb, &should_transmit_mb);
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
int sgsh_run() {
	return PROT_STATE_RUN;
}

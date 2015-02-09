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
#define OP_EXISTS 3
#define OP_CREATE 4
#define OP_NOOP 5

/* TODO: message block size vs page size check before write. */

/* Identifies the node and node's fd that sent the message block. */
struct dispatcher_node {
	int index;
	int fd_direction;
};

/* Models an I/O connection between tools on an sgsh graph. */
struct sgsh_edge {
	int from; /* Index of node on the graph where data comes from (out). */
	int to; /* Index of node on the graph that receives the data (in). */
};

/* Each tool that participates in an sgsh graph is modelled as follows. */
struct sgsh_node {
        int unique_id; 
        char name[100];
        int requires_channels; /* Input channels it can take. */
        int provides_channels; /* Output channels it can provide. */
	int sgsh_in;   /* Takes input from other tool(s) in sgsh graph. */
	int sgsh_out;  /* Provides output to other tool(s) in sgsh graph. */
};

/* The message block structure that provides the vehicle for negotiation. */
struct sgsh_negotiation {
        struct sgsh_node *node_array;
        int n_nodes;
	struct sgsh_edge *edge_array;
        int n_edges;
	pid_t initiator_pid;
        int state_flag;
        int serial_no;
	struct dispatcher_node origin;
	size_t total_size; /* Compact allocation that includes sgsh_node
				memory allocation. */
};

/* Memory organisation of message block.
 * Because message block will be passed around process address spaces, 
 * any pointers should point to slots internal to the message block. 
 * Therefore a message block allocation (structure sgsh_negotiation) 
 * contains related structure instances such as the node and edge 
 * arrays organised as follows:
 *
 * struct sgsh_negotiation
 * struct sgsh_node (array)
 * ... (x n_nodes) 
 * struct sgsh_edge (array)
 * ... (x n_edges) 
 */

static struct sgsh_negotiation *chosen_mb; /* Our king message block. */
static struct sgsh_node self_node; /* The sgsh node that models this tool. */
static struct dispatcher_node self_dispatcher; /* Dispatch info for this tool.*/


/* Reallocate message block to fit new node coming in. */
static int realloc_mb_new_node() {
	int new_size = chosen_mb->total_size + sizeof(struct sgsh_node);
	int old_size = chosen_mb->total_size;

	/* Make room for new node. Copy-move edge array. */
	int n_edges = chosen_mb->n_edges;
	int edges_size = sizeof(struct sgsh_edge) * chosen_mb->n_edges;
	struct sgsh_edge edge_array_store[n_edges];
	memcpy(edge_array_store, chosen_mb->edge_array, edges_size);

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
		DPRINTF("Reallocated memory (%d -> %d) to message block to fit new node.\n", old_size, new_size);
		chosen_mb->edge_array = (struct sgsh_edge *)&chosen_mb->node_array[chosen_mb->n_nodes + 1]; /* New node has not been added yet. */
		memcpy(chosen_mb->edge_array, edge_array_store, edges_size);
	}
	return OP_SUCCESS;
}

/* Copy the dispatcher static object that identifies the node
 * in the message block node array and shows the write point of
 * the send operation. This is a deep copy for simplicity. */
static void set_dispatcher() {
	chosen_mb->origin.index = self_dispatcher.index;
	assert(self_dispatcher.index >= 0); /* Node is added to the graph. */
	chosen_mb->origin.fd_direction = self_dispatcher.fd_direction;
}

/* Lookup an edge in the sgsh graph. */
static int lookup_sgsh_edge(struct sgsh_edge *e) {
	int i;
	for (i = 0; i < chosen_mb->n_edges; i++) {
		if ((chosen_mb->edge_array[i].from == e->from) &&
		    (chosen_mb->edge_array[i].to == e->to))
			return OP_EXISTS;
	}
	return OP_CREATE;
}

/* Fill edge depending on input/output fd information 
 * passed by sender and found in receiver (this tool or self).
 */
static int fill_sgsh_edge(struct sgsh_edge *e) {
	int i;
	int n_nodes = chosen_mb->n_nodes;
	for (i = 0; n_nodes; i++)
		if (i == chosen_mb->origin.index) break;
	if (i == n_nodes) {
		err(1, "Dispatcher node with index position %d not present in graph.\n", chosen_mb->origin.index);
		return OP_ERROR;
	}
	if (chosen_mb->origin.fd_direction == STDIN_FILENO) {
	/* MB sent from stdin, so sender is the destination of the edge.
	 * Self should be sgsh-active on output side.Self's current fd is stdin 
	 * if self is sgsh-active on input side or output side otherwise. 
	 * Self (the recipient) is the source of the edge. */
		e->to = chosen_mb->origin.index; 
		assert(self_node.sgsh_out); 
		assert((self_node.sgsh_in && 
			self_dispatcher.fd_direction == STDIN_FILENO) ||
			self_dispatcher.fd_direction == STDOUT_FILENO);
		e->from = self_dispatcher.index; 
	} else if (chosen_mb->origin.fd_direction == STDOUT_FILENO) { 
		/* Vice versa. */
		e->from = chosen_mb->origin.index;
		assert(self_node.sgsh_in);
		assert((self_node.sgsh_out && 
			self_dispatcher.fd_direction == STDOUT_FILENO) ||
			self_dispatcher.fd_direction == STDIN_FILENO);
		e->to = self_dispatcher.index;
	}
	return OP_SUCCESS;
} /* Assert or return error? */

/* Reallocate message block to fit new edge coming in. */
static int realloc_mb_new_edge() {
	int new_size = chosen_mb->total_size + sizeof(struct sgsh_edge);
	int old_size = chosen_mb->total_size;
	void *p = realloc(chosen_mb, new_size);
	if (!p) {
		err(1, "Message block reallocation for adding a new edge failed.\n");
		return OP_ERROR;
	} else {
		chosen_mb = (struct sgsh_negotiation *)p;
		chosen_mb->total_size = new_size;
		chosen_mb->node_array = (struct sgsh_node *)&chosen_mb[1]; /* 
						* Node instances go after
						* the message block instance. */
		chosen_mb->edge_array = (struct sgsh_edge *)&chosen_mb->node_array[chosen_mb->n_nodes];
		DPRINTF("Reallocated memory (%d -> %d) to message block to fit new edge.\n", old_size, new_size);
	}
	return OP_SUCCESS;
}

/* Try to add a newly occured edge in the sgsh graph. */
static int try_add_sgsh_edge() {
	if (chosen_mb->origin.index >= 0) { /* If MB not created just now: */
		struct sgsh_edge new_edge;
		fill_sgsh_edge(&new_edge);
		if (lookup_sgsh_edge(&new_edge) == OP_CREATE) {
			int n_edges = chosen_mb->n_edges;
			if (realloc_mb_new_edge() == OP_ERROR) return OP_ERROR;
			memcpy(&chosen_mb->edge_array[n_edges], &new_edge, 
						sizeof(struct sgsh_edge));
			DPRINTF("Added edge (%d -> %d) in sgsh graph.\n",
					new_edge.from, new_edge.to);
			chosen_mb->n_edges++;
			DPRINTF("Sgsh graph now has %d edges.\n", 
							chosen_mb->n_edges);
			chosen_mb->serial_no++;
			return OP_SUCCESS;
		}
		return OP_EXISTS;
	}
	return OP_NOOP;
}

/* Add node to message block. Copy the node using offset-based
 * calculation from the start of the array of nodes.
 */
static int try_add_sgsh_node() {
	int n_nodes = chosen_mb->n_nodes;
	int i;
	for (i = 0; i < n_nodes; i++)
		if (chosen_mb->node_array[i].unique_id == self_node.unique_id) 
			break;
	if (i == n_nodes) {
		if (realloc_mb_new_node() == OP_ERROR) return OP_ERROR;
		memcpy(&chosen_mb->node_array[n_nodes], &self_node, 
					sizeof(struct sgsh_node));
		self_dispatcher.index = n_nodes;
		DPRINTF("Added node %s indexed in position %d in sgsh graph.\n",
					self_node.name, self_dispatcher.index);
		chosen_mb->n_nodes++;
		DPRINTF("Sgsh graph now has %d nodes.\n", chosen_mb->n_nodes);
		chosen_mb->serial_no++;
		return OP_SUCCESS;
	}
	return OP_EXISTS;
}

/* A constructor-like function for struct sgsh_node. */
static void fill_sgsh_node(const char *tool_name, int unique_id,
				int requires_channels, int provides_channels) {
	self_node.unique_id = unique_id;
	memcpy(self_node.name, tool_name, strlen(tool_name) + 1);
	self_node.requires_channels = requires_channels;
	self_node.provides_channels = provides_channels;
	DPRINTF("Sgsh node for tool %s with unique id %d created.\n", 
						tool_name, unique_id);
}

/* Check if the arrived message block preexists our chosen one
 * and substitute the chosen if so.
 * If the arrived message block is younger discard it and don't
 * forward it.
 * If the arrived is the chosen, try to add the edge.
 */
static int compete_message_block(struct sgsh_negotiation *fresh_mb, 
			int *should_transmit_mb, int *updated_mb_serial_no) {
        *should_transmit_mb = 1; /* Default value. */
	*updated_mb_serial_no = 0; /* Default value. */
        if (fresh_mb->initiator_pid < chosen_mb->initiator_pid) { /* New chosen! .*/
		free(chosen_mb); /* Heil compact allocation. */
		chosen_mb = fresh_mb;
                if (try_add_sgsh_node() == OP_ERROR)
			return OP_ERROR; /* Double realloc: one for node, */
                if (try_add_sgsh_edge() == OP_ERROR)
			return OP_ERROR; /* one for edge. */
		*updated_mb_serial_no = 1; /*Substituting chosen_mb is an update.*/
        } else if (fresh_mb->initiator_pid > chosen_mb->initiator_pid) {
		free(fresh_mb);
                *should_transmit_mb = 0;
	} else {
		if (fresh_mb->serial_no > chosen_mb->serial_no) {
			*updated_mb_serial_no = 1;
			free(chosen_mb);
			chosen_mb = fresh_mb;
		} else 
			free(fresh_mb);
                if (try_add_sgsh_edge() == OP_ERROR) return OP_ERROR;
	}
	return OP_SUCCESS;
}

/* Point next write operation to the correct file descriptor: stdin or stdout.
 * If only one is active, stay with that one.
 */
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
	chosen_mb->origin.index = -1;
	chosen_mb->origin.fd_direction = -1;
	chosen_mb->total_size = memory_allocation_size;
	DPRINTF("Message block created by pid %d.\n", (int)self_pid);
	return OP_SUCCESS;
}

/* Get environment variable env_var. */
static int get_env_var(const char *env_var,int *value) {
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
		    int unique_id, /* For multiple appearances of same tool. */ 
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
	int updated_mb_serial_no = 1;
	pid_t self_pid = getpid();
	int buf_size = getpagesize();
	char buf[buf_size];
	struct sgsh_negotiation *fresh_mb = NULL;
	
	memset(buf, 0, buf_size); /* Clear buffer used to read/write messages.*/
	DPRINTF("Tool %s with pid %d entered sgsh_negotiation.\n", tool_name,
							(int)self_pid);
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
	fill_sgsh_node(tool_name, unique_id, channels_required, 
						channels_provided);
	if (try_add_sgsh_node() == OP_ERROR) return PROT_STATE_ERROR;
	if (try_add_sgsh_edge() == OP_ERROR) return PROT_STATE_ERROR;
	
	while (chosen_mb->state_flag == PROT_STATE_NEGOTIATION) {
		if (self_pid == chosen_mb->initiator_pid) { /* Round end. */
			negotiation_round++;
			if ((negotiation_round == 3) && 
			    (!updated_mb_serial_no)) {
				/* Placeholder: run algorithm. */
				chosen_mb->state_flag = 
						PROT_STATE_NEGOTIATION_END;
				chosen_mb->serial_no++;
				done_negotiating = 1;
				DPRINTF("Negotiation protocol state change: end of negotiation phase.\n");
			}
		}
		if (should_transmit_mb) {
			set_dispatcher();
			memcpy(buf, chosen_mb, chosen_mb->total_size);
			write(self_dispatcher.fd_direction, buf, 
						chosen_mb->total_size);
			DPRINTF("Ship message block to next node in graph from file descriptor: %s.\n", (self_dispatcher.fd_direction) ? "stdout" : "stdin");
		}
		if (done_negotiating) break; /* Did spread the word,now leave.*/
		if (try_read_message_block(buf, buf_size, fresh_mb) == OP_ERROR)
			return PROT_STATE_ERROR;
		if (compete_message_block(fresh_mb, &should_transmit_mb,
					&updated_mb_serial_no) == OP_ERROR)
			return PROT_STATE_ERROR;
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

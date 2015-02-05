#include <stdlib.h> /* getenv() */
#include <err.h> /* err() */
#include <unistd.h> /* getpid() */
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
};

static struct sgsh_negotiation *selected_message_block;

void free_message_block() {

}

int construct_message_block() {
	selected_message_block = (struct sgsh_negotiation *)malloc(
				sizeof(struct sgsh_negotiation));
	if (!selected_message_block) {
		err(1, "Memory allocation of memory block failed.");
		return OP_ERROR;
	}
	selected_message_block->n_nodes = 0;
	selected_message_block->selected_pid = getpid();
	selected_message_block->state_flag = PROT_STATE_ZERO;
	selected_message_block->serial_no = 0;
	return OP_SUCCESS;
}


int get_env_var(const char *env_var,int *value) {
	char *string_value = getenv(env_var);
	if (!string_value) {
		err(1, "Getting environment variable \
		%s failed.\n", env_var);
		return OP_ERROR;
	}
	*value = atoi(string_value);
	return OP_SUCCESS;
}

int get_environment_vars(int *sgsh_in, int *sgsh_out) {
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
	if (get_environment_vars(&sgsh_in, &sgsh_out) == OP_ERROR)
		err(1, "Failed to extract SGSH_IN, SGSH_OUT \ 
			environment variables.";)
        if ((sgsh_out) && (!sgsh_in)) {      /* Negotiation starter. */
                construct_message_block();
                write_direction_stream = stdout;
        } else {
		selected_message_block = NULL;
	}
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

#include <stdlib.h> /* getenv() */
#include <err.h> /* err() */
#include "sgsh-negotiate.h" /* sgsh_negotiate(), sgsh_run() */


#define OP_ERROR 1
#define OP_SUCCESS 0

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
                    int hash,        /* Identifier. (Distinguish multiple 
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
}

/* If negotiation is successful, tools configure input and output 
 * according to the provided file descriptors and then they call
 * sgsh_run() to signal that they have set their input/output and
 * are ready for execution (or that they failed) by setting STATE_RUN
 * or STATE_ERROR. An algorithm
 * verifies that all tools completed this stage too successfully 
 * and the function returns success or failure.
 */
int sgsh_run(int hash) {

}

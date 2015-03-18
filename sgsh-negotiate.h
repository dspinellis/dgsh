#define PROT_STATE_NEGOTIATION 1
#define PROT_STATE_NEGOTIATION_END 2
#define PROT_STATE_SOLUTION_SHARE 3
#define PROT_STATE_ERROR 4

#ifdef UNIT_TESTING
#define STATIC 
#else
#define STATIC static
#endif

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
                    int **input_fds,  /* Input file descriptors. */
                    int *n_input_fds, /* Number of input file descriptors. */
                    int **output_fds, /* Output file descriptors. */
                    int *n_output_fds); /* Number of output file descriptors. */

#ifdef UNIT_TESTING
int validate_input(int channels_required, int channels_provided, const char *tool_name);
#endif

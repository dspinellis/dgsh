#ifndef DGSH_INTERNAL_API_H
#define DGSH_INTERNAL_API_H

#include <stdbool.h>

#include "dgsh-negotiate.h"

/* The message block implicitly used by many functions */
extern struct dgsh_negotiation *chosen_mb;

/* Identifies the node and node's fd that sent the message block. */
struct node_io_side {
	int index;		/* Node index on message block node array */
	int fd_direction;	/* Message block origin node's file
				 * descriptor
				 */
};

/* Stores the number of a conc's IO fds */
struct dgsh_conc {
	pid_t pid;
	int input_fds;
	int output_fds;
	int n_proc_pids;
	int *proc_pids;		/* pids at the multipipe end */
	int endpoint_pid;	/* pid at the other end */
	bool multiple_inputs;	/* true for input conc */
};

/* The message block structure that provides the vehicle for negotiation. */
struct dgsh_negotiation {
	int version;			/* Protocol version. */
        struct dgsh_node *node_array;	/* Nodes, that is tools, on the dgsh
					 * graph.
					 */
        int n_nodes;			/* Number of nodes */
	struct dgsh_edge *edge_array;	/* Edges, that is connections between
					 * nodes, on the dgsh graph
					 */
        int n_edges;			/* Number of edges */
	pid_t initiator_pid;		/* pid of the tool initiating this
					 * negotiation block. All processes that
					 * only contribute their output
					 * channel to the dgsh graph will
					 * initiate the negotiation process
					 * by constructing and sharing a
					 * message block. The one with the
					 * smaller pid will prevail.
					 */
	enum prot_state state;		/* State of the negotiation process */
	int origin_index;		/* The node from which the message
					 * block is dispatched.
					 */
	int origin_fd_direction;	/* The origin's input or output channel
					 */
	bool is_origin_conc;		/* True if origin is a concentrator */
	pid_t conc_pid;			/* Concentrator pid, otherwise -1 */
	struct dgsh_node_connections *graph_solution; /* The solution to the
						       * I/O constraint problem
						       * at hand.
						       */
	struct dgsh_conc *conc_array;	/* Array of concentrators facilitating
					 * the negotiation. The need for this
					 * array emerged in cases where a conc
					 * is directly connected to another
					 * conc. The array stores a conc's
					 * inputs/outputs so that a conc can
					 * retrieve another conc's
					 * inputs/outputs.
					 */
	int n_concs;

};

enum op_result solve_dgsh_graph(void);
enum op_result construct_message_block(const char *tool_name, pid_t pid);
struct dgsh_conc *find_conc(struct dgsh_negotiation *mb, pid_t pid);
pid_t get_origin_pid(struct dgsh_negotiation *mb);
int get_expected_fds_n(struct dgsh_negotiation *mb, pid_t pid);
int get_provided_fds_n(struct dgsh_negotiation *mb, pid_t pid);
enum op_result read_message_block(int read_fd,
		struct dgsh_negotiation **fresh_mb);
enum op_result write_message_block(int write_fd);
void free_mb(struct dgsh_negotiation *mb);
int read_fd(int input_socket);
void write_fd(int output_socket, int fd_to_write);

#ifdef UNIT_TESTING

extern bool multiple_inputs;
extern int nfd;
extern int next_fd(int fd, bool *ro);
extern int read_fd(int input_socket);
extern void write_fd(int output_socket, int fd_to_write);

#endif

#endif

#ifndef SGSH_INTERNAL_API_H
#define SGSH_INTERNAL_API_H

#include <stdbool.h>

#include "sgsh-negotiate.h"

/* The message block implicitly used by many functions */
extern struct sgsh_negotiation *chosen_mb;

/* Identifies the node and node's fd that sent the message block. */
struct node_io_side {
	int index;		/* Node index on message block node array */
	int fd_direction;	/* Message block origin node's file
				 * descriptor
				 */
};

/* The message block structure that provides the vehicle for negotiation. */
struct sgsh_negotiation {
	int version;			/* Protocol version. */
        struct sgsh_node *node_array;	/* Nodes, that is tools, on the sgsh
					 * graph.
					 */
        int n_nodes;			/* Number of nodes */
	struct sgsh_edge *edge_array;	/* Edges, that is connections between
					 * nodes, on the sgsh graph
					 */
        int n_edges;			/* Number of edges */
	pid_t initiator_pid;		/* pid of the tool initiating this
					 * negotiation block. All processes that
					 * only contribute their output
					 * channel to the sgsh graph will
					 * initiate the negotiation process
					 * by constructing and sharing a
					 * message block. The one with the
					 * smaller pid will prevail.
					 */
	pid_t preceding_process_pid;	/* The pid of the process that passed the
					 * message block to the process that
					 * found a solution and set the PS_RUN
					 * flag.
					 */
	enum prot_state state;		/* State of the negotiation process */
        int serial_no;			/* Message block serial no.
					 * It shows how many times it has been
					 * updated.
					 */
	int origin_index;		/* The node from which the message
					 * block is dispatched.
					 */
	int origin_fd_direction;	/* The origin's input or output channel
					 */
};

enum op_result read_message_block(fd_set read_fds, int *read_fd,
		struct sgsh_negotiation **fresh_mb);
enum op_result write_message_block(int write_fd);
void free_mb(struct sgsh_negotiation *mb);
#endif

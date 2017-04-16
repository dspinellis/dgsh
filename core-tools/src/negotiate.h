/*
 * Copyright 2016-2017 Marios Fragkoulis
 *
 * Dgsh private negotiation API
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

#ifndef NEGOTIATE_H
#define NEGOTIATE_H

#include <stdbool.h>
#include <sys/socket.h> /* struct cmsghdr */

#include <signal.h>	/* sig_atomic_t */

#include "dgsh.h"

/* Negotiation protocol states */
enum prot_state {
	PS_COMPLETE,		/* Negotiation process is complete */
	PS_NEGOTIATION,		/* Negotiation phase */
	PS_NEGOTIATION_END,	/* End of negotiation phase */
	PS_RUN,			/* Share solution; prepare to run */
	PS_ERROR,		/* Error in negotiation process */
	PS_DRAW_EXIT,		/* Compute and write the solution and exit */
};

union fdmsg {
	struct cmsghdr h;
	char buf[CMSG_SPACE(sizeof(int))];
};

/*
 * Results of operations
 * Also negative values signify a failed operation's errno value
 */
enum op_result {
	OP_SUCCESS,		/* Successful */
	OP_ERROR,		/* Unresolvable error due to I/O problem
				 * constraints provided by the processes
				 * on the dgsh graph or memory constraints
				 * of the systems.
				 */
	OP_EXISTS,		/* Node or edge already registered with the
				 * dgsh graph.
				 */
	OP_CREATE,		/* Node ar edge registered with the dgsh 
				 * graph.
				 */
	OP_NOOP,		/* No operation when trying to add an edge
				 * on a graph with just one node at the time.
				 */
	OP_RETRY,		/* Not all constraints of an I/O constraint
				 * problem have been satisfied yet.
				 * Retry by leveraging flexible constraints.
				 */
	OP_DRAW_EXIT,		/* Compute and write the solution and exit */
};


#ifdef UNIT_TESTING

#define STATIC

/* Models an I/O connection between tools on an dgsh graph. */
struct dgsh_edge {
        int from; /* Index of node on the graph where data comes from (out). */
        int to; /* Index of node on the graph that receives the data (in). */
        int instances; /* Number of instances of an edge. */
	int from_instances; /* Number of instances the origin node of an edge can provide. */
	int to_instances; /* Number of instances the destination of an edge can require. */
};

extern bool multiple_inputs;
extern int nfd;
extern int next_fd(int fd, bool *ro);
extern int read_fd(int input_socket);
extern void write_fd(int output_socket, int fd_to_write);
#else

#define STATIC static

#endif /* UNIT_TESTING */

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
	bool is_error_confirmed;	/* Error state is confirmed by the initiator
					   and propagated to the graph */
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

enum op_result solve_graph(void);
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
/* Alarm mechanism and on_exit handling */
void set_negotiation_complete();
void dgsh_alarm_handler(int);

#endif /* NEGOTIATE_H */

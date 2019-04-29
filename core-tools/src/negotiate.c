/*
 * Copyright 2016, 2017 Marios Fragkoulis
 *
 * A passive component that aids the dgsh negotiation by passing
 * message blocks among participating processes.
 * When the negotiation is finished and the processes get connected by
 * pipes, it exits.
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

#include <assert.h>		/* assert() */
#include <errno.h>		/* ENOBUFS */
#include <err.h>		/* err() */
#include <limits.h>		/* IOV_MAX */
#include <stdbool.h>		/* bool, true, false */
#include <stdio.h>		/* fprintf() in DPRINTF() */
#include <stdlib.h>		/* getenv(), errno, atexit() */
#include <string.h>		/* memcpy() */
#include <sysexits.h>		/* EX_PROTOCOL, EX_OK */
#include <sys/socket.h>		/* sendmsg(), recvmsg() */
#include <unistd.h>		/* getpid(), getpagesize(),
				 * STDIN_FILENO, STDOUT_FILENO,
				 * STDERR_FILENO, alarm(), sysconf()
				 */
#include <signal.h>		/* signal(), SIGALRM */
#include <time.h>		/* nanosleep() */
#include <sys/select.h>		/* select(), fd_set, */
#include <stdio.h>		/* printf family */

#include "negotiate.h"		/* Message block and I/O */
#include "dgsh-debug.h"		/* DPRINTF() */

#ifdef TIME
#include <time.h>
static struct timespec tstart={0,0}, tend={0,0};
#endif

/* Default negotiation timeout (s) */
#define DGSH_TIMEOUT 5

#ifndef UNIT_TESTING

/* Models an I/O connection between tools on an dgsh graph. */
struct dgsh_edge {
	int from;		/* Index of node on the graph where data
				 * comes from (out).
				 */
	int to;			/* Index of node on the graph that
				 * receives the data (in).
				 */
	int instances;		/* Number of instances of an edge. */
	int from_instances;	/* Number of instances the origin node of
				 * an edge can provide.
				 */
	int to_instances;	/* Number of instances the destination
				 * of an edge can require.
				 */
};

#endif

/* Each tool that participates in an dgsh graph is modelled as follows. */
struct dgsh_node {
        pid_t pid;
	int index;		/* Position in message block's node array. */
        char name[100];		/* Tool's name */
        int requires_channels;	/* Input channels it can take. */
        int provides_channels;	/* Output channels it can provide. */
	int dgsh_in;		/* Takes input from other tool(s) on
				 * dgsh graph.
				 */
	int dgsh_out;		/* Provides output to other tool(s)
				 * on dgsh graph.
				 */
};

/* Holds a node's connections. It contains a piece of the solution. */
struct dgsh_node_connections {
	int node_index;				/* The subject of the
						 * connections. For
						 * verification.
						 */
	struct dgsh_edge *edges_incoming;	/* Array of edges through
						 * which other nodes provide
						 * input to node at node_index.
						 */
	int n_edges_incoming;			/* Number of incoming edges */
	int n_instances_incoming_free;		/* Number of incoming edges
						 * not yet bound to a pair
						 * node's output channel.
						 */
	struct dgsh_edge *edges_outgoing;	/* Array of edges through
						 * which a node provides
						 * output to other nodes.
						 */
	int n_edges_outgoing;			/* Number of outgoing edges */
	int n_instances_outgoing_free;		/* Number of outgoing edges
						 * not yet binded to a pair
						 * node's outgoing edges.
						 */
};

/* The output of the negotiation process. */
struct dgsh_node_pipe_fds {
	int *input_fds;		/* Array of input file descriptors */
	int n_input_fds;	/* Number of input file descriptors */
	int *output_fds;	/* Array of output file descriptors */
	int n_output_fds;	/* Number of output file descriptors */
};

/**
 * Memory organisation of message block.
 * Message block will be passed around process address spaces.
 * Message block contains a number of scalar fields and two pointers
 * to an array of dgsh nodes and edges respectively.
 * To pass the message block along with nodes and edges, three writes
 * in this order take place.
 */

/* The message block implicitly used by many functions */
struct dgsh_negotiation *chosen_mb;
static struct dgsh_node self_node;		/* The dgsh node that models
						   this tool. */
static char *programname;

static struct node_io_side self_node_io_side;	/* Dispatch info for this tool. */
static struct dgsh_node_pipe_fds self_pipe_fds;	/* A tool's read and write file
						 * descriptors to use at execution.
						 */
static bool init_error = false;
static volatile sig_atomic_t negotiation_completed = 0;
int dgsh_debug_level = 0;

static void get_environment_vars();
static int dgsh_exit(int state, int flags);

/* Force the inclusion of the ELF note section */
extern int dgsh_force_include;
void
dgsh_force_include_function(void)
{
	dgsh_force_include = 1;
}


#ifndef UNIT_TESTING
static void
dgsh_exit_handler(void)
{
	if (negotiation_completed)
		return;
	init_error = true;
	/* Finish negotiation, if required */
	get_environment_vars();
	if (self_node.dgsh_in != 0 || self_node.dgsh_out != 0) {
		warnx("exiting before dgsh negotiation is complete");
		DPRINTF(4, "dgsh: error state. Enter negotiation to inform the graph");
		dgsh_negotiate(0, programname ? programname : "dgsh client", NULL,
				NULL, NULL, NULL);
	}
}
#endif

void
dgsh_alarm_handler(int signal)
{
	if (signal == SIGALRM)
		if (negotiation_completed == 0) {
			char msg[100];
			sprintf(msg, "%d dgsh: timeout for negotiation. Exit.\n",
					getpid());
			negotiation_completed = 1;
			write(2, msg, strlen(msg));
			_exit(EX_PROTOCOL);
		}
}

#ifndef UNIT_TESTING
__attribute__((constructor))
static void
install_exit_handler(void)
{
	atexit(dgsh_exit_handler);
}
#endif

static int iov_max;

// Setup iov_max handling runtime, even if it is defined at runtime
__attribute__((constructor))
static void
setup_iov_max(void)
{
#if defined(IOV_MAX)
        iov_max = IOV_MAX;
#else
        iov_max = (int)sysconf(_SC_IOV_MAX);
#endif
}

/**
 * Remove path to command to save space in the graph plot
 * Find first space if any and take the name up to there
 * Find and remove path prepended to the name
 * Rejoin the name with arguments
 * Escape double quotes
 */
STATIC void
process_node_name(char *name, char **processed_name)
{
	char no_path_name[strlen(name)];
	memset(no_path_name, 0, sizeof(no_path_name));
	char *s = strstr(name, " ");

	DPRINTF(4, "Node name to process: %s", name);
	if (s)
		strncpy(no_path_name, name, s - name);
	else
		strcpy(no_path_name, name);

	DPRINTF(4, "no_path_name: %s, s: %s", no_path_name, s);
	char *p = no_path_name;
	char *m = strstr(no_path_name, "/");
	while (m) {
		p = ++m;
		m = strstr(m, "/");
	}
	DPRINTF(4, "no_path_name: %s, m: %s", no_path_name, m);

	if (s)
		sprintf(no_path_name, "%s%s", p, s);

	DPRINTF(4, "no_path_name: %s, p: %s", no_path_name, p);
	m = strstr(no_path_name, "\"");
	char *mm = NULL;
	while (m) {
		DPRINTF(4, "processed_name: %s, m: %s, mm: %s",
				*processed_name, m, mm);
		if (strlen(*processed_name) == 0)
			strncpy(*processed_name, no_path_name, m - no_path_name);
		else {
			strcat(*processed_name, "\\");
			strncat(*processed_name, mm, m - mm);
			DPRINTF(4, "processed_name: %s, m - mm: %ld",
					*processed_name, (long)(m - mm));
		}
		mm = m;
		m = strstr(++m, "\"");
	}
	if (mm) {
		strcat(*processed_name, "\\");
		strcat(*processed_name, mm);
	} else
		strcpy(*processed_name, no_path_name);

	DPRINTF(4, "final processed_name: %s, m: %s, mm: %s",
				*processed_name, m, mm);
}

STATIC enum op_result
output_graph(char *filename)
{
	char ffilename[strlen(filename) + 5];	// + .dot
	sprintf(ffilename, "%s.dot", filename);
	FILE *f = fopen(ffilename, "a");
	if (f == NULL) {
		fprintf(stderr, "Unable to open file %s", ffilename);
		return OP_ERROR;
	}

	char fnfilename[strlen(filename) + 9];	// + -ngt + .dot
	sprintf(fnfilename, "%s-ngt.dot", filename);
	FILE *fn = fopen(fnfilename, "a");
	if (fn == NULL) {
		fprintf(stderr, "Unable to open file %s", fnfilename);
		return OP_ERROR;
	}

	int i, j;
	int n_nodes = chosen_mb->n_nodes;
	struct dgsh_node_connections *graph_solution =
					chosen_mb->graph_solution;

	DPRINTF(4, "Output graph in file %s for %d nodes and %d edges",
			filename, n_nodes, chosen_mb->n_edges);

	fprintf(f, "digraph {\n");
	fprintf(fn, "digraph {\n");

	for (i = 0; i < n_nodes; i++) {
		struct dgsh_node *node = &chosen_mb->node_array[i];
		struct dgsh_node_connections *connections =
						&graph_solution[i];
		int n_edges_outgoing = connections->n_edges_outgoing;

		DPRINTF(4, "Output node: %s", node->name);
		// Reserve space for quotes
		int q = 0;
		char *m = strstr(node->name, "\"");
		while (m) {
			q++;
			m = strstr(++m, "\"");
		}
		char *processed_name = (char *)malloc(sizeof(char) *
						(strlen(node->name) + q + 1));
		DPRINTF(4, "Malloc %d bytes for processed_name",
					(int)strlen(node->name) + q + 1);
		memset(processed_name, 0, strlen(node->name) + q + 1);
		process_node_name(node->name, &processed_name);

#ifdef DEBUG
		fprintf(f, "	n%d [label=\"%d %s\"];\n",
				node->index, node->index,
				processed_name);
		fprintf(fn, "	n%d [label=\"%d %s\"];\n",
				node->index, node->index,
				processed_name);
#else
		fprintf(f, "	n%d [label=\"%s\"];\n",
				node->index, processed_name);
		fprintf(fn, "	n%d [label=\"%s\"];\n",
				node->index, processed_name);
#endif
		DPRINTF(4, "Node: (%d) %s", node->index, processed_name);

		free(processed_name);
		for (j = 0; j < n_edges_outgoing; j++) {
			fprintf(fn, "	n%d -> n%d;\n",
				node->index,
				chosen_mb->node_array[connections->edges_outgoing[j].to].index);
			
			if (connections->edges_outgoing[j].instances == 0)
				continue;

			fprintf(f, "	n%d -> n%d;\n",
				node->index,
				chosen_mb->node_array[connections->edges_outgoing[j].to].index);
			DPRINTF(4, "Edge: (%d) %s -> %s (%d)",
				node->index, node->name,
				chosen_mb->node_array[connections->edges_outgoing[j].to].name,
				chosen_mb->node_array[connections->edges_outgoing[j].to].index);
		}
	}

	fprintf(f, "}\n");
	fprintf(fn, "}\n");

	fclose(f);
	fclose(fn);
	return OP_SUCCESS;
}

/**
 * Allocate node indexes to store a node's (at node_index)
 * node outgoing or incoming connections (nc_edges).
 */
STATIC enum op_result
alloc_node_connections(struct dgsh_edge **nc_edges, int nc_n_edges, int type,
								int node_index)
{
	if (!nc_edges) {
		DPRINTF(4, "ERROR: Double pointer to node connection edges is NULL.\n");
		return OP_ERROR;
	}
	if (node_index < 0) {
		DPRINTF(4, "ERROR: Index of node whose connections will be allocated is negative number.\n");
		return OP_ERROR;
	}
	if (type > 1 || type < 0) {
		DPRINTF(4, "ERROR: Type of edge is neither incoming (1) nor outgoing(0).\ntyep is: %d.\n", type);
		return OP_ERROR;
	}

	*nc_edges = (struct dgsh_edge *)malloc(sizeof(struct dgsh_edge) *
								nc_n_edges);
	if (!*nc_edges) {
		DPRINTF(4, "ERROR: Memory allocation for node's index %d %s connections \
failed.\n", node_index, (type) ? "incoming" : "outgoing");

		return OP_ERROR;
	}
	return OP_SUCCESS;
}

/**
 * Copy the array of pointers to edges that go to or leave from a node
 * (i.e. its incoming or outgoing connections) to a self-contained compact
 * array of edges for easy transmission and receipt in one piece.
 */
STATIC enum op_result
make_compact_edge_array(struct dgsh_edge **nc_edges, int nc_n_edges,
			struct dgsh_edge **p_edges)
{
	int i;
	int array_size = sizeof(struct dgsh_edge) * nc_n_edges;

	if (nc_n_edges <= 0) {
		DPRINTF(4, "ERROR: Size identifier to be used in malloc() is non-positive number: %d.\n", nc_n_edges);
		return OP_ERROR;
	}
	if (nc_edges == NULL) {
		DPRINTF(4, "ERROR: Compact edge array to put edges (connections) is NULL.\n");
		return OP_ERROR;
	}
	if (p_edges == NULL) {
		DPRINTF(4, "ERROR: Pointer to edge array is NULL.\n");
		return OP_ERROR;
	}

	*nc_edges = (struct dgsh_edge *)malloc(array_size);
	if (!(*nc_edges)) {
		DPRINTF(4, "ERROR: Memory allocation of size %d for edge array failed.\n",
								array_size);
		return OP_ERROR;
	}

	/**
	 * Copy the edges of interest to the node-specific edge array
	 * that contains its connections.
	 */
	for (i = 0; i < nc_n_edges; i++) {
		if (p_edges[i] == NULL) {
			DPRINTF(4, "ERROR: Pointer to edge array contains NULL pointer.\n");
			return OP_ERROR;
		}
		/**
		 * Dereference to reach the array base, make i hops of size
		 * sizeof(struct dgsh_edge), and point to that memory block.
		 */
		memcpy(&(*nc_edges)[i], p_edges[i], sizeof(struct dgsh_edge));
		DPRINTF(4, "%s():Copied edge %d -> %d (%d) at index %d.",
				__func__, p_edges[i]->from, p_edges[i]->to,
				p_edges[i]->instances, i);
	}

	return OP_SUCCESS;
}

/* Reallocate array to edge pointers. */
STATIC enum op_result
reallocate_edge_pointer_array(struct dgsh_edge ***edge_array, int n_elements)
{
	void **p = NULL;
	if (edge_array == NULL) {
		DPRINTF(4, "ERROR: Edge array is NULL pointer.\n");
		return OP_ERROR;
	}
	if (n_elements <= 0) {
		DPRINTF(4, "ERROR: Size identifier to be used in malloc() is non-positive number: %d.\n", n_elements);
		return OP_ERROR;
	} else if (n_elements == 1)
		p = malloc(sizeof(struct dgsh_edge *) * n_elements);
	else
		p = realloc(*edge_array,
				sizeof(struct dgsh_edge *) * n_elements);
	if (!p) {
		DPRINTF(4, "ERROR: Memory reallocation for edge failed.\n");
		return OP_ERROR;
	} else
		*edge_array = (struct dgsh_edge **)p;
	return OP_SUCCESS;
}

/**
 * Gather the constraints on a node's input or output channel
 * and then try to find a solution that respects both the node's
 * channel constraint and the pair nodes' corresponding channel
 * constraints if edges on the channel exist.
 * The function is not called otherwise.

 * If a solution is found, allocate edge instances to each edge that
 * includes the node's channel (has to do with the flexible constraint).
 */
static enum op_result
satisfy_io_constraints(int *free_instances,
		       int this_channel_constraint,	/* A node's required or
							 * provided constraint
							 * on this channel
							 */
                       struct dgsh_edge **edges, /* Gathered pointers to edges
						  * of this channel
						  */
		       int n_edges,		/* Number of edges */
                       bool is_edge_incoming)	/* Incoming or outgoing */
{
	int i;
	int weight = -1, modulo = 0;

	if (this_channel_constraint > 0) {
		*free_instances = this_channel_constraint;
		weight = this_channel_constraint / n_edges;
		modulo = this_channel_constraint % n_edges;

	/* Edges that have no place in actual execution */
	} else if (this_channel_constraint == 0) {
			*free_instances = 0;
			weight = 0;
			modulo = 0;
	} else	/* Flexible constraint */
		*free_instances = -1;

	/* Aggregate the constraints for the node's channel. */
	for (i = 0; i < n_edges; i++) {
		if (this_channel_constraint > 0)
			*free_instances -= weight + (modulo > 0);
		if (is_edge_incoming) /* Outgoing for the pair node of edge */
			edges[i]->to_instances = weight + (modulo > 0);
		else
			edges[i]->from_instances = weight + (modulo > 0);
		if (modulo > 0)
			modulo--;
        	DPRINTF(4, "%s(): edge from %d to %d, is_edge_incoming: %d, free_instances: %d, weight: %d, modulo: %d, from_instances: %d, to_instances: %d.\n", __func__, edges[i]->from, edges[i]->to, is_edge_incoming, *free_instances, weight, modulo, edges[i]->from_instances, edges[i]->to_instances);
	}
	DPRINTF(4, "%s(): Number of edges: %d, this_channel_constraint: %d, free instances: %d.\n", __func__, n_edges, this_channel_constraint, *free_instances);
	return OP_SUCCESS;
}

/**
 * Lookup this tool's edges and store pointers to them in order
 * to then allow the evaluation of constraints for the current node's
 * input and output channels.
 */
static enum op_result
dry_match_io_constraints(struct dgsh_node *current_node,
			 struct dgsh_node_connections *current_connections,
			 struct dgsh_edge ***edges_incoming, /* Uninitialised*/
			 struct dgsh_edge ***edges_outgoing) /* Uninitialised*/
{
	int n_edges = chosen_mb->n_edges;
	int n_free_in_channels = current_node->requires_channels;
	int n_free_out_channels = current_node->provides_channels;
	int node_index = current_node->index;
        int *n_edges_incoming = &current_connections->n_edges_incoming;
        int *n_edges_outgoing = &current_connections->n_edges_outgoing;
	int i;

	assert(node_index < chosen_mb->n_nodes);

	/* Gather incoming/outgoing edges for node at node_index. */
	for (i = 0; i < n_edges; i++) {
		struct dgsh_edge *edge = &chosen_mb->edge_array[i];
		DPRINTF(4, "%s(): edge at index %d from %d to %d, instances %d, from_instances %d, to_instances %d.", __func__, i, edge->from, edge->to, edge->instances, edge->from_instances, edge->to_instances);
		if (edge->from == node_index) {
			(*n_edges_outgoing)++;
			if (reallocate_edge_pointer_array(edges_outgoing,
					*n_edges_outgoing) == OP_ERROR)
				return OP_ERROR;
			(*edges_outgoing)[*n_edges_outgoing - 1] = edge;
		}
		if (edge->to == node_index) {
			(*n_edges_incoming)++;
			if (reallocate_edge_pointer_array(edges_incoming,
					*n_edges_incoming) == OP_ERROR)
				return OP_ERROR;
			(*edges_incoming)[*n_edges_incoming - 1] = edge;
		}
	}
	DPRINTF(4, "%s(): Node at index %d has %d outgoing edges and %d incoming.",
				__func__, node_index, *n_edges_outgoing,
				*n_edges_incoming);

	/* Record the input/output constraints at node level. */
	if (*n_edges_outgoing > 0)
		if (satisfy_io_constraints(
		    &current_connections->n_instances_outgoing_free,
		    n_free_out_channels,
		    *edges_outgoing, *n_edges_outgoing, 0) == OP_ERROR)
			return OP_ERROR;
	if (*n_edges_incoming > 0)
		if (satisfy_io_constraints(
		    &current_connections->n_instances_incoming_free,
		    n_free_in_channels,
		    *edges_incoming, *n_edges_incoming, 1) == OP_ERROR)
			return OP_ERROR;

	return OP_SUCCESS;
}

/**
 * Free the dgsh graph's solution in face of an error.
 * node_index: the last node we setup conenctions before error.
 */
static enum op_result
free_graph_solution(int node_index)
{
	int i;
	struct dgsh_node_connections *graph_solution =
					chosen_mb->graph_solution;
	assert(node_index < chosen_mb->n_nodes);
	for (i = 0; i <= node_index; i++) {
		if (graph_solution[i].n_edges_incoming > 0)
			free(graph_solution[i].edges_incoming);
		if (graph_solution[i].n_edges_outgoing > 0)
			free(graph_solution[i].edges_outgoing);
	}
	free(graph_solution);
	chosen_mb->graph_solution = NULL;
	DPRINTF(4, "%s: freed %d nodes.", __func__, chosen_mb->n_nodes);
	return OP_SUCCESS;
}

/**
 * Add or subtract edge instances from an edge that meets a pair node's
 * flexible constraint.
 */
static enum op_result
record_move_flexible(int *diff, int *index, int to_move_index, int *instances,
		     int to_move)
{
	if (*diff > 0 || (*diff < 0 && to_move > 1)) {
		/* In subtracting at least one edge instance should remain. */
		if (*diff < 0 && *diff + (to_move - 1) <= 0)
				*instances = -(to_move - 1);
		else
				*instances = *diff;
	
		*diff -= *instances;
		*index = to_move_index;
		return OP_SUCCESS;
	}
	return OP_NOOP;
}

/**
 * Add or subtract edge instances from an edge that is unbalanced wrt
 * the pair node's constraint.
 */
static enum op_result
record_move_unbalanced(int *diff, int *index, int to_move_index,
		int *instances, int to_move, int pair)
{
	DPRINTF(4, "%s(): to_move: %d, pair: %d, diff: %d", __func__, to_move, pair, *diff);
	/* Can either to_move or pair be 0? I don't think so */
	if ((*diff > 0 && to_move < pair) ||
	    (*diff < 0 && to_move > pair)) {
		*index = to_move_index;
		if ((*diff > 0 && *diff - (pair - to_move) >= 0) ||
		    (*diff < 0 && *diff - (pair - to_move) <= 0))
			*instances = pair - to_move;
		else
			*instances = *diff;
		*diff -= *instances;
		DPRINTF(4, "%s(): move successful: to_move: %d, pair: %d, diff: %d, instances: %d, edge index: %d", __func__, to_move, pair, *diff, *instances, *index);
		return OP_SUCCESS;
	}
	return OP_NOOP;
}

/**
 * From the set of unbalanced constraints of a node wrt the pair node's
 * constraint on a specific channel, that is, input or output,
 * find instances to subtract or add to satisfy the constraint.
 * If that does not work try edges where the pair has a flexible constraint.
 */
static enum op_result
move(struct dgsh_edge** edges, int n_edges, int diff, bool is_edge_incoming)
{
	int i = 0, j = 0;
	int indexes[n_edges];
	int instances[n_edges];
	/* Try move unbalanced edges first.
	 * Avoid doing the move at the same edge.
	 */
	for (i = 0; i < n_edges; i++) {
		struct dgsh_edge *edge = edges[i];
		int *from = &edge->from_instances;
		int *to = &edge->to_instances;
		DPRINTF(4, "%s(): before move %s edge %d: from: %d, to: %d, diff %d.", __func__, is_edge_incoming ? "incoming" : "outgoing", i, *from, *to, diff);
		if (*from == -1 || *to == -1)
			continue;
		if (is_edge_incoming) {
			if (record_move_unbalanced(&diff, &indexes[j], i,
				&instances[j], *to, *from) == OP_SUCCESS)
				j++;
		} else {
			if (record_move_unbalanced(&diff, &indexes[j], i,
				&instances[j], *from, *to) == OP_SUCCESS)
				j++;
		}
		DPRINTF(4, "%s(): after move %s edge %d: from: %d, to: %d, diff %d.", __func__, is_edge_incoming ? "incoming" : "outgoing", i, *from, *to, diff);
		if (diff == 0)
			goto checkout;
	}
	/* Edges with flexible constraints are by default balanced. Try move */
	for (i = 0; i < n_edges; i++) {
		struct dgsh_edge *edge = edges[i];
		int *from = &edge->from_instances;
		int *to = &edge->to_instances;
		if (is_edge_incoming) {
			if (*from >= 0)
				continue;
			if (record_move_flexible(&diff, &indexes[j], i,
				&instances[j], *to) == OP_SUCCESS)
				j++;
		} else {
			if (*to >= 0)
				continue;
			if (record_move_flexible(&diff, &indexes[j], i,
				&instances[j], *from) == OP_SUCCESS)
				j++;
		}
		if (diff == 0)
			goto checkout;
	}

checkout:
	if (diff == 0) {
		int k = 0;
		for (k = 0; k < j; k++) {
			if (is_edge_incoming)
				edges[indexes[k]]->to_instances += instances[k];
			else
				edges[indexes[k]]->from_instances +=
								instances[k];
			DPRINTF(4, "%s(): succeeded: move %d from edge %d.", __func__, instances[k], indexes[k]);
		}
		return OP_SUCCESS;
	}
	return OP_RETRY;
}

/**
 * Try to find a solution that respects both the node's
 * channel constraint and the pair nodes' corresponding channel
 * constraints if edges on the channel exist.
 * The function is not called otherwise.

 * If a solution is found, allocate edge instances to each edge that
 * includes the node's channel (has to do with the flexible constraint).
 */
static enum op_result
cross_match_io_constraints(int *free_instances,
			int this_channel_constraint,	/* A node's required
							 * provided constraint
							 * on the channel
							 */
                       struct dgsh_edge **edges,	/* Gathered pointers
							 * to edges
							 */
		       int n_edges,		/* Number of edges */
                       bool is_edge_incoming,	/* Incoming or outgoing edges*/
		       bool *constraints_matched,
		       int *edges_matched)
{
	int i;
	int from_flex = 0;
	int to_flex = 0;

	for (i = 0; i < n_edges; i++) {
		struct dgsh_edge *e = edges[i];
		int *from = &e->from_instances;
		int *to = &e->to_instances;
		int matched = *edges_matched;
		if (*from == -1 || *to == -1) {
        		DPRINTF(4, "%s(): edge from %d to %d, this_channel_constraint: %d, is_incoming: %d, from_instances: %d, to_instances %d.\n", __func__, e->from, e->to, this_channel_constraint, is_edge_incoming, *from, *to);
			if (*from == -1 && *to == -1) {
				from_flex++;
				to_flex++;
				e->instances = 1;	// TODO
			} else if (*from == -1) {
				from_flex++;
				e->instances = *to;
			} else if (*to == -1) {
				to_flex++;
				e->instances = *from;
			}
			(*edges_matched)++;
			/* fixed to more than one flexible
			 * is not solvable in the general case
			 */
			if (this_channel_constraint > 0 &&
				((is_edge_incoming && from_flex > 1) ||
				(!is_edge_incoming && to_flex > 1))) {
				fprintf(stderr,
					"ERROR: More than one edges are flexible. Cannot compute solution. Exiting.\n");
				return OP_ERROR;
			}
		} else if (*from == *to) {
			(*edges_matched)++;
			e->instances = *from;
		} else if (*from < *to) { /* e.g. from=1, to=3; then: */
			if (is_edge_incoming) {         /* +2:  */
				if (move(edges, n_edges, (*to - *from), 1)
							== OP_SUCCESS) {
					*to -= (*to - *from); /* -2: 3 -> 1 */
					(*edges_matched)++;
				}
			} else
				if (move(edges, n_edges, -(*to - *from), 0)
							== OP_SUCCESS) {
					*from += (*to - *from);
					(*edges_matched)++;
				}
		} else {
			if (is_edge_incoming) { /* e.g. from=3, to=1 */
				if (move(edges, n_edges, -(*from - *to), 1)
							== OP_SUCCESS) {
					*to += (*from - *to);
					(*edges_matched)++;
				}
			} else
				if (move(edges, n_edges, (*from - *to), 0)
							== OP_SUCCESS) {
					*from -= (*from - *to);
					(*edges_matched)++;
				}
		}
		DPRINTF(4, "%s(): edge from %d to %d, this_channel_constraint: %d, is_incoming: %d, from_instances: %d, to_instances %d, edge instances: %d.\n", __func__, e->from, e->to, this_channel_constraint, is_edge_incoming, *from, *to, e->instances);
		if (matched == *edges_matched){
			DPRINTF(4, "%s(): WARNING: did not manage to match this edge",
					__func__);
			return OP_SUCCESS;
		}
	}

	/* Is the matching for this channel in line with the (fixed)
	 * constraint? */
	if (this_channel_constraint == -1) {
		*constraints_matched = true;
		return OP_SUCCESS;
	}

	int fds = 0;
	for (i = 0; i < n_edges; i++) {
		struct dgsh_edge *e = edges[i];
		fds += e->instances;
	}
	DPRINTF(4, "%s communication endpoints to setup: %d, constraint: %d",
			is_edge_incoming ? "Incoming" : "Outgoing",
			fds, this_channel_constraint);

	*constraints_matched = (fds == this_channel_constraint);

	return OP_SUCCESS;
}

/**
 * Search for conc with pid in message block mb
 * and return a pointer to the structure or
 * NULL if not found.
 */
struct dgsh_conc *
find_conc(struct dgsh_negotiation *mb, pid_t pid)
{
	int i;
	struct dgsh_conc *ca = mb->conc_array;
	for (i = 0; i < mb->n_concs; i++) {
		if (ca[i].pid == pid)
			return &ca[i];
	}
	return NULL;
}

/**
 * Calculate fds for concs at the multi-pipe
 * endpoint.
 */
static enum op_result
calculate_conc_fds(void)
{
	int i, calculated = 0, retries = 0;
	int n_concs = chosen_mb->n_concs;

	DPRINTF(4, "%s for %d n_concs", __func__, n_concs);
	if (n_concs == 0)
		return OP_SUCCESS;

repeat:
	for (i = 0; i < n_concs; i++) {
		struct dgsh_conc *c = &chosen_mb->conc_array[i];
		DPRINTF(4, "%s() for conc %d at index %d with %d n_proc_pids",
				__func__, c->pid, i, c->n_proc_pids);

		if (c->input_fds >= 0 && c->output_fds >= 0)
			continue;

		c->input_fds = 0;
		c->output_fds = 0;

		if (c->multiple_inputs)
			c->output_fds = get_expected_fds_n(chosen_mb,
					c->endpoint_pid);
		else
			c->input_fds = get_provided_fds_n(chosen_mb,
					c->endpoint_pid);

		DPRINTF(4, "%s(): conc pid %d at index %d: %d %s fds for endpoint pid %d recovered",
				__func__, c->pid, i,
				c->multiple_inputs ? c->output_fds : c->input_fds,
				c->multiple_inputs ? "outgoing" : "incoming",
				c->endpoint_pid);

		int j, fds;
		for (j = 0; j < c->n_proc_pids; j++) {
			if (c->multiple_inputs)
				fds = get_provided_fds_n(chosen_mb,
						c->proc_pids[j]);
			else
				fds = get_expected_fds_n(chosen_mb,
						c->proc_pids[j]);

			if (find_conc(chosen_mb, c->proc_pids[j]) && fds == -1) {
				c->input_fds = c->output_fds = -1;
				DPRINTF(4, "%s(): conc pid %d at index %d: fds for conc with pid %d not yet available",
					__func__, c->pid, i, c->proc_pids[j]);
				break;
			} else
				if (c->multiple_inputs)
					c->input_fds += fds;
				else
					c->output_fds += fds;
			DPRINTF(4, "%s(): conc pid %d at index %d: %d %s fds for pid %d recovered",
				__func__, c->pid, i, fds,
				c->multiple_inputs ? "incoming" : "outgoing",
				c->proc_pids[j]);
		}
		// Use what we know for the multi-pipe end to compute the endpoint
		if (c->multiple_inputs && c->input_fds >= 0 && c->output_fds == -1)
			c->output_fds = c->input_fds;
		else if (!c->multiple_inputs && c->output_fds >= 0
				&& c->input_fds == -1)
			c->input_fds = c->output_fds;

		if (c->input_fds >= 0 && c->output_fds >= 0) {
			assert(c->input_fds == c->output_fds);
			calculated++;
		}
		DPRINTF(4, "%s(): Conc pid %d at index %d has %d %s fds and %d %s fds",
				__func__, c->pid, i,
				c->multiple_inputs ? c->input_fds : c->output_fds,
				c->multiple_inputs ? "incoming" : "outgoing",
				c->multiple_inputs ? c->output_fds : c->input_fds,
				c->multiple_inputs ? "outgoing" : "incoming");
		DPRINTF(4, "%s(): Calculated fds for %d concs so far", __func__,
				calculated);

	}
	if (calculated != n_concs && retries < n_concs) {
		retries++;
		goto repeat;
	}

	if (retries == n_concs)
		return OP_ERROR;

	return OP_SUCCESS;
}

/**
 * For each node substitute pointers to edges with proper edge structures
 * (copies) to facilitate transmission and receipt in one piece.
 */
static enum op_result
prepare_solution(void)
{
	int i;
	int n_nodes = chosen_mb->n_nodes;
	struct dgsh_node_connections *graph_solution =
					chosen_mb->graph_solution;
	enum op_result exit_state = OP_SUCCESS;

	for (i = 0; i < n_nodes; i++) {
		struct dgsh_node_connections *current_connections =
							&graph_solution[i];
		/* Hack: struct dgsh_edge* -> struct dgsh_edge** */
		struct dgsh_edge **edges_incoming =
		       (struct dgsh_edge **)current_connections->edges_incoming;
		current_connections->edges_incoming = NULL;
		/* Hack: struct dgsh_edge* -> struct dgsh_edge** */
		struct dgsh_edge **edges_outgoing =
		       (struct dgsh_edge **)current_connections->edges_outgoing;
		current_connections->edges_outgoing = NULL;
        	int *n_edges_incoming = &current_connections->n_edges_incoming;
        	int *n_edges_outgoing = &current_connections->n_edges_outgoing;
		DPRINTF(3, "%s(): Node %s, pid: %d, connections in: %d, connections out: %d.",
				__func__, chosen_mb->node_array[i].name,
				chosen_mb->node_array[i].pid,
				*n_edges_incoming, *n_edges_outgoing);

		if (*n_edges_incoming > 0) {
			if (exit_state == OP_SUCCESS)
				if (make_compact_edge_array(
					&current_connections->edges_incoming,
			       		*n_edges_incoming, edges_incoming)
								== OP_ERROR)
					exit_state = OP_ERROR;
			free(edges_incoming);
		}
		if (*n_edges_outgoing > 0) {
			if (exit_state == OP_SUCCESS)
				if (make_compact_edge_array(
					&current_connections->edges_outgoing,
					*n_edges_outgoing, edges_outgoing)
								== OP_ERROR)
					exit_state = OP_ERROR;
			free(edges_outgoing);
		}
	}
	return exit_state;
}

static void
print_solution_error(int index_argc, int *index_commands_notmatched,
		int *side_commands_notmatched)
{
	int i = 0, index = 0, side = 0, reqs = 0;
	fprintf(stderr, "dgsh: No solution was found to satisfy the I/O requirements of the following %d participating processes:\n",
			index_argc);
	for (i = 0; i < index_argc; i++) {
		index = index_commands_notmatched[i];
		side = side_commands_notmatched[i];
		if (side == STDIN_FILENO)
			reqs = chosen_mb->node_array[index].requires_channels;
		else
			reqs = chosen_mb->node_array[index].provides_channels;
		fprintf(stderr, "%s (n%s=%d)\n",
				chosen_mb->node_array[index].name,
				side == STDIN_FILENO ? "in" : "out",
				reqs);
	}
	free(index_commands_notmatched);
	free(side_commands_notmatched);
}

/**
 * Check that a node's input/output channel matched its requirements
 */
static void
check_constraints_matched(int node_index, bool *constraints_matched,
				int **index_commands_notmatched,
				int **side_commands_notmatched,
				int *index_argc, int side)
{
	if (!*constraints_matched) {
		(*index_argc)++;
		DPRINTF(4, "Constraint not matched for node at index %d. So far %d nodes not matched",
				node_index, *index_argc);
		if (*index_argc == 1) {
			*index_commands_notmatched = (int *)malloc(
					sizeof(int) * *index_argc);
			*side_commands_notmatched = (int *)malloc(
					sizeof(int) * *index_argc);
		} else {
			*index_commands_notmatched = (int *)realloc(
				*index_commands_notmatched,
				sizeof(int) * *index_argc);
			*side_commands_notmatched = (int *)realloc(
				*side_commands_notmatched,
				sizeof(int) * *index_argc);
		}
		(*index_commands_notmatched)[*index_argc - 1] = node_index;
		(*side_commands_notmatched)[*index_argc - 1] = side;
	}
	*constraints_matched = false;
}

/**
 * This function implements the algorithm that tries to satisfy reported
 * I/O constraints of tools on an dgsh graph.
 */
static enum op_result
cross_match_constraints(int **index_commands_notmatched,
		int **side_commands_notmatched, int *index_argc)
{
	int i;
	int n_nodes = chosen_mb->n_nodes;
	int n_edges = chosen_mb->n_edges;
	int edges_matched = 0;
	bool constraints_matched = false;
	struct dgsh_node_connections *graph_solution =
					chosen_mb->graph_solution;

	/* Check constraints for each node on the dgsh graph. */
	for (i = 0; i < n_nodes; i++) {
		struct dgsh_node_connections *current_connections =
							&graph_solution[i];
		/* Hack: struct dgsh_edge* -> struct dgsh_edge** */
		struct dgsh_edge **edges_incoming =
		       (struct dgsh_edge **)current_connections->edges_incoming;
		/* Hack: struct dgsh_edge* -> struct dgsh_edge** */
		struct dgsh_edge **edges_outgoing =
		       (struct dgsh_edge **)current_connections->edges_outgoing;
		struct dgsh_node *current_node = &chosen_mb->node_array[i];
		int out_constraint = current_node->provides_channels;
		int in_constraint = current_node->requires_channels;
        	int *n_edges_incoming = &current_connections->n_edges_incoming;
        	int *n_edges_outgoing = &current_connections->n_edges_outgoing;
		DPRINTF(4, "%s(): node %s, index %d, channels required %d, channels_provided %d, dgsh_in %d, dgsh_out %d.", __func__, current_node->name, current_node->index, in_constraint, out_constraint, current_node->dgsh_in, current_node->dgsh_out);

		/* Try to satisfy the I/O channel constraints at graph level.
		 * Assign instances to each edge.
		 */
		if (*n_edges_outgoing > 0){
			if (cross_match_io_constraints(
		    	    &current_connections->n_instances_outgoing_free,
			    out_constraint,
		    	    edges_outgoing, *n_edges_outgoing, 0,
			    &constraints_matched, &edges_matched) == OP_ERROR) {
				DPRINTF(4, "ERROR: Failed to satisfy requirements for tool %s, pid %d: requires %d and gets %d, provides %d and is offered %d.\n",
				current_node->name,
				current_node->pid,
				current_node->requires_channels,
				*n_edges_incoming,
				current_node->provides_channels,
				*n_edges_outgoing);
				return OP_ERROR;
			}
			check_constraints_matched(i, &constraints_matched,
					index_commands_notmatched,
					side_commands_notmatched, index_argc,
					STDOUT_FILENO);
		}
		if (*n_edges_incoming > 0){
			if (cross_match_io_constraints(
		    	    &current_connections->n_instances_incoming_free,
			    in_constraint,
		    	    edges_incoming, *n_edges_incoming, 1,
			    &constraints_matched, &edges_matched) == OP_ERROR) {
				DPRINTF(4, "ERROR: Failed to satisfy requirements for tool %s, pid %d: requires %d and gets %d, provides %d and is offered %d.\n",
				current_node->name,
				current_node->pid,
				current_node->requires_channels,
				*n_edges_incoming,
				current_node->provides_channels,
				*n_edges_outgoing);
				return OP_ERROR;
			}
			check_constraints_matched(i, &constraints_matched,
					index_commands_notmatched,
					side_commands_notmatched, index_argc,
					STDIN_FILENO);
		}
	}
	DPRINTF(4, "%s(): Cross matched constraints of %d out of %d nodes for %d edges out of %d edges.", __func__, n_nodes - *index_argc, n_nodes, edges_matched / 2, n_edges);
	if (edges_matched / 2 == n_edges && *index_argc == 0)
		return OP_SUCCESS;
	else
		return OP_RETRY;
}


/**
 * This function implements the algorithm that tries to satisfy reported
 * I/O constraints of tools on an dgsh graph.
 */
static enum op_result
node_match_constraints(void)
{
	int i;
	int n_nodes = chosen_mb->n_nodes;
	enum op_result exit_state = OP_SUCCESS;
	int graph_solution_size = sizeof(struct dgsh_node_connections) *
								n_nodes;
	/* Prealloc */
	chosen_mb->graph_solution = (struct dgsh_node_connections *)malloc(
							graph_solution_size);
	struct dgsh_node_connections *graph_solution =
					chosen_mb->graph_solution;
	if (!graph_solution) {
		DPRINTF(4, "ERROR: Failed to allocate memory of size %d for dgsh negotiation graph solution structure.\n", graph_solution_size);
		return OP_ERROR;
	}

	/* Check constraints for each node on the dgsh graph. */
	for (i = 0; i < n_nodes; i++) {
		DPRINTF(4, "%s(): node at index %d.", __func__, i);
		struct dgsh_node_connections *current_connections =
							&graph_solution[i];
		memset(current_connections, 0,
					sizeof(struct dgsh_node_connections));
		struct dgsh_edge **edges_incoming;
		struct dgsh_edge **edges_outgoing;
		struct dgsh_node *current_node = &chosen_mb->node_array[i];
		current_connections->node_index = current_node->index;
		DPRINTF(4, "Node %s, index %d, channels required %d, channels_provided %d, dgsh_in %d, dgsh_out %d.", current_node->name, current_node->index, current_node->requires_channels, current_node->provides_channels, current_node->dgsh_in, current_node->dgsh_out);

		/* Find and store pointers to node's at node_index edges.
		 * Try to satisfy the I/O channel constraints at node level.
		 */
		if (dry_match_io_constraints(current_node, current_connections,
			&edges_incoming, &edges_outgoing) == OP_ERROR) {
			DPRINTF(4, "ERROR: Failed to satisfy requirements for tool %s, pid %d: requires %d and gets %d, provides %d and is offered %d.\n",
				current_node->name,
				current_node->pid,
				current_node->requires_channels,
				current_connections->n_edges_incoming,
				current_node->provides_channels,
				current_connections->n_edges_outgoing);
			exit_state = OP_ERROR;
		}
		if (exit_state == OP_ERROR) {
			free_graph_solution(current_node->index);
			break;
		}
		/* Hack to retain references to edge pointer arrays. */
		current_connections->edges_incoming =
			(struct dgsh_edge *)edges_incoming;
		current_connections->edges_outgoing =
			(struct dgsh_edge *)edges_outgoing;
	}
	return exit_state;
}


/**
 * This function implements the algorithm that tries to satisfy reported
 * I/O constraints of tools on an dgsh graph.
 */
enum op_result
solve_graph(void)
{
	char *filename;
	enum op_result exit_state = OP_SUCCESS;
	int retries = 0;
	int index_argc = 0;
	int *index_commands_notmatched;
	int *side_commands_notmatched;

	/**
	 * The initial layout of the solution plays an important
	 * role. We could add a scheme that allows restarting
	 * the solution process with a different initial layout
	 * after a failure. The key points of the scheme would be:
	 * while
	 * int retries = 0;
	 * int max_retries = 10;
	 * assign a node's available edges to adjacent nodes differently
	 */

	/**
	 * Try to match each node's I/O resources with constraints
	 * expressed by incoming and outgoing edges.
	 */
	if ((exit_state = node_match_constraints()) == OP_ERROR)
		return exit_state;

	/* Optimise solution using flexible constraints */
	exit_state = OP_RETRY;

	while (exit_state == OP_RETRY) {
		if ((exit_state = cross_match_constraints(
				&index_commands_notmatched,
				&side_commands_notmatched,
				&index_argc)) ==
				OP_ERROR ||
				(exit_state == OP_RETRY && retries > 10)) {
			print_solution_error(index_argc,
					index_commands_notmatched,
					side_commands_notmatched);
			exit_state = OP_ERROR;
			goto exit;
		}
		DPRINTF(4, "%s(): exit_state: %d, retries: %d",
				__func__, exit_state, retries);
		retries++;
		if (index_argc > 0) {
			free(index_commands_notmatched);
			free(side_commands_notmatched);
			index_argc = 0;
		}
	}

	/**
	 * Substitute pointers to edges with proper edge structures
	 * (copies) to facilitate transmission and receipt in one piece.
	 */
	if ((exit_state = prepare_solution()) == OP_ERROR)
		goto exit;

	if ((exit_state = calculate_conc_fds()) == OP_ERROR)
		goto exit;

	if ((filename = getenv("DGSH_DOT_DRAW")))
		if ((exit_state = output_graph(filename)) == OP_ERROR)
			goto exit;

	if (getenv("DGSH_DRAW_EXIT")) {
		DPRINTF(1, "Document the solution and exit\n");
		exit_state = OP_DRAW_EXIT;
	}

	DPRINTF(4, "%s: exit_state: %d", __func__, exit_state);

exit:
	if (exit_state == OP_ERROR || exit_state == OP_DRAW_EXIT)
		free_graph_solution(chosen_mb->n_nodes - 1);
	return exit_state;
} /* memory deallocation when in error state? */

/**
 * Assign the pipes to the data structs that will carry them back to the tool.
 * The tool is responsible for freeing the memory allocated to these data
 * structures.
 */
static enum op_result
establish_io_connections(int **input_fds, int *n_input_fds, int **output_fds,
							int *n_output_fds)
{
	enum op_result re = OP_SUCCESS;
	DPRINTF(4, "%s(): input fds: %d, output fds: %d", __func__,
			self_pipe_fds.n_input_fds, self_pipe_fds.n_output_fds);

	if (self_pipe_fds.n_input_fds > 0) {
		/* Have the first returned file descriptor
		 * take the place of stdin.
		 */
		int fd_to_dup = self_pipe_fds.input_fds[0];
		if (close(STDIN_FILENO) == -1)
			err(1, "Close stdin failed");
		if ((self_pipe_fds.input_fds[0] = dup(fd_to_dup)) == -1)
			err(1, "dup failed with errno %d", errno);
		DPRINTF(4, "%s(): closed STDIN, dup %d returned %d",
				__func__,fd_to_dup, self_pipe_fds.input_fds[0]);
		assert(self_pipe_fds.input_fds[0] == STDIN_FILENO);
		close(fd_to_dup);

		if (n_input_fds) {
			*n_input_fds = self_pipe_fds.n_input_fds;
			assert(*n_input_fds >= 0);
			if (input_fds)
				*input_fds = self_pipe_fds.input_fds;
		} else {
			self_pipe_fds.n_input_fds = 0;
			free(self_pipe_fds.input_fds);
		}
	} else
		if (n_input_fds)
			*n_input_fds = 0;


	if (self_pipe_fds.n_output_fds > 0) {
		/* Have the first returned file descriptor
		 * take the place of stdin.
		 */
		int fd_to_dup = self_pipe_fds.output_fds[0];
		if (close(STDOUT_FILENO) == -1)
			err(1, "Close stdout failed");
		if ((self_pipe_fds.output_fds[0] = dup(fd_to_dup)) == -1)
			err(1, "dup failed with errno %d", errno);
		DPRINTF(4, "%s(): closed STDOUT, dup %d returned %d",
				__func__,fd_to_dup,self_pipe_fds.output_fds[0]);
		assert(self_pipe_fds.output_fds[0] == STDOUT_FILENO);
		close(fd_to_dup);

		if (n_output_fds) {
			*n_output_fds = self_pipe_fds.n_output_fds;
			assert(*n_output_fds >= 0);
			if (output_fds)
				*output_fds = self_pipe_fds.output_fds;
		} else {
			self_pipe_fds.n_output_fds = 0;
			free(self_pipe_fds.output_fds);
		}
	} else
		if (n_output_fds)
			*n_output_fds = 0;

	DPRINTF(2, "%s(): %s for node %s at index %d", __func__,
			(re == OP_SUCCESS ? "successful" : "failed"),
			self_node.name, self_node.index);

	return re;
}

/* Transmit file descriptors that will pipe this
 * tool's output to another tool.
 */
static enum op_result
write_output_fds(int output_socket, int *output_fds, int flags)
{
	/**
	 * A node's connections are located at the same position
         * as the node in the node array.
	 */
	struct dgsh_node_connections *graph_solution =
					chosen_mb->graph_solution;
	struct dgsh_node_connections *this_nc =
					&graph_solution[self_node.index];
	DPRINTF(4, "%s(): for node at index %d with %d outgoing edges.", __func__,
				self_node.index, this_nc->n_edges_outgoing);
	assert(this_nc->node_index == self_node.index);
	int i;
	int total_edge_instances = 0;
	enum op_result re = OP_SUCCESS;

	/**
	 * Create a pipe for each instance of each outgoing edge connection.
	 * Inject the pipe read side in the cont.
	 * Send each pipe fd as a message to a socket descriptor,
	 * that is write_fd, that has been
	 * set up by the shell to support the dgsh negotiation phase.
	 */
	for (i = 0; i < this_nc->n_edges_outgoing; i++) {
		int k;
		/**
		 * Due to channel constraint flexibility,
		 * each edge can have more than one instances.
		 */
		for (k = 0; k < this_nc->edges_outgoing[i].instances; k++) {
			int fd[2];
			/* Create pipe, inject the read side to the msg control
			 * data and close the read side to let the recipient
			 * process handle it.
			 */
			if (pipe(fd) == -1) {
				perror("pipe open failed");
				dgsh_exit(-1, flags);
			}
			DPRINTF(4, "%s(): created pipe pair %d - %d. Transmitting fd %d through sendmsg().", __func__, fd[0], fd[1], fd[0]);

			write_fd(output_socket, fd[0]);
			close(fd[0]);

			output_fds[total_edge_instances] = fd[1];
			total_edge_instances++;
		}
		/* XXX */
		if (re == OP_ERROR)
			break;
	}
	if (re == OP_ERROR) {
		DPRINTF(4, "%s(): ERROR. Aborting.", __func__);
		free_graph_solution(chosen_mb->n_nodes - 1);
		free(self_pipe_fds.output_fds);
	}
	return re;
}

static int
write_piece (int write_fd, void *datastruct, int struct_size)
{
	int retries = 0, wsize;
retry:
	DPRINTF(4, "Try write struct of size: %d", struct_size);
	wsize = write(write_fd, datastruct, struct_size);
	if (wsize == -1 && errno == ENOBUFS && retries < 3) {	// sleep for 10ms
		nanosleep((const struct timespec[]){{0, 10000000L}}, NULL);
		retries++;
		goto retry;
	}
	return wsize;
}

static int
get_struct_size(int struct_type)
{
		switch (struct_type) {
		case 1:
			return sizeof(struct dgsh_node);
		case 2:
			return sizeof(struct dgsh_edge);
		case 3:
			return sizeof(struct dgsh_conc);
		case 4:
			return sizeof(struct dgsh_node_connections);
		}
		return 0;
}

static int
do_write (int write_fd, void *datastruct, int datastruct_size, int struct_type)
{
	int wsize = 0;
	if (datastruct_size > iov_max) {
		int all_elements, max_elements, elements = 0, pieces, 
			prev_elements = 0, size, struct_size, i;

		struct_size = get_struct_size(struct_type);
		all_elements = datastruct_size / struct_size;
		max_elements = iov_max / struct_size;
		pieces = all_elements / max_elements;
		pieces += (all_elements % max_elements > 0);
		DPRINTF(4, "struct_type: %d, pieces: %d, all_elements: %d, max_elements: %d",
			struct_type, pieces, all_elements, max_elements);

		for (i = 0; i < pieces; i++) {
			if (all_elements - max_elements > 0)
				elements = max_elements;
			else if (all_elements > 0)
				elements = all_elements;
			all_elements -= elements;
			size = struct_size * elements;
			DPRINTF(4, "Round %d: elements: %d, size: %d, prev_elements: %d",
				i, elements, size, prev_elements);

			if (size > 0) {
				void *struct_piece = malloc(size);
				switch (struct_type) {
				case 1:
					memcpy(struct_piece,
						&((struct dgsh_node *)
						datastruct)[i * prev_elements],
						size);
					break;
				case 2:
					memcpy(struct_piece,
						&((struct dgsh_edge *)
						datastruct)[i * prev_elements],
						size);
					break;
				case 3:
					memcpy(struct_piece,
						&((struct dgsh_conc *)
						datastruct)[i * prev_elements],
						size);
					break;
				case 4:
					memcpy(struct_piece,
						&((struct dgsh_node_connections *)
						datastruct)[i * prev_elements],
						size);
				}
				wsize = write_piece(write_fd, struct_piece,
							size);
				free(struct_piece);
				prev_elements = elements;
			}
		}
	} else
		wsize = write_piece(write_fd, datastruct, datastruct_size);

	return wsize;
}

static enum op_result
write_concs(int write_fd)
{
	int wsize, i;
	int n_concs = chosen_mb->n_concs;
	int conc_size = sizeof(struct dgsh_conc) * n_concs;

	if (!chosen_mb->conc_array)
		return OP_SUCCESS;

	wsize = do_write(write_fd, chosen_mb->conc_array, conc_size, 3);
	if (wsize == -1) {
		DPRINTF(4, "ERROR: write failed: errno: %d", errno);
		return OP_ERROR;
	}
	DPRINTF(4, "%s(): Wrote conc structures of size %d bytes ", __func__, wsize);

	for (i = 0; i < n_concs; i++) {
		struct dgsh_conc *c = &chosen_mb->conc_array[i];
		int proc_pids_size = sizeof(int) * c->n_proc_pids;
		wsize = do_write(write_fd, c->proc_pids, proc_pids_size, 5);
		if (wsize == -1) {
			DPRINTF(4, "ERROR: write failed: errno: %d", errno);
			return OP_ERROR;
		}
		DPRINTF(4, "%s(): Wrote %d proc_pids for conc %d at index %d of size %d bytes ",
				__func__, c->n_proc_pids, c->pid, i, wsize);
	}

	return OP_SUCCESS;
}

/* Transmit dgsh negotiation graph solution to the next tool on the graph. */
static enum op_result
write_graph_solution(int write_fd)
{
	int i;
	int n_nodes = chosen_mb->n_nodes;
	int graph_solution_size = sizeof(struct dgsh_node_connections) *
								n_nodes;
	struct dgsh_node_connections *graph_solution =
					chosen_mb->graph_solution;
	int wsize = -1;

	/* Transmit node connection structures. */
	wsize = do_write(write_fd, graph_solution, graph_solution_size, 4);
	if (wsize == -1) {
		DPRINTF(4, "ERROR: write failed: errno: %d", errno);
		return OP_ERROR;
	}
	DPRINTF(4, "%s(): Wrote graph solution of size %d bytes ", __func__, wsize);

	/* We haven't invalidated pointers to arrays of node indices. */

	for (i = 0; i < n_nodes; i++) {
		struct dgsh_node_connections *nc = &graph_solution[i];
		int in_edges_size = sizeof(struct dgsh_edge) * nc->n_edges_incoming;
		int out_edges_size = sizeof(struct dgsh_edge) * nc->n_edges_outgoing;
		if (nc->n_edges_incoming) {
			/* Transmit a node's incoming connections. */
			wsize = do_write(write_fd, nc->edges_incoming,
							in_edges_size, 2);
			if (wsize == -1) {
				DPRINTF(4, "ERROR: write failed: errno: %d", errno);
				return OP_ERROR;
			}
			DPRINTF(4, "%s(): Wrote node's %d %d incoming edges of size %d bytes ", __func__, nc->node_index, nc->n_edges_incoming, wsize);
		}

		if (nc->n_edges_outgoing) {
			/* Transmit a node's outgoing connections. */
			wsize = do_write(write_fd, nc->edges_outgoing,
							out_edges_size, 2);
			if (wsize == -1) {
				DPRINTF(4, "ERROR: write failed: errno: %d", errno);
				return OP_ERROR;
			}
			DPRINTF(4, "%s(): Wrote node's %d %d outgoing edges of size %d bytes ", __func__, nc->node_index, nc->n_edges_outgoing, wsize);
		}
	}
	return OP_SUCCESS;
}

/**
 * Copy the dispatcher static object that identifies the node
 * in the message block node array and shows the write point of
 * the send operation. This is a deep copy for simplicity.
 */
static void
set_dispatcher(void)
{
	chosen_mb->origin_index = self_node_io_side.index;
	/* The process preceding the one to find the solution
	 * will eventually set its PID.
	 */
	assert(self_node_io_side.index >= 0); /* Node is added to the graph. */
	chosen_mb->origin_fd_direction = self_node_io_side.fd_direction;
	chosen_mb->is_origin_conc = false;
	chosen_mb->conc_pid = -1;
	DPRINTF(4, "%s(): message block origin set to %d and writing on the %s side", __func__, chosen_mb->origin_index,
	(chosen_mb->origin_fd_direction == 0) ? "input" : "output");
}

/*
 * Write the chosen_mb message block to the specified file descriptor.
 */
enum op_result
write_message_block(int write_fd)
{
	int wsize = -1;
	int mb_size = sizeof(struct dgsh_negotiation);
	int nodes_size = chosen_mb->n_nodes * sizeof(struct dgsh_node);
	int edges_size = chosen_mb->n_edges * sizeof(struct dgsh_edge);
	struct dgsh_node *p_nodes = chosen_mb->node_array;

	DPRINTF(3, "%s(): %s (%d)", __func__, programname, self_node.index);

	if (chosen_mb->state == PS_ERROR && errno == 0)
		errno = EPROTO;

	/**
	 * Prepare and perform message block transmission.
	 * Formally invalidate pointers to nodes and edges
	 * to avoid accidents on the receiver's side.
	 */
	chosen_mb->node_array = NULL;
	DPRINTF(4, "%s(): Write message block.", __func__);
	wsize = do_write(write_fd, chosen_mb, mb_size, 0);
	if (wsize == -1) {
		DPRINTF(4, "ERROR: write failed: errno: %d", errno);
		return OP_ERROR;
	}
	DPRINTF(4, "%s(): Wrote message block of size %d bytes ", __func__, wsize);

	/* Transmit nodes. */
	if (chosen_mb->n_nodes > 0) {
		wsize = do_write(write_fd, p_nodes, nodes_size, 1);
		if (wsize == -1) {
			DPRINTF(4, "ERROR: write failed: errno: %d", errno);
			return OP_ERROR;
		}
		DPRINTF(4, "%s(): Wrote nodes of size %d bytes ",
				__func__, wsize);
	}

	chosen_mb->node_array = p_nodes; // Reinstate pointers to nodes.

	if (write_concs(write_fd) == OP_ERROR)
		return OP_ERROR;

	if (chosen_mb->state == PS_NEGOTIATION) {
		if (chosen_mb->n_edges > 0) {
			/* Transmit edges. */
			struct dgsh_edge *p_edges = chosen_mb->edge_array;
			chosen_mb->edge_array = NULL;
			wsize = do_write(write_fd, p_edges, edges_size, 2);
			if (wsize == -1) {
				DPRINTF(4, "ERROR: write failed: errno: %d", errno);
				return OP_ERROR;
			}
			DPRINTF(4, "%s(): Wrote edges of size %d bytes ", __func__, wsize);

			chosen_mb->edge_array = p_edges; /* Reinstate edges. */
		}
	} else if (chosen_mb->state == PS_RUN) {
		/* Transmit solution. */
		if (write_graph_solution(write_fd) == OP_ERROR)
			return OP_ERROR;
	}

	DPRINTF(4, "%s(): Shipped message block or solution to next node in graph from file descriptor: %d.\n", __func__, write_fd);
	return OP_SUCCESS;
}


/* Reallocate message block to fit new node coming in. */
static enum op_result
add_node(void)
{
	int n_nodes = chosen_mb->n_nodes;
	void *p = realloc(chosen_mb->node_array,
		sizeof(struct dgsh_node) * (n_nodes + 1));
	if (!p) {
		DPRINTF(4, "ERROR: Node array expansion for adding a new node failed.\n");
		return OP_ERROR;
	} else {
		chosen_mb->node_array = (struct dgsh_node *)p;
		self_node.index = n_nodes;
		memcpy(&chosen_mb->node_array[n_nodes], &self_node,
					sizeof(struct dgsh_node));
		self_node_io_side.index = n_nodes;
		DPRINTF(2, "%s(): Added node %s in position %d on dgsh graph, initiator: %d",
				__func__, self_node.name, self_node_io_side.index,
				chosen_mb->initiator_pid);
		chosen_mb->n_nodes++;
	}
	return OP_SUCCESS;
}

/* Lookup an edge in the dgsh graph. */
static enum op_result
lookup_dgsh_edge(struct dgsh_edge *e)
{
	int i;
	for (i = 0; i < chosen_mb->n_edges; i++) {
		if ((chosen_mb->edge_array[i].from == e->from &&
			chosen_mb->edge_array[i].to == e->to) ||
		    (chosen_mb->edge_array[i].from == e->to &&
		     chosen_mb->edge_array[i].to == e->from)) {
			DPRINTF(4, "%s(): Edge %d to %d exists.", __func__,
								e->from, e->to);
			return OP_EXISTS;
		}
	}
	return OP_CREATE;
}

/**
 * Fill edge depending on input/output fd information
 * passed by sender and found in receiver (this tool or self).
 */
static enum op_result
fill_dgsh_edge(struct dgsh_edge *e)
{
	int i;
	int n_nodes = chosen_mb->n_nodes;
	for (i = 0; i < n_nodes; i++) /* Check dispatcher node exists. */
		if (i == chosen_mb->origin_index)
			break;
	if (i == n_nodes) {
		DPRINTF(4, "ERROR: Dispatcher node with index position %d not present in graph.\n", chosen_mb->origin_index);
		return OP_ERROR;
	}
	if (chosen_mb->origin_fd_direction == STDIN_FILENO) {
	/**
         * MB sent from stdin, so dispatcher is the destination of the edge.
	 * Self should be dgsh-active on output side. Self's current fd is stdin
	 * if self is dgsh-active on input side or output side otherwise.
	 * Self (the recipient) is the source of the edge.
         */
		e->to = chosen_mb->origin_index;
		if (self_node.dgsh_in == 1)
			self_node_io_side.fd_direction = STDIN_FILENO;
		else
			self_node_io_side.fd_direction = STDOUT_FILENO;
		assert((self_node.dgsh_in &&
			self_node_io_side.fd_direction == STDIN_FILENO) ||
			self_node_io_side.fd_direction == STDOUT_FILENO);
		e->from = self_node_io_side.index;
	} else if (chosen_mb->origin_fd_direction == STDOUT_FILENO) {
		/* Similarly. */
		e->from = chosen_mb->origin_index;
		if (self_node.dgsh_out == 1)
			self_node_io_side.fd_direction = STDOUT_FILENO;
		else
			self_node_io_side.fd_direction = STDIN_FILENO;
		assert((self_node.dgsh_out &&
			self_node_io_side.fd_direction == STDOUT_FILENO) ||
			self_node_io_side.fd_direction == STDIN_FILENO);
		e->to = self_node_io_side.index;
	}
	assert(e->from != e->to);
	e->instances = 0;
	e->from_instances = 0;
	e->to_instances = 0;
        DPRINTF(4, "New dgsh edge from %d to %d with %d instances.", e->from, e->to, e->instances);
	return OP_SUCCESS;
}

/* Add new edge coming in. */
static enum op_result
add_edge(struct dgsh_edge *edge)
{
	int n_edges = chosen_mb->n_edges;
	void *p = realloc(chosen_mb->edge_array,
			sizeof(struct dgsh_edge) * (n_edges + 1));
	if (!p) {
		DPRINTF(4, "ERROR: Edge array expansion for adding a new edge failed.\n");
		return OP_ERROR;
	} else {
		chosen_mb->edge_array = (struct dgsh_edge *)p;
		memcpy(&chosen_mb->edge_array[n_edges], edge,
						sizeof(struct dgsh_edge));
		DPRINTF(4, "Added edge (%d -> %d) in dgsh graph.\n",
					edge->from, edge->to);
		chosen_mb->n_edges++;
	}
	return OP_SUCCESS;
}

/* Try to add a newly occured edge in the dgsh graph. */
static enum op_result
try_add_dgsh_edge(void)
{
	if (chosen_mb->origin_index >= 0) { /* If MB not created just now: */
		struct dgsh_edge new_edge;
		fill_dgsh_edge(&new_edge);
		if (lookup_dgsh_edge(&new_edge) == OP_CREATE) {
			if (add_edge(&new_edge) == OP_ERROR)
				return OP_ERROR;
			DPRINTF(4, "Dgsh graph now has %d edges.\n",
							chosen_mb->n_edges);
			return OP_SUCCESS;
		}
		return OP_EXISTS;
	}
	return OP_NOOP;
}

/* A constructor-like function for struct dgsh_node. */
static void
fill_node(const char *tool_name, pid_t self_pid, int *n_input_fds,
						int *n_output_fds)
{
	self_node.pid = self_pid;
	memcpy(self_node.name, tool_name, strlen(tool_name) + 1);

	if (n_input_fds == NULL)
		if (self_node.dgsh_in)
			self_node.requires_channels = 1;
		else
			self_node.requires_channels = 0;
	else
		self_node.requires_channels = *n_input_fds;
	DPRINTF(4, "%s(): dgsh_in: %d, self_node.requires_channels: %d", __func__,
			self_node.dgsh_in, self_node.requires_channels);

	if (n_output_fds == NULL) {
		if (self_node.dgsh_out)
			self_node.provides_channels = 1;
		else
			self_node.provides_channels = 0;
	} else
		self_node.provides_channels = *n_output_fds;
	DPRINTF(4, "%s(): dgsh_out: %d, self_node.provides_channels: %d", __func__,
			self_node.dgsh_out, self_node.provides_channels);

	DPRINTF(4, "Dgsh node for tool %s with pid %d created.\n", tool_name,
			self_pid);
}

/**
 * Add node to message block. Copy the node using offset-based
 * calculation from the start of the array of nodes.
 */
static enum op_result
try_add_dgsh_node(const char *tool_name, pid_t self_pid, int *n_input_fds,
						int *n_output_fds)
{
	int n_nodes = chosen_mb->n_nodes;
	int i;
	for (i = 0; i < n_nodes; i++) {
		DPRINTF(4, "node name: %s, pid: %d",
						chosen_mb->node_array[i].name,
						chosen_mb->node_array[i].pid);
		if (chosen_mb->node_array[i].pid == self_pid)
			break;
	}
	if (i == n_nodes) {
		fill_node(tool_name, self_pid, n_input_fds, n_output_fds);
		if (add_node() == OP_ERROR)
			return OP_ERROR;
		DPRINTF(4, "Dgsh graph now has %d nodes.\n", chosen_mb->n_nodes);
		return OP_SUCCESS;
	}
	return OP_EXISTS;
}

static void
free_conc_array(struct dgsh_negotiation *mb)
{
	int i, n_concs = mb->n_concs;
	for (i = 0; i < n_concs; i++)
		if (mb->conc_array[i].proc_pids)
			free(mb->conc_array[i].proc_pids);
	free(mb->conc_array);
}

/* Deallocate message block together with nodes and edges. */
void
free_mb(struct dgsh_negotiation *mb)
{
	if (mb->graph_solution)
		free_graph_solution(mb->n_nodes - 1);
	if (mb->node_array)
		free(mb->node_array);
	if (mb->edge_array)
		free(mb->edge_array);
	if (mb->conc_array)
		free_conc_array(mb);
	free(mb);
	DPRINTF(4, "%s(): Freed message block.", __func__);
}

static enum op_result
register_node_edge(const char *tool_name, pid_t self_pid, int *n_input_fds,
		int *n_output_fds)
{
	/* Create dgsh node representation and add node, edge to the graph. */
	if (try_add_dgsh_node(tool_name, self_pid, n_input_fds,
				n_output_fds) == OP_ERROR)
		return OP_ERROR;

	if (try_add_dgsh_edge() == OP_ERROR)
		return OP_ERROR;

	return OP_SUCCESS;
}

/**
 * Check if the arrived message block preexists our chosen one
 * and substitute the chosen if so.
 * If the arrived message block is younger discard it and don't
 * forward it.
 * If the arrived is the chosen, try to add the edge.
 */
static enum op_result
analyse_read(struct dgsh_negotiation *fresh_mb,
			int *ntimes_seen_run,
			int *ntimes_seen_error,
			int *ntimes_seen_draw_exit,
			const char *tool_name,
			pid_t pid, int *n_input_fds, int *n_output_fds)
{
	if (fresh_mb != NULL) {
		if (chosen_mb != NULL)
			free(chosen_mb);
		chosen_mb = fresh_mb;
	} else
		if (chosen_mb == NULL)
			construct_message_block(tool_name, pid);

	if (init_error)
		chosen_mb->state = PS_ERROR;

	if (chosen_mb->state == PS_ERROR) {
		if (errno == 0)
			errno = ECONNRESET;
		if (chosen_mb->is_error_confirmed)
			(*ntimes_seen_error)++;
	} else if (chosen_mb->state == PS_DRAW_EXIT)
		(*ntimes_seen_draw_exit)++;
	else if (chosen_mb->state == PS_RUN)
		(*ntimes_seen_run)++;
	else if (chosen_mb->state == PS_NEGOTIATION)
		if (register_node_edge(tool_name, pid, n_input_fds,
				n_output_fds) == OP_ERROR)
			chosen_mb->state = PS_ERROR;
	return OP_SUCCESS;
}

static enum op_result
check_read(int bytes_read, int buf_size, int expected_read_size) {
	if (bytes_read != expected_read_size) {
		DPRINTF(4, "%s(): ERROR: Read %d bytes of message block, expected to read %d.\n",
			__func__, bytes_read, expected_read_size);
		return OP_ERROR;
	}
	if (bytes_read > buf_size) {
		DPRINTF(4, "%s(): ERROR: Read %d bytes of message block, but buffer can hold up to %d.",
				__func__, bytes_read, buf_size);
		return OP_ERROR;
	}
	return OP_SUCCESS;
}

static enum op_result
alloc_copy_proc_pids(struct dgsh_conc *c, char *buf, int bytes_read,
		int buf_size)
{
	int expected_read_size = sizeof(int) * c->n_proc_pids;
	if (check_read(bytes_read, buf_size, expected_read_size) == OP_ERROR)
		return OP_ERROR;
	c->proc_pids = (int *)malloc(bytes_read);
	memcpy(c->proc_pids, buf, bytes_read);
	return OP_SUCCESS;
}

static enum op_result
alloc_copy_concs(struct dgsh_negotiation *mb, char *buf, int bytes_read,
		int buf_size)
{
	int expected_read_size = sizeof(struct dgsh_conc) * mb->n_concs;
	if (check_read(bytes_read, buf_size, expected_read_size) == OP_ERROR)
		return OP_ERROR;
	mb->conc_array = (struct dgsh_conc *)malloc(bytes_read);
	memcpy(mb->conc_array, buf, bytes_read);
	return OP_SUCCESS;
}

/* Allocate memory for graph solution and copy from buffer. */
static enum op_result
alloc_copy_graph_solution(struct dgsh_negotiation *mb, char *buf, int bytes_read,
								int buf_size)
{
	int expected_read_size = sizeof(struct dgsh_node_connections) *
								mb->n_nodes;
	if (check_read(bytes_read, buf_size, expected_read_size) == OP_ERROR)
		return OP_ERROR;
	mb->graph_solution = (struct dgsh_node_connections *)malloc(bytes_read);
	memcpy(mb->graph_solution, buf, bytes_read);
	return OP_SUCCESS;
}

/* Allocate memory for message_block edges and copy from buffer. */
static enum op_result
alloc_copy_edges(struct dgsh_negotiation *mb, char *buf, int bytes_read,
								int buf_size)
{
	int expected_read_size = sizeof(struct dgsh_edge) * mb->n_edges;
	if (check_read(bytes_read, buf_size, expected_read_size) == OP_ERROR)
		return OP_ERROR;
	mb->edge_array = (struct dgsh_edge *)malloc(bytes_read);
	memcpy(mb->edge_array, buf, bytes_read);
	return OP_SUCCESS;
}

/* Allocate memory for message_block nodes and copy from buffer. */
static enum op_result
alloc_copy_nodes(struct dgsh_negotiation *mb, char *buf, int bytes_read,
								int buf_size)
{
	int expected_read_size = sizeof(struct dgsh_node) * mb->n_nodes;
	if (check_read(bytes_read, buf_size, expected_read_size) == OP_ERROR)
		return OP_ERROR;
	mb->node_array = (struct dgsh_node *)malloc(bytes_read);
	memcpy(mb->node_array, buf, bytes_read);
	DPRINTF(4, "%s(): Node array recovered.", __func__);
	return OP_SUCCESS;
}

/* Allocate memory for core message_block and copy from buffer. */
static enum op_result
alloc_copy_mb(struct dgsh_negotiation **mb, char *buf, int bytes_read,
							int buf_size)
{
	int expected_read_size = sizeof(struct dgsh_negotiation);
	if (check_read(bytes_read, buf_size, expected_read_size) == OP_ERROR)
		return OP_ERROR;
	*mb = (struct dgsh_negotiation *)malloc(bytes_read);
	memcpy(*mb, buf, bytes_read);
	(*mb)->node_array = NULL;
	(*mb)->edge_array = NULL;
	(*mb)->graph_solution = NULL;
	return OP_SUCCESS;
}

/**
 * The actual call to read in the message block.
 * If the call does not succeed or does not signal retry we have
 * to quit operation.
 */
static enum op_result
call_read(int fd, char *buf, int buf_size, int *bytes_read, int *error_code)
{
	*error_code = 0;
	DPRINTF(4, "Try read from fd %d.", fd);
	if ((*bytes_read = read(fd, buf, buf_size)) == -1) {
		*error_code = -errno;
		return OP_ERROR;
	}
	return OP_SUCCESS;
}

/**
 * Try to read a chunk of data from either side (stdin or stdout).
 * This function is agnostic as to what it is reading.
 * Its job is to manage a read operation.
 */
static enum op_result
read_chunk(int read_fd, char *buf, int buf_size, int *bytes_read,
						int struct_type)
{
	int error_code;
	DPRINTF(4, "%s(): buf_size: %d, IOV_MAX: %d",
			__func__, buf_size, iov_max);

	if (buf_size > iov_max) {
		int buf_size_piece, pieces, i, size_copied = 0, rsize = 0,
			struct_size, all_elements, max_elements, elements = 0;

		struct_size = get_struct_size(struct_type);
		all_elements = buf_size / struct_size;
		max_elements = iov_max / struct_size;
		pieces = all_elements / max_elements;
		pieces += (all_elements % max_elements > 0);
		DPRINTF(4, "struct_type: %d, pieces: %d, all_elements: %d, max_elements: %d",
			struct_type, pieces, all_elements, max_elements);
		
		for (i = 0; i < pieces; i++) {
			void *buf_piece;
			if (all_elements - max_elements > 0)
				elements = max_elements;
			else if (all_elements > 0)
				elements = all_elements;
			all_elements -= elements;
			buf_size_piece = struct_size * elements;
			DPRINTF(4, "Round %d: elements: %d, size: %d",
				i, elements, buf_size_piece);

			if (buf_size_piece > 0) {
				buf_piece = malloc(buf_size_piece);
				call_read(read_fd, buf_piece, buf_size_piece,
					bytes_read, &error_code);
				if (*bytes_read == -1) {
					free(buf_piece);
					return OP_ERROR;
				}
				rsize += *bytes_read;
				memcpy(buf + size_copied, buf_piece,
							buf_size_piece);
				size_copied += buf_size_piece;
				free(buf_piece);
			}
		}
		*bytes_read = rsize;
	} else
		call_read(read_fd, buf, buf_size, bytes_read, &error_code);

	if (*bytes_read == -1) {  /* Read failed. */
	 	DPRINTF(4, "ERROR: Reading from fd %d failed with error code %d.",
			read_fd, error_code);
		return error_code;
	} else  /* Read succeeded. */
		DPRINTF(4, "Read succeeded: %d bytes read from %d.\n",
		*bytes_read, read_fd);
	return OP_SUCCESS;
}

/* Allocate memory for file descriptors. */
static enum op_result
alloc_fds(int **fds, int n_fds)
{
	if (n_fds) {
		*fds = (int *)malloc(sizeof(int) * n_fds);
		if (*fds == NULL)
			return OP_ERROR;
	}
	return OP_SUCCESS;
}

/* Allocate memory for output file descriptors. */
static enum op_result
alloc_io_fds()
{
	int i = 0;
	struct dgsh_node_connections *this_nc =
				&chosen_mb->graph_solution[self_node.index];
	DPRINTF(4, "%s(): self node: %d, incoming edges: %d, outgoing edges: %d", __func__, self_node.index, this_nc->n_edges_incoming, this_nc->n_edges_outgoing);

	self_pipe_fds.n_input_fds = 0; /* For safety. */
	for (i = 0; i < this_nc->n_edges_incoming; i++) {
		int k;
		for (k = 0; k < this_nc->edges_incoming[i].instances; k++)
			self_pipe_fds.n_input_fds++;
	}
	if (alloc_fds(&self_pipe_fds.input_fds, self_pipe_fds.n_input_fds) ==
									OP_ERROR)
		return OP_ERROR;

	self_pipe_fds.n_output_fds = 0;
	for (i = 0; i < this_nc->n_edges_outgoing; i++) {
		int k;
		for (k = 0; k < this_nc->edges_outgoing[i].instances; k++)
			self_pipe_fds.n_output_fds++;
	}
	if (alloc_fds(&self_pipe_fds.output_fds, self_pipe_fds.n_output_fds) ==
									OP_ERROR)
		return OP_ERROR;

	return OP_SUCCESS;
}

/* Return the pid of the node that dispatched
 * the provided message block.
 */
pid_t
get_origin_pid(struct dgsh_negotiation *mb)
{
	/* Nodes may not be recorded if an error manifests early */
	if (mb->node_array) {
		struct dgsh_node *n = &mb->node_array[mb->origin_index];
		DPRINTF(4, "Logical origin: tool %s with pid %d",
				n->name, n->pid);
		return n->pid;
	} else
		return 0;
}

/* Return the number of input file descriptors
 * expected by process with pid PID.
 * It is applicable to concentrators too.
 */
int
get_expected_fds_n(struct dgsh_negotiation *mb, pid_t pid)
{
	int expected_fds_n = 0;
	int i = 0, j = 0;
	for (i = 0; i < mb->n_nodes; i++) {
		if (mb->node_array[i].pid == pid) {
			struct dgsh_node_connections *graph_solution =
				mb->graph_solution;
			for (j = 0; j < graph_solution[i].n_edges_incoming; j++)
				expected_fds_n +=
					graph_solution[i].edges_incoming[j].instances;
			return expected_fds_n;
		}
	}
	/* pid may belong to another conc */
	for (i = 0; i < mb->n_concs; i++) {
		if (mb->conc_array[i].pid == pid)
			return mb->conc_array[i].input_fds;
	}
	/* Invalid pid */
	return -1;
}

/* Return the number of output file descriptors
 * provided by process with pid PID.
 * It is applicable to concentrators too.
 */
int
get_provided_fds_n(struct dgsh_negotiation *mb, pid_t pid)
{
	int provided_fds_n = 0;
	int i = 0, j = 0;
	for (i = 0; i < mb->n_nodes; i++) {
		if (mb->node_array[i].pid == pid) {
			struct dgsh_node_connections *graph_solution =
				mb->graph_solution;
			for (j = 0; j < graph_solution[i].n_edges_outgoing; j++) {
				provided_fds_n +=
					graph_solution[i].edges_outgoing[j].instances;
			}
			return provided_fds_n;
		}
	}
	/* pid may belong to another conc */
	for (i = 0; i < mb->n_concs; i++)
		if (mb->conc_array[i].pid == pid)
			return mb->conc_array[i].output_fds;
	/* Invalid pid */
	return -1;
}

/*
 * Write the file descriptor fd_to_write to
 * the socket file descriptor output_socket.
 */
void
write_fd(int output_socket, int fd_to_write)
{
	struct msghdr    msg;
	struct cmsghdr  *cmsg;
	unsigned char    buf[CMSG_SPACE(sizeof(int))];
	struct iovec io = { .iov_base = " ", .iov_len = 1 };

	memset(&msg, 0, sizeof(msg));
	msg.msg_iov = &io;
	msg.msg_iovlen = 1;
	msg.msg_control = buf;
	msg.msg_controllen = sizeof(buf);

	cmsg = CMSG_FIRSTHDR(&msg);
	cmsg->cmsg_len = CMSG_LEN(sizeof(int));
	cmsg->cmsg_level = SOL_SOCKET;
	cmsg->cmsg_type = SCM_RIGHTS;
	*(int *)CMSG_DATA(cmsg) = fd_to_write;

	if (sendmsg(output_socket, &msg, 0) == -1)
		err(1, "sendmsg on fd %d", output_socket);
}

/*
 * Read a file descriptor from socket input_socket and return it.
 */
int
read_fd(int input_socket)
{
	struct msghdr msg;
	struct cmsghdr *cmsg;
	unsigned char buf[CMSG_SPACE(sizeof(int))];
	char m_buffer[2];
	struct iovec io = { .iov_base = m_buffer, .iov_len = sizeof(m_buffer) };

	memset(&msg, 0, sizeof(msg));
	msg.msg_control = buf;
	msg.msg_controllen = sizeof(buf);
	msg.msg_iov = &io;
	msg.msg_iovlen = 1;

again:
	if (recvmsg(input_socket, &msg, 0) == -1) {
		if (errno == EAGAIN) {
			sleep(1);
			goto again;
		}
		err(1, "recvmsg on fd %d", input_socket);
	}
	if ((msg.msg_flags & MSG_TRUNC) || (msg.msg_flags & MSG_CTRUNC))
		errx(1, "control message truncated on fd %d", input_socket);
	for (cmsg = CMSG_FIRSTHDR(&msg); cmsg != NULL;
	    cmsg = CMSG_NXTHDR(&msg, cmsg)) {
		if (cmsg->cmsg_len == CMSG_LEN(sizeof(int)) &&
		    cmsg->cmsg_level == SOL_SOCKET &&
		    cmsg->cmsg_type == SCM_RIGHTS)
			return *(int *)CMSG_DATA(cmsg);
	}
	errx(1, "unable to read file descriptor from fd %d", input_socket);
}

/* Read file descriptors piping input from another tool in the dgsh graph. */
static enum op_result
read_input_fds(int input_socket, int *input_fds)
{
	/**
	 * A node's connections are located at the same position
         * as the node in the node array.
	 */
	struct dgsh_node_connections *graph_solution =
					chosen_mb->graph_solution;
	struct dgsh_node_connections *this_nc =
					&graph_solution[self_node.index];
	assert(this_nc->node_index == self_node.index);
	int i;
	int total_edge_instances = 0;
	enum op_result re = OP_SUCCESS;

	DPRINTF(4, "%s(): %d incoming edges to inspect of node %d.", __func__,
			this_nc->n_edges_incoming, self_node.index);
	for (i = 0; i < this_nc->n_edges_incoming; i++) {
		int k;
		/**
		 * Due to channel constraint flexibility,
		 * each edge can have more than one instances.
		 */
		for (k = 0; k < this_nc->edges_incoming[i].instances; k++) {
			input_fds[total_edge_instances] = read_fd(input_socket);
			DPRINTF(4, "%s: Node %d received file descriptor %d.",
					__func__, this_nc->node_index,
					input_fds[total_edge_instances]);
			total_edge_instances++;
		}
		/* XXX */
		if (re == OP_ERROR)
			break;
	}
	if (re == OP_ERROR) {
		free_graph_solution(chosen_mb->n_nodes - 1);
		free(input_fds);
	}
	return re;
}

static enum op_result
read_concs(int read_fd, struct dgsh_negotiation *fresh_mb)
{
	int bytes_read;
	size_t buf_size = sizeof(struct dgsh_conc) * fresh_mb->n_concs;
	char *buf = (char *)malloc(buf_size);
	enum op_result error_code = OP_SUCCESS;

	if (!fresh_mb->conc_array)
		return error_code;

	if ((error_code = read_chunk(read_fd, buf, buf_size, &bytes_read, 3))
			!= OP_SUCCESS)
		return error_code;
	error_code = alloc_copy_concs(fresh_mb, buf, bytes_read, buf_size);
	free(buf);

	int i, n_concs = fresh_mb->n_concs;
	for (i = 0; i < n_concs; i++) {
		struct dgsh_conc *c = &fresh_mb->conc_array[i];
		size_t buf_size = sizeof(int) * c->n_proc_pids;
		buf = (char *)malloc(buf_size);
		if ((error_code = read_chunk(read_fd, buf, buf_size,
						&bytes_read, 5)) != OP_SUCCESS)
			return error_code;
		error_code = alloc_copy_proc_pids(c, buf, bytes_read, buf_size);
		free(buf);
		DPRINTF(4, "%s(): Read %d proc_pids for conc %d at index %d of size %d bytes ",
				__func__, c->n_proc_pids, c->pid, i, bytes_read);
	}

	return error_code;
}

/* Try read solution to the dgsh negotiation graph. */
static enum op_result
read_graph_solution(int read_fd, struct dgsh_negotiation *fresh_mb)
{
	int i;
	int bytes_read = 0;
	int n_nodes = fresh_mb->n_nodes;
	size_t buf_size = sizeof(struct dgsh_node_connections) * n_nodes;
	char *buf = (char *)malloc(buf_size);
	enum op_result error_code = OP_SUCCESS;

	/* Read node connection structures of the solution. */
	if ((error_code = read_chunk(read_fd, buf, buf_size, &bytes_read, 4))
			!= OP_SUCCESS)
		return error_code;
	if ((error_code = alloc_copy_graph_solution(fresh_mb, buf, bytes_read,
					buf_size)) == OP_ERROR)
		return error_code;
	free(buf);

	struct dgsh_node_connections *graph_solution =
					fresh_mb->graph_solution;
	for (i = 0; i < n_nodes; i++) {
		struct dgsh_node_connections *nc = &graph_solution[i];
		DPRINTF(4, "Node %d with %d incoming edges at %lx and %d outgoing edges at %lx.", nc->node_index, nc->n_edges_incoming, (long)nc->edges_incoming, nc->n_edges_outgoing, (long)nc->edges_outgoing);
		int in_edges_size = sizeof(struct dgsh_edge) * nc->n_edges_incoming;
		int out_edges_size = sizeof(struct dgsh_edge) * nc->n_edges_outgoing;

		/* Read a node's incoming connections. */
		if (nc->n_edges_incoming > 0) {
			buf = (char *)malloc(in_edges_size);
			if ((error_code = read_chunk(read_fd, buf, in_edges_size,
				&bytes_read, 2)) != OP_SUCCESS)
				return error_code;
			if (in_edges_size != bytes_read) {
				DPRINTF(4, "%s(): ERROR: Expected %d bytes, got %d.", __func__,
						in_edges_size, bytes_read);
				return OP_ERROR;
			}
			if (alloc_node_connections(&nc->edges_incoming,
				nc->n_edges_incoming, 0, i) == OP_ERROR)
				return OP_ERROR;
			memcpy(nc->edges_incoming, buf, in_edges_size);
			free(buf);
		}

		/* Read a node's outgoing connections. */
		if (nc->n_edges_outgoing) {
			buf = (char *)malloc(out_edges_size);
			if ((error_code = read_chunk(read_fd, buf, out_edges_size,
				&bytes_read, 2)) != OP_SUCCESS)
				return error_code;
			if (out_edges_size != bytes_read) {
				DPRINTF(4, "%s(): ERROR: Expected %d bytes, got %d.", __func__,
						out_edges_size, bytes_read);
				return OP_ERROR;
			}
			if (alloc_node_connections(&nc->edges_outgoing,
				nc->n_edges_outgoing, 1, i) == OP_ERROR)
				return OP_ERROR;
			memcpy(nc->edges_outgoing, buf, out_edges_size);
			free(buf);
		}
	}
	return OP_SUCCESS;
}

/**
 * Read a circulated message block coming in on any of the specified read_fds.
 * In most cases these will specify the input or output side. This capability
 * relies on an extension to a standard shell implementation,
 * e.g., bash, that allows reading and writing to both sides
 * for the negotiation phase.
 * On return:
 * read_fd will contain the file descriptor number that was read.
 * fresh_mb will contain the read message block in dynamically allocated
 * memory. This should be freed by calling free_mb.
 */
enum op_result
read_message_block(int read_fd, struct dgsh_negotiation **fresh_mb)
{
	size_t buf_size = sizeof(struct dgsh_negotiation);
	char *buf = (char *)malloc(buf_size);
	int bytes_read = 0;
	enum op_result error_code = 0;

	DPRINTF(3, "%s(): %s (%d)", __func__, programname, self_node.index);

	memset(buf, 0, buf_size);

	/* Try read core message block: struct negotiation state fields. */
	if ((error_code = read_chunk(read_fd, buf, buf_size, &bytes_read, 0))
			!= OP_SUCCESS)
		return error_code;
	if (alloc_copy_mb(fresh_mb, buf, bytes_read, buf_size) == OP_ERROR)
		return OP_ERROR;
	free(buf);

	if ((*fresh_mb)->n_nodes > 0) {
		buf_size = sizeof(struct dgsh_node) * (*fresh_mb)->n_nodes;
		buf = (char *)malloc(buf_size);
		if ((error_code = read_chunk(read_fd, buf, buf_size,
					&bytes_read, 1)) != OP_SUCCESS)
			return error_code;
		if (alloc_copy_nodes(*fresh_mb, buf, bytes_read, buf_size)
								== OP_ERROR)
			return OP_ERROR;
		free(buf);
	}

	if (read_concs(read_fd, *fresh_mb) == OP_ERROR)
		return OP_ERROR;

	if ((*fresh_mb)->state == PS_NEGOTIATION) {
		if ((*fresh_mb)->n_edges > 0) {
			DPRINTF(4, "%s(): Read %d negotiation graph edges.",
					__func__, (*fresh_mb)->n_edges);
			buf_size = sizeof(struct dgsh_edge) * (*fresh_mb)->n_edges;
			buf = (char *)malloc(buf_size);
			if ((error_code = read_chunk(read_fd, buf, buf_size,
			     &bytes_read, 2)) != OP_SUCCESS)
				return error_code;
			if (alloc_copy_edges(*fresh_mb, buf, bytes_read,
			    buf_size) == OP_ERROR)
				return OP_ERROR;
			free(buf);
		}
	} else if ((*fresh_mb)->state == PS_RUN) {
		/**
		 * Try read solution.
		 * fresh_mb should be an updated version of the chosen_mb
		 * or even the same structure because this is the phase
		 * where we share the solution across the dgsh graph.
                 */
		if (read_graph_solution(read_fd, *fresh_mb) == OP_ERROR)
			return OP_ERROR;
	}
	DPRINTF(4, "%s(): Read message block or solution from node %d sent from file descriptor: %s.\n", __func__, (*fresh_mb)->origin_index, ((*fresh_mb)->origin_fd_direction) ? "stdout" : "stdin");
	return OP_SUCCESS;
}

/* Construct a message block to use as a vehicle for the negotiation phase. */
enum op_result
construct_message_block(const char *tool_name, pid_t self_pid)
{
	chosen_mb = (struct dgsh_negotiation *)malloc(
				sizeof(struct dgsh_negotiation));
	if (!chosen_mb) {
		DPRINTF(4, "ERROR: Memory allocation of message block failed.");
		return OP_ERROR;
	}

	chosen_mb->version = 1;
	chosen_mb->node_array = NULL;
	chosen_mb->n_nodes = 0;
	chosen_mb->edge_array = NULL;
	chosen_mb->n_edges = 0;
	chosen_mb->initiator_pid = self_pid;
	chosen_mb->state = (init_error ? PS_ERROR : PS_NEGOTIATION);
	chosen_mb->is_error_confirmed = false;
	chosen_mb->origin_index = -1;
	chosen_mb->origin_fd_direction = -1;
	chosen_mb->is_origin_conc = false;
	chosen_mb->conc_pid = -1;
	chosen_mb->graph_solution = NULL;
	chosen_mb->conc_array = NULL;
	chosen_mb->n_concs = 0;
	DPRINTF(3, "Message block created by process %s with pid %d.\n",
						tool_name, (int)self_pid);
	return OP_SUCCESS;
}

/* Get environment variable env_var. */
static void
get_env_var(const char *env_var, int *value)
{
	char *string_value = getenv(env_var);
	if (string_value == NULL)
		DPRINTF(4, "Getting environment variable %s failed.",
				env_var);
	else {
		DPRINTF(4, "getenv() returned string value %s.",
				string_value);
		*value = atoi(string_value);
		DPRINTF(4, "Integer form of value is %d.", *value);
	}
}

/**
 * Get environment variables DGSH_IN, DGSH_OUT set up by
 * the shell (through execvpe()).
 */
static void
get_environment_vars()
{
	DPRINTF(4, "Try to get environment variable DGSH_IN.");
	get_env_var("DGSH_IN", &self_node.dgsh_in);

	DPRINTF(4, "Try to get environment variable DGSH_OUT.");
	get_env_var("DGSH_OUT", &self_node.dgsh_out);
}

/**
 * Verify tool's I/O channel requirements are sane.
 * We might need some upper barrier for requirements too,
 * such as, not more than 100 or 1000.
 */
STATIC enum op_result
validate_input(int *channels_required, int *channels_provided, const char *tool_name)
{

	if (!tool_name) {
		DPRINTF(4, "ERROR: NULL pointer provided as tool name.\n");
		return OP_ERROR;
	}
	if (channels_required == NULL || channels_provided == NULL)
		return OP_SUCCESS;
	if (*channels_required < -1 || *channels_provided < -1) {
		DPRINTF(4, "ERROR: I/O requirements entered for tool %s are less than -1. \nChannels required %d \nChannels provided: %d",
			tool_name, *channels_required, *channels_provided);
		return OP_ERROR;
	}
	return OP_SUCCESS;
}

static int
set_fds(fd_set *read_fds, fd_set *write_fds, bool isread)
{
	fd_set *fds;
	FD_ZERO(read_fds);
	FD_ZERO(write_fds);

	DPRINTF(4, "Next operation is a %s", isread ? "read" : "write");
	/* The next operation is a read or a write */
	if (isread)
		fds = read_fds;
	else
		fds = write_fds;

	if (self_node.dgsh_out && !self_node.dgsh_in) {
		self_node_io_side.fd_direction = STDOUT_FILENO;
		FD_SET(STDOUT_FILENO, fds);
	} else if (!self_node.dgsh_out && self_node.dgsh_in) {
		self_node_io_side.fd_direction = STDIN_FILENO;
		FD_SET(STDIN_FILENO, fds);
	} else {
		/* We should have all ears open for a read */
		if (isread) {
			FD_SET(STDIN_FILENO, fds);
			FD_SET(STDOUT_FILENO, fds);
		} else {
			/* But for writing we should pass the message across.
			 * If mb came from stdout channel, we got it from stdin.
			 * So we should send it from stdout */
			if (chosen_mb->origin_fd_direction == STDOUT_FILENO) {
				FD_SET(STDOUT_FILENO, fds);
				self_node_io_side.fd_direction = STDOUT_FILENO;
				DPRINTF(4, "STDOUT set for write");
			} else {
				FD_SET(STDIN_FILENO, fds);
				self_node_io_side.fd_direction = STDIN_FILENO;
				DPRINTF(4, "STDIN set for write");
			}
		}
	}
	/* so that after select() we try both 0 and 1 to see if they are set */
	return 2;
}

void
set_negotiation_complete()
{
	negotiation_completed = 1;
}

static int
setup_file_descriptors(int *n_input_fds, int *n_output_fds,
		int **input_fds, int **output_fds)
{
	DPRINTF(4, "%s()", __func__);
	if (n_input_fds != NULL && (*n_input_fds == 1 ||
			*n_input_fds == -1) && input_fds != NULL) {
		DPRINTF(4, "n_input_fds: %d\n", *n_input_fds);
		*n_input_fds = 1;
		*input_fds = malloc(sizeof(int));
		(*input_fds)[0] = STDIN_FILENO;
	}
	if (n_output_fds != NULL && (*n_output_fds == 1 ||
			*n_output_fds == -1) && output_fds != NULL) {
		DPRINTF(4, "n_output_fds: %d\n", *n_output_fds);
		*n_output_fds = 1;
		*output_fds = malloc(sizeof(int));
		(*output_fds)[0] = STDOUT_FILENO;
	}
	return 0;
}


/*
 * Return function for dgsh_negotiate
 * Exit by printing an error (if needed)
 * otherwise return 0 for successful termination
 * or -1 for error
 */
static int
dgsh_exit(int ret, int flags)
{
	if (ret == PS_COMPLETE)
		return 0;
	if (ret == PS_DRAW_EXIT)
		exit(EX_OK);
	if (!(flags & DGSH_HANDLE_ERROR))
		return -1;

	switch (errno) {
	case ECONNRESET:
		exit(EX_PROTOCOL);
	case 0:
		errx(EX_PROTOCOL, "dgsh negotiation failed");
	default:
		err(EX_PROTOCOL, "dgsh negotiation failed");
	}
}

/**
 * Return the name of the specified state
 */
const char *
state_name(enum prot_state s)
{
	switch (s) {
	case PS_COMPLETE:
		return "COMPLETE";
	case PS_NEGOTIATION:
		return "NEGOTIATION";
	case PS_NEGOTIATION_END:
		return "NEGOTIATION_END";
	case PS_RUN:
		return "RUN";
	case PS_ERROR:
		return "ERROR";
	default:
		assert(0);
	}
}

/**
 * Each tool in the dgsh graph calls dgsh_negotiate() to take part in
 * peer-to-peer negotiation. A message block (MB) is circulated among tools
 * and is filled with tools' I/O requirements. When all requirements are in
 * place, an algorithm runs that tries to find a solution that satisfies
 * all requirements. If a solution is found, pipes are allocated and
 * set up according to the solution. The appropriate file descriptors
 * are provided to each tool and the negotiation phase ends.
 * The function's return value signifies success or failure of the
 * negotiation phase.
 */
int
dgsh_negotiate(int flags, const char *tool_name, int *n_input_fds,
		int *n_output_fds, int **input_fds, int **output_fds)
{
	int i = 0;
	int ntimes_seen_run = 0;
	int ntimes_seen_error = 0;
	int ntimes_seen_draw_exit = 0;
	pid_t self_pid = getpid();    /* Get tool's pid */
	struct dgsh_negotiation *fresh_mb = NULL; /* MB just read. */

	int nfds = 0, n_io_sides;
	bool isread = false;
	fd_set read_fds, write_fds;
	char *timeout;
	char *debug_level;

	if (negotiation_completed) {
		errno = EALREADY;
		return dgsh_exit(-1, flags);
	}

	/* Get and set user-provided debug level.
	 * dgsh_debug_level is defined in debug.h.
	 */
	debug_level = getenv("DGSH_DEBUG_LEVEL");
	if (debug_level != NULL)
		dgsh_debug_level = atoi(debug_level);

#ifndef UNIT_TESTING
	self_pipe_fds.input_fds = NULL;		/* Clean garbage */
	self_pipe_fds.output_fds = NULL;	/* Ditto */
#endif
	programname = strdup(tool_name);
	DPRINTF(2, "%s(): Tool %s with pid %d negotiating: nin=%d nout=%d.",
			__func__, tool_name, (int)self_pid,
			n_input_fds ? *n_input_fds : 1,
			n_output_fds ? *n_output_fds : 1);

	if (validate_input(n_input_fds, n_output_fds, tool_name)
							== OP_ERROR) {
		negotiation_completed = 1;
		return dgsh_exit(-1, flags);
	}

	self_node.dgsh_in = 0;
	self_node.dgsh_out = 0;
	get_environment_vars();
	n_io_sides = self_node.dgsh_in + self_node.dgsh_out;

	/* Verify dgsh available on the required sides */
	if ((n_input_fds != NULL && *n_input_fds > 1 && !self_node.dgsh_in) ||
	    (n_output_fds != NULL && *n_output_fds > 1 && !self_node.dgsh_out)) {
		errno = ENOTSOCK;
		negotiation_completed = 1;
		return dgsh_exit(-1, flags);
	}

	/* Easy case, no dgsh I/O */
	if (n_io_sides == 0) {
		negotiation_completed = 1;
		return dgsh_exit(setup_file_descriptors(n_input_fds,
					n_output_fds, input_fds, output_fds), flags);
	}

	signal(SIGALRM, dgsh_alarm_handler);
	if ((timeout = getenv("DGSH_TIMEOUT")) != NULL)
		alarm(atoi(timeout));
	else
		alarm(DGSH_TIMEOUT);

	/* Start negotiation */
	if (self_node.dgsh_out && !self_node.dgsh_in) {
#ifdef TIME
		clock_gettime(CLOCK_MONOTONIC, &tstart);
#endif
		if (construct_message_block(tool_name, self_pid) == OP_ERROR)
			chosen_mb->state = PS_ERROR;
		if (register_node_edge(tool_name, self_pid, n_input_fds,
				n_output_fds) == OP_ERROR)
			chosen_mb->state = PS_ERROR;
		isread = false;
        } else { /* or wait to receive MB. */
		isread = true;
		chosen_mb = NULL;
	}

	/* Perform phases and rounds. */
	while (1) {
again:
		DPRINTF(4, "%s(): perform round", __func__);
		nfds = set_fds(&read_fds, &write_fds, isread);
		if (select(nfds, &read_fds, &write_fds, NULL, NULL) < 0) {
			if (errno == EINTR)
				goto again;
			perror("select");
			chosen_mb->state = PS_ERROR;
		}

		for (i = 0; i < nfds; i++) {
			if (FD_ISSET(i, &write_fds)) {
				DPRINTF(4, "write on fd %d is active.", i);
				/* Write message block et al. */
				set_dispatcher();
				if (write_message_block(i) == OP_ERROR)
					chosen_mb->state = PS_ERROR;
				if (n_io_sides == ntimes_seen_run ||
				    n_io_sides == ntimes_seen_error ||
				    n_io_sides == ntimes_seen_draw_exit) {
					if (chosen_mb->state == PS_RUN)
						chosen_mb->state = PS_COMPLETE;
					goto exit;
				}
				isread = true;
			}
			if (FD_ISSET(i, &read_fds)) {
				DPRINTF(4, "read on fd %d is active.", i);
				/* Read message block et al. */
				if (read_message_block(i, &fresh_mb)
						== OP_ERROR &&
						fresh_mb != NULL)
					fresh_mb->state = PS_ERROR;
				/* Check state */
				analyse_read(fresh_mb,
						&ntimes_seen_run,
						&ntimes_seen_error,
						&ntimes_seen_draw_exit,
						tool_name,
						self_pid, n_input_fds,
						n_output_fds);

				/**
				 * Initiator process.
				 * It receives the block, so all IO
				 * constraints have been stated. Now:
				 * - solves the I/O constraint problem,
				 * - communicates the solution,
				 * and when it receives the block again,
				 * it leaves negotiation.
				 */
				if (self_node.pid ==
						chosen_mb->initiator_pid) {
					switch (chosen_mb->state) {
					case PS_NEGOTIATION:
						chosen_mb->state = PS_NEGOTIATION_END;
						DPRINTF(1, "%s(): Gathered I/O requirements.", __func__);
						int state = solve_graph();
						if (state == OP_ERROR) {
							chosen_mb->state = PS_ERROR;
							chosen_mb->is_error_confirmed = true;
						} else if (state == OP_DRAW_EXIT)
							chosen_mb->state = PS_DRAW_EXIT;
						else {
							DPRINTF(1, "%s(): Computed solution", __func__);
							chosen_mb->state = PS_RUN;
						}
						break;
					case PS_RUN:
						DPRINTF(1, "%s(): Communicated the solution", __func__);
						chosen_mb->state = PS_COMPLETE;
						goto exit;
					case PS_ERROR:
						if (chosen_mb->is_error_confirmed)
							goto exit;
						else
							chosen_mb->is_error_confirmed = true;
						break;
					case PS_DRAW_EXIT:
						goto exit;
					default:
						assert(0);
					}
				}
				isread = false;
			}
		}
	}
exit:
	DPRINTF(2, "%s(): %s (%d) leaves after %s with state %s.", __func__,
			programname, self_node.index, isread ? "read" : "write",
			state_name(chosen_mb->state));
	if (chosen_mb->state == PS_COMPLETE) {
		if (alloc_io_fds() == OP_ERROR)
			chosen_mb->state = PS_ERROR;
		if (read_input_fds(STDIN_FILENO, self_pipe_fds.input_fds) ==
									OP_ERROR)
			chosen_mb->state = PS_ERROR;
		if (write_output_fds(STDOUT_FILENO,
				self_pipe_fds.output_fds, flags) == OP_ERROR)
			chosen_mb->state = PS_ERROR;
		if (establish_io_connections(input_fds, n_input_fds, output_fds,
						n_output_fds) == OP_ERROR)
			chosen_mb->state = PS_ERROR;
	} else if (chosen_mb->state == PS_DRAW_EXIT) {
		if (n_input_fds != NULL)
			*n_input_fds = 0;
		if (n_output_fds != NULL)
			*n_output_fds = 0;
	}
	int state = chosen_mb->state;
#ifdef TIME
	if (self_node.pid == chosen_mb->initiator_pid) {
		clock_gettime(CLOCK_MONOTONIC, &tend);
		fprintf(stderr, "The dgsh negotiation procedure took about %.5f seconds\n",
			((double)tend.tv_sec + 1.0e-9*tend.tv_nsec) -
			((double)tstart.tv_sec + 1.0e-9*tstart.tv_nsec));
		fflush(stderr);
	}
#endif
	free_mb(chosen_mb);
	negotiation_completed = 1;
	alarm(0);			// Cancel alarm
	signal(SIGALRM, SIG_IGN);	// Do not handle the signal
	return dgsh_exit(state, flags);
}

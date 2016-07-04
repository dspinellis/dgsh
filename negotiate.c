/* TODO:
 * - adapt tools that work with sgsh
 * - integration testing
 * - adapt the bash shell
 * Thinking aloud:
 * - watch out when a tool should exit the negotiation loop: it has to have
 * gathered all input pipes required. How many rounds could this take at the
 * worst case?
 * - assert edge constraint, i.e. channels required, provided, is rational.
 * - substitute DPRINTF with appropriate error reporting function in case of errors.
 */

/* Placeholder: LICENSE. */
#include <assert.h>		/* assert() */
#include <errno.h>		/* EAGAIN */
#include <err.h>		/* err() */
#include <stdbool.h>		/* bool, true, false */
#include <stdio.h>		/* fprintf() in DPRINTF() */
#include <stdlib.h>		/* getenv(), errno */
#include <string.h>		/* memcpy() */
#include <sys/socket.h>		/* sendmsg(), recvmsg() */
#include <unistd.h>		/* getpid(), getpagesize(),
				 * STDIN_FILENO, STDOUT_FILENO,
				 * STDERR_FILENO 
				 */
#include <sys/select.h>		/* select(), fd_set, */
#include "sgsh-negotiate.h"	/* sgsh_negotiate(), sgsh_run(), union fdmsg */
#include "sgsh-internal-api.h"	/* Message block and I/O */
#include "sgsh.h"		/* DPRINTF() */

#ifndef UNIT_TESTING

/* Models an I/O connection between tools on an sgsh graph. */
struct sgsh_edge {
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

/* Each tool that participates in an sgsh graph is modelled as follows. */
struct sgsh_node {
        pid_t pid;
	int index;		/* Position in message block's node array. */
        char name[100];		/* Tool's name */
        int requires_channels;	/* Input channels it can take. */
        int provides_channels;	/* Output channels it can provide. */
	int sgsh_in;		/* Takes input from other tool(s) on
				 * sgsh graph.
				 */
	int sgsh_out;		/* Provides output to other tool(s)
				 * on sgsh graph.
				 */
};

/* Holds a node's connections. It contains a piece of the solution. */
struct sgsh_node_connections {
	int node_index;				/* The subject of the
						 * connections. For
						 * verification.
						 */
	struct sgsh_edge *edges_incoming;	/* Array of edges through
						 * which other nodes provide
						 * input to node at node_index.
						 */
	int n_edges_incoming;			/* Number of incoming edges */
	int n_instances_incoming_free;		/* Number of incoming edges
						 * not yet bound to a pair
						 * node's output channel.
						 */
	struct sgsh_edge *edges_outgoing;	/* Array of edges through
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
struct sgsh_node_pipe_fds {
	int *input_fds;		/* Array of input file descriptors */
	int n_input_fds;	/* Number of input file descriptors */
	int *output_fds;	/* Array of output file descriptors */
	int n_output_fds;	/* Number of output file descriptors */
};

/**
 * Memory organisation of message block.
 * Message block will be passed around process address spaces.
 * Message block contains a number of scalar fields and two pointers
 * to an array of sgsh nodes and edges respectively.
 * To pass the message block along with nodes and edges, three writes
 * in this order take place.
 */

/* The message block implicitly used by many functions */
struct sgsh_negotiation *chosen_mb;
static bool mb_is_updated;			/* Boolean value that signals
						 * an update to the mb.
						 */
static struct sgsh_node self_node;		/* The sgsh node that models
						 * this tool.
						 */
static struct node_io_side self_node_io_side;	/* Dispatch info for this tool.
						 */
static struct sgsh_node_pipe_fds self_pipe_fds;		/* A tool's read and
							 * write file
							 * descriptors to use
							 * at execution.
							 */

/**
 * Allocate node indexes to store a node's (at node_index)
 * node outgoing or incoming connections (nc_edges).
 */
STATIC enum op_result
alloc_node_connections(struct sgsh_edge **nc_edges, int nc_n_edges, int type,
								int node_index)
{
	if (!nc_edges) {
		DPRINTF("Double pointer to node connection edges is NULL.\n");
		return OP_ERROR;
	}
	if (node_index < 0) {
		DPRINTF("Index of node whose connections will be allocated is negative number.\n");
		return OP_ERROR;
	}
	if (type > 1 || type < 0) {
		DPRINTF("Type of edge is neither incoming (1) nor outgoing(0).\ntyep is: %d.\n", type);
		return OP_ERROR;
	}

	*nc_edges = (struct sgsh_edge *)malloc(sizeof(struct sgsh_edge) *
								nc_n_edges);
	if (!*nc_edges) {
		DPRINTF("Memory allocation for node's index %d %s connections \
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
make_compact_edge_array(struct sgsh_edge **nc_edges, int nc_n_edges,
			struct sgsh_edge **p_edges)
{
	int i;
	int array_size = sizeof(struct sgsh_edge) * nc_n_edges;

	if (nc_n_edges <= 0) {
		DPRINTF("Size identifier to be used in malloc() is non-positive number: %d.\n", nc_n_edges);
		return OP_ERROR;
	}
	if (nc_edges == NULL) {
		DPRINTF("Compact edge array to put edges (connections) is NULL.\n");
		return OP_ERROR;
	}
	if (p_edges == NULL) {
		DPRINTF("Pointer to edge array is NULL.\n");
		return OP_ERROR;
	}

	*nc_edges = (struct sgsh_edge *)malloc(array_size);
	if (!(*nc_edges)) {
		DPRINTF("Memory allocation of size %d for edge array failed.\n",
								array_size);
		return OP_ERROR;
	}

	/**
	 * Copy the edges of interest to the node-specific edge array
	 * that contains its connections.
	 */
	for (i = 0; i < nc_n_edges; i++) {
		if (p_edges[i] == NULL) {
			DPRINTF("Pointer to edge array contains NULL pointer.\n");
			return OP_ERROR;
		}
		/**
		 * Dereference to reach the array base, make i hops of size
		 * sizeof(struct sgsh_edge), and point to that memory block.
		 */
		memcpy(&(*nc_edges)[i], p_edges[i], sizeof(struct sgsh_edge));
		DPRINTF("%s():Copied edge %d -> %d (%d) at index %d.",
				__func__, p_edges[i]->from, p_edges[i]->to,
				p_edges[i]->instances, i);
	}

	return OP_SUCCESS;
}

/* Reallocate array to edge pointers. */
STATIC enum op_result
reallocate_edge_pointer_array(struct sgsh_edge ***edge_array, int n_elements)
{
	void **p = NULL;
	if (edge_array == NULL) {
		DPRINTF("Edge array is NULL pointer.\n");
		return OP_ERROR;
	}
	if (n_elements <= 0) {
		DPRINTF("Size identifier to be used in malloc() is non-positive number: %d.\n", n_elements);
		return OP_ERROR;
	} else if (n_elements == 1)
		p = malloc(sizeof(struct sgsh_edge *) * n_elements);
	else
		p = realloc(*edge_array,
				sizeof(struct sgsh_edge *) * n_elements);
	if (!p) {
		DPRINTF("Memory reallocation for edge failed.\n");
		return OP_ERROR;
	} else
		*edge_array = (struct sgsh_edge **)p;
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
                       struct sgsh_edge **edges, /* Gathered pointers to edges
						  * of this channel
						  */
		       int n_edges,		/* Number of edges */
                       bool is_edge_incoming)	/* Incoming or outgoing */
{
	int i;
	int weight = -1, modulo = 0;

	/* We can't possibly solve this situation. */
	if (this_channel_constraint > 0)
		if (this_channel_constraint < n_edges)
			return OP_ERROR;
		else {
			*free_instances = this_channel_constraint;
			weight = this_channel_constraint / n_edges;
			modulo = this_channel_constraint % n_edges;
		}
	else			/* Flexible constraint */
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
        	DPRINTF("%s(): edge from %d to %d, is_edge_incoming: %d, free_instances: %d, weight: %d, modulo: %d, from_instances: %d, to_instances: %d.\n", __func__, edges[i]->from, edges[i]->to, is_edge_incoming, *free_instances, weight, modulo, edges[i]->from_instances, edges[i]->to_instances);
	}
	DPRINTF("%s(): Number of edges: %d, this_channel_constraint: %d, free instances: %d.\n", __func__, n_edges, this_channel_constraint, *free_instances);
	return OP_SUCCESS;
}

/**
 * Lookup this tool's edges and store pointers to them in order
 * to then allow the evaluation of constraints for the current node's
 * input and output channels.
 */
static enum op_result
dry_match_io_constraints(struct sgsh_node *current_node,
			 struct sgsh_node_connections *current_connections,
			 struct sgsh_edge ***edges_incoming, /* Uninitialised*/
			 struct sgsh_edge ***edges_outgoing) /* Uninitialised*/
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
		struct sgsh_edge *edge = &chosen_mb->edge_array[i];
		DPRINTF("%s(): edge at index %d from %d to %d, instances %d, from_instances %d, to_instances %d.", __func__, i, edge->from, edge->to, edge->instances, edge->from_instances, edge->to_instances);
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
	DPRINTF("%s(): Node at index %d has %d outgoing edges and %d incoming.",
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
 * Free the sgsh graph's solution in face of an error.
 * node_index: the last node we setup conenctions before error.
 */
static enum op_result
free_graph_solution(int node_index)
{
	int i;
	struct sgsh_node_connections *graph_solution =
					chosen_mb->graph_solution;
	assert(node_index < chosen_mb->n_nodes);
	for (i = 0; i <= node_index; i++) {
		if (graph_solution[i].n_edges_incoming > 0)
			free(graph_solution[i].edges_incoming);
		if (graph_solution[i].n_edges_outgoing > 0)
			free(graph_solution[i].edges_outgoing);
	}
	free(graph_solution);
	DPRINTF("%s: freed %d nodes.", __func__, chosen_mb->n_nodes);
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
	DPRINTF("%s(): to_move: %d, pair: %d, diff: %d", __func__, to_move, pair, *diff);
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
		DPRINTF("%s(): move successful: to_move: %d, pair: %d, diff: %d, instances: %d, edge index: %d", __func__, to_move, pair, *diff, *instances, *index);
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
move(struct sgsh_edge** edges, int n_edges, int diff, bool is_edge_incoming)
{
	int i = 0, j = 0;
	int indexes[n_edges];
	int instances[n_edges];
	/* Try move unbalanced edges first.
	 * Avoid doing the move at the same edge.
	 */
	for (i = 0; i < n_edges; i++) {
		struct sgsh_edge *edge = edges[i];
		int *from = &edge->from_instances;
		int *to = &edge->to_instances;
		DPRINTF("%s(): before move %s edge %d: from: %d, to: %d, diff %d.", __func__, is_edge_incoming ? "incoming" : "outgoing", i, *from, *to, diff);
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
		DPRINTF("%s(): after move %s edge %d: from: %d, to: %d, diff %d.", __func__, is_edge_incoming ? "incoming" : "outgoing", i, *from, *to, diff);
		if (diff == 0)
			goto checkout;
	}
	/* Edges with flexible constraints are by default balanced. Try move */
	for (i = 0; i < n_edges; i++) {
		struct sgsh_edge *edge = edges[i];
		int *from = &edge->from_instances;
		int *to = &edge->to_instances;
		if (is_edge_incoming) {
			if (*from > 0)
				continue;
			if (record_move_flexible(&diff, &indexes[j], i,
				&instances[j], *to) == OP_SUCCESS)
				j++;
		} else {
			if (*to > 0)
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
			DPRINTF("%s(): succeeded: move %d from edge %d.", __func__, instances[k], indexes[k]);
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
                       struct sgsh_edge **edges,	/* Gathered pointers
							 * to edges
							 */
		       int n_edges,		/* Number of edges */
                       bool is_edge_incoming,	/* Incoming or outgoing edges*/
		       int *edges_matched)
{
	int i;

	for (i = 0; i < n_edges; i++) {
		struct sgsh_edge *e = edges[i];
		int *from = &e->from_instances;
		int *to = &e->to_instances;
		if (*from == -1 || *to == -1) {
        		DPRINTF("%s(): edge from %d to %d, this_channel_constraint: %d, is_incoming: %d, from_instances: %d, to_instances %d.\n", __func__, e->from, e->to, this_channel_constraint, is_edge_incoming, *from, *to);
			if (*from == -1 && *to == -1)
				e->instances = 5;
			else if (*from == -1)
				e->instances = *to;
			else if (*to == -1)
				e->instances = *from;
			(*edges_matched)++;
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
        	DPRINTF("%s(): edge from %d to %d, this_channel_constraint: %d, is_incoming: %d, from_instances: %d, to_instances %d.\n", __func__, e->from, e->to, this_channel_constraint, is_edge_incoming, *from, *to);
	}
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
	struct sgsh_node_connections *graph_solution =
					chosen_mb->graph_solution;
	enum op_result exit_state = OP_SUCCESS;

	for (i = 0; i < n_nodes; i++) {
		struct sgsh_node_connections *current_connections =
							&graph_solution[i];
		/* Hack: struct sgsh_edge* -> struct_sgsh_edge** */
		struct sgsh_edge **edges_incoming =
		       (struct sgsh_edge **)current_connections->edges_incoming;
		current_connections->edges_incoming = NULL;
		/* Hack: struct sgsh_edge* -> struct_sgsh_edge** */
		struct sgsh_edge **edges_outgoing =
		       (struct sgsh_edge **)current_connections->edges_outgoing;
		current_connections->edges_outgoing = NULL;
        	int *n_edges_incoming = &current_connections->n_edges_incoming;
        	int *n_edges_outgoing = &current_connections->n_edges_outgoing;
		DPRINTF("%s(): Node %s, connections in: %d, connections out: %d.",__func__, chosen_mb->node_array[i].name, *n_edges_incoming, *n_edges_outgoing);

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

/**
 * This function implements the algorithm that tries to satisfy reported
 * I/O constraints of tools on an sgsh graph.
 */
static enum op_result
cross_match_constraints(void)
{
	int i;
	int n_nodes = chosen_mb->n_nodes;
	int n_edges = chosen_mb->n_edges;
	int edges_matched = 0;
	struct sgsh_node_connections *graph_solution =
					chosen_mb->graph_solution;

	/* Check constraints for each node on the sgsh graph. */
	for (i = 0; i < n_nodes; i++) {
		struct sgsh_node_connections *current_connections =
							&graph_solution[i];
		/* Hack: struct sgsh_edge* -> struct_sgsh_edge** */
		struct sgsh_edge **edges_incoming =
		       (struct sgsh_edge **)current_connections->edges_incoming;
		/* Hack: struct sgsh_edge* -> struct_sgsh_edge** */
		struct sgsh_edge **edges_outgoing =
		       (struct sgsh_edge **)current_connections->edges_outgoing;
		struct sgsh_node *current_node = &chosen_mb->node_array[i];
		int out_constraint = current_node->provides_channels;
		int in_constraint = current_node->requires_channels;
        	int *n_edges_incoming = &current_connections->n_edges_incoming;
        	int *n_edges_outgoing = &current_connections->n_edges_outgoing;
		DPRINTF("%s(): node %s, index %d, channels required %d, channels_provided %d, sgsh_in %d, sgsh_out %d.", __func__, current_node->name, current_node->index, in_constraint, out_constraint, current_node->sgsh_in, current_node->sgsh_out);

		/* Try to satisfy the I/O channel constraints at graph level.
		 * Assign instances to each edge.
		 */
		if (*n_edges_outgoing > 0)
			if (cross_match_io_constraints(
		    	    &current_connections->n_instances_outgoing_free,
			    out_constraint,
		    	    edges_outgoing, *n_edges_outgoing, 0,
			    &edges_matched) == OP_ERROR) {
			DPRINTF("Failed to satisfy requirements for tool %s, pid %d: requires %d and gets %d, provides %d and is offered %d.\n",
				current_node->name,
				current_node->pid,
				current_node->requires_channels,
				*n_edges_incoming,
				current_node->provides_channels,
				*n_edges_outgoing);
				return OP_ERROR;
		}
		if (*n_edges_incoming > 0)
			if (cross_match_io_constraints(
		    	    &current_connections->n_instances_incoming_free,
			    in_constraint,
		    	    edges_incoming, *n_edges_incoming, 1,
			    &edges_matched) == OP_ERROR) {
			DPRINTF("Failed to satisfy requirements for tool %s, pid %d: requires %d and gets %d, provides %d and is offered %d.\n",
				current_node->name,
				current_node->pid,
				current_node->requires_channels,
				*n_edges_incoming,
				current_node->provides_channels,
				*n_edges_outgoing);
				return OP_ERROR;
		}
	}
	DPRINTF("%s(): Cross matched constraints for %d edges out of %d edges.", __func__, edges_matched / 2, n_edges);
	return (edges_matched / 2 == n_edges ? OP_SUCCESS : OP_RETRY);
}


/**
 * This function implements the algorithm that tries to satisfy reported
 * I/O constraints of tools on an sgsh graph.
 */
static enum op_result
node_match_constraints(void)
{
	int i;
	int n_nodes = chosen_mb->n_nodes;
	enum op_result exit_state = OP_SUCCESS;
	int graph_solution_size = sizeof(struct sgsh_node_connections) *
								n_nodes;
	/* Prealloc */
	chosen_mb->graph_solution = (struct sgsh_node_connections *)malloc(
							graph_solution_size);
	struct sgsh_node_connections *graph_solution =
					chosen_mb->graph_solution;
	if (!graph_solution) {
		DPRINTF("Failed to allocate memory of size %d for sgsh negotiation graph solution structure.\n", graph_solution_size);
		return OP_ERROR;
	}

	/* Check constraints for each node on the sgsh graph. */
	for (i = 0; i < n_nodes; i++) {
		DPRINTF("%s(): node at index %d.", __func__, i);
		struct sgsh_node_connections *current_connections =
							&graph_solution[i];
		memset(current_connections, 0,
					sizeof(struct sgsh_node_connections));
		struct sgsh_edge **edges_incoming;
		struct sgsh_edge **edges_outgoing;
		struct sgsh_node *current_node = &chosen_mb->node_array[i];
		current_connections->node_index = current_node->index;
		DPRINTF("Node %s, index %d, channels required %d, channels_provided %d, sgsh_in %d, sgsh_out %d.", current_node->name, current_node->index, current_node->requires_channels, current_node->provides_channels, current_node->sgsh_in, current_node->sgsh_out);

		/* Find and store pointers to node's at node_index edges.
		 * Try to satisfy the I/O channel constraints at node level.
		 */
		if (dry_match_io_constraints(current_node, current_connections,
			&edges_incoming, &edges_outgoing) == OP_ERROR) {
			DPRINTF("Failed to satisfy requirements for tool %s, pid %d: requires %d and gets %d, provides %d and is offered %d.\n",
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
			(struct sgsh_edge *)edges_incoming;
		current_connections->edges_outgoing =
			(struct sgsh_edge *)edges_outgoing;
	}
	return exit_state;
}


/**
 * This function implements the algorithm that tries to satisfy reported
 * I/O constraints of tools on an sgsh graph.
 */
static enum op_result
solve_sgsh_graph(void)
{
	enum op_result exit_state = OP_SUCCESS;

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
	while (exit_state == OP_RETRY)
		if ((exit_state = cross_match_constraints()) == OP_ERROR)
			goto exit;


	/**
	 * Substitute pointers to edges with proper edge structures
	 * (copies) to facilitate transmission and receipt in one piece.
	 */
	if ((exit_state = prepare_solution()) == OP_ERROR)
		goto exit;

exit:
	if (exit_state == OP_ERROR)
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

	*n_input_fds = self_pipe_fds.n_input_fds;
	assert(*n_input_fds >= 0);
	if (*n_input_fds > 0)
		*input_fds = self_pipe_fds.input_fds;

	*n_output_fds = self_pipe_fds.n_output_fds;
	assert(*n_output_fds >= 0);
	if (*n_output_fds > 0)
		*output_fds = self_pipe_fds.output_fds;

	DPRINTF("%s(): %s. input fds: %d, output fds: %d", __func__, (re == OP_SUCCESS ? "successful" : "failed"), *n_input_fds, *n_output_fds);

	return re;
}

/* Transmit file descriptors that will pipe this
 * tool's output to another tool.
 */
static enum op_result
write_output_fds(int output_socket, int *output_fds)
{
	/**
	 * A node's connections are located at the same position
         * as the node in the node array.
	 */
	struct sgsh_node_connections *graph_solution =
					chosen_mb->graph_solution;
	struct sgsh_node_connections *this_nc =
					&graph_solution[self_node.index];
	DPRINTF("%s(): for node at index %d with %d outgoing edges.", __func__,
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
	 * set up by the shell to support the sgsh negotiation phase.
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
				exit(1);
			}
			DPRINTF("%s(): created pipe pair %d - %d. Transmitting fd %d through sendmsg().", __func__, fd[0], fd[1], fd[0]);

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
		DPRINTF("%s(): OP_ERROR. Aborting.", __func__);
		free_graph_solution(chosen_mb->n_nodes - 1);
		free(self_pipe_fds.output_fds);
	}
	return re;
}

/* Transmit sgsh negotiation graph solution to the next tool on the graph. */
static enum op_result
write_graph_solution(int write_fd)
{
	int i;
	int buf_size = getpagesize();
	char buf[buf_size];
	int n_nodes = chosen_mb->n_nodes;
	int graph_solution_size = sizeof(struct sgsh_node_connections) *
								n_nodes;
	struct sgsh_node_connections *graph_solution =
					chosen_mb->graph_solution;
	int wsize = -1;
	if (graph_solution_size > buf_size) {
		DPRINTF("Sgsh negotiation graph solution of size %d does not fit to buffer of size %d.\n", graph_solution_size, buf_size);
		return OP_ERROR;
	}

	/* Transmit node connection structures. */
	memcpy(buf, graph_solution, graph_solution_size);
	wsize = write(write_fd, buf, graph_solution_size);
	if (wsize == -1)
		return OP_ERROR;
	DPRINTF("%s(): Wrote graph solution of size %d bytes ", __func__, wsize);

	/* We haven't invalidated pointers to arrays of node indices. */

	for (i = 0; i < n_nodes; i++) {
		struct sgsh_node_connections *nc = &graph_solution[i];
		int in_edges_size = sizeof(struct sgsh_edge) * nc->n_edges_incoming;
		int out_edges_size = sizeof(struct sgsh_edge) * nc->n_edges_outgoing;
		if (in_edges_size > buf_size || out_edges_size > buf_size) {
			DPRINTF("Sgsh negotiation graph solution for node at index %d: incoming connections of size %d or outgoing connections of size %d do not fit to buffer of size %d.\n", nc->node_index, in_edges_size, out_edges_size, buf_size);
			return OP_ERROR;
		}

		if (nc->n_edges_incoming) {
			/* Transmit a node's incoming connections. */
			memcpy(buf, nc->edges_incoming, in_edges_size);
			wsize = write(write_fd, buf, in_edges_size);
			if (wsize == -1)
				return OP_ERROR;
			DPRINTF("%s(): Wrote node's %d %d incoming edges of size %d bytes ", __func__, nc->node_index, nc->n_edges_incoming, wsize);
		}

		if (nc->n_edges_outgoing) {
			/* Transmit a node's outgoing connections. */
			memcpy(buf, nc->edges_outgoing, out_edges_size);
			wsize = write(write_fd, buf, out_edges_size);
			if (wsize == -1)
				return OP_ERROR;
			DPRINTF("%s(): Wrote node's %d %d outgoing edges of size %d bytes ", __func__, nc->node_index, nc->n_edges_outgoing, wsize);
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
	assert(self_node_io_side.index >= 0); /* Node is added to the graph. */
	chosen_mb->origin_fd_direction = self_node_io_side.fd_direction;
	DPRINTF("%s(): message block origin set to %d and writing on the %s side", __func__, chosen_mb->origin_index,
	(chosen_mb->origin_fd_direction == 0) ? "input" : "output");
}

/*
 * Write the chosen_mb message block to the specified file descriptor.
 */
enum op_result
write_message_block(int write_fd)
{
	int wsize = -1;
	int buf_size = getpagesize(); /* Make buffer page-wide. */
	int mb_size = sizeof(struct sgsh_negotiation);
	int nodes_size = chosen_mb->n_nodes * sizeof(struct sgsh_node);
	int edges_size = chosen_mb->n_edges * sizeof(struct sgsh_edge);
	struct sgsh_node *p_nodes = chosen_mb->node_array;

	if (nodes_size > buf_size || edges_size > buf_size) {
		DPRINTF("%s size exceeds buffer size.\n",
			(nodes_size > buf_size) ? "Nodes" : "Edges");
		return OP_ERROR;
	}

	/**
	 * Prepare and perform message block transmission.
	 * Formally invalidate pointers to nodes and edges
	 * to avoid accidents on the receiver's side.
	 */
	chosen_mb->node_array = NULL;
	DPRINTF("%s(): Write message block.", __func__);
	wsize = write(write_fd, chosen_mb, mb_size);
	if (wsize == -1)
		return OP_ERROR;
	DPRINTF("%s(): Wrote message block of size %d bytes ", __func__, wsize);


	if (chosen_mb->state == PS_NEGOTIATION) {
		/* Transmit nodes. */
		wsize = write(write_fd, p_nodes, nodes_size);
		if (wsize == -1)
			return OP_ERROR;
		DPRINTF("%s(): Wrote nodes of size %d bytes ", __func__, wsize);

		chosen_mb->node_array = p_nodes; // Reinstate pointers to nodes.

		if (chosen_mb->n_nodes > 1) {
			/* Transmit edges. */
			struct sgsh_edge *p_edges = chosen_mb->edge_array;
			chosen_mb->edge_array = NULL;
			wsize = write(write_fd, p_edges, edges_size);
			if (wsize == -1)
				return OP_ERROR;
			DPRINTF("%s(): Wrote edges of size %d bytes ", __func__, wsize);

			chosen_mb->edge_array = p_edges; /* Reinstate edges. */
		}
	} else if (chosen_mb->state == PS_RUN) {
		/* Transmit solution. */
		if (write_graph_solution(write_fd) == OP_ERROR)
			return OP_ERROR;
	}

	DPRINTF("%s(): Shipped message block or solution to next node in graph from file descriptor: %d.\n", __func__, write_fd);
	return OP_SUCCESS;
}

/* If negotiation is still going, Check whether it should end. */
static void
check_negotiation_round(int serialno_ntimes_same)
{
	if (chosen_mb->state == PS_NEGOTIATION) {
		if (serialno_ntimes_same == 3) {
			chosen_mb->state = PS_NEGOTIATION_END;
			chosen_mb->serial_no++;
			mb_is_updated = true;
			DPRINTF("%s(): ***Negotiation protocol state change: end of negotiation phase.***\n", __func__);
		}
	}
}

/* Reallocate message block to fit new node coming in. */
static enum op_result
add_node(void)
{
	int n_nodes = chosen_mb->n_nodes;
	void *p = realloc(chosen_mb->node_array,
		sizeof(struct sgsh_node) * (n_nodes + 1));
	if (!p) {
		DPRINTF("Node array expansion for adding a new node failed.\n");
		return OP_ERROR;
	} else {
		chosen_mb->node_array = (struct sgsh_node *)p;
		self_node.index = n_nodes;
		memcpy(&chosen_mb->node_array[n_nodes], &self_node,
					sizeof(struct sgsh_node));
		self_node_io_side.index = n_nodes;
		DPRINTF("%s(): Added node %s in position %d on sgsh graph.\n",
				__func__, self_node.name, self_node_io_side.index);
		chosen_mb->n_nodes++;
	}
	return OP_SUCCESS;
}

/* Lookup an edge in the sgsh graph. */
static enum op_result
lookup_sgsh_edge(struct sgsh_edge *e)
{
	int i;
	for (i = 0; i < chosen_mb->n_edges; i++) {
		if ((chosen_mb->edge_array[i].from == e->from &&
			chosen_mb->edge_array[i].to == e->to) ||
		    (chosen_mb->edge_array[i].from == e->to &&
		     chosen_mb->edge_array[i].to == e->from)) {
			DPRINTF("%s(): Edge %d to %d exists.", __func__,
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
fill_sgsh_edge(struct sgsh_edge *e)
{
	int i;
	int n_nodes = chosen_mb->n_nodes;
	for (i = 0; i < n_nodes; i++) /* Check dispatcher node exists. */
		if (i == chosen_mb->origin_index)
			break;
	if (i == n_nodes) {
		DPRINTF("Dispatcher node with index position %d not present in graph.\n", chosen_mb->origin_index);
		return OP_ERROR;
	}
	if (chosen_mb->origin_fd_direction == STDIN_FILENO) {
	/**
         * MB sent from stdin, so dispatcher is the destination of the edge.
	 * Self should be sgsh-active on output side. Self's current fd is stdin
	 * if self is sgsh-active on input side or output side otherwise.
	 * Self (the recipient) is the source of the edge.
         */
		e->to = chosen_mb->origin_index;
		if (self_node.sgsh_in == 1)
			self_node_io_side.fd_direction = STDIN_FILENO;
		else
			self_node_io_side.fd_direction = STDOUT_FILENO;
		assert((self_node.sgsh_in &&
			self_node_io_side.fd_direction == STDIN_FILENO) ||
			self_node_io_side.fd_direction == STDOUT_FILENO);
		e->from = self_node_io_side.index;
	} else if (chosen_mb->origin_fd_direction == STDOUT_FILENO) {
		/* Similarly. */
		e->from = chosen_mb->origin_index;
		if (self_node.sgsh_out == 1)
			self_node_io_side.fd_direction = STDOUT_FILENO;
		else
			self_node_io_side.fd_direction = STDIN_FILENO;
		assert((self_node.sgsh_out &&
			self_node_io_side.fd_direction == STDOUT_FILENO) ||
			self_node_io_side.fd_direction == STDIN_FILENO);
		e->to = self_node_io_side.index;
	}
	e->instances = 0;
	e->from_instances = 0;
	e->to_instances = 0;
        DPRINTF("New sgsh edge from %d to %d with %d instances.", e->from, e->to, e->instances);
	return OP_SUCCESS;
}

/* Add new edge coming in. */
static enum op_result
add_edge(struct sgsh_edge *edge)
{
	int n_edges = chosen_mb->n_edges;
	void *p = realloc(chosen_mb->edge_array,
			sizeof(struct sgsh_edge) * (n_edges + 1));
	if (!p) {
		DPRINTF("Edge array expansion for adding a new edge failed.\n");
		return OP_ERROR;
	} else {
		chosen_mb->edge_array = (struct sgsh_edge *)p;
		memcpy(&chosen_mb->edge_array[n_edges], edge,
						sizeof(struct sgsh_edge));
		DPRINTF("Added edge (%d -> %d) in sgsh graph.\n",
					edge->from, edge->to);
		chosen_mb->n_edges++;
	}
	return OP_SUCCESS;
}

/* Try to add a newly occured edge in the sgsh graph. */
static enum op_result
try_add_sgsh_edge(void)
{
	if (chosen_mb->origin_index >= 0) { /* If MB not created just now: */
		struct sgsh_edge new_edge;
		fill_sgsh_edge(&new_edge);
		if (lookup_sgsh_edge(&new_edge) == OP_CREATE) {
			if (add_edge(&new_edge) == OP_ERROR)
				return OP_ERROR;
			DPRINTF("Sgsh graph now has %d edges.\n",
							chosen_mb->n_edges);
			chosen_mb->serial_no++; /* Message block updated. */
			mb_is_updated = true;
			return OP_SUCCESS;
		}
		return OP_EXISTS;
	}
	return OP_NOOP;
}

/**
 * Add node to message block. Copy the node using offset-based
 * calculation from the start of the array of nodes.
 */
static enum op_result
try_add_sgsh_node(void)
{
	int n_nodes = chosen_mb->n_nodes;
	int i;
	for (i = 0; i < n_nodes; i++)
		if (chosen_mb->node_array[i].pid == self_node.pid)
			break;
	if (i == n_nodes) {
		if (add_node() == OP_ERROR)
			return OP_ERROR;
		DPRINTF("Sgsh graph now has %d nodes.\n", chosen_mb->n_nodes);
		chosen_mb->serial_no++;
		mb_is_updated = true;
		return OP_SUCCESS;
	}
	return OP_EXISTS;
}

/* A constructor-like function for struct sgsh_node. */
static void
fill_sgsh_node(const char *tool_name, pid_t pid, int requires_channels,
						int provides_channels)
{
	self_node.pid = pid;
	memcpy(self_node.name, tool_name, strlen(tool_name) + 1);
	self_node.requires_channels = requires_channels;
	self_node.provides_channels = provides_channels;
	self_node.index = -1; /* Will be filled in when added to the graph. */
	DPRINTF("Sgsh node for tool %s with pid %d created.\n", tool_name, pid);
}

/* Deallocate message block together with nodes and edges. */
void
free_mb(struct sgsh_negotiation *mb)
{
	if (mb->graph_solution)
		free_graph_solution(mb->n_nodes - 1);
	if (mb->node_array)
		free(mb->node_array);
	if (mb->edge_array)
		free(mb->edge_array);
	free(mb);
	DPRINTF("%s(): Freed message block.", __func__);
}

/**
 * Check if the arrived message block preexists our chosen one
 * and substitute the chosen if so.
 * If the arrived message block is younger discard it and don't
 * forward it.
 * If the arrived is the chosen, try to add the edge.
 */
static enum op_result
analyse_read(struct sgsh_negotiation *fresh_mb,
			bool *should_transmit_mb,
			int *serialno_ntimes_same,
			int *run_ntimes_same,
			int *error_ntimes_same)
{
        *should_transmit_mb = false; /* Default value. */
	mb_is_updated = false; /* Default value. */

	if (fresh_mb->state == PS_ERROR) {
		(*error_ntimes_same)++;
		free_mb(chosen_mb);
		chosen_mb = fresh_mb;
		/* Records whether process is terminal or non-terminal */
		int n_io_channels = self_node.sgsh_in + self_node.sgsh_out;
		/* The second time terminals processes
		 * and first time non-terminal processes see the error
		 * flag in the message block is time to exit.
		 * All processes but the preceding process to the one
		 * found the solution (if we were in the run phase when
		 * the error stroke) pass the block before exiting.
		 */
		DPRINTF("%s(): preceding pid: %d, self pid: %d", __func__, chosen_mb->preceding_process_pid, self_node.pid);
		if ((n_io_channels == *error_ntimes_same &&
					chosen_mb->preceding_process_pid !=
					self_node.pid) ||
				(n_io_channels != *error_ntimes_same))
			*should_transmit_mb = true;
	} else if (fresh_mb->state == PS_RUN) {
		(*run_ntimes_same)++;
		free_mb(chosen_mb);
		chosen_mb = fresh_mb;
		/* Records whether process is terminal or non-terminal */
		int n_io_channels = self_node.sgsh_in + self_node.sgsh_out;
		/* The second time terminals processes
		 * and first time non-terminal processes see the run
		 * flag in the message block is time to exit.
		 * All processes but the preceding process to the one
		 * found the solution pass the block before exiting.
		 */
		if ((n_io_channels == *run_ntimes_same &&
					chosen_mb->preceding_process_pid !=
					self_node.pid) ||
				(n_io_channels != *run_ntimes_same))
			*should_transmit_mb = true;
	} else if (fresh_mb->state == PS_NEGOTIATION) {
		/* New chosen message block */
        	if (fresh_mb->initiator_pid < chosen_mb->initiator_pid) {
			free_mb(chosen_mb);
			chosen_mb = fresh_mb;
			mb_is_updated = true; /*Substituting chosen_mb is an update.*/
			if (try_add_sgsh_node() == OP_ERROR)
				return OP_ERROR; /* Double realloc: one for node, */
			if (try_add_sgsh_edge() == OP_ERROR)
				return OP_ERROR; /* one for edge. */
			*should_transmit_mb = true;
		} else if (fresh_mb->initiator_pid > chosen_mb->initiator_pid)
			free_mb(fresh_mb); /* Discard MB just read. */
		else {
                	*should_transmit_mb = true;
			DPRINTF("%s(): Fresh vs chosen message block: same initiator pid.", __func__);
			if (fresh_mb->serial_no > chosen_mb->serial_no) {
				DPRINTF("%s(): Fresh vs chosen message block: serial no updated.", __func__);
				free_mb(chosen_mb);
				chosen_mb = fresh_mb;
				mb_is_updated = true;
				if (try_add_sgsh_edge() == OP_ERROR)
					return OP_ERROR;
			} else { /* serial_no of the mb has not changed
			 	  * in the interim, but we accept it to
			  	  * receive the id of the dispatcher.
			  	  */
				DPRINTF("Fresh vs chosen message block: serial no not updated.");
				(*serialno_ntimes_same)++;
				free_mb(chosen_mb);
				chosen_mb = fresh_mb;
			}
		}
	}
	return OP_SUCCESS;
}

static enum op_result
check_read(int bytes_read, int buf_size, int expected_read_size) {
	if (bytes_read != expected_read_size) {
		DPRINTF("Read %d bytes of message block, expected to read %d.\n",
			bytes_read, expected_read_size);
		return OP_ERROR;
	}
	if (bytes_read > buf_size) {
		DPRINTF("Read %d bytes of message block, but buffer can hold up to %d.", bytes_read, buf_size);
		return OP_ERROR;
	}
	return OP_SUCCESS;
}

/* Allocate memory for graph solution and copy from buffer. */
static enum op_result
alloc_copy_graph_solution(struct sgsh_negotiation *mb, char *buf, int bytes_read,
								int buf_size)
{
	int expected_read_size = sizeof(struct sgsh_node_connections) *
								mb->n_nodes;
	if (check_read(bytes_read, buf_size, expected_read_size) == OP_ERROR)
		return OP_ERROR;
	mb->graph_solution = (struct sgsh_node_connections *)malloc(bytes_read);
	memcpy(mb->graph_solution, buf, bytes_read);
	return OP_SUCCESS;
}

/* Allocate memory for message_block edges and copy from buffer. */
static enum op_result
alloc_copy_edges(struct sgsh_negotiation *mb, char *buf, int bytes_read,
								int buf_size)
{
	int expected_read_size = sizeof(struct sgsh_edge) * mb->n_edges;
	if (check_read(bytes_read, buf_size, expected_read_size) == OP_ERROR)
		return OP_ERROR;
	mb->edge_array = (struct sgsh_edge *)malloc(bytes_read);
	memcpy(mb->edge_array, buf, bytes_read);
	return OP_SUCCESS;
}

/* Allocate memory for message_block nodes and copy from buffer. */
static enum op_result
alloc_copy_nodes(struct sgsh_negotiation *mb, char *buf, int bytes_read,
								int buf_size)
{
	int expected_read_size = sizeof(struct sgsh_node) * mb->n_nodes;
	if (check_read(bytes_read, buf_size, expected_read_size) == OP_ERROR)
		return OP_ERROR;
	mb->node_array = (struct sgsh_node *)malloc(bytes_read);
	memcpy(mb->node_array, buf, bytes_read);
	DPRINTF("%s(): Node array recovered.", __func__);
	return OP_SUCCESS;
}

/* Allocate memory for core message_block and copy from buffer. */
static enum op_result
alloc_copy_mb(struct sgsh_negotiation **mb, char *buf, int bytes_read,
							int buf_size)
{
	int expected_read_size = sizeof(struct sgsh_negotiation);
	if (check_read(bytes_read, buf_size, expected_read_size) == OP_ERROR)
		return OP_ERROR;
	*mb = (struct sgsh_negotiation *)malloc(bytes_read);
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
	DPRINTF("Try read from fd %d.", fd);
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
read_chunk(int read_fd, char *buf, int buf_size, int *bytes_read)
{
	int error_code = -EAGAIN;
	DPRINTF("%s()", __func__);

	call_read(read_fd, buf, buf_size, bytes_read, &error_code);
	if (*bytes_read == -1) {  /* Read failed. */
	 	DPRINTF("Reading from fd %d failed with error code %d.",
			read_fd, error_code);
		return error_code;
	} else  /* Read succeeded. */
		DPRINTF("Read succeeded: %d bytes read from %d.\n",
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
	struct sgsh_node_connections *this_nc =
				&chosen_mb->graph_solution[self_node.index];
	DPRINTF("%s(): self node: %d, incoming edges: %d, outgoing edges: %d", __func__, self_node.index, this_nc->n_edges_incoming, this_nc->n_edges_outgoing);

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
get_origin_pid(struct sgsh_negotiation *mb)
{
	return mb->node_array[mb->origin_index].pid;
}

/* Return the number of input file descriptors
 * expected by process with pid PID.
 */
int
get_expected_fds_n(pid_t pid)
{
	int expected_fds_n = 0;
	int i = 0, j = 0;
	for (i = 0; i < chosen_mb->n_nodes; i++) {
		if (chosen_mb->node_array[i].pid == pid) {
			struct sgsh_node_connections *graph_solution =
				chosen_mb->graph_solution;
			for (j = 0; j < graph_solution[i].n_edges_incoming; j++)
				expected_fds_n +=
					graph_solution[i].edges_incoming[j].instances;
			return expected_fds_n;
		}
	}
	/* Invalid pid */
	return -1;
}

/* Return the number of output file descriptors
 * provided by process with pid PID.
 */
int
get_provided_fds_n(pid_t pid)
{
	int provided_fds_n = 0;
	int i = 0, j = 0;
	for (i = 0; i < chosen_mb->n_nodes; i++) {
		if (chosen_mb->node_array[i].pid == pid) {
			struct sgsh_node_connections *graph_solution =
				chosen_mb->graph_solution;
			for (j = 0; j < graph_solution[i].n_edges_outgoing; j++)
				provided_fds_n +=
					graph_solution[i].edges_outgoing[j].instances;
			return provided_fds_n;
		}
	}
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

/* Read file descriptors piping input from another tool in the sgsh graph. */
static enum op_result
read_input_fds(int input_socket, int *input_fds)
{
	/**
	 * A node's connections are located at the same position
         * as the node in the node array.
	 */
	struct sgsh_node_connections *graph_solution =
					chosen_mb->graph_solution;
	struct sgsh_node_connections *this_nc =
					&graph_solution[self_node.index];
	assert(this_nc->node_index == self_node.index);
	int i;
	int total_edge_instances = 0;
	enum op_result re = OP_SUCCESS;

	DPRINTF("%s(): %d incoming edges to inspect of node %d.", __func__,
			this_nc->n_edges_incoming, self_node.index);
	for (i = 0; i < this_nc->n_edges_incoming; i++) {
		int k;
		/**
		 * Due to channel constraint flexibility,
		 * each edge can have more than one instances.
		 */
		for (k = 0; k < this_nc->edges_incoming[i].instances; k++) {
			input_fds[total_edge_instances] = read_fd(input_socket);
			DPRINTF("%s: Node %d received file descriptor %d.",
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

/* Try read solution to the sgsh negotiation graph. */
static enum op_result
read_graph_solution(int read_fd, struct sgsh_negotiation *fresh_mb)
{
	int i;
	int buf_size = getpagesize();		/* Make buffer page-wide. */
	char buf[buf_size];
	int bytes_read = 0;
	int n_nodes = fresh_mb->n_nodes;
	enum op_result error_code = OP_SUCCESS;

	/* Read node connection structures of the solution. */
	if ((error_code = read_chunk(read_fd, buf, buf_size, &bytes_read))
			!= OP_SUCCESS)
		return error_code;
	if (alloc_copy_graph_solution(fresh_mb, buf, bytes_read, buf_size) ==
									OP_ERROR)
		return error_code;

	struct sgsh_node_connections *graph_solution =
					fresh_mb->graph_solution;
	for (i = 0; i < n_nodes; i++) {
		struct sgsh_node_connections *nc = &graph_solution[i];
		DPRINTF("Node %d with %d incoming edges at %lx and %d outgoing edges at %lx.", nc->node_index, nc->n_edges_incoming, (long)nc->edges_incoming, nc->n_edges_outgoing, (long)nc->edges_outgoing);
		int in_edges_size = sizeof(struct sgsh_edge) * nc->n_edges_incoming;
		int out_edges_size = sizeof(struct sgsh_edge) * nc->n_edges_outgoing;
		if (in_edges_size > buf_size || out_edges_size > buf_size) {
			DPRINTF("Sgsh negotiation graph solution for node at index %d: incoming connections of size %d or outgoing connections of size %d do not fit to buffer of size %d.\n", nc->node_index, in_edges_size, out_edges_size, buf_size);
			return OP_ERROR;
		}

		/* Read a node's incoming connections. */
		if (nc->n_edges_incoming > 0) {
			if ((error_code = read_chunk(read_fd, buf, buf_size,
				&bytes_read)) != OP_SUCCESS)
				return error_code;
			if (in_edges_size != bytes_read) {
				DPRINTF("%s(): Expected %d bytes, got %d.", __func__,
						in_edges_size, bytes_read);
				return OP_ERROR;
			}
			if (alloc_node_connections(&nc->edges_incoming,
				nc->n_edges_incoming, 0, i) == OP_ERROR)
				return OP_ERROR;
			memcpy(nc->edges_incoming, buf, in_edges_size);
		}

		/* Read a node's outgoing connections. */
		if (nc->n_edges_outgoing) {
			if ((error_code = read_chunk(read_fd, buf, buf_size,
				&bytes_read)) != OP_SUCCESS)
				return error_code;
			if (out_edges_size != bytes_read) {
				DPRINTF("%s(): Expected %d bytes, got %d.", __func__,
						out_edges_size, bytes_read);
				return OP_ERROR;
			}
			if (alloc_node_connections(&nc->edges_outgoing,
				nc->n_edges_outgoing, 1, i) == OP_ERROR)
				return OP_ERROR;
			memcpy(nc->edges_outgoing, buf, out_edges_size);
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
read_message_block(int read_fd, struct sgsh_negotiation **fresh_mb)
{
	int buf_size = getpagesize();		/* Make buffer page-wide. */
	char buf[buf_size];
	int bytes_read = 0;
	enum op_result error_code = 0;

	memset(buf, 0, buf_size);

	/* Try read core message block: struct negotiation state fields. */
	if ((error_code = read_chunk(read_fd, buf, buf_size, &bytes_read))
			!= OP_SUCCESS)
		return error_code;
	if (alloc_copy_mb(fresh_mb, buf, bytes_read, buf_size) == OP_ERROR)
		return OP_ERROR;

	if ((*fresh_mb)->state == PS_NEGOTIATION) {
		DPRINTF("%s(): Read negotiation graph nodes.", __func__);
		if ((error_code = read_chunk(read_fd, buf, buf_size,
				&bytes_read)) != OP_SUCCESS)
			return error_code;
		if (alloc_copy_nodes(*fresh_mb, buf, bytes_read, buf_size)
								== OP_ERROR)
		return OP_ERROR;

        	if ((*fresh_mb)->n_nodes > 1) {
			DPRINTF("%s(): Read negotiation graph edges.",__func__);
			if ((error_code = read_chunk(read_fd, buf, buf_size,
			     &bytes_read)) != OP_SUCCESS)
				return error_code;
			if (alloc_copy_edges(*fresh_mb, buf, bytes_read,
			    buf_size) == OP_ERROR)
				return OP_ERROR;
		}
	} else if ((*fresh_mb)->state == PS_RUN) {
		/**
		 * Try read solution.
		 * fresh_mb should be an updated version of the chosen_mb
		 * or even the same structure because this is the phase
		 * where we share the solution across the sgsh graph.
                 */
		if (read_graph_solution(read_fd, *fresh_mb) == OP_ERROR)
			return OP_ERROR;
	}
	DPRINTF("%s(): Read message block or solution from previous node in graph from file descriptor: %s.\n", __func__, ((*fresh_mb)->origin_fd_direction) ? "stdout" : "stdin");
	return OP_SUCCESS;
}

/* Construct a message block to use as a vehicle for the negotiation phase. */
static enum op_result
construct_message_block(const char *tool_name, pid_t self_pid)
{
	int memory_allocation_size = sizeof(struct sgsh_negotiation);
	chosen_mb = (struct sgsh_negotiation *)malloc(
				memory_allocation_size);
	if (!chosen_mb) {
		DPRINTF("Memory allocation of message block failed.");
		return OP_ERROR;
	}
	chosen_mb->version = 1;
	chosen_mb->node_array = NULL;
	chosen_mb->n_nodes = 0;
	chosen_mb->edge_array = NULL;
	chosen_mb->n_edges = 0;
	chosen_mb->initiator_pid = self_pid;
	chosen_mb->preceding_process_pid = -1;
	chosen_mb->state = PS_NEGOTIATION;
	chosen_mb->serial_no = 0;
	chosen_mb->origin_index = -1;
	chosen_mb->origin_fd_direction = -1;
	chosen_mb->graph_solution = NULL;
	DPRINTF("Message block created by process %s with pid %d.\n",
						tool_name, (int)self_pid);
	return OP_SUCCESS;
}

/* Get environment variable env_var. */
static enum op_result
get_env_var(const char *env_var,int *value)
{
	char *string_value = getenv(env_var);
	if (!string_value) {
		DPRINTF("Getting environment variable %s failed.\n", env_var);
		return OP_ERROR;
	} else
		DPRINTF("getenv() returned string value %s.\n", string_value);
	*value = atoi(string_value);
	DPRINTF("Integer form of value is %d.\n", *value);
	return OP_SUCCESS;
}

/**
 * Get environment variables SGSH_IN, SGSH_OUT set up by
 * the shell (through execvpe()).
 */
static enum op_result
get_environment_vars(void)
{
	DPRINTF("Try to get environment variable SGSH_IN.");
	if (get_env_var("SGSH_IN", &self_node.sgsh_in) == OP_ERROR)
		return OP_ERROR;
	DPRINTF("Try to get environment variable SGSH_OUT.");
	if (get_env_var("SGSH_OUT", &self_node.sgsh_out) == OP_ERROR)
		return OP_ERROR;
	return OP_SUCCESS;
}

/**
 * Verify tool's I/O channel requirements are sane.
 * We might need some upper barrier for requirements too,
 * such as, not more than 100 or 1000.
 */
STATIC enum op_result
validate_input(int channels_required, int channels_provided, const char *tool_name)
{
	if (!tool_name) {
		DPRINTF("NULL pointer provided as tool name.\n");
		return OP_ERROR;
	}
	if (channels_required < -1 || channels_provided < -1) {
		DPRINTF("I/O requirements entered for tool %s are less than -1. \nChannels required %d \nChannels provided: %d",
			tool_name, channels_required, channels_provided);
		return OP_ERROR;
	}
	if (channels_required == 0 && channels_provided == 0) {
		DPRINTF("I/O requirements entered for tool %s are zero. \nChannels required %d \nChannels provided: %d",
			tool_name, channels_required, channels_provided);
		return OP_ERROR;
	}
	if (channels_required > 1000 || channels_provided > 1000) {
		DPRINTF("I/O requirements entered for tool %s are too high (> 1000). \nChannels required %d \nChannels provided: %d",
			tool_name, channels_required, channels_provided);
		return OP_ERROR;
	}
	return OP_SUCCESS;
}


/**
 * Count times we read a message block in error state
 * and return whether we should continue transmitting it or
 * exit the negotiation.
 * The decision is dependent on whether the node is terminal
 * or not. Terminal nodes (those that only take input or only
 * provide output) need get the message block with the error
 * flag set once. After they pass it along they exit.
 * Non-terminal ones pass the block and exit the second time
 * they receive it in error state.
 */
static bool
leave_negotiation(int run_ntimes_same, int error_ntimes_same)
{
	int n_io_channels = self_node.sgsh_in + self_node.sgsh_out;
	if ((chosen_mb->state == PS_RUN &&
			n_io_channels == run_ntimes_same) ||
			(chosen_mb->state == PS_ERROR &&
			 n_io_channels == error_ntimes_same))
		return true;
	return false;
}

int
set_fds(fd_set *read_fds, fd_set *write_fds)
{
	int nfds = 0;
	if (self_node.sgsh_out) {
		if (read_fds) {
			FD_SET(STDOUT_FILENO, read_fds);
			nfds++;
		}
		if (write_fds) {
			FD_SET(STDOUT_FILENO, write_fds);
			nfds++;
		}
	}
	if (self_node.sgsh_in) {
		if (read_fds) {
			FD_SET(STDIN_FILENO, read_fds);
			nfds++;
		}
		if (write_fds) {
			FD_SET(STDIN_FILENO, write_fds);
			nfds++;
		}
	}
	return nfds == 1 ? nfds + 1 : nfds;
}

/**
 * Each tool in the sgsh graph calls sgsh_negotiate() to take part in
 * peer-to-peer negotiation. A message block (MB) is circulated among tools
 * and is filled with tools' I/O requirements. When all requirements are in
 * place, an algorithm runs that tries to find a solution that satisfies
 * all requirements. If a solution is found, pipes are allocated and
 * set up according to the solution. The appropriate file descriptors
 * are provided to each tool and the negotiation phase ends.
 * The function's return value signifies success or failure of the
 * negotiation phase.
 */
enum prot_state
sgsh_negotiate(const char *tool_name, /* Input. Try remove. */
                    int channels_required, /* How many input channels can take. */
                    int channels_provided, /* How many output channels can
						provide. */
                                     /* Output: to fill. */
                    int **input_fds,  /* Input file descriptors. */
                    int *n_input_fds, /* Number of input file descriptors. */
                    int **output_fds, /* Output file descriptors. */
                    int *n_output_fds) /* Number of output file descriptors. */
{
	int i = 0;
	int serialno_ntimes_same = 0;
	int run_ntimes_same = 0;
	int error_ntimes_same = 0;
	bool should_transmit_mb = true;
	pid_t self_pid = getpid();    /* Get tool's pid */
	struct sgsh_negotiation *fresh_mb = NULL; /* MB just read. */

	int nfds = 0;
	fd_set read_fds, write_fds;
	FD_ZERO(&read_fds);
	FD_ZERO(&write_fds);

#ifndef UNIT_TESTING
	self_pipe_fds.input_fds = NULL;		/* Clean garbage */
	self_pipe_fds.output_fds = NULL;	/* Ditto */
#endif
	DPRINTF("%s(): Tool %s with pid %d entered sgsh negotiation.",
			__func__, tool_name, (int)self_pid);

	if (validate_input(channels_required, channels_provided, tool_name)
							== OP_ERROR)
		return PS_ERROR;

	if (get_environment_vars() == OP_ERROR) {
		DPRINTF("Failed to extract SGSH_IN, SGSH_OUT environment variables.");
		return PS_ERROR;
	}


	/* Start negotiation */
        if (self_node.sgsh_out && !self_node.sgsh_in) {
                if (construct_message_block(tool_name, self_pid) == OP_ERROR)
			chosen_mb->state = PS_ERROR;
                self_node_io_side.fd_direction = STDOUT_FILENO;
        } else { /* or wait to receive MB. */
		chosen_mb = NULL;
		if (read_message_block(STDIN_FILENO, &fresh_mb) == OP_ERROR) {
			chosen_mb->state = PS_ERROR;
			error_ntimes_same++;
		}
		chosen_mb = fresh_mb;
	}
	
	/* Either we just read or we are about to write */
	nfds = set_fds(NULL, &write_fds);

	/* Create sgsh node representation and add node, edge to the graph. */
	fill_sgsh_node(tool_name, self_pid, channels_required,
						channels_provided);
	if (try_add_sgsh_node() == OP_ERROR)
		chosen_mb->state = PS_ERROR;

	if (try_add_sgsh_edge() == OP_ERROR)
		chosen_mb->state = PS_ERROR;

	/* Perform phases and rounds. */
	while (1) {
again:
		if (select(nfds, &read_fds, &write_fds, NULL, NULL) < 0) {
			if (errno == EINTR)
				goto again;
			perror("select");
			chosen_mb->state = PS_ERROR;
		}

		for (i = 0; i < nfds; i++) {
			if (FD_ISSET(i, &write_fds)) {
				DPRINTF("write on fd %d is active.", i);
				/* Decision to drop message block */
				if (should_transmit_mb) {	
					/* Write message block et al. */
					set_dispatcher();
					if (write_message_block(i) == OP_ERROR)
						chosen_mb->state = PS_ERROR;
					should_transmit_mb = false;
				}
				if (leave_negotiation(run_ntimes_same,
							error_ntimes_same)) {
					if (chosen_mb->state == PS_RUN)
						chosen_mb->state = PS_COMPLETE;
					DPRINTF("%s(): leave after write with state %d.", __func__, chosen_mb->state);
					goto exit;
				}
				FD_ZERO(&write_fds);
				nfds = set_fds(&read_fds, NULL);
			}
			if (FD_ISSET(i, &read_fds)) {
				DPRINTF("read on fd %d is active.", i);
				/* Read message block et al. */
				if (read_message_block(i, &fresh_mb)
						== OP_ERROR)
					chosen_mb->state = PS_ERROR;
				/* Accept? message block, check state */
				if (analyse_read(fresh_mb, &should_transmit_mb,
						&serialno_ntimes_same,
						&run_ntimes_same,
						&error_ntimes_same) == OP_ERROR)
					chosen_mb->state = PS_ERROR;

				check_negotiation_round(serialno_ntimes_same);
				/**
				 * If all I/O constraints have been contributed,
				 * try to solve the I/O constraint problem,
				 * then spread the word, and leave negotiation.
				 * Only initiator executes this block; once.
				 */
				if (chosen_mb->state == PS_NEGOTIATION_END) {
					if (solve_sgsh_graph() == OP_ERROR) {
						chosen_mb->state = PS_ERROR;
							fprintf(stderr, "No solution was found to satisfy the I/O requirements of the participating processes.");
					} else {
						chosen_mb->state = PS_RUN;
						run_ntimes_same++;
					}
					chosen_mb->preceding_process_pid =
						chosen_mb->node_array[chosen_mb->origin_index].pid;
				}
				if (leave_negotiation(run_ntimes_same,
							error_ntimes_same) &&
						!should_transmit_mb) {
					if (chosen_mb->state == PS_RUN)
						chosen_mb->state = PS_COMPLETE;
					DPRINTF("%s(): leave after read with state %d.", __func__, chosen_mb->state);
					goto exit;
				}
				FD_ZERO(&read_fds);
				nfds = set_fds(NULL, &write_fds);
			}
		}
	}
exit:
	/* XXX: error handling */
	if (chosen_mb->state == PS_COMPLETE) {
		if (alloc_io_fds() == OP_ERROR)
			chosen_mb->state = PS_ERROR;
		if (read_input_fds(STDIN_FILENO, self_pipe_fds.input_fds) ==
									OP_ERROR)
			chosen_mb->state = PS_ERROR;
		if (write_output_fds(STDOUT_FILENO, self_pipe_fds.output_fds) ==
									OP_ERROR)
			chosen_mb->state = PS_ERROR;
		if (establish_io_connections(input_fds, n_input_fds, output_fds,
						n_output_fds) == OP_ERROR)
			chosen_mb->state = PS_ERROR;
	}
	int state = chosen_mb->state;
	free_mb(chosen_mb);
	return state;
}

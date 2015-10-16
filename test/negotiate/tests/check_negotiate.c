#include <check.h>  /* Check unit test framework API. */
#include <stdlib.h> /* EXIT_SUCCESS, EXIT_FAILURE */
#include <unistd.h> /* pipe() */
#include <fcntl.h>  /* fcntl() */
#include <err.h>    /* err(), errx() */
#include <time.h>   /* nanosleep() */
#include "../src/sgsh-negotiate.h"
#include "../src/negotiate.c" /* struct definitions, static structures */

struct sgsh_negotiation *fresh_mb;
struct sgsh_edge *compact_edges;
struct sgsh_edge **pointers_to_edges;
int n_ptedges;

int *args;

int *input_fds;
int n_input_fds;
int *output_fds;
int n_output_fds;

/* Depending on whether a test triggers a failure or not, a different sequence
 * of actions may be needed to exit normally.
 * exit_state is the control variable for following the correct 
 * sequence of actions.
 */
int exit_state = 0;

void
setup_chosen_mb(void)
{
	struct sgsh_node *nodes;
	struct sgsh_edge *edges;
	int n_nodes;
	int n_edges;
	n_nodes = 4;
        nodes = (struct sgsh_node *)malloc(sizeof(struct sgsh_node) * n_nodes);
        nodes[0].pid = 100;
	nodes[0].index = 0;
        strcpy(nodes[0].name, "proc0");
        nodes[0].requires_channels = 2;
	nodes[0].provides_channels = 1;
	nodes[0].sgsh_in = 1;
        nodes[0].sgsh_out = 1;

        nodes[1].pid = 101;
	nodes[1].index = 1;
        strcpy(nodes[1].name, "proc1");
        nodes[1].requires_channels = 1;
	nodes[1].provides_channels = 2;
	nodes[1].sgsh_in = 1;
        nodes[1].sgsh_out = 1;

	/* sgsh OUT and not IN = initiator node.
         * This node could start the negotiation.
         * Fix.
	 */
        nodes[2].pid = 102;
	nodes[2].index = 2;
        strcpy(nodes[2].name, "proc2");
        nodes[2].requires_channels = 0;
	nodes[2].provides_channels = 2;
	nodes[2].sgsh_in = 0;
        nodes[2].sgsh_out = 1;

	/* sgsh IN and not OUT = termination node.
         * This node couldn't start the negotiation.
         * Fix.
	 */
        nodes[3].pid = 103;
	nodes[3].index = 3;
        strcpy(nodes[3].name, "proc3");
        nodes[3].requires_channels = 2;
	nodes[3].provides_channels = 0;
	nodes[3].sgsh_in = 1;
        nodes[3].sgsh_out = 0;

        n_edges = 5;
        edges = (struct sgsh_edge *)malloc(sizeof(struct sgsh_edge) *n_edges);
        edges[0].from = 2;
        edges[0].to = 0;
        edges[0].instances = 0;

        edges[1].from = 2;
        edges[1].to = 1;
        edges[1].instances = 0;

        edges[2].from = 1;
        edges[2].to = 0;
        edges[2].instances = 0;

        edges[3].from = 1;
        edges[3].to = 3;
        edges[3].instances = 0;

        edges[4].from = 0;
        edges[4].to = 3;
        edges[4].instances = 0;

        double sgsh_version = 0.1;
        chosen_mb = (struct sgsh_negotiation *)malloc(sizeof(struct sgsh_negotiation));
        chosen_mb->version = sgsh_version;
        chosen_mb->node_array = nodes;
        chosen_mb->n_nodes = n_nodes;
        chosen_mb->edge_array = edges;
        chosen_mb->n_edges = n_edges;

	/* check_negotiation_round() */
	chosen_mb->state_flag = PROT_STATE_NEGOTIATION;
	chosen_mb->initiator_pid = 103; /* Node 3 */
	mb_is_updated = 0;
	chosen_mb->serial_no = 0;
}

/* Identical to chosen_mb except for the initiator field. */
void
setup_mb(struct sgsh_negotiation **mb)
{
	struct sgsh_node *nodes;
	struct sgsh_edge *edges;
	int n_nodes;
	int n_edges;
	n_nodes = 4;
        nodes = (struct sgsh_node *)malloc(sizeof(struct sgsh_node) * n_nodes);
        nodes[0].pid = 100;
	nodes[0].index = 0;
        strcpy(nodes[0].name, "proc0");
        nodes[0].requires_channels = 2;
	nodes[0].provides_channels = 1;
	nodes[0].sgsh_in = 1;
        nodes[0].sgsh_out = 1;

        nodes[1].pid = 101;
	nodes[1].index = 1;
        strcpy(nodes[1].name, "proc1");
        nodes[1].requires_channels = 1;
	nodes[1].provides_channels = 2;
	nodes[1].sgsh_in = 1;
        nodes[1].sgsh_out = 1;

        nodes[2].pid = 102;
	nodes[2].index = 2;
        strcpy(nodes[2].name, "proc2");
        nodes[2].requires_channels = 0;
	nodes[2].provides_channels = 2;
	nodes[2].sgsh_in = 0;
        nodes[2].sgsh_out = 1;

        nodes[3].pid = 103;
	nodes[3].index = 3;
        strcpy(nodes[3].name, "proc3");
        nodes[3].requires_channels = 2;
	nodes[3].provides_channels = 0;
	nodes[3].sgsh_in = 1;
        nodes[3].sgsh_out = 0;

        n_edges = 5;
        edges = (struct sgsh_edge *)malloc(sizeof(struct sgsh_edge) *n_edges);
        edges[0].from = 2;
        edges[0].to = 0;
        edges[0].instances = 0;

        edges[1].from = 2;
        edges[1].to = 1;
        edges[1].instances = 0;

        edges[2].from = 1;
        edges[2].to = 0;
        edges[2].instances = 0;

        edges[3].from = 1;
        edges[3].to = 3;
        edges[3].instances = 0;

        edges[4].from = 0;
        edges[4].to = 3;
        edges[4].instances = 0;

        double sgsh_version = 0.1;
        struct sgsh_negotiation *temp_mb = (struct sgsh_negotiation *)malloc(sizeof(struct sgsh_negotiation));
        temp_mb->version = sgsh_version;
        temp_mb->node_array = nodes;
        temp_mb->n_nodes = n_nodes;
        temp_mb->edge_array = edges;
        temp_mb->n_edges = n_edges;

	/* check_negotiation_round() */
	temp_mb->state_flag = PROT_STATE_NEGOTIATION;
	temp_mb->initiator_pid = 102; /* Node 2 */
	mb_is_updated = 0;
	temp_mb->serial_no = 0;
	temp_mb->origin.index = 2;
	temp_mb->origin.fd_direction = STDOUT_FILENO;

	*mb = temp_mb;
}

void
setup_pointers_to_edges(void)
{
        n_ptedges = 2;
	pointers_to_edges = (struct sgsh_edge **)malloc(sizeof(struct sgsh_edge *) *n_ptedges);
	int i;
	for (i = 0; i < n_ptedges; i++) {
		pointers_to_edges[i] = (struct sgsh_edge *)malloc(sizeof(struct sgsh_edge));	
		pointers_to_edges[i]->from = i;
		pointers_to_edges[i]->to = 3; // the node.
		pointers_to_edges[i]->instances = 0;
        }
}

void
setup_self_node(void)
{
	/* fill in self_node */
	memcpy(&self_node, &chosen_mb->node_array[3], sizeof(struct sgsh_node));
}

void
setup_self_node_io_side(void)
{
	self_node_io_side.index = 3;
	self_node_io_side.fd_direction = 0;
}

/* establish_io_connections() */
void
setup_pipe_fds(void)
{
	/* fill in self_pipe_fds */
	self_pipe_fds.n_input_fds = 2;
	self_pipe_fds.input_fds = (int *)malloc(sizeof(int) *
						self_pipe_fds.n_input_fds);
	self_pipe_fds.input_fds[0] = 0;
	self_pipe_fds.input_fds[0] = 3;
	self_pipe_fds.n_output_fds = 0;
}

void
setup_graph_solution(struct sgsh_node_connections **gs)
{
	int i;
	struct sgsh_node_connections *graph_solution;
	graph_solution = (struct sgsh_node_connections *)malloc(
			sizeof(struct sgsh_node_connections) * 
			chosen_mb->n_nodes);
	graph_solution[0].node_index = 0;
	graph_solution[0].n_edges_incoming = 2;
	graph_solution[0].edges_incoming = (struct sgsh_edge *)malloc(
		sizeof(struct sgsh_edge) * graph_solution[0].n_edges_incoming);
	memcpy(&graph_solution[0].edges_incoming[0], &chosen_mb->edge_array[0],
					sizeof(struct sgsh_edge));
	memcpy(&graph_solution[0].edges_incoming[1], &chosen_mb->edge_array[2],
					sizeof(struct sgsh_edge));
	graph_solution[0].n_edges_outgoing = 1;
	graph_solution[0].edges_outgoing = (struct sgsh_edge *)malloc(
		sizeof(struct sgsh_edge) * graph_solution[0].n_edges_outgoing);
	memcpy(&graph_solution[0].edges_outgoing[0], &chosen_mb->edge_array[4],
					sizeof(struct sgsh_edge));

	graph_solution[1].node_index = 1;
	graph_solution[1].n_edges_incoming = 1;
	graph_solution[1].edges_incoming = (struct sgsh_edge *)malloc(
		sizeof(struct sgsh_edge) * graph_solution[1].n_edges_incoming);
	memcpy(&graph_solution[1].edges_incoming[0], &chosen_mb->edge_array[1],
					sizeof(struct sgsh_edge));
	graph_solution[1].n_edges_outgoing = 2;
	graph_solution[1].edges_outgoing = (struct sgsh_edge *)malloc(
		sizeof(struct sgsh_edge) * graph_solution[1].n_edges_outgoing);
	memcpy(&graph_solution[1].edges_outgoing[0], &chosen_mb->edge_array[2],
					sizeof(struct sgsh_edge));
	memcpy(&graph_solution[1].edges_outgoing[1], &chosen_mb->edge_array[3],
					sizeof(struct sgsh_edge));

	graph_solution[2].node_index = 2;
	graph_solution[2].n_edges_incoming = 0;
	graph_solution[2].edges_incoming = NULL;
	graph_solution[2].n_edges_outgoing = 2;
	graph_solution[2].edges_outgoing = (struct sgsh_edge *)malloc(
		sizeof(struct sgsh_edge) * graph_solution[2].n_edges_outgoing);
	memcpy(&graph_solution[2].edges_outgoing[0], &chosen_mb->edge_array[0],
					sizeof(struct sgsh_edge));
	memcpy(&graph_solution[2].edges_outgoing[1], &chosen_mb->edge_array[1],
					sizeof(struct sgsh_edge));

	graph_solution[3].node_index = 3;
	graph_solution[3].n_edges_incoming = 2;
	graph_solution[3].edges_incoming = (struct sgsh_edge *)malloc(
		sizeof(struct sgsh_edge) * graph_solution[3].n_edges_incoming);
	memcpy(&graph_solution[0].edges_incoming[0], &chosen_mb->edge_array[3],
					sizeof(struct sgsh_edge));
	memcpy(&graph_solution[0].edges_incoming[1], &chosen_mb->edge_array[4],
					sizeof(struct sgsh_edge));
	graph_solution[3].n_edges_outgoing = 0;
	graph_solution[3].edges_outgoing = NULL;

	*gs = graph_solution;
}

void setup_args(void)
{
        args = (int *)malloc(sizeof(int) * 3);
        args[0] = -1;
        args[1] = -1;
        args[2] = -1;
}

void
setup(void)
{
	setup_chosen_mb();
	setup_self_node();
	setup_self_node_io_side();
	setup_pipe_fds();
	setup_graph_solution(&graph_solution);
}

void
setup_test_add_node(void)
{
	setup_chosen_mb();
	setup_self_node();
	setup_self_node_io_side();
}

void
setup_test_lookup_sgsh_edge(void)
{
	setup_chosen_mb();
}

void
setup_test_fill_sgsh_edge(void)
{
	setup_chosen_mb();
	setup_self_node();
	setup_self_node_io_side();
}

void
setup_test_add_edge(void)
{
	setup_chosen_mb();
}

void
setup_test_try_add_sgsh_edge(void)
{
	setup_chosen_mb();
	setup_self_node();
	setup_self_node_io_side();
}

void
setup_test_try_add_sgsh_node(void)
{
	setup_chosen_mb();
	setup_self_node();
}

void
setup_test_fill_sgsh_node(void)
{
	/* setup_self_node() requires setup_chosen_mb() */
	setup_chosen_mb();
	setup_self_node();
}

void
setup_test_free_mb(void)
{
	setup_chosen_mb();
}

void
setup_test_compete_message_block(void)
{
	setup_chosen_mb();
	setup_mb(&fresh_mb);
	setup_self_node();
	setup_self_node_io_side();
}

void
setup_test_point_io_direction(void)
{
	setup_chosen_mb();
	setup_self_node();
	setup_self_node_io_side();
}

void
setup_test_alloc_copy_edges(void)
{
	setup_mb(&fresh_mb);
}

void
setup_test_alloc_copy_nodes(void)
{
	setup_mb(&fresh_mb);
}

void
setup_test_try_read_chunk(void)
{
	setup_self_node_io_side();
}

void
setup_test_read_input_fds(void)
{
	setup_chosen_mb();
	setup_graph_solution(&graph_solution);
	setup_self_node();
	setup_pipe_fds();
}

void
setup_test_read_graph_solution(void)
{
	setup_mb(&fresh_mb);
	setup_chosen_mb();
	setup_self_node_io_side();
}

void
setup_test_try_read_message_block(void)
{
	setup_chosen_mb();
	setup_self_node();
	setup_self_node_io_side();
}

void
setup_test_make_compact_edge_array(void)
{
	setup_pointers_to_edges();
}

void
setup_test_reallocate_edge_pointer_array(void)
{
	setup_pointers_to_edges();
}

void
setup_test_assign_edge_instances(void)
{
	setup_chosen_mb();
	setup_pointers_to_edges();
}

void
setup_test_eval_constraints(void)
{
	setup_args();
}

void
setup_test_satisfy_io_constraints(void)
{
	setup_chosen_mb();
	setup_pointers_to_edges();
	setup_args();
}

void
setup_test_dry_match_io_constraints(void)
{
	setup_chosen_mb();
	setup_pointers_to_edges();
	setup_args();
}

void
setup_test_free_graph_solution(void)
{
	setup_chosen_mb();
	setup_graph_solution(&graph_solution);
}

void
setup_test_solve_sgsh_graph(void)
{
	setup_chosen_mb();
	setup_graph_solution(&graph_solution);
	setup_pointers_to_edges();
	setup_args();
}

void
setup_test_alloc_write_output_fds(void)
{
	setup_chosen_mb(); /* For setting up graph_solution. */
	setup_graph_solution(&graph_solution);
	setup_self_node();
	setup_pipe_fds();
}

void
setup_test_set_dispatcher(void)
{
	setup_chosen_mb();
	setup_self_node_io_side();
}

void
setup_test_establish_io_connections(void)
{
	setup_pipe_fds();
}

void
retire_pointers_to_edges(void)
{
	int i;
	for (i = 0; i < n_ptedges; i++)
		free(pointers_to_edges[i]);
	free(pointers_to_edges);
}

void
retire_chosen_mb(void)
{
        free(chosen_mb->node_array);
        free(chosen_mb->edge_array);
        free(chosen_mb);
}

void
retire_mb(struct sgsh_negotiation *mb)
{
        free(mb->node_array);
        free(mb->edge_array);
        free(mb);
}

void
retire_graph_solution(struct sgsh_node_connections *graph_solution,
								int node_index)
{
	int i;
        for (i = 0; i <= node_index; i++) {
                free(graph_solution[i].edges_incoming);
                free(graph_solution[i].edges_outgoing);
        }
        free(graph_solution);
}

/* establish_io_connections() */
void
retire_pipe_fds(void)
{
	if (n_input_fds > 0)
		free(input_fds);
	if (n_output_fds > 0)
		free(output_fds);
	/* What about self_pipe_fds.input_fds? */
}

void
retire_args(void)
{
	free(args);
}

void
retire(void)
{
	free_graph_solution(chosen_mb->n_nodes - 1);
	retire_chosen_mb();
	retire_pipe_fds();
}

void
retire_test_construct_message_block(void)
{
	retire_chosen_mb();
}

void
retire_test_add_node(void)
{
	retire_chosen_mb();
}

void
retire_test_lookup_sgsh_edge(void)
{
	retire_chosen_mb();
}

void
retire_test_fill_sgsh_edge(void)
{
	retire_chosen_mb();
}

void
retire_test_add_edge(void)
{
	retire_chosen_mb();
}

void
retire_test_try_add_sgsh_edge(void)
{
	retire_chosen_mb();
}

void
retire_test_try_add_sgsh_node(void)
{
	retire_chosen_mb();
}

void
retire_test_compete_message_block(void)
{
	if (exit_state == 1) {
		retire_mb(fresh_mb);
		exit_state = 0;
	} else retire_chosen_mb();
}

void
retire_test_point_io_direction(void)
{
	retire_chosen_mb();
}

void
retire_test_alloc_copy_edges(void)
{
	retire_mb(fresh_mb);
}

void
retire_test_alloc_copy_nodes(void)
{
	retire_mb(fresh_mb);
}

void
retire_test_read_input_fds(void)
{
	retire_pipe_fds();
	free_graph_solution(chosen_mb->n_nodes - 1);
	retire_chosen_mb();
}

void
retire_test_try_read_message_block(void)
{
	retire_chosen_mb();
}

void
retire_test_read_graph_solution(void)
{
	DPRINTF("%s()", __func__);
	retire_chosen_mb();
	retire_mb(fresh_mb);
}

void
retire_test_make_compact_edge_array(void)
{
	free(compact_edges);
	retire_pointers_to_edges();
}

void
retire_test_reallocate_edge_pointer_array(void)
{
	retire_pointers_to_edges();
}

void
retire_test_assign_edge_instances(void)
{
	retire_chosen_mb();
	retire_pointers_to_edges();
}

void
retire_test_eval_constraints(void)
{
        retire_args();
}

void
retire_test_satisfy_io_constraints(void)
{
	retire_chosen_mb();
	retire_pointers_to_edges();
	retire_args();
}

void
retire_test_dry_match_io_constraints(void)
{
	retire_chosen_mb();
	retire_pointers_to_edges();
	retire_args();
}

void
retire_test_free_graph_solution(void)
{
	retire_chosen_mb();
}

void
retire_test_solve_sgsh_graph(void)
{
	/* Are the other data structures handled correctly?
	 * They could be deallocated above our feet.
	 */
	if (!exit_state) free_graph_solution(chosen_mb->n_nodes - 1);
	else exit_state = 0;
	retire_chosen_mb();
	retire_pointers_to_edges();
	retire_args();
}

void
retire_test_alloc_write_output_fds(void)
{
	free_graph_solution(chosen_mb->n_nodes - 1);
	retire_pipe_fds();
}

void
retire_test_set_dispatcher(void)
{
	retire_chosen_mb();
}

void
retire_test_establish_io_connections(void)
{
	retire_pipe_fds();
}

START_TEST(test_solve_sgsh_graph)
{
        /* A normal case with fixed, tight constraints. */
	ck_assert_int_eq(solve_sgsh_graph(), OP_SUCCESS);
	ck_assert_int_eq(graph_solution[3].n_edges_incoming, 2);
	ck_assert_int_eq(graph_solution[3].n_edges_outgoing, 0);
	ck_assert_int_eq(chosen_mb->edge_array[3].instances, 1);
	ck_assert_int_eq(chosen_mb->edge_array[4].instances, 1);
	ck_assert_int_eq(graph_solution[3].edges_incoming[0].instances, 1);
	ck_assert_int_eq(graph_solution[0].edges_outgoing[0].instances, 1);
	ck_assert_int_eq(graph_solution[3].edges_incoming[1].instances, 1);
	ck_assert_int_eq(graph_solution[1].edges_outgoing[1].instances, 1);
	ck_assert_int_eq((long int)graph_solution[3].edges_outgoing, 0);
	retire_test_solve_sgsh_graph();

	/* An impossible case. */
	setup_test_solve_sgsh_graph();
	chosen_mb->node_array[3].requires_channels = 1;
	ck_assert_int_eq(solve_sgsh_graph(), OP_ERROR);
	exit_state = 1;
	retire_test_solve_sgsh_graph();

	/* Relaxing our target node's constraint. */
	setup_test_solve_sgsh_graph();
	chosen_mb->node_array[3].requires_channels = -1;
	ck_assert_int_eq(solve_sgsh_graph(), OP_SUCCESS);
	ck_assert_int_eq(graph_solution[3].n_edges_incoming, 2);
	ck_assert_int_eq(graph_solution[3].n_edges_outgoing, 0);
	/* Pair edges still have tight constraints. */
	ck_assert_int_eq(chosen_mb->edge_array[3].instances, 1);
	ck_assert_int_eq(chosen_mb->edge_array[4].instances, 1);
	ck_assert_int_eq(graph_solution[3].edges_incoming[0].instances, 1);
	ck_assert_int_eq(graph_solution[0].edges_outgoing[0].instances, 1);
	ck_assert_int_eq(graph_solution[3].edges_incoming[1].instances, 1);
	ck_assert_int_eq(graph_solution[1].edges_outgoing[1].instances, 1);
	ck_assert_int_eq((long int)graph_solution[3].edges_outgoing, 0);
	retire_test_solve_sgsh_graph();

	/* Relaxing also pair nodes' constraints. */
	setup_test_solve_sgsh_graph();
	chosen_mb->node_array[3].requires_channels = -1;
	chosen_mb->node_array[0].provides_channels = -1;
	chosen_mb->node_array[1].provides_channels = -1;
	ck_assert_int_eq(solve_sgsh_graph(), OP_SUCCESS);
	ck_assert_int_eq(graph_solution[3].n_edges_incoming, 2);
	ck_assert_int_eq(graph_solution[3].n_edges_outgoing, 0);
	ck_assert_int_eq(chosen_mb->edge_array[3].instances, 5);
	ck_assert_int_eq(chosen_mb->edge_array[4].instances, 5);
	ck_assert_int_eq(graph_solution[3].edges_incoming[0].instances, 5);
	ck_assert_int_eq(graph_solution[0].edges_outgoing[0].instances, 5);
	ck_assert_int_eq(graph_solution[3].edges_incoming[1].instances, 5);
	ck_assert_int_eq(graph_solution[1].edges_outgoing[1].instances, 5);
	ck_assert_int_eq((long int)graph_solution[3].edges_outgoing, 0);
	/* Collateral impact. Node 1 (flex) -> Node 0 (tight) */
	ck_assert_int_eq(chosen_mb->edge_array[2].instances, 1);
	ck_assert_int_eq(graph_solution[1].edges_outgoing[0].instances, 1);
}
END_TEST


START_TEST(test_free_graph_solution)
{
	ck_assert_int_eq(free_graph_solution(3), OP_SUCCESS);
	/* Invalid node indexes, the function's argument, are checked by assertion. */
}
END_TEST


START_TEST(test_establish_io_connections)
{
	/* Should be in the solution propagation test suite. */
	/* The test case contains an arrangement of 0 fds and another of >0 fds. */
	ck_assert_int_eq(establish_io_connections(&input_fds, &n_input_fds,
					&output_fds, &n_output_fds), OP_SUCCESS);
}
END_TEST

struct sgsh_edge **edges_in;
int n_edges_in;
struct sgsh_edge **edges_out;
int n_edges_out;

void
retire_dmic(void)
{
	retire();
	if (n_edges_in)
		free(edges_in);
	if (n_edges_out)
		free(edges_out);
}

START_TEST(test_dry_match_io_constraints)
{
        /* A normal case with fixed, tight constraints. */
	n_edges_in = 0;
	n_edges_out = 0;
	ck_assert_int_eq(dry_match_io_constraints(&chosen_mb->node_array[3],
			&edges_in, &n_edges_in,&edges_out, &n_edges_out), OP_SUCCESS);
        /* Hard coded. Observe the topology of the prototype solution in setup(). */
	ck_assert_int_eq(n_edges_in, 2);
	ck_assert_int_eq(n_edges_out, 0);

	/* An impossible case. */
	n_edges_in = 0;
	n_edges_out = 0;
	chosen_mb->node_array[3].requires_channels = 1;
	ck_assert_int_eq(dry_match_io_constraints(&chosen_mb->node_array[3],
			&edges_in, &n_edges_in, &edges_out, &n_edges_out), OP_ERROR);
	ck_assert_int_eq(n_edges_in, 2);
	ck_assert_int_eq(n_edges_out, 0);

	/* Relaxing our target node's constraint. */
	n_edges_in = 0;
	n_edges_out = 0;
	chosen_mb->node_array[3].requires_channels = -1;
	ck_assert_int_eq(dry_match_io_constraints(&chosen_mb->node_array[3],
			&edges_in, &n_edges_in, &edges_out, &n_edges_out), OP_SUCCESS);
	ck_assert_int_eq(n_edges_in, 2);
	ck_assert_int_eq(n_edges_out, 0);
	/* Pair edges still have tight constraints. */
	ck_assert_int_eq(chosen_mb->edge_array[3].instances, 1);
	ck_assert_int_eq(chosen_mb->edge_array[4].instances, 1);
	retire_test_dry_match_io_constraints();

	/* Relaxing also pair nodes' constraints. Need to reset the testbed. */
	setup_test_dry_match_io_constraints();
	n_edges_in = 0;
	n_edges_out = 0;
	chosen_mb->node_array[3].requires_channels = -1;
	chosen_mb->node_array[0].provides_channels = -1;
	chosen_mb->node_array[1].provides_channels = -1;
	ck_assert_int_eq(dry_match_io_constraints(&chosen_mb->node_array[3],
			&edges_in, &n_edges_in, &edges_out, &n_edges_out), OP_SUCCESS);
	ck_assert_int_eq(n_edges_in, 2);
	ck_assert_int_eq(n_edges_out, 0);
	ck_assert_int_eq(chosen_mb->edge_array[3].instances, 5);
	ck_assert_int_eq(chosen_mb->edge_array[4].instances, 5);
}
END_TEST

START_TEST(test_satisfy_io_constraints)
{
        /* To be concise, when changing the first argument that mirrors
         * the channel constraint of the node under evaluation, we should
         * also change it in the node array, but it does not matter since it
         * is the pair nodes that we are interested in.
         */

        /* Fixed constraint both sides, just satisfy. */
	ck_assert_int_eq(satisfy_io_constraints(2, pointers_to_edges, 2, 1),
									OP_SUCCESS);
        /* Fixed constraint both sides, inadequate. */
	ck_assert_int_eq(satisfy_io_constraints(1, pointers_to_edges, 2, 1),
									OP_ERROR);
        /* Fixed constraint bith sides, plenty. */
	ck_assert_int_eq(satisfy_io_constraints(5, pointers_to_edges, 2, 1),
									OP_SUCCESS);
        /* Fixed constraint node, flexible pair, just one. */
        chosen_mb->node_array[0].provides_channels = -1;
	ck_assert_int_eq(satisfy_io_constraints(2, pointers_to_edges, 2, 1),
									OP_SUCCESS);
        /* Fixed constraint node, flexible pair, inadequate. */
        chosen_mb->node_array[0].provides_channels = -1;
	ck_assert_int_eq(satisfy_io_constraints(1, pointers_to_edges, 2, 1),
									OP_ERROR);
	retire_test_satisfy_io_constraints();

        /* Expand the semantics of remaining_free_channels to fixed constraints
           as in this case. */  
        /* Fixed constraint node, flexible pair, plenty. */
	setup_test_satisfy_io_constraints();
        chosen_mb->node_array[0].provides_channels = -1;
	ck_assert_int_eq(satisfy_io_constraints(5, pointers_to_edges, 2, 1),
									OP_SUCCESS);
	retire_test_satisfy_io_constraints();

        /* Flexible constraint both sides */
	setup_test_satisfy_io_constraints();
        chosen_mb->node_array[0].provides_channels = -1;
	ck_assert_int_eq(satisfy_io_constraints(-1, pointers_to_edges, 2, 1),
									OP_SUCCESS);
}
END_TEST

START_TEST(test_eval_constraints)
{
	/* 0 flexible constraints. */
	ck_assert_int_eq(eval_constraints(2, 3, 0, &args[0], &args[1],
                                                              &args[2]), OP_ERROR);
	ck_assert_int_eq(eval_constraints(2, 2, 0, &args[0], &args[1],
                                                              &args[2]), OP_SUCCESS);
	ck_assert_int_eq(args[0], 0);
	ck_assert_int_eq(args[1], 0);
	ck_assert_int_eq(args[2], 2);

        args[0] = -1;
        args[1] = -1;
        args[2] = -1;
	ck_assert_int_eq(eval_constraints(3, 2, 0, &args[0], &args[1],
                                                              &args[2]), OP_SUCCESS);
	ck_assert_int_eq(args[0], 0);
	ck_assert_int_eq(args[1], 0);
	ck_assert_int_eq(args[2], 2);

	/* Pair nodes have flexible constraints; the test node has fixed. */
        args[0] = -1;
        args[1] = -1;
        args[2] = -1;
	ck_assert_int_eq(eval_constraints(2, 2, 1, &args[0], &args[1],
                                                              &args[2]), OP_ERROR);
	ck_assert_int_eq(eval_constraints(2, 1, 1, &args[0], &args[1],
                                                              &args[2]), OP_SUCCESS);
	ck_assert_int_eq(args[0], 1);
	ck_assert_int_eq(args[1], 0);
	ck_assert_int_eq(args[2], 2);

        args[0] = -1;
        args[1] = -1;
        args[2] = -1;
	ck_assert_int_eq(eval_constraints(3, 1, 1, &args[0], &args[1],
                                                              &args[2]), OP_SUCCESS);
	ck_assert_int_eq(args[0], 2);
	ck_assert_int_eq(args[1], 0);
	ck_assert_int_eq(args[2], 3);

        args[0] = -1;
        args[1] = -1;
        args[2] = -1;
	ck_assert_int_eq(eval_constraints(5, 2, 2, &args[0], &args[1],
                                                              &args[2]), OP_SUCCESS);
	ck_assert_int_eq(args[0], 1);
	ck_assert_int_eq(args[1], 1);
	ck_assert_int_eq(args[2], 5); // remaining free channels included

	/* The test node has flexible constraints; the pair nodes have fixed. */
        args[0] = -1;
        args[1] = -1;
        args[2] = -1;
	ck_assert_int_eq(eval_constraints(-1, 2, 0, &args[0], &args[1],
                                                              &args[2]), OP_SUCCESS);
	ck_assert_int_eq(args[0], 0);
	ck_assert_int_eq(args[1], 0);
	ck_assert_int_eq(args[2], 2);

	/* The test node has flexible constraints; so do the pair nodes. */
        args[0] = -1;
        args[1] = -1;
        args[2] = -1;
	ck_assert_int_eq(eval_constraints(-1, 2, 3, &args[0], &args[1],
                                                              &args[2]), OP_SUCCESS);
	ck_assert_int_eq(args[0], 5);
	ck_assert_int_eq(args[1], 0);
	ck_assert_int_eq(args[2], 17);

}
END_TEST

START_TEST(test_assign_edge_instances)
{
	/*   No flexible. */
	ck_assert_int_eq(assign_edge_instances(pointers_to_edges, n_ptedges, 2, 1, 0, 0, 0, 2), OP_SUCCESS);

	/*   Flexible with standard instances. */
        chosen_mb->node_array[0].provides_channels = -1;
	ck_assert_int_eq(assign_edge_instances(pointers_to_edges, n_ptedges, 2, 1, 1, 1, 0, 2), OP_SUCCESS);
	retire_test_assign_edge_instances();

        /* Flexible with extra instances, but no remaining. */
	setup_test_assign_edge_instances();
        chosen_mb->node_array[0].provides_channels = -1;
	ck_assert_int_eq(assign_edge_instances(pointers_to_edges, n_ptedges, -1, 1, 1, 5, 0, 6), OP_SUCCESS);
	retire_test_assign_edge_instances();

        /* Flexible with extra instances, including remaining. */
	setup_test_assign_edge_instances();
        chosen_mb->node_array[0].provides_channels = -1;
        chosen_mb->node_array[0].provides_channels = -1;
	ck_assert_int_eq(assign_edge_instances(pointers_to_edges, n_ptedges, 7, 1, 1, 5, 1, 7), OP_SUCCESS);

}
END_TEST

START_TEST(test_reallocate_edge_pointer_array)
{
	ck_assert_int_eq(reallocate_edge_pointer_array(NULL, 1), OP_ERROR);
	ck_assert_int_eq(reallocate_edge_pointer_array(&pointers_to_edges, -2), OP_ERROR);
	ck_assert_int_eq(reallocate_edge_pointer_array(&pointers_to_edges, 0), OP_ERROR);
	/* Not incresing the value of n_ptedges to not perplex freeing 
         * pointers_to_edges because reallocation only accounts for
         * struct sgsh_edge *.
         */
	ck_assert_int_eq(reallocate_edge_pointer_array(&pointers_to_edges, n_ptedges + 1), OP_SUCCESS);
}
END_TEST


START_TEST(test_make_compact_edge_array)
{
	ck_assert_int_eq(make_compact_edge_array(NULL, 2, pointers_to_edges), OP_ERROR);
	ck_assert_int_eq(make_compact_edge_array(&compact_edges, -2, pointers_to_edges), OP_ERROR);
	ck_assert_int_eq(make_compact_edge_array(&compact_edges, 0, pointers_to_edges), OP_ERROR);
	ck_assert_int_eq(make_compact_edge_array(&compact_edges, n_ptedges, NULL), OP_ERROR);

	struct sgsh_edge *p = pointers_to_edges[0];
	pointers_to_edges[0] = NULL;
	ck_assert_int_eq(make_compact_edge_array(&compact_edges, n_ptedges, pointers_to_edges), OP_ERROR);

	pointers_to_edges[0] = p;
	ck_assert_int_eq(make_compact_edge_array(&compact_edges, n_ptedges, pointers_to_edges), OP_SUCCESS);
}
END_TEST

START_TEST(test_alloc_write_output_fds)
{
	/* 0 outgoing edges for node 3, so no action really. */
	ck_assert_int_eq(alloc_write_output_fds(), OP_SUCCESS);
	retire_test_alloc_write_output_fds();

	/* Switch to node 2 that has 2 outgoing edges. */
	setup_test_alloc_write_output_fds();
	memcpy(&self_node, &chosen_mb->node_array[2], sizeof(struct sgsh_node));
	ck_assert_int_eq(alloc_write_output_fds(), OP_SUCCESS);

	/* Incomplete testing since socket descriptors have not yet been setup.
	 * This will hapeen through the shell.
	 */
}
END_TEST

START_TEST(test_set_dispatcher)
{
	set_dispatcher();
	ck_assert_int_eq(chosen_mb->origin.index, 3);
	ck_assert_int_eq(chosen_mb->origin.fd_direction, 0); /* The input side */
}
END_TEST

START_TEST(test_alloc_node_connections)
{
	struct sgsh_edge *test;
	/* It is assumed that negative number of edges have already
         * been checked. See e.g. read_graph_solution().
         */
	ck_assert_int_eq(alloc_node_connections(NULL, 2, 1, 2), OP_ERROR);
	ck_assert_int_eq(alloc_node_connections(&test, 1, 2, 2), OP_ERROR);
	ck_assert_int_eq(alloc_node_connections(&test, 1, -1, 2), OP_ERROR);
	ck_assert_int_eq(alloc_node_connections(&test, 1, 1, -2), OP_ERROR);

	ck_assert_int_eq(alloc_node_connections(&test, 1, 1, 2), OP_SUCCESS);
	free(test);
}
END_TEST

/* orphan test case. */
START_TEST(test_check_negotiation_round)
{
	/* End of negotiation rounds */
	int negotiation_round = 0;
	check_negotiation_round(&negotiation_round);
	ck_assert_int_eq(negotiation_round, 1);
	ck_assert_int_eq(chosen_mb->state_flag, PROT_STATE_NEGOTIATION_END);
	ck_assert_int_eq(chosen_mb->serial_no, 1);
	ck_assert_int_eq(mb_is_updated, 1);
	retire();

	/* Continue negotiation rounds, but don't update the counter */
	setup();
	chosen_mb->initiator_pid = 100; /* Node at index 0 */
	negotiation_round = 0;
	mb_is_updated = 1;
	check_negotiation_round(&negotiation_round);
	ck_assert_int_eq(negotiation_round, 0);
	ck_assert_int_eq(chosen_mb->state_flag, PROT_STATE_NEGOTIATION);
	ck_assert_int_eq(chosen_mb->serial_no, 0);
	ck_assert_int_eq(mb_is_updated, 1);

	/* Don't update negotiation rounds, message block not updated */
	setup();
	chosen_mb->initiator_pid = 100; /* Node at index 0 */
	negotiation_round = 0;
	check_negotiation_round(&negotiation_round);
	ck_assert_int_eq(negotiation_round, 0);
	ck_assert_int_eq(chosen_mb->state_flag, PROT_STATE_NEGOTIATION_END);
	ck_assert_int_eq(chosen_mb->serial_no, 1);
	ck_assert_int_eq(mb_is_updated, 1);
	retire();

	/* Don't update and continue negotiation rounds. */
	setup();
	negotiation_round = 0;
	mb_is_updated = 1;
	check_negotiation_round(&negotiation_round);
	ck_assert_int_eq(negotiation_round, 1);
	ck_assert_int_eq(chosen_mb->state_flag, PROT_STATE_NEGOTIATION);
	ck_assert_int_eq(chosen_mb->serial_no, 0);
	ck_assert_int_eq(mb_is_updated, 1);
	retire();

	/* Negotiation has ended. */
	setup();
	negotiation_round = 0;
	chosen_mb->state_flag = PROT_STATE_NEGOTIATION_END;
	check_negotiation_round(&negotiation_round);
	ck_assert_int_eq(negotiation_round, 0);
	ck_assert_int_eq(chosen_mb->state_flag, PROT_STATE_NEGOTIATION_END);
	ck_assert_int_eq(chosen_mb->serial_no, 0);
	ck_assert_int_eq(mb_is_updated, 0);
	retire();

}
END_TEST

/* Incomplete? */
START_TEST(test_try_read_message_block)
{

	int fd[2];
	int fd_side = -1;
	int pid;
        /* 1 millisecond sleep. */
	struct timespec sleep;
	sleep.tv_sec = 0;
	sleep.tv_nsec = 1000000;
	DPRINTF("%s()", __func__);

	if(pipe(fd) == -1){
		perror("pipe open failed");
		exit(1);
	}
	DPRINTF("Opened pipe pair %d - %d.", fd[0], fd[1]);	

	pid = fork();
	if (pid <= 0) {
		DPRINTF("Child speaking with pid %d.", (int)getpid());
		struct sgsh_negotiation *test_mb;
		setup_mb(&test_mb);
        	int n_nodes = test_mb->n_nodes;
        	int n_edges = test_mb->n_edges;
        	int mb_struct_size = sizeof(struct sgsh_negotiation);
        	int mb_nodes_size = sizeof(struct sgsh_node) * n_nodes;
        	int mb_edges_size = sizeof(struct sgsh_edge) * n_edges;
                int i = 0;

		close(fd[0]);
		DPRINTF("Child writes message block structure of size %d.",
					mb_struct_size);
        	write(fd[1], test_mb, mb_struct_size);
		/* Sleep for 1 millisecond before the next operation. */
		nanosleep(&sleep, NULL);

		DPRINTF("Child writes message block node array of size %d.",
					mb_nodes_size);
        	write(fd[1], test_mb->node_array, mb_nodes_size);
		nanosleep(&sleep, NULL);

		DPRINTF("Child writes message block edge array of size %d.",
					mb_edges_size);
                for (i = 0; i < test_mb->n_edges; i++) {
                        struct sgsh_edge *e = &test_mb->edge_array[i];
                        DPRINTF("Edge from: %d, to: %d", e->from, e->to);
                }
		write(fd[1], test_mb->edge_array, mb_edges_size);
		nanosleep(&sleep, NULL);

		DPRINTF("Child: closes fd %d.", fd[1]);
		close(fd[1]);
		DPRINTF("Child with pid %d exits.", (int)getpid());
		retire_mb(test_mb);
	} else {
		int buf_size = getpagesize();
		char buf[buf_size];
		DPRINTF("Parent speaking with pid %d.", (int)getpid());
		ck_assert_int_eq(try_read_message_block(buf, buf_size,
							&fresh_mb), OP_SUCCESS);
	}
}
END_TEST

/* Incomplete? */
START_TEST(test_read_graph_solution)
{
	int fd[2];
	int fd_side = -1;
	int bytes_read = -1;
	int buf_size = getpagesize();
	int pid;
	int i;
        int n_nodes = fresh_mb->n_nodes;
        int graph_solution_size = sizeof(struct sgsh_node_connections) *
                                                                n_nodes;
	char buf[buf_size];
	struct sgsh_node_connections *graph_solution;
        /* 1 millisecond sleep. */
	struct timespec sleep;
	sleep.tv_sec = 0;
	sleep.tv_nsec = 1000000;
	DPRINTF("%s()", __func__);

	if(pipe(fd) == -1){
		perror("pipe open failed");
		exit(1);
	}
	DPRINTF("Opened pipe pair %d - %d.", fd[0], fd[1]);	

	pid = fork();
	if (pid <= 0) {
		DPRINTF("Child speaking with pid %d.", (int)getpid());
		setup_graph_solution(&graph_solution);

		close(fd[0]);
		DPRINTF("Child writes graph solution of size %d.",
					graph_solution_size);
        	write(fd[1], graph_solution, graph_solution_size);
		/* Sleep for 1 millisecond before the next operation. */
		nanosleep(&sleep, NULL);

		for (i = 0; i < chosen_mb->n_nodes; i++) {
                	struct sgsh_node_connections *nc = &graph_solution[i];
                	int in_edges_size = sizeof(struct sgsh_edge) *
							nc->n_edges_incoming;
                	int out_edges_size = sizeof(struct sgsh_edge) *
							nc->n_edges_outgoing;
                	if ((in_edges_size > buf_size) || 
						(out_edges_size > buf_size)) {
                        	DPRINTF("Sgsh negotiation graph solution for node at index %d: incoming connections of size %d or outgoing connections of size %d do not fit to buffer of size %d.\n", nc->node_index, in_edges_size, out_edges_size, buf_size);
                        	exit(1);
                	}

			DPRINTF("Child writes incoming edges of node %d in fd %d. Total size: %d", i, fd[1], in_edges_size);
                	/* Transmit a node's incoming connections. */
                	write(fd[1], nc->edges_incoming, in_edges_size);
			nanosleep(&sleep, NULL);

			DPRINTF("Child writes outgoing edges of node %d in fd %d. Total size: %d", i, fd[1], out_edges_size);
                	/* Transmit a node's outgoing connections. */
                	write(fd[1], nc->edges_outgoing, out_edges_size);
			nanosleep(&sleep, NULL);
        	}
		DPRINTF("Child: closes fd %d.", fd[1]);
		close(fd[1]);
		DPRINTF("Child with pid %d exits.", (int)getpid());
		retire_graph_solution(graph_solution, chosen_mb->n_nodes - 1);
	} else {
		DPRINTF("Parent speaking with pid %d.", (int)getpid());
		ck_assert_int_eq(read_graph_solution(fresh_mb, buf, buf_size),
								OP_SUCCESS);
	}
}
END_TEST

/* Incomplete? */
START_TEST(test_read_input_fds)
{
	int sockets[2];
	char buf[1024];
	int fd, ping;
	struct msghdr msg;
	struct iovec vec[1];
	union fdmsg cmsg;
	struct cmsghdr *h;

	DPRINTF("%s()...pid %d", __func__, (int)getpid());

	if (socketpair(AF_UNIX, SOCK_DGRAM, 0, sockets) < 0) {
		perror("Error opening stream socket pair. Exiting now.");
		exit(1);
	}
	DPRINTF("Opened socket pair %d - %d.", sockets[0], sockets[1]);

	fd = open("unit-test-sgsh", O_CREAT | O_WRONLY);
	write(fd, "Unit testing sgsh...", 21);
        close(fd);
	fd = open("unit-test-sgsh", O_RDONLY);
	if (fd < 0)
		perror("Failed to open file test-sgsh for reading.");

        int pid = fork();
	if (pid <= 0) {
		DPRINTF("Child speaking with pid %d.", (int)getpid());

		DPRINTF("Child closes socket %d.", sockets[1]);
		close(sockets[1]);

		vec[0].iov_base = &ping;
		vec[0].iov_len = 1;

		msg.msg_iov = vec;
		msg.msg_iovlen = 1;
		msg.msg_name = 0;
		msg.msg_namelen = 0;
		msg.msg_control = cmsg.buf;
		msg.msg_controllen = sizeof(union fdmsg);
		msg.msg_flags = 0;

		h = CMSG_FIRSTHDR(&msg);
		h->cmsg_level = SOL_SOCKET;
		h->cmsg_type = SCM_RIGHTS;
		h->cmsg_len = CMSG_LEN(sizeof(int));
		*((int*)CMSG_DATA(h)) = fd;

		DPRINTF("Child goes sendmsg()");
		if((sendmsg(sockets[0], &msg, 0)) < 0){
			perror("sendmsg()");
			exit(EXIT_FAILURE);
		}
		DPRINTF("Child: closes fd %d.", fd);
		close(fd);
		DPRINTF("Child with pid %d exits.", (int)getpid());
	} else {
		memcpy(&self_node, &chosen_mb->node_array[1],
						sizeof(struct sgsh_node));
		/* Edges have been setup with 0 instances.
		 * See setup_chosen_mb().
	 	 * Edges then copied to graph_solution.
		 * See setup_graph_solution().
	 	 */
		graph_solution[1].edges_incoming[0].instances = 1;
	
		DPRINTF("Parent speaking with pid %d.", (int)getpid());
		ck_assert_int_eq(read_input_fds(), OP_SUCCESS);
		DPRINTF("Parent with pid %d exits.", (int)getpid()); 
	}
	close(sockets[0]);
	close(sockets[1]);
}
END_TEST

/* Incomplete? */
START_TEST(test_try_read_chunk)
{
	/* Requires setting up of I/O multiplexing (a bash shell extension)
	 * in order to be able to support bidirectional negotiation.
	 * Without it we cannot test this function because it tries to read
	 * repeatedly from stdin and then stdout until it manages.
         * We cannot feed it; writing to stdin or stdout
	 * ends up in the unit test file output).
	 * The good news is that the core of this function is the call to
	 * call read, which has been successfully tested.
	 * Then there is variable checking to determine the exit code.
	 */
	int fd[2];
	DPRINTF("%s()...", __func__);
	if(pipe(fd) == -1){
		perror("pipe open failed");
		exit(1);
	}
	/* Non-blocking in order to be able to try read stdin, then stdout
	 * and again.
	 */
        fcntl(fd[0], F_SETFL, O_NONBLOCK);

	/* Broken pipe error if we close the other side. */
	//close(fd[0]);
	write(fd[1], "test-in", 9);
	char buf[32];
	int fd_return = -1;
	int bytes_read = -1;
	ck_assert_int_eq(try_read_chunk(buf, 32, &bytes_read, &fd_return),OP_SUCCESS);
	ck_assert_int_eq(fd_return, 4);
	ck_assert_int_eq(bytes_read, 9);
	
	close(fd[0]);
	close(fd[1]);
}
END_TEST

START_TEST(test_call_read)
{
	int fd[2];
	DPRINTF("%s()...", __func__);
	if(pipe(fd) == -1){
		perror("pipe open failed");
		exit(1);
	}
	/* Non-blocking in order to be able to try read stdin, then stdout
	 * and again.
	 */
        fcntl(fd[0], F_SETFL, O_NONBLOCK);

	write(fd[1], "test", 5);
	char buf[32];
	int fd_side = -1;
	int bytes_read = -1;
	int error_code = -1;
	ck_assert_int_eq(call_read(fd[0], buf, 32, &fd_side, &bytes_read,
				&error_code), OP_QUIT);
	ck_assert_int_eq(fd_side, 4);
	ck_assert_int_eq(bytes_read, 5);
	ck_assert_int_eq(error_code, 0);

	fd_side = -1;
	bytes_read = -1;
	error_code = -1;
	ck_assert_int_eq(call_read(fd[0], buf, 32, &fd_side, &bytes_read,
				&error_code), OP_SUCCESS);
	/* No write, no side */
	ck_assert_int_eq(fd_side, -1);
	ck_assert_int_eq(bytes_read, -1);
	ck_assert_int_eq(error_code, -EAGAIN);
	
	close(fd[0]);
	close(fd[1]);
}
END_TEST

START_TEST(test_alloc_copy_mb)
{
	const int size = sizeof(struct sgsh_negotiation);
	char buf[512];
	struct sgsh_negotiation *mb;
	ck_assert_int_eq(alloc_copy_mb(&mb, buf, 86, 512), OP_ERROR);

	char buf2[32];
	ck_assert_int_eq(alloc_copy_mb(&mb, buf2, size, 32), OP_ERROR);

	ck_assert_int_eq(alloc_copy_mb(&mb, buf, size, 512), OP_SUCCESS);
	free(mb);
}
END_TEST

START_TEST(test_alloc_copy_nodes)
{
	const int size = sizeof(struct sgsh_node) * fresh_mb->n_nodes;
	char buf[512];
	ck_assert_int_eq(alloc_copy_nodes(fresh_mb, buf, 86, 512), OP_ERROR);

	char buf2[32];
	ck_assert_int_eq(alloc_copy_nodes(fresh_mb, buf2, size, 32), OP_ERROR);

	free(fresh_mb->node_array);  /* to avoid memory leak */
	ck_assert_int_eq(alloc_copy_nodes(fresh_mb, buf, size, 512), OP_SUCCESS);
}
END_TEST

START_TEST(test_alloc_copy_edges)
{
	const int size = sizeof(struct sgsh_edge) * fresh_mb->n_edges;
	char buf[256];
	ck_assert_int_eq(alloc_copy_edges(fresh_mb, buf, 86, 256), OP_ERROR);

	char buf2[32];
	ck_assert_int_eq(alloc_copy_edges(fresh_mb, buf2, size, 32), OP_ERROR);

	free(fresh_mb->edge_array);  /* to avoid memory leak */
	ck_assert_int_eq(alloc_copy_edges(fresh_mb, buf, size, 256), OP_SUCCESS);
}
END_TEST

START_TEST(test_check_read)
{
	ck_assert_int_eq(check_read(512, 1024, 256), OP_ERROR);
	ck_assert_int_eq(check_read(512, 256, 512), OP_ERROR);
	ck_assert_int_eq(check_read(512, 1024, 512), OP_SUCCESS);
}
END_TEST

START_TEST(test_point_io_direction)
{
	point_io_direction(STDOUT_FILENO);
	ck_assert_int_eq(self_node_io_side.fd_direction, STDIN_FILENO);

	memcpy(&self_node, &chosen_mb->node_array[2], sizeof(struct sgsh_node));
	point_io_direction(STDIN_FILENO);
	ck_assert_int_eq(self_node_io_side.fd_direction, STDOUT_FILENO);
	
}
END_TEST

START_TEST(test_compete_message_block)
{
	int should_transmit_mb = -1;
	memcpy(&self_node, &chosen_mb->node_array[3], sizeof(struct sgsh_node));
	/* set_dispatcher() */
	self_node_io_side.index = 3;
	self_node_io_side.fd_direction = STDIN_FILENO;
	ck_assert_int_eq(compete_message_block(fresh_mb, &should_transmit_mb),
								OP_SUCCESS);
	ck_assert_int_eq(should_transmit_mb, 1);
	ck_assert_int_eq(mb_is_updated, 1);
	ck_assert_int_eq((long int)chosen_mb, (long int)fresh_mb);
	exit_state = 1;
	retire_test_compete_message_block();

	setup_test_compete_message_block();
	should_transmit_mb = -1;
	memcpy(&self_node, &chosen_mb->node_array[3], sizeof(struct sgsh_node));
	/* set_dispatcher() */
	self_node_io_side.index = 3;
	self_node_io_side.fd_direction = STDIN_FILENO;
	fresh_mb->initiator_pid = 110; /* Younger than chosen_mb. */
	ck_assert_int_eq(compete_message_block(fresh_mb, &should_transmit_mb),
								OP_SUCCESS);
	ck_assert_int_eq(should_transmit_mb, 0);
	ck_assert_int_eq(mb_is_updated, 0);
	retire_test_compete_message_block();

	setup_test_compete_message_block();
	should_transmit_mb = -1;
	memcpy(&self_node, &chosen_mb->node_array[3], sizeof(struct sgsh_node));
	/* set_dispatcher() */
	self_node_io_side.index = 3;
	self_node_io_side.fd_direction = STDIN_FILENO;
	fresh_mb->initiator_pid = 103; /* Draw */
	fresh_mb->serial_no = 1; /* fresh_mb prevails */
	ck_assert_int_eq(compete_message_block(fresh_mb, &should_transmit_mb),
								OP_SUCCESS);
	ck_assert_int_eq(should_transmit_mb, 1);
	ck_assert_int_eq(mb_is_updated, 1);
	ck_assert_int_eq((long int)chosen_mb, (long int)fresh_mb);
	exit_state = 1;
	retire_test_compete_message_block();

	setup_test_compete_message_block();
	should_transmit_mb = -1;
	memcpy(&self_node, &chosen_mb->node_array[0], sizeof(struct sgsh_node));
	/* set_dispatcher() */
	self_node_io_side.index = 0;
	self_node_io_side.fd_direction = STDOUT_FILENO;
	chosen_mb->origin.index = 3;
	chosen_mb->origin.fd_direction = STDIN_FILENO;
	fresh_mb->initiator_pid = 103; /* Younger than chosen_mb. */
	fresh_mb->serial_no = 0; /* fresh_mb does not prevail */
	ck_assert_int_eq(compete_message_block(fresh_mb, &should_transmit_mb),
								OP_SUCCESS);
	ck_assert_int_eq(should_transmit_mb, 0);
	ck_assert_int_eq(mb_is_updated, 0);

}
END_TEST

START_TEST(test_free_mb)
{
	free_mb(chosen_mb);
}
END_TEST

START_TEST(test_fill_sgsh_node)
{
	fill_sgsh_node("test", 1003, 1, 1);
	ck_assert_int_eq(strcmp(self_node.name, "test"), 0);
	ck_assert_int_eq(self_node.pid, 1003);
	ck_assert_int_eq(self_node.requires_channels, 1);
	ck_assert_int_eq(self_node.provides_channels, 1);
	ck_assert_int_eq(self_node.index, -1);
}
END_TEST

START_TEST(test_try_add_sgsh_node)
{
	ck_assert_int_eq(try_add_sgsh_node(), OP_EXISTS);
	ck_assert_int_eq(chosen_mb->n_nodes, 4);
	ck_assert_int_eq(chosen_mb->serial_no, 0);
	ck_assert_int_eq(mb_is_updated, 0);
	ck_assert_int_eq(self_node_io_side.index, 0);
	ck_assert_int_eq(self_node.index, 3);

	struct sgsh_node new;
	new.pid = 104;
	memcpy(new.name, "proc4", 6);
	new.requires_channels = 1;
	new.provides_channels = 1;
	memcpy(&self_node, &new, sizeof(struct sgsh_node));
	ck_assert_int_eq(try_add_sgsh_node(), OP_SUCCESS);
	ck_assert_int_eq(chosen_mb->n_nodes, 5);
	ck_assert_int_eq(chosen_mb->serial_no, 1);
	ck_assert_int_eq(mb_is_updated, 1);
	ck_assert_int_eq(self_node_io_side.index, 4);
	ck_assert_int_eq(self_node.index, 4);

}
END_TEST

START_TEST(test_try_add_sgsh_edge)
{
	/* Better in a setup function. */ 
	chosen_mb->origin.fd_direction = STDOUT_FILENO;   
	chosen_mb->origin.index = 0;
	/* self_node_io_side should also be set; it is set in setup */
	ck_assert_int_eq(try_add_sgsh_edge(), OP_EXISTS);

	/* New edge: from new node to existing */
	struct sgsh_node new;
	new.index = 4;
	new.pid = 104;
	memcpy(new.name, "proc4", 6);
	new.requires_channels = 1;
	new.provides_channels = 1;
	new.sgsh_in = 1;
        new.sgsh_out = 1;
	/* Better in a setup function. */ 
	chosen_mb->origin.fd_direction = STDOUT_FILENO;   
	chosen_mb->origin.index = new.index;
	/* self_node_io_side should also be set; it is set in setup */
	memcpy(&self_node, &new, sizeof(struct sgsh_node));
	chosen_mb->n_nodes++;
	chosen_mb->node_array = realloc(chosen_mb->node_array,
				sizeof(struct sgsh_node) * chosen_mb->n_nodes);
	memcpy(&chosen_mb->node_array[chosen_mb->n_nodes - 1], &new,
		sizeof(struct sgsh_node));
	ck_assert_int_eq(try_add_sgsh_edge(), OP_SUCCESS);

	/* New edge: from existing to new node */
	/* Better in a setup function. */ 
	chosen_mb->origin.fd_direction = STDOUT_FILENO;   
	chosen_mb->origin.index = 0;
	/* self_node_io_side should also be set; it is set in setup */
	self_node_io_side.index = new.index;
	self_node_io_side.fd_direction = STDIN_FILENO;
	ck_assert_int_eq(try_add_sgsh_edge(), OP_SUCCESS);

	/* NOOP: message block created just now */
	chosen_mb->origin.index = -1;
	ck_assert_int_eq(try_add_sgsh_edge(), OP_NOOP);
}
END_TEST

START_TEST(test_add_edge)
{
	struct sgsh_edge new;
	new.from = 2;
	new.to = 3;
	new.instances = 0;
	ck_assert_int_eq(add_edge(&new), OP_SUCCESS);
	ck_assert_int_eq(chosen_mb->n_edges, 6);
}
END_TEST

START_TEST(test_fill_sgsh_edge)
{
	struct sgsh_edge new;
	/* STDIN -> STDOUT */
	/* Better in a setup function. */ 
	chosen_mb->origin.fd_direction = STDOUT_FILENO;   
	chosen_mb->origin.index = 0;
	/* self_node_io_side should also be set; it is set in setup */
	ck_assert_int_eq(fill_sgsh_edge(&new), OP_SUCCESS);
	
	/* Impossible case. No such origin. */
	chosen_mb->origin.index = 7;
	ck_assert_int_eq(fill_sgsh_edge(&new), OP_ERROR);
	
	/* STDOUT -> STDIN */
	chosen_mb->origin.fd_direction = STDIN_FILENO;   
	chosen_mb->origin.index = 3;
	memcpy(&self_node, &chosen_mb->node_array[0], sizeof(struct sgsh_node));
	self_node_io_side.fd_direction = STDOUT_FILENO;   
	self_node_io_side.index = 0;
	/* self_node_io_side should also be set; it is set in setup */
	ck_assert_int_eq(fill_sgsh_edge(&new), OP_SUCCESS);
	
}
END_TEST

START_TEST(test_lookup_sgsh_edge)
{
	struct sgsh_edge new;
	new.from = 2;
	new.to = 3;
	ck_assert_int_eq(lookup_sgsh_edge(&new), OP_CREATE);
	ck_assert_int_eq(lookup_sgsh_edge(&chosen_mb->edge_array[4]), OP_EXISTS);
}
END_TEST

START_TEST(test_add_node)
{
	struct sgsh_node new;
	new.pid = 104;
	memcpy(new.name, "proc4", 6);
	new.requires_channels = 1;
	new.provides_channels = 1;
	memcpy(&self_node, &new, sizeof(struct sgsh_node));
	ck_assert_int_eq(add_node(), OP_SUCCESS);
	ck_assert_int_eq(chosen_mb->n_nodes, 5);
	ck_assert_int_eq(self_node_io_side.index, 4);
	ck_assert_int_eq(self_node.index, 4);
}
END_TEST

START_TEST(test_construct_message_block)
{
	DPRINTF("%s()", __func__);
	int pid = 7;
	ck_assert_int_eq(construct_message_block(pid), OP_SUCCESS);
	ck_assert_int_eq(chosen_mb->version, 1.0);
        ck_assert_int_eq((long)chosen_mb->node_array, 0);
        ck_assert_int_eq(chosen_mb->n_nodes, 0);
        ck_assert_int_eq(chosen_mb->initiator_pid, pid);
        ck_assert_int_eq(chosen_mb->state_flag, PROT_STATE_NEGOTIATION);
        ck_assert_int_eq(chosen_mb->serial_no, 0);
        ck_assert_int_eq(chosen_mb->origin.index, -1);
        ck_assert_int_eq(chosen_mb->origin.fd_direction, -1);
	free(chosen_mb);
}
END_TEST

START_TEST(test_validate_input)
{
	ck_assert_int_eq(validate_input(0, 0, NULL), OP_ERROR); 
	ck_assert_int_eq(validate_input(0, 0, "test"), OP_ERROR); 
	ck_assert_int_eq(validate_input(0, 1, "test"), OP_SUCCESS); 
	ck_assert_int_eq(validate_input(1, 0, "test"), OP_SUCCESS); 
	ck_assert_int_eq(validate_input(-1, -1, "test"), OP_SUCCESS); 
	ck_assert_int_eq(validate_input(-2, -1, "test"), OP_ERROR); 
	ck_assert_int_eq(validate_input(-1, -2, "test"), OP_ERROR); 
	ck_assert_int_eq(validate_input(1000, 1000, "test"), OP_SUCCESS); 
	ck_assert_int_eq(validate_input(1000, 1001, "test"), OP_ERROR); 
	ck_assert_int_eq(validate_input(1001, 1000, "test"), OP_ERROR); 
}
END_TEST

START_TEST(test_sgsh_negotiate)
{
	int *input_fds;
	int n_input_fds;
	int *output_fds;
	int n_output_fds;
	ck_assert_int_eq(sgsh_negotiate("test", 0, 0, &input_fds, &n_input_fds, 
				&output_fds, &n_output_fds), PROT_STATE_ERROR);
}
END_TEST

Suite *
suite_connect(void)
{
	Suite *s = suite_create("Connect");

	TCase *tc_rif = tcase_create("read input fds");
	tcase_add_checked_fixture(tc_rif, setup_test_read_input_fds,
					  retire_test_read_input_fds);
	tcase_add_test(tc_rif, test_read_input_fds);
	suite_add_tcase(s, tc_rif);

	TCase *tc_eic = tcase_create("establish io connections");
	tcase_add_checked_fixture(tc_eic, setup_test_establish_io_connections,
					  retire_test_establish_io_connections);
	tcase_add_test(tc_eic, test_establish_io_connections);
	suite_add_tcase(s, tc_eic);

	TCase *tc_anc = tcase_create("alloc node connections");
	tcase_add_checked_fixture(tc_anc, NULL, NULL);
	tcase_add_test(tc_anc, test_alloc_node_connections);
	suite_add_tcase(s, tc_anc);

	TCase *tc_sd = tcase_create("set dispatcher");
	tcase_add_checked_fixture(tc_sd, setup_test_set_dispatcher,
					 retire_test_set_dispatcher);
	tcase_add_test(tc_sd, test_set_dispatcher);
	suite_add_tcase(s, tc_sd);

	/* Need to also simulate sendmsg; make sure it works. */
	TCase *tc_awof = tcase_create("allocate write output fds");
	tcase_add_checked_fixture(tc_awof, setup_test_alloc_write_output_fds,
					   retire_test_alloc_write_output_fds);
	tcase_add_test(tc_awof, test_alloc_write_output_fds);
	suite_add_tcase(s, tc_awof);

	return s;
}

Suite *
suite_solve(void)
{
	Suite *s = suite_create("Solve");

	TCase *tc_rgs = tcase_create("read graph solution");
	tcase_add_checked_fixture(tc_rgs, setup_test_read_graph_solution,
					  retire_test_read_graph_solution);
	tcase_add_test(tc_rgs, test_read_graph_solution);
	suite_add_tcase(s, tc_rgs);

	TCase *tc_ssg = tcase_create("solve sgsh graph");
	tcase_add_checked_fixture(tc_ssg, setup_test_solve_sgsh_graph,
					  retire_test_solve_sgsh_graph);
	tcase_add_test(tc_ssg, test_solve_sgsh_graph);
	suite_add_tcase(s, tc_ssg);

	TCase *tc_fgs = tcase_create("free graph solution");
	tcase_add_checked_fixture(tc_fgs, setup_test_free_graph_solution,
					  retire_test_free_graph_solution);
	tcase_add_test(tc_fgs, test_free_graph_solution);
	suite_add_tcase(s, tc_fgs);

	TCase *tc_dmic = tcase_create("dry match io constraints");
	tcase_add_checked_fixture(tc_dmic, setup_test_dry_match_io_constraints,
					  retire_test_dry_match_io_constraints);
	tcase_add_test(tc_dmic, test_dry_match_io_constraints);
	suite_add_tcase(s, tc_dmic);

	TCase *tc_sic = tcase_create("satisfy io constraints");
	tcase_add_checked_fixture(tc_sic, setup_test_satisfy_io_constraints,
					  retire_test_satisfy_io_constraints);
	tcase_add_test(tc_sic, test_satisfy_io_constraints);
	suite_add_tcase(s, tc_sic);

	TCase *tc_ec = tcase_create("evaluate constraints");
	tcase_add_checked_fixture(tc_ec, setup_test_eval_constraints,
					 retire_test_eval_constraints);
	tcase_add_test(tc_ec, test_eval_constraints);
	suite_add_tcase(s, tc_ec);

	TCase *tc_aei = tcase_create("assign edge instances");
	tcase_add_checked_fixture(tc_aei, setup_test_assign_edge_instances,
					  retire_test_assign_edge_instances);
	tcase_add_test(tc_aei, test_assign_edge_instances);
	suite_add_tcase(s, tc_aei);

	TCase *tc_repa = tcase_create("reallocate edge pointer array");
	tcase_add_checked_fixture(tc_repa,
				setup_test_reallocate_edge_pointer_array,
				retire_test_reallocate_edge_pointer_array);
	tcase_add_test(tc_repa, test_reallocate_edge_pointer_array);
	suite_add_tcase(s, tc_repa);

	TCase *tc_mcea = tcase_create("make compact edge array");
	tcase_add_checked_fixture(tc_mcea, setup_test_make_compact_edge_array,
					   retire_test_make_compact_edge_array);
	tcase_add_test(tc_mcea, test_make_compact_edge_array);
	suite_add_tcase(s, tc_mcea);

	return s;
}

Suite *
suite_broadcast(void)
{
	Suite *s = suite_create("Broadcast");

	TCase *tc_trm = tcase_create("try read message block");
	tcase_add_checked_fixture(tc_trm, setup_test_try_read_message_block,
					  retire_test_try_read_message_block);
	tcase_add_test(tc_trm, test_try_read_message_block);
	suite_add_tcase(s, tc_trm);

	TCase *tc_trc = tcase_create("try read chunk");
	tcase_add_checked_fixture(tc_trc, setup_test_try_read_chunk, NULL);
	tcase_add_test(tc_trc, test_try_read_chunk);
	suite_add_tcase(s, tc_trc);

	TCase *tc_clr = tcase_create("call read");
	tcase_add_checked_fixture(tc_clr, NULL, NULL);
	tcase_add_test(tc_clr, test_call_read);
	suite_add_tcase(s, tc_clr);

	TCase *tc_acm = tcase_create("alloc copy message block");
	tcase_add_checked_fixture(tc_acm, NULL, NULL);
	tcase_add_test(tc_acm, test_alloc_copy_mb);
	suite_add_tcase(s, tc_acm);

	TCase *tc_acn = tcase_create("alloc copy nodes");
	tcase_add_checked_fixture(tc_acn, setup_test_alloc_copy_nodes,
					  retire_test_alloc_copy_nodes);
	tcase_add_test(tc_acn, test_alloc_copy_nodes);
	suite_add_tcase(s, tc_acn);

	TCase *tc_ace = tcase_create("alloc copy edges");
	tcase_add_checked_fixture(tc_ace, setup_test_alloc_copy_edges,
					  retire_test_alloc_copy_edges);
	tcase_add_test(tc_ace, test_alloc_copy_edges);
	suite_add_tcase(s, tc_ace);

	TCase *tc_cr = tcase_create("check read");
	tcase_add_checked_fixture(tc_cr, NULL, NULL);
	tcase_add_test(tc_cr, test_check_read);
	suite_add_tcase(s, tc_cr);

	TCase *tc_pid = tcase_create("point io direction");
	tcase_add_checked_fixture(tc_pid, setup_test_point_io_direction,
					  retire_test_point_io_direction);
	tcase_add_test(tc_pid, test_point_io_direction);
	suite_add_tcase(s, tc_pid);

	TCase *tc_cmb = tcase_create("compete message block");
	tcase_add_checked_fixture(tc_cmb, setup_test_compete_message_block,
					  retire_test_compete_message_block);
	tcase_add_test(tc_cmb, test_compete_message_block);
	suite_add_tcase(s, tc_cmb);

	TCase *tc_fm = tcase_create("free message block");
	tcase_add_checked_fixture(tc_fm, setup_test_free_mb, NULL);
	tcase_add_test(tc_fm, test_free_mb);
	suite_add_tcase(s, tc_fm);

	TCase *tc_fsn = tcase_create("fill sgsh node");
	tcase_add_checked_fixture(tc_fsn, setup_test_fill_sgsh_node, NULL);
	tcase_add_test(tc_fsn, test_fill_sgsh_node);
	suite_add_tcase(s, tc_fsn);

	TCase *tc_tasn = tcase_create("try add sgsh node");
	tcase_add_checked_fixture(tc_tasn, setup_test_try_add_sgsh_node,
					   retire_test_try_add_sgsh_node);
	tcase_add_test(tc_tasn, test_try_add_sgsh_node);
	suite_add_tcase(s, tc_tasn);

	TCase *tc_tase = tcase_create("try add sgsh edge");
	tcase_add_checked_fixture(tc_tase, setup_test_try_add_sgsh_edge,
					   retire_test_try_add_sgsh_edge);
	tcase_add_test(tc_tase, test_try_add_sgsh_edge);
	suite_add_tcase(s, tc_tase);

	TCase *tc_ae = tcase_create("add edge");
	tcase_add_checked_fixture(tc_ae, setup_test_add_edge,
					 retire_test_add_edge);
	tcase_add_test(tc_ae, test_add_edge);
	suite_add_tcase(s, tc_ae);

	TCase *tc_fse = tcase_create("fill sgsh edge");
	tcase_add_checked_fixture(tc_fse, setup_test_fill_sgsh_edge,
					 retire_test_fill_sgsh_edge);
	tcase_add_test(tc_fse, test_fill_sgsh_edge);
	suite_add_tcase(s, tc_fse);

	TCase *tc_lse = tcase_create("lookup sgsh edge");
	tcase_add_checked_fixture(tc_lse, setup_test_lookup_sgsh_edge,
					 retire_test_lookup_sgsh_edge);
	tcase_add_test(tc_lse, test_lookup_sgsh_edge);
	suite_add_tcase(s, tc_lse);

	TCase *tc_an = tcase_create("add node");
	tcase_add_checked_fixture(tc_an, setup_test_add_node,
					 retire_test_add_node);
	tcase_add_test(tc_an, test_add_node);
	suite_add_tcase(s, tc_an);

	TCase *tc_cnmb = tcase_create("construct message block");
	tcase_add_checked_fixture(tc_cnmb, NULL, NULL);
	tcase_add_test(tc_cnmb, test_construct_message_block);
	suite_add_tcase(s, tc_cnmb);

	TCase *tc_vi = tcase_create("validate input");
	tcase_add_checked_fixture(tc_vi, NULL, NULL);
	tcase_add_test(tc_vi, test_validate_input);
	suite_add_tcase(s, tc_vi);

	TCase *tc_sn = tcase_create("sgsh negotiate");
	tcase_add_checked_fixture(tc_sn, setup, retire);
	tcase_add_test(tc_sn, test_sgsh_negotiate);
	suite_add_tcase(s, tc_sn);

	return s;
}

int run_suite(Suite *s) {
	int number_failed;
	SRunner *sr = srunner_create(s);
	srunner_run_all(sr, CK_VERBOSE);
	number_failed = srunner_ntests_failed(sr);
	srunner_free(sr);
	return (number_failed == 0) ? 1 : 0;
}

int
run_suite_connect(void) {
	Suite *s = suite_connect();
	return run_suite(s);
}

int
run_suite_solve(void) {
	Suite *s = suite_solve();
	return run_suite(s);
}

int
run_suite_broadcast(void) {
	Suite *s = suite_broadcast();
	return run_suite(s);
}

/* Output is not appropriate; only pass fail. */
int main() {
	int failed_neg, failed_sol, failed_con;
	failed_neg = run_suite_broadcast();
	failed_sol = run_suite_solve();
	failed_con = run_suite_connect();
	return (failed_neg && failed_sol && failed_con) ? EXIT_SUCCESS : EXIT_FAILURE;
}

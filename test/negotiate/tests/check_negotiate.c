#include <check.h> /* Check unit test framework API. */
#include <stdlib.h> /* EXIT_SUCCESS, EXIT_FAILURE */
#include "../src/sgsh-negotiate.h"
#include "../src/negotiate.c" /* struct definitions, static structures */


struct sgsh_edge *edges;
struct sgsh_edge *compact_edges;
struct sgsh_edge **pointers_to_edges;
struct sgsh_node *nodes;
int n_ptedges;
int n_nodes;
int n_edges;

int *args;

int *input_fds;
int n_input_fds;
int *output_fds;
int n_output_fds;

void
setup(void)
{
        
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
        chosen_mb = (struct sgsh_negotiation *)malloc(sizeof(struct sgsh_negotiation));
        chosen_mb->version = sgsh_version;
        chosen_mb->node_array = nodes;
        chosen_mb->n_nodes = n_nodes;
        chosen_mb->edge_array = edges;
        chosen_mb->n_edges = n_edges;
        
        n_ptedges = 2;
	pointers_to_edges = (struct sgsh_edge **)malloc(sizeof(struct sgsh_edge *) *n_ptedges);
	int i;
	for (i = 0; i < n_ptedges; i++) {
		pointers_to_edges[i] = (struct sgsh_edge *)malloc(sizeof(struct sgsh_edge));	
		pointers_to_edges[i]->from = i;
		pointers_to_edges[i]->to = 3; // the node.
		pointers_to_edges[i]->instances = 0;
        }

        /* test_eval_constraints */
        args = (int *)malloc(sizeof(int) * 3);
        args[0] = -1;
        args[1] = -1;
        args[2] = -1;

	/* establish_io_connections */
	/* fill in self_node */
	memcpy(&self_node, &nodes[3], sizeof(struct sgsh_node));
	/* fill in self_pipe_fds */
	self_pipe_fds.n_input_fds = 2;
	self_pipe_fds.input_fds = (int *)malloc(sizeof(int) *
						self_pipe_fds.n_input_fds);
	self_pipe_fds.input_fds[0] = 0;
	self_pipe_fds.input_fds[0] = 3;
	self_pipe_fds.n_output_fds = 0;

	
}

void
retire(void)
{
	int i;
	for (i = 0; i < n_ptedges; i++)
		free(pointers_to_edges[i]);
	free(pointers_to_edges);
	free(compact_edges);
        free(nodes);
        free(edges);
        free(chosen_mb);

        free(args);
	if (n_input_fds > 0)
		free(input_fds);
	if (n_output_fds > 0)
		free(output_fds);
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
	free_graph_solution(chosen_mb->n_nodes - 1);
	retire();

	/* An impossible case. */
	setup();
	chosen_mb->node_array[3].requires_channels = 1;
	ck_assert_int_eq(solve_sgsh_graph(), OP_ERROR);
	retire();

	/* Relaxing our target node's constraint. */
	setup();
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
	free_graph_solution(chosen_mb->n_nodes - 1);	
	retire();

	/* Relaxing also pair nodes' constraints. */
	setup();
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
	free_graph_solution(chosen_mb->n_nodes - 1);
}
END_TEST

void
setup_gs(void)
{
	setup();
	int i;
	graph_solution = (struct sgsh_node_connections *)malloc(
			sizeof(struct sgsh_node_connections) * n_nodes);
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

}

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
	retire_dmic();

	/* Relaxing also pair nodes' constraints. Need to reset the testbed. */
	setup();
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
	retire();

        /* Expand the semantics of remaining_free_channels to fixed constraints
           as in this case. */  
        /* Fixed constraint node, flexible pair, plenty. */
	setup();
        chosen_mb->node_array[0].provides_channels = -1;
	DPRINTF("3.1: %d", chosen_mb->node_array[0].provides_channels);
	ck_assert_int_eq(satisfy_io_constraints(5, pointers_to_edges, 2, 1),
									OP_SUCCESS);
	retire();

        /* Flexible constraint both sides */
	setup();
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
	retire();

        /* Flexible with extra instances, but no remaining. */
	setup();
        chosen_mb->node_array[0].provides_channels = -1;
	ck_assert_int_eq(assign_edge_instances(pointers_to_edges, n_ptedges, -1, 1, 1, 5, 0, 6), OP_SUCCESS);
	retire();

        /* Flexible with extra instances, including remaining. */
	setup();
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

START_TEST(test_alloc_node_connections)
{
	struct sgsh_edge *test;
	ck_assert_int_eq(alloc_node_connections(NULL, 2, 1, 2), OP_ERROR);
	ck_assert_int_eq(alloc_node_connections(&test, 0, 1, 2), OP_ERROR);
	ck_assert_int_eq(alloc_node_connections(&test, -1, 1, 2), OP_ERROR);
	ck_assert_int_eq(alloc_node_connections(&test, 1, 2, 2), OP_ERROR);
	ck_assert_int_eq(alloc_node_connections(&test, 1, -1, 2), OP_ERROR);
	ck_assert_int_eq(alloc_node_connections(&test, 1, 1, -2), OP_ERROR);
	ck_assert_int_eq(alloc_node_connections(&test, 1, 1, 2), OP_SUCCESS);
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

START_TEST(test_negotiate_api)
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

	TCase *tc_eic = tcase_create("establish_io_connections");
	tcase_add_checked_fixture(tc_eic, setup, retire);
	tcase_add_test(tc_eic, test_establish_io_connections);
	tcase_add_test(tc_eic, test_alloc_node_connections);
	suite_add_tcase(s, tc_eic);

	return s;
}

Suite *
suite_solve(void)
{
	Suite *s = suite_create("Solve");

	TCase *tc_ssg = tcase_create("solve_sgsh_graph");
	tcase_add_checked_fixture(tc_ssg, setup, retire);
	tcase_add_test(tc_ssg, test_solve_sgsh_graph);
	suite_add_tcase(s, tc_ssg);

	TCase *tc_fgs = tcase_create("free_graph_solution");
	tcase_add_checked_fixture(tc_fgs, setup_gs, retire);
	tcase_add_test(tc_fgs, test_free_graph_solution);
	suite_add_tcase(s, tc_fgs);

	TCase *tc_dmic = tcase_create("dry_match_io_constraints");
	tcase_add_checked_fixture(tc_dmic, setup, retire_dmic);
	tcase_add_test(tc_dmic, test_dry_match_io_constraints);
	suite_add_tcase(s, tc_dmic);

	/* Unsure whether to break one by one. */
	TCase *tc_core = tcase_create("solution_functions");
	tcase_add_checked_fixture(tc_core, setup, retire);
	tcase_add_test(tc_core, test_satisfy_io_constraints);
	tcase_add_test(tc_core, test_eval_constraints);
	tcase_add_test(tc_core, test_assign_edge_instances);
	tcase_add_test(tc_core, test_reallocate_edge_pointer_array);
	tcase_add_test(tc_core, test_make_compact_edge_array);
	suite_add_tcase(s, tc_core);

	return s;
}

Suite *
suite_broadcast(void)
{
	Suite *s = suite_create("Broadcast");

	TCase *tc_core = tcase_create("Core");
	tcase_add_checked_fixture(tc_core, setup, retire);
	tcase_add_test(tc_core, test_validate_input);
	tcase_add_test(tc_core, test_negotiate_api);
	suite_add_tcase(s, tc_core);

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

int main() {
	int failed_neg, failed_sol, failed_con;
	failed_neg = run_suite_broadcast();
	failed_sol = run_suite_solve();
	failed_con = run_suite_connect();
	return (failed_neg && failed_sol && failed_con) ? EXIT_SUCCESS : EXIT_FAILURE;
}

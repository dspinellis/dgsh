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
        edges[0].instances = 1;

        edges[1].from = 2;
        edges[1].to = 1;
        edges[1].instances = 1;

        edges[2].from = 1;
        edges[2].to = 0;
        edges[2].instances = 1;

        edges[3].from = 1;
        edges[3].to = 3;
        edges[3].instances = 1;

        edges[4].from = 0;
        edges[4].to = 3;
        edges[4].instances = 1;

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
		pointers_to_edges[i]->instances = 1;
        }

        /* test_eval_constraints */
        args = (int *)malloc(sizeof(int) * 3);
        args[0] = -1;
        args[1] = -1;
        args[2] = -1;
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
}

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
        /* Expand the semantics of remaining_free_channels to fixed constraints
           as in this case. */  
        /* Fixed constraint node, flexible pair, plenty. */
        chosen_mb->node_array[0].provides_channels = -1;
	ck_assert_int_eq(satisfy_io_constraints(5, pointers_to_edges, 2, 1),
									OP_SUCCESS);
        /* Flexible constraint both sides */
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
        /* Flexible with extra instances, but no remaining. */
	ck_assert_int_eq(assign_edge_instances(pointers_to_edges, n_ptedges, -1, 1, 1, 5, 0, 6), OP_SUCCESS);
        /* Flexible with extra instances, including remaining. */
	ck_assert_int_eq(assign_edge_instances(pointers_to_edges, n_ptedges, 7, 1, 1, 5, 1, 7), OP_SUCCESS);

}
END_TEST

START_TEST(test_reallocate_edge_pointer_array)
{
	ck_assert_int_eq(reallocate_edge_pointer_array(NULL, 1), OP_ERROR);
	struct sgsh_edge **p = pointers_to_edges;
	pointers_to_edges = NULL;
	ck_assert_int_eq(reallocate_edge_pointer_array(&pointers_to_edges, n_ptedges + 1), OP_ERROR);
	pointers_to_edges = p;
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
suite_negotiate(void)
{
	Suite *s = suite_create("Negotiate");
	TCase *tc_core = tcase_create("Core");
	tcase_add_checked_fixture(tc_core, setup, retire);
	tcase_add_test(tc_core, test_satisfy_io_constraints);
	tcase_add_test(tc_core, test_eval_constraints);
	tcase_add_test(tc_core, test_assign_edge_instances);
	tcase_add_test(tc_core, test_reallocate_edge_pointer_array);
	tcase_add_test(tc_core, test_make_compact_edge_array);
	tcase_add_test(tc_core, test_alloc_node_connections);
	tcase_add_test(tc_core, test_validate_input);
	tcase_add_test(tc_core, test_negotiate_api);
	suite_add_tcase(s, tc_core);
	return s;
}

int
main(void) {
	int number_failed;
	Suite *s = suite_negotiate();
	SRunner *sr = srunner_create(s);
	srunner_run_all(sr, CK_NORMAL);
	number_failed = srunner_ntests_failed(sr);
	srunner_free(sr);
	return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}

#include <check.h> /* Check unit test framework API. */
#include <stdlib.h> /* EXIT_SUCCESS, EXIT_FAILURE */
#include "../src/sgsh-negotiate.h"


struct sgsh_edge *compact_edges;
struct sgsh_edge **pointers_to_edges;
int n_edges;

void
setup(void)
{
        n_edges = 3;
	pointers_to_edges = (struct sgsh_edge **)malloc(sizeof(struct sgsh_edge *) *n_edges);
	int i;
	for (i = 0; i < n_edges; i++)
		pointers_to_edges[i] = (struct sgsh_edge *)malloc(sizeof(struct sgsh_edge) * n_edges);	
}

void
retire(void)
{
	int i;
	for (i = 0; i < n_edges; i++)
		free(pointers_to_edges[i]);
	free(pointers_to_edges);
	free(compact_edges);
}

START_TEST(test_assign_edge_instances)
{
	ck_assert_int_eq(assign_edge_instances(NULL, n_edges, 2, 1, 0, 0, 2, 5), OP_ERROR);
}
END_TEST

START_TEST(test_reallocate_edge_pointer_array)
{
	ck_assert_int_eq(reallocate_edge_pointer_array(NULL, 1), OP_ERROR);
	struct sgsh_edge **p = pointers_to_edges;
	pointers_to_edges = NULL;
	ck_assert_int_eq(reallocate_edge_pointer_array(&pointers_to_edges, n_edges + 1), OP_ERROR);
	pointers_to_edges = p;
	ck_assert_int_eq(reallocate_edge_pointer_array(&pointers_to_edges, -2), OP_ERROR);
	ck_assert_int_eq(reallocate_edge_pointer_array(&pointers_to_edges, 0), OP_ERROR);
	/* Not incresing the value of n_edges to not perplex freeing 
         * pointers_to_edges because reallocation only accounts for
         * struct sgsh_edge *.
         */
	ck_assert_int_eq(reallocate_edge_pointer_array(&pointers_to_edges, n_edges + 1), OP_SUCCESS);
}
END_TEST


START_TEST(test_make_compact_edge_array)
{
	ck_assert_int_eq(make_compact_edge_array(NULL, 2, pointers_to_edges), OP_ERROR);
	ck_assert_int_eq(make_compact_edge_array(&compact_edges, -2, pointers_to_edges), OP_ERROR);
	ck_assert_int_eq(make_compact_edge_array(&compact_edges, 0, pointers_to_edges), OP_ERROR);
	ck_assert_int_eq(make_compact_edge_array(&compact_edges, n_edges, NULL), OP_ERROR);
	struct sgsh_edge *p = pointers_to_edges[0];
	pointers_to_edges[0] = NULL;
	ck_assert_int_eq(make_compact_edge_array(&compact_edges, n_edges, pointers_to_edges), OP_ERROR);
	pointers_to_edges[0] = p;
	ck_assert_int_eq(make_compact_edge_array(&compact_edges, n_edges, pointers_to_edges), OP_SUCCESS);
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

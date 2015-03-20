#include <check.h> /* Check unit test framework API. */
#include <stdlib.h> /* EXIT_SUCCESS, EXIT_FAILURE */
#include "../src/sgsh-negotiate.h"

/* Defined in ../src/negotiate.c */
#define OP_SUCCESS 0
#define OP_ERROR 1

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
	ck_assert_int_eq(validate_input(0, 0, "test"), OP_ERROR); 
}
END_TEST

Suite *
suite_negotiate(void)
{
	Suite *s = suite_create("Negotiate");
	TCase *tc_core = tcase_create("Core");
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

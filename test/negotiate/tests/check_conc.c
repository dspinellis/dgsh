

#include <check.h>  /* Check unit test framework API. */
#include <stdlib.h> /* EXIT_SUCCESS, EXIT_FAILURE */
#include <stdbool.h> /* EXIT_SUCCESS, EXIT_FAILURE */
#include "../src/sgsh-internal-api.h"

START_TEST (test_next_fd)
{
	multiple_inputs = true;
	nfd = 5;
	ck_assert_int_eq(next_fd(0), 1);
	ck_assert_int_eq(next_fd(1), 4);
	ck_assert_int_eq(next_fd(4), 3);
	ck_assert_int_eq(next_fd(3), 0);

	multiple_inputs = false;
	ck_assert_int_eq(next_fd(0), 1);
	ck_assert_int_eq(next_fd(1), 3);
	ck_assert_int_eq(next_fd(3), 4);
	ck_assert_int_eq(next_fd(4), 0);
}
END_TEST

Suite *
suite_conc(void)
{
	Suite *s = suite_create("Concentrator");
	TCase *tc_tn = tcase_create("test next_fd");
	tcase_add_checked_fixture(tc_tn, NULL, NULL);
	tcase_add_test(tc_tn, test_next_fd);
	suite_add_tcase(s, tc_tn);
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

int main(void)
{
	Suite *s = suite_conc();
	return (run_suite(s) ? EXIT_SUCCESS : EXIT_FAILURE);
}



#include <check.h>  /* Check unit test framework API. */
#include <stdlib.h> /* EXIT_SUCCESS, EXIT_FAILURE */
#include <stdbool.h> /* EXIT_SUCCESS, EXIT_FAILURE */
#include "../src/sgsh-internal-api.h"		/* chosen_mb */
#include "../src/sgsh-conc.c"			/* pi */

void
setup_test_is_ready(void)
{
	pi = (struct portinfo *)calloc(3, sizeof(struct portinfo));
	pi[0].pid = 100;
	pi[0].seen = true;
	pi[0].written = true;
	pi[1].pid = 101;
	pi[1].seen = true;
	pi[1].written = false;

	chosen_mb = (struct sgsh_negotiation *)
		malloc(sizeof(struct sgsh_negotiation));
	chosen_mb->state = PS_RUN;
	chosen_mb->preceding_process_pid = 101;
}

void
retire_test_is_ready(void)
{
	free(pi);
	free(chosen_mb);
}

START_TEST(test_is_ready)
{
	ck_assert_int_eq(is_ready(0, chosen_mb), true);

	pi[0].seen = false;
	ck_assert_int_eq(is_ready(0, chosen_mb), false);

	chosen_mb->preceding_process_pid = 100;
	ck_assert_int_eq(is_ready(0, chosen_mb), true);
	ck_assert_int_eq(pi[0].seen, true);
	ck_assert_int_eq(pi[1].written, true);
	ck_assert_int_eq(pi[1].run_ready, true);
}
END_TEST

START_TEST (test_next_fd)
{
	multiple_inputs = true;
	nfd = 5;
	bool ro = false;		/* restore origin */
	ck_assert_int_eq(next_fd(0, &ro), 1);
	ck_assert_int_eq(ro, false);
	ck_assert_int_eq(next_fd(1, &ro), 4);
	ck_assert_int_eq(ro, false);
	ck_assert_int_eq(next_fd(4, &ro), 3);
	ck_assert_int_eq(ro, true);
	ro = false;
	ck_assert_int_eq(next_fd(3, &ro), 0);
	ck_assert_int_eq(ro, true);

	pass_origin = true;
	ro = false;		/* restore origin */
	ck_assert_int_eq(next_fd(0, &ro), 1);
	ck_assert_int_eq(ro, false);
	ck_assert_int_eq(next_fd(1, &ro), 0);
	ck_assert_int_eq(ro, false);
	ck_assert_int_eq(next_fd(4, &ro), 4);
	ck_assert_int_eq(ro, true);
	ro = false;
	ck_assert_int_eq(next_fd(3, &ro), 3);
	ck_assert_int_eq(ro, true);
	pass_origin = false;

	multiple_inputs = false;
	ro = false;
	ck_assert_int_eq(next_fd(0, &ro), 1);
	ck_assert_int_eq(ro, false);
	ck_assert_int_eq(next_fd(1, &ro), 3);
	ck_assert_int_eq(ro, true);
	ro = false;
	ck_assert_int_eq(next_fd(3, &ro), 4);
	ck_assert_int_eq(ro, true);
	ro = false;
	ck_assert_int_eq(next_fd(4, &ro), 0);
	ck_assert_int_eq(ro, false);
}
END_TEST


Suite *
suite_conc(void)
{
	Suite *s = suite_create("Concentrator");
	TCase *tc_tn = tcase_create("test next_fd");
	TCase *tc_ir = tcase_create("test is_ready");

	tcase_add_checked_fixture(tc_tn, NULL, NULL);
	tcase_add_test(tc_tn, test_next_fd);
	suite_add_tcase(s, tc_tn);
	tcase_add_checked_fixture(tc_ir, setup_test_is_ready,
			retire_test_is_ready);
	tcase_add_test(tc_ir, test_is_ready);
	suite_add_tcase(s, tc_ir);

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

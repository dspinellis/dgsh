

#include <check.h>  /* Check unit test framework API. */
#include <stdio.h> /* snprintf */
#include <stdlib.h> /* EXIT_SUCCESS, EXIT_FAILURE */
#include <stdbool.h> /* EXIT_SUCCESS, EXIT_FAILURE */
#include <unistd.h> /* pipe */
#include <sys/types.h>
#include <sys/socket.h> /* socket */
#include <sys/un.h> /* sockaddr_un */
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

START_TEST (test_read_write_fd)
{
	int pipefd[2];
	int sock, rsock;
	int readfd;
	char msg[] = "hello";
	char buff[20];
	int n;
	pid_t pid;
	socklen_t len;
	struct sockaddr_un local, remote;

	if (pipe(pipefd) == -1)
		err(1, "pipe");
	local.sun_family = AF_UNIX;
	snprintf(local.sun_path, sizeof(local.sun_path), "/tmp/conc-%d", getpid());
	len = strlen(local.sun_path) + 1 + sizeof(local.sun_family);

	switch ((pid = fork())) {
	case 0:
		/* Child: connect, pass fd and write test message */
		if ((sock = socket(AF_UNIX, SOCK_STREAM, 0)) == -1)
			err(1, "socket");
		sleep(1);
		if (connect(sock, (struct sockaddr *)&local, len) == -1)
			err(1, "connect %s", local.sun_path);
		write_fd(sock, pipefd[STDIN_FILENO]);
		close(sock); // Should wait for data to be transmitted
		close(pipefd[STDIN_FILENO]);
		sleep(1);
		if (write(pipefd[STDOUT_FILENO], msg, sizeof(msg)) <= 0)
			err(1, "write");
		exit(0);
	default:
		/* Parent: accept connection, read fd and read test message */
		close(pipefd[STDIN_FILENO]);
		close(pipefd[STDOUT_FILENO]);
		if ((sock = socket(AF_UNIX, SOCK_STREAM, 0)) == -1)
			err(1, "socket");
		if (bind(sock, (struct sockaddr *)&local, len) == -1)
			err(1, "bind %s", local.sun_path);
		if (listen(sock, 5) == -1)
			err(1, "listen");
		rsock = accept(sock, (struct sockaddr *)&remote, &len);
		readfd = read_fd(rsock);
		if ((n = read(readfd, buff, sizeof(buff))) == -1)
			err(1, "read");
		(void)unlink(local.sun_path);
		ck_assert_int_eq(n, sizeof(msg));
		ck_assert_str_eq(msg, buff);
		break;
	case -1:	/* Error */
		err(1, "fork");
	}

}
END_TEST

Suite *
suite_conc(void)
{
	Suite *s = suite_create("Concentrator");
	TCase *tc_tn = tcase_create("test next_fd");
	TCase *tc_trw = tcase_create("test read/write fd");

	tcase_add_checked_fixture(tc_tn, NULL, NULL);
	tcase_add_test(tc_tn, test_next_fd);
	suite_add_tcase(s, tc_tn);

	tcase_add_checked_fixture(tc_trw, NULL, NULL);
	tcase_add_test(tc_trw, test_read_write_fd);
	suite_add_tcase(s, tc_trw);

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
